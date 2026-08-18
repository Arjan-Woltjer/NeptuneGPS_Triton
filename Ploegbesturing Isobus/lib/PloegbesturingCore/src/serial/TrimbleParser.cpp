/*
  TrimbleParser - Trimble ROXTE proprietary sentence parser
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
#include "TrimbleParser.hpp"

#include <string.h>

namespace triton
{

bool TrimbleParser::claimsSentenceType(const char* header) {
    if (strcmp(header, "ROXTE") != 0) return false;
    newXte = 0;
    return true;
}

void TrimbleParser::parseTerm(byte termNumber, const char* term) {
    if (termNumber == 1) newXte = (int)(atof(term) * 100);
}

void TrimbleParser::commitTo(GuidanceSource* state) {
    state->SetXte(newXte);
}

}  // namespace triton
