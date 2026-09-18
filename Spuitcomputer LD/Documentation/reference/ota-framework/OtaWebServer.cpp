/*
  OtaWebServer - Local web interface for manual OTA firmware upload
  Copyright (C) 2011-2026 J.A. Woltjer.
  All rights reserved.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "OtaWebServer.h"

#include <Update.h>
#include <WiFi.h>

#include "ConfigOta.h"

static const char* kAuthRealm = "Triton IO";

#if OTA_WEB_ENABLED
// A firmware-flashing endpoint reachable over the network with a blank password
// is an open door, so refuse to build one. Both credentials come from
// build_flags; see ConfigOta.h.
static_assert(sizeof(OTA_WEB_PASSWORD) > 1,
              "OTA_WEB_ENABLED=1 requires OTA_WEB_PASSWORD to be set via build_flags");
static_assert(sizeof(OTA_WEB_USER) > 1,
              "OTA_WEB_ENABLED=1 requires OTA_WEB_USER to be set via build_flags");
#endif

OtaWebServer::OtaWebServer(uint16_t port)
    : server(port), uploadError(false), uploadAuthorized(false) {
}

void OtaWebServer::Begin() {
#if OTA_WEB_ENABLED
    server.on("/", HTTP_GET, [this]() { handleRoot(); });

    server.on("/update", HTTP_POST,
        [this]() { handleUpdate(); },
        [this]() { handleUploadChunk(); }
    );

    server.begin();
    Serial.printf("[WEB-OTA] Listening on http://%s:%d\n",
                  WiFi.localIP().toString().c_str(), OTA_WEB_PORT);
#else
    Serial.println("[WEB-OTA] Disabled at build time (build with -D OTA_WEB_ENABLED=1).");
#endif
}

void OtaWebServer::Update() {
#if OTA_WEB_ENABLED
    server.handleClient();
#endif
}

bool OtaWebServer::authenticated() {
    return server.authenticate(OTA_WEB_USER, OTA_WEB_PASSWORD);
}

void OtaWebServer::requestAuth() {
    // Digest rather than basic: this server speaks plaintext HTTP, and basic
    // auth would put the password on the wire base64-encoded, which is to say
    // in clear. Digest is still not confidentiality -- it only keeps the
    // password itself off the wire.
    server.requestAuthentication(DIGEST_AUTH, kAuthRealm, "Authentication required");
}

void OtaWebServer::handleRoot() {
    if (!authenticated()) {
        return requestAuth();
    }
    server.send(200, "text/html", buildPage(""));
}

void OtaWebServer::handleUpdate() {
    // uploadAuthorized is what the upload handler actually acted on; re-checking
    // authenticated() as well covers a POST that carried no file part at all, in
    // which case handleUploadChunk() never ran and the latch is stale.
    const bool authorized = uploadAuthorized && authenticated();
    uploadAuthorized = false;

    if (!authorized) {
        // Nothing was flashed: handleUploadChunk() does not call Update.begin()
        // without authorisation.
        return requestAuth();
    }
    if (uploadError) {
        server.send(500, "text/html",
            buildPage("<p class='err'>Update failed &#8212; invalid image or flash error.</p>"));
    } else {
        server.send(200, "text/html",
            buildPage("<p class='ok'>Update successful &#8212; rebooting&hellip;</p>"));
        delay(1000);
        ESP.restart();
    }
}

void OtaWebServer::handleUploadChunk() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        uploadError      = false;
        // The WebServer calls this handler while it is still parsing the request
        // body -- before handleUpdate() runs. Authenticating only there would let
        // an unauthenticated POST run to completion through Update.end(true),
        // which sets the boot partition, and receive its 401 afterwards. By then
        // the board is flashed and boots the uploaded image on the next reset.
        // Authorisation has to be decided here, before the first byte is written.
        uploadAuthorized = authenticated();

        if (!uploadAuthorized) {
            uploadError = true;
            Serial.println("[WEB-OTA] Rejected unauthenticated upload.");
            return;
        }

        Serial.printf("[WEB-OTA] Upload started: %s\n", upload.filename.c_str());
        if (!::Update.begin(UPDATE_SIZE_UNKNOWN)) {
            ::Update.printError(Serial);
            uploadError = true;
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!uploadAuthorized) {
            return;
        }
        if (!uploadError && ::Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            ::Update.printError(Serial);
            uploadError = true;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!uploadAuthorized) {
            return;
        }
        if (!uploadError) {
            if (::Update.end(true)) {
                Serial.printf("[WEB-OTA] Complete: %u bytes flashed.\n", upload.totalSize);
            } else {
                ::Update.printError(Serial);
                uploadError = true;
            }
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        // A client that disconnects mid-upload otherwise leaves Update holding a
        // partially written partition.
        if (uploadAuthorized && ::Update.isRunning()) {
            ::Update.abort();
        }
        uploadError = true;
        Serial.println("[WEB-OTA] Upload aborted.");
    }
}

String OtaWebServer::buildPage(const String& statusHtml) {
    String p;
    p.reserve(1500);
    p += F("<!DOCTYPE html><html><head>"
           "<meta charset='utf-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>Triton IO &#8212; Firmware Update</title>"
           "<style>"
           "body{font-family:sans-serif;max-width:480px;margin:40px auto;padding:0 16px}"
           "h1{font-size:1.2em;color:#333}"
           ".info{color:#666;font-size:.9em;margin-bottom:24px}"
           "input[type=file]{display:block;margin-bottom:12px}"
           "button{background:#0066cc;color:#fff;border:none;"
           "padding:10px 20px;border-radius:4px;cursor:pointer}"
           "button:hover{background:#0052a3}"
           ".ok{margin-top:20px;padding:12px;border-radius:4px;"
           "background:#e6f4ea;color:#1e7e34}"
           ".err{margin-top:20px;padding:12px;border-radius:4px;"
           "background:#fce8e6;color:#c5221f}"
           "</style></head><body>"
           "<h1>Triton IO &#8212; Firmware Update</h1>"
           "<p class='info'>Version: v");
    p += CURRENT_FW_VERSION;
    p += F(" &nbsp;|&nbsp; IP: ");
    p += WiFi.localIP().toString();
    p += F("</p>"
           "<form method='POST' action='/update' enctype='multipart/form-data'>"
           "<input type='file' name='firmware' accept='.bin' required>"
           "<button type='submit'>Upload &amp; Flash</button>"
           "</form>");
    p += statusHtml;
    p += F("</body></html>");
    return p;
}
