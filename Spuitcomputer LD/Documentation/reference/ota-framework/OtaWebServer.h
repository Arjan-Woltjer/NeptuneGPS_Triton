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
#pragma once

#include <Arduino.h>

#include <WebServer.h>

class OtaWebServer {
public:
    explicit OtaWebServer(uint16_t port = 80);

    void Begin();
    void Update();

private:
    void   handleRoot();
    void   handleUpdate();
    void   handleUploadChunk();
    bool   authenticated();
    void   requestAuth();
    String buildPage(const String& statusHtml);

    WebServer server;
    bool      uploadError;
    // Latched at UPLOAD_FILE_START. The upload handler runs while the request
    // body is still being parsed, so this is the only point at which an
    // unauthenticated upload can be refused before it reaches flash.
    bool      uploadAuthorized;
};
