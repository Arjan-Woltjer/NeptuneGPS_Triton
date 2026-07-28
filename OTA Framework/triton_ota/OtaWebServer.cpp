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

OtaWebServer::OtaWebServer(uint16_t port)
    : server(port), uploadError(false) {
}

void OtaWebServer::Begin() {
    server.on("/", HTTP_GET, [this]() { handleRoot(); });

    server.on("/update", HTTP_POST,
        [this]() { handleUpdate(); },
        [this]() { handleUploadChunk(); }
    );

    server.begin();
    Serial.printf("[WEB-OTA] Listening on http://%s:%d\n",
                  WiFi.localIP().toString().c_str(), OTA_WEB_PORT);
}

void OtaWebServer::Update() {
    server.handleClient();
}

void OtaWebServer::handleRoot() {
    if (!server.authenticate(OTA_WEB_USER, OTA_WEB_PASSWORD)) {
        return server.requestAuthentication();
    }
    server.send(200, "text/html", buildPage(""));
}

void OtaWebServer::handleUpdate() {
    if (!server.authenticate(OTA_WEB_USER, OTA_WEB_PASSWORD)) {
        return server.requestAuthentication();
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
        uploadError = false;
        Serial.printf("[WEB-OTA] Upload started: %s\n", upload.filename.c_str());
        if (!::Update.begin(UPDATE_SIZE_UNKNOWN)) {
            ::Update.printError(Serial);
            uploadError = true;
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (!uploadError && ::Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            ::Update.printError(Serial);
            uploadError = true;
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (!uploadError) {
            if (::Update.end(true)) {
                Serial.printf("[WEB-OTA] Complete: %u bytes flashed.\n", upload.totalSize);
            } else {
                ::Update.printError(Serial);
                uploadError = true;
            }
        }
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
