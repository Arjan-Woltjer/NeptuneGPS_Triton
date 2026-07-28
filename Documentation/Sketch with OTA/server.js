/**
 * Triton IO OTA Server
 * Node.js / Express — firmware version management & binary hosting
 *
 * Endpoints:
 *   GET  /api/firmware/latest        → returns latest firmware metadata
 *   GET  /api/firmware/:version/bin  → streams the .bin file
 *   POST /api/firmware/upload        → upload a new firmware binary (admin)
 *   GET  /api/devices                → list devices & their reported versions
 *   POST /api/devices/checkin        → device check-in with current version
 *
 * Usage:
 *   npm install
 *   node server.js
 */

const express    = require('express');
const multer     = require('multer');
const fs         = require('fs');
const path       = require('path');
const crypto     = require('crypto');
const https      = require('https');
const helmet     = require('helmet');
const rateLimit  = require('express-rate-limit');

const app  = express();
const PORT = process.env.PORT || 3000;

// ─── Config ───────────────────────────────────────────────────────────────────

const FIRMWARE_DIR  = path.join(__dirname, 'firmware_store');
const METADATA_FILE = path.join(FIRMWARE_DIR, 'metadata.json');
const DEVICE_DB     = path.join(__dirname, 'devices.json');
const ADMIN_KEY     = process.env.ADMIN_API_KEY || 'change-me-in-production';

// Ensure dirs exist
fs.mkdirSync(FIRMWARE_DIR, { recursive: true });

// ─── Middleware ───────────────────────────────────────────────────────────────

app.use(helmet());
app.use(express.json());

// Rate limit — prevents abuse / DoS from devices
const deviceLimiter = rateLimit({
  windowMs: 15 * 60 * 1000,  // 15 minutes
  max: 60,
  message: { error: 'Too many requests' }
});

const uploadLimiter = rateLimit({
  windowMs: 60 * 60 * 1000,
  max: 10,
  message: { error: 'Upload rate limit exceeded' }
});

// ─── Helpers ──────────────────────────────────────────────────────────────────

function loadMetadata() {
  if (!fs.existsSync(METADATA_FILE)) return { versions: [], latest: null };
  return JSON.parse(fs.readFileSync(METADATA_FILE, 'utf8'));
}

function saveMetadata(data) {
  fs.writeFileSync(METADATA_FILE, JSON.stringify(data, null, 2));
}

function loadDevices() {
  if (!fs.existsSync(DEVICE_DB)) return {};
  return JSON.parse(fs.readFileSync(DEVICE_DB, 'utf8'));
}

function saveDevices(data) {
  fs.writeFileSync(DEVICE_DB, JSON.stringify(data, null, 2));
}

function sha256File(filePath) {
  return new Promise((resolve, reject) => {
    const hash   = crypto.createHash('sha256');
    const stream = fs.createReadStream(filePath);
    stream.on('data',  d => hash.update(d));
    stream.on('end',   () => resolve(hash.digest('hex')));
    stream.on('error', reject);
  });
}

function requireAdmin(req, res, next) {
  const key = req.headers['x-admin-key'];
  if (key !== ADMIN_KEY) return res.status(401).json({ error: 'Unauthorized' });
  next();
}

// ─── Routes ───────────────────────────────────────────────────────────────────

/**
 * GET /api/firmware/latest
 * Devices poll this to check if a newer version exists.
 * Returns metadata including download URL and sha256 hash.
 */
app.get('/api/firmware/latest', deviceLimiter, (req, res) => {
  const deviceId      = req.headers['x-device-id'] || 'unknown';
  const currentVersion = parseInt(req.headers['x-current-version'] || '0', 10);
  const meta          = loadMetadata();

  console.log(`[CHECKIN] Device ${deviceId} | running v${currentVersion}`);

  if (!meta.latest) {
    return res.status(404).json({ error: 'No firmware available' });
  }

  const latest = meta.versions.find(v => v.version === meta.latest);
  if (!latest) {
    return res.status(500).json({ error: 'Metadata corrupt' });
  }

  res.json({
    version: latest.version,
    url:     `${process.env.PUBLIC_BASE_URL || `http://localhost:${PORT}`}/api/firmware/${latest.version}/bin`,
    sha256:  latest.sha256,
    size:    latest.size,
    notes:   latest.notes,
    released_at: latest.released_at
  });
});

/**
 * GET /api/firmware/:version/bin
 * Streams the firmware binary to the requesting device.
 */
app.get('/api/firmware/:version/bin', deviceLimiter, (req, res) => {
  const version = parseInt(req.params.version, 10);
  const meta    = loadMetadata();
  const entry   = meta.versions.find(v => v.version === version);

  if (!entry) return res.status(404).json({ error: 'Version not found' });

  const binPath = path.join(FIRMWARE_DIR, entry.filename);
  if (!fs.existsSync(binPath)) {
    return res.status(404).json({ error: 'Binary file missing' });
  }

  console.log(`[DOWNLOAD] Firmware v${version} → ${req.ip}`);

  res.setHeader('Content-Type', 'application/octet-stream');
  res.setHeader('Content-Disposition', `attachment; filename="${entry.filename}"`);
  res.setHeader('Content-Length', entry.size);
  res.setHeader('X-SHA256', entry.sha256);

  fs.createReadStream(binPath).pipe(res);
});

/**
 * POST /api/firmware/upload
 * Admin endpoint to upload a new firmware binary.
 * Body (multipart/form-data):
 *   file    — the .bin file
 *   version — integer version number
 *   notes   — release notes string
 */
const upload = multer({
  dest: FIRMWARE_DIR,
  limits: { fileSize: 4 * 1024 * 1024 }  // 4MB max
});

app.post('/api/firmware/upload', uploadLimiter, requireAdmin, upload.single('file'), async (req, res) => {
  try {
    const version = parseInt(req.body.version, 10);
    const notes   = req.body.notes || '';

    if (!version || isNaN(version)) {
      fs.unlinkSync(req.file.path);
      return res.status(400).json({ error: 'Invalid version number' });
    }

    const meta = loadMetadata();
    if (meta.versions.some(v => v.version === version)) {
      fs.unlinkSync(req.file.path);
      return res.status(409).json({ error: `Version ${version} already exists` });
    }

    // Rename file to descriptive name
    const filename = `triton_fw_v${version}.bin`;
    const destPath = path.join(FIRMWARE_DIR, filename);
    fs.renameSync(req.file.path, destPath);

    // Compute SHA256
    const sha256 = await sha256File(destPath);
    const size   = fs.statSync(destPath).size;

    const entry = {
      version,
      filename,
      sha256,
      size,
      notes,
      released_at: new Date().toISOString()
    };

    meta.versions.push(entry);
    meta.versions.sort((a, b) => b.version - a.version);
    meta.latest = version;
    saveMetadata(meta);

    console.log(`[UPLOAD] New firmware v${version} | ${size} bytes | SHA256: ${sha256}`);

    res.json({ success: true, version, sha256, size });
  } catch (err) {
    console.error('[UPLOAD ERROR]', err);
    res.status(500).json({ error: 'Upload failed' });
  }
});

/**
 * GET /api/firmware/versions
 * List all available firmware versions (admin).
 */
app.get('/api/firmware/versions', requireAdmin, (req, res) => {
  const meta = loadMetadata();
  res.json(meta);
});

/**
 * PUT /api/firmware/latest/:version
 * Promote a specific version to "latest" (rollback support).
 */
app.put('/api/firmware/latest/:version', requireAdmin, (req, res) => {
  const version = parseInt(req.params.version, 10);
  const meta    = loadMetadata();

  if (!meta.versions.some(v => v.version === version)) {
    return res.status(404).json({ error: 'Version not found' });
  }

  meta.latest = version;
  saveMetadata(meta);

  console.log(`[PROMOTE] Latest set to v${version}`);
  res.json({ success: true, latest: version });
});

/**
 * POST /api/devices/checkin
 * Devices report their current version & status.
 */
app.post('/api/devices/checkin', deviceLimiter, (req, res) => {
  const { device_id, version, uptime_s, free_heap, rssi } = req.body;
  if (!device_id) return res.status(400).json({ error: 'device_id required' });

  const devices = loadDevices();
  devices[device_id] = {
    version,
    uptime_s,
    free_heap,
    rssi,
    last_seen: new Date().toISOString(),
    ip: req.ip
  };
  saveDevices(devices);

  res.json({ success: true });
});

/**
 * GET /api/devices
 * List all devices and their firmware versions (admin).
 */
app.get('/api/devices', requireAdmin, (req, res) => {
  const devices = loadDevices();
  const meta    = loadMetadata();
  const latest  = meta.latest;

  const enriched = Object.entries(devices).map(([id, d]) => ({
    device_id: id,
    ...d,
    up_to_date: d.version === latest
  }));

  res.json({ latest, devices: enriched });
});

// ─── Start ────────────────────────────────────────────────────────────────────

// For production: use HTTPS with your TLS certificate
// const tlsOptions = {
//   key:  fs.readFileSync('server.key'),
//   cert: fs.readFileSync('server.crt')
// };
// https.createServer(tlsOptions, app).listen(PORT, () => {
//   console.log(`Triton OTA server running on https://0.0.0.0:${PORT}`);
// });

app.listen(PORT, () => {
  console.log(`Triton OTA server running on http://0.0.0.0:${PORT}`);
  console.log(`Admin key: ${ADMIN_KEY}`);
});
