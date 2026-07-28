# Triton IO Board — OTA Firmware Update System

Production-grade OTA for the ESP32-based Triton IO board.  
Supports secure HTTPS delivery, automatic rollback, version tracking, and fleet management.

---

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                   OTA Server (Node.js)               │
│                                                      │
│  POST /api/firmware/upload   ← your CI/CD pipeline  │
│  GET  /api/firmware/latest   ← devices poll this    │
│  GET  /api/firmware/:v/bin   ← binary download      │
│  GET  /api/devices           ← fleet dashboard      │
└──────────────────────┬──────────────────────────────┘
                       │  HTTPS
          ┌────────────┴────────────┐
          │                         │
   ┌──────▼──────┐           ┌──────▼──────┐
   │ Triton IO   │           │ Triton IO   │  ...
   │ Board #1    │           │ Board #2    │
   │ running v3  │           │ running v2  │
   └─────────────┘           └─────────────┘
```

### How it works

1. **Device boots** → checks `ESP_OTA_IMG_PENDING_VERIFY` → runs self-test  
2. **Self-test passes** → marks firmware valid (cancels potential rollback)  
3. **Every hour** → polls `/api/firmware/latest` with its current version  
4. **Newer version found** → downloads binary over HTTPS into inactive OTA partition  
5. **Flash complete** → sets boot partition, reboots  
6. **On next boot** → step 1 repeats; if self-test fails, ESP32 rolls back automatically  

---

## File Layout

```
triton-ota/
├── firmware/
│   ├── triton_ota.ino      # Main sketch
│   ├── ota_config.h        # Version, Wi-Fi, server URL, timing
│   ├── ota_certs.h         # CA certificate for HTTPS validation
│   └── partitions.csv      # OTA-capable partition table
├── server/
│   ├── server.js           # Node.js OTA server
│   └── package.json
├── deploy_firmware.sh      # Build + upload automation script
└── README.md
```

---

## Quick Start

### 1. Set up the OTA server

```bash
cd server
npm install

# Set your admin key
export ADMIN_KEY="your-secret-admin-key"

node server.js
# → Listening on http://0.0.0.0:3000
```

For production, enable HTTPS (see commented section in `server.js`) and set:
```bash
export PUBLIC_BASE_URL="https://ota.yourdomain.com"
```

---

### 2. Configure the firmware

Edit `firmware/ota_config.h`:

```c
#define CURRENT_FW_VERSION  1           // Start at 1
#define WIFI_SSID           "MyNetwork"
#define WIFI_PASSWORD       "MyPassword"
#define OTA_SERVER_HOST     "https://ota.yourdomain.com"
```

Replace the CA certificate in `ota_certs.h` with your server's CA cert.

---

### 3. Flash initial firmware (USB)

In Arduino IDE:
1. Select your ESP32 board
2. **Tools → Partition Scheme → Custom** → select `partitions.csv`
3. Upload via USB

All subsequent updates happen over-the-air.

---

### 4. Deploy a new firmware version

```bash
export ADMIN_KEY="your-secret-admin-key"
export OTA_SERVER="https://ota.yourdomain.com"

chmod +x deploy_firmware.sh
./deploy_firmware.sh 2 "Fixed relay debounce, improved ADC reading"
```

This script:
- Bumps `CURRENT_FW_VERSION` in `ota_config.h`
- Compiles via `arduino-cli`
- Uploads binary + metadata to the server
- Verifies the server reports the new version as latest
- Creates a git commit and tag

Devices receive the update on their next hourly poll (or immediately if they check in).

---

## Server API Reference

All admin endpoints require header: `X-Admin-Key: <your-key>`

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET`  | `/api/firmware/latest` | Latest version metadata (devices use this) |
| `GET`  | `/api/firmware/:v/bin` | Download firmware binary |
| `POST` | `/api/firmware/upload` | Upload new firmware (multipart: `file`, `version`, `notes`) |
| `GET`  | `/api/firmware/versions` | List all versions |
| `PUT`  | `/api/firmware/latest/:v` | Promote a version to latest (for rollback) |
| `GET`  | `/api/devices` | List all devices + their reported versions |
| `POST` | `/api/devices/checkin` | Device reports status |

### Example: manual rollback to v1

```bash
curl -X PUT https://ota.yourdomain.com/api/firmware/latest/1 \
  -H "X-Admin-Key: your-key"
```

Devices will then download v1 on their next poll.

---

## Security Checklist

- [ ] Use **HTTPS** on the OTA server with a valid TLS certificate
- [ ] Set a strong `ADMIN_KEY` (not the default)
- [ ] Replace the CA cert in `ota_certs.h` with your actual server cert
- [ ] Store `WIFI_SSID` / `WIFI_PASSWORD` outside of version control (use a secrets manager or provisioning flow)
- [ ] Enable `http.setInsecure(false)` (default) — never disable certificate validation in production
- [ ] Rate limiting is enabled on the server (60 requests / 15 min per device IP)

---

## Rollback

The ESP32 dual-partition scheme provides automatic rollback:

| Scenario | Outcome |
|----------|---------|
| New firmware boots and self-test passes | `esp_ota_mark_app_valid_cancel_rollback()` called → stays on new version |
| New firmware crashes before self-test | Bootloader detects 3 failed boots → reverts to previous slot |
| Self-test explicitly fails | `esp_ota_mark_app_invalid_rollback_and_reboot()` called → immediate rollback |
| Server-side rollback needed | Admin calls `PUT /api/firmware/latest/:v` → devices pull old version |

---

## Customising the Self-Test

Edit `runSelfTest()` in `triton_ota.ino` to match the Triton IO board peripherals:

```cpp
bool runSelfTest() {
  // Add your checks:
  if (!i2c_expander_ping())   return false;  // IO expander
  if (!adc_rail_ok())         return false;  // Power rail
  if (!relay_coil_check())    return false;  // Relay driver
  if (WiFi.RSSI() < -90)      return false;  // Signal too weak
  return true;
}
```
