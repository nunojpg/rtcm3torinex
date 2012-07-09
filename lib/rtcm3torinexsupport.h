#ifndef RTCM3TORINEXSUPPORT_H
#define RTCM3TORINEXSUPPORT_H

/*
  Converter for RTCM3 data to RINEX.
  $Id$
  Copyright (C) 2005-2012 by Dirk Stöcker <stoecker@alberding.eu>

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

int rrinex3codetoentry(const char *code);

#endif /* RTCM3TORINEXSUPPORT_H */
