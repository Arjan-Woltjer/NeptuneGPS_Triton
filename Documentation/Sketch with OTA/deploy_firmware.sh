#!/usr/bin/env bash
# deploy_firmware.sh — Build & publish a new Triton IO firmware release
#
# Usage:
#   ./deploy_firmware.sh <version> "Release notes here"
#
# Prerequisites:
#   - arduino-cli installed and configured
#   - OTA server running and accessible
#   - ADMIN_KEY environment variable set

set -euo pipefail

# ─── Config ───────────────────────────────────────────────────────────────────

SKETCH_PATH="./firmware/triton_ota.ino"
BOARD_FQBN="esp32:esp32:esp32"            # Adjust for your exact ESP32 variant
OTA_SERVER="${OTA_SERVER:-http://localhost:3000}"
ADMIN_KEY="${ADMIN_KEY:?Set ADMIN_KEY env var}"
BUILD_DIR="/tmp/triton_build"

# ─── Args ─────────────────────────────────────────────────────────────────────

VERSION="${1:?Usage: $0 <version> \"Release notes\"}"
NOTES="${2:-No release notes provided}"

echo "=== Triton IO Firmware Deploy ==="
echo "  Version : $VERSION"
echo "  Notes   : $NOTES"
echo "  Server  : $OTA_SERVER"
echo ""

# ─── Bump version in config header ───────────────────────────────────────────

CONFIG_H="./firmware/ota_config.h"
sed -i.bak "s/^#define CURRENT_FW_VERSION.*/#define CURRENT_FW_VERSION  $VERSION/" "$CONFIG_H"
echo "[1/4] Version bumped to $VERSION in ota_config.h"

# ─── Compile ──────────────────────────────────────────────────────────────────

mkdir -p "$BUILD_DIR"
echo "[2/4] Compiling firmware..."

arduino-cli compile \
  --fqbn "$BOARD_FQBN" \
  --build-path "$BUILD_DIR" \
  --warnings default \
  "$SKETCH_PATH"

BIN_FILE="$BUILD_DIR/triton_ota.ino.bin"
if [ ! -f "$BIN_FILE" ]; then
  echo "ERROR: Binary not found after compile. Check build output."
  exit 1
fi

BIN_SIZE=$(wc -c < "$BIN_FILE")
echo "[2/4] Compiled. Binary size: ${BIN_SIZE} bytes"

# ─── Upload to OTA server ─────────────────────────────────────────────────────

echo "[3/4] Uploading firmware v$VERSION to $OTA_SERVER..."

RESPONSE=$(curl -sf \
  -X POST \
  -H "X-Admin-Key: $ADMIN_KEY" \
  -F "file=@${BIN_FILE}" \
  -F "version=$VERSION" \
  -F "notes=$NOTES" \
  "$OTA_SERVER/api/firmware/upload")

echo "Server response: $RESPONSE"

SHA256=$(echo "$RESPONSE" | python3 -c "import sys,json; print(json.load(sys.stdin)['sha256'])")
echo "[3/4] Upload complete. SHA256: $SHA256"

# ─── Verify ───────────────────────────────────────────────────────────────────

echo "[4/4] Verifying server reports v$VERSION as latest..."

LATEST=$(curl -sf "$OTA_SERVER/api/firmware/latest" \
  -H "X-Device-ID: deploy-script" \
  -H "X-Current-Version: 0")

LATEST_VER=$(echo "$LATEST" | python3 -c "import sys,json; print(json.load(sys.stdin)['version'])")

if [ "$LATEST_VER" = "$VERSION" ]; then
  echo ""
  echo "✓ Firmware v$VERSION successfully deployed!"
  echo "  Devices will receive it on their next OTA check."
else
  echo "WARNING: Server reports latest=$LATEST_VER, expected $VERSION"
  exit 1
fi

# ─── Git tag (optional) ───────────────────────────────────────────────────────

if git rev-parse --git-dir > /dev/null 2>&1; then
  git add "$CONFIG_H"
  git commit -m "chore: bump firmware to v${VERSION}"
  git tag -a "fw-v${VERSION}" -m "$NOTES"
  echo "  Git commit and tag fw-v${VERSION} created."
fi
