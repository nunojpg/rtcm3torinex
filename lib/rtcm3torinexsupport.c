/*
  Converter for RTCM3 data to RINEX.
  $Id$
  Copyright (C) 2005-2012 by Dirk Stöcker <stoecker@alberding.eu>

  This software is a complete NTRIP-RTCM3 to RINEX converter as well as
  a module of the BNC tool for multiformat conversion. Contact Dirk
  Stöcker for suggestions and bug reports related to the RTCM3 to RINEX
  conversion problems and the author of BNC for all the other problems.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
  or read http://www.gnu.org/licenses/gpl.txt
*/

#include "rtcm3torinex.h"

int rrinex3codetoentry(const char *code)
{
  int res = 0;
  switch(code[0])
  {
  case 'C': res += GNSSENTRY_CODE; break;
  case 'L': res += GNSSENTRY_PHASE; break;
  case 'D': res += GNSSENTRY_DOPPLER; break;
  case 'S': res += GNSSENTRY_SNR; break;
  default:
    return -1;
  }
  switch(code[1])
  {
  case '1':
    switch(code[2])
    {
    default:
    case 'C': res += GNSSENTRY_TYPEC1; break;
    case 'P': case'W': case 'Y': res += GNSSENTRY_TYPEP1; break;
    case 'A': case'B':
    case 'S': case'L': case 'X': res += GNSSENTRY_TYPEC1N; break;
    case 'Z': res += GNSSENTRY_TYPECSAIF; break;
    }
    break;
  case '2':
    switch(code[2])
    {
    default:
    case 'P': case 'W': case 'Y': res += GNSSENTRY_TYPEP2; break;
    case 'C': case 'S': case 'L': case 'X': res += GNSSENTRY_TYPEC2; break;
    }
    break;
  case '5':
    res += GNSSENTRY_TYPEC5;
    break;
  case '6':
    res += GNSSENTRY_TYPEC6;
    break;
  case '7':
    res += GNSSENTRY_TYPEC5B;
    break;
  case '8':
    res += GNSSENTRY_TYPEC5AB;
    break;
  }
  return res;
}
