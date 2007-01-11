#ifndef RTCM3TORINEX_H
#define RTCM3TORINES_H

/*
  Converter for RTCM3 data to RINEX.
  $Id: rtcm3torinex.h,v 1.4 2006/11/24 09:53:42 stoecker Exp $
  Copyright (C) 2005-2006 by Dirk Stoecker <stoecker@euronik.eu>

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

#define PRN_GPS_START             1
#define PRN_GPS_END               32
#define PRN_GLONASS_START         38
#define PRN_GLONASS_END           61
#define PRN_WAAS_START            120
#define PRN_WAAS_END              138

#define GNSSENTRY_C1DATA     0
#define GNSSENTRY_C2DATA     1
#define GNSSENTRY_P1DATA     2
#define GNSSENTRY_P2DATA     3
#define GNSSENTRY_L1CDATA    4
#define GNSSENTRY_L1PDATA    5
#define GNSSENTRY_L2CDATA    6
#define GNSSENTRY_L2PDATA    7
#define GNSSENTRY_D1CDATA    8
#define GNSSENTRY_D1PDATA    9
#define GNSSENTRY_D2CDATA    10
#define GNSSENTRY_D2PDATA    11
#define GNSSENTRY_S1CDATA    12
#define GNSSENTRY_S1PDATA    13
#define GNSSENTRY_S2CDATA    14
#define GNSSENTRY_S2PDATA    15
#define GNSSENTRY_NUMBER     16 /* number of types!!! */

/* Data flags. These flags are used in the dataflags field of gpsdata structure
   and are used the determine, which data fields are filled with valid data. */
#define GNSSDF_C1DATA         (1<<GNSSENTRY_C1DATA)
#define GNSSDF_C2DATA         (1<<GNSSENTRY_C2DATA)
#define GNSSDF_P1DATA         (1<<GNSSENTRY_P1DATA)
#define GNSSDF_P2DATA         (1<<GNSSENTRY_P2DATA)
#define GNSSDF_L1CDATA        (1<<GNSSENTRY_L1CDATA)
#define GNSSDF_L1PDATA        (1<<GNSSENTRY_L1PDATA)
#define GNSSDF_L2CDATA        (1<<GNSSENTRY_L2CDATA)
#define GNSSDF_L2PDATA        (1<<GNSSENTRY_L2PDATA)
#define GNSSDF_D1CDATA        (1<<GNSSENTRY_D1CDATA)
#define GNSSDF_D1PDATA        (1<<GNSSENTRY_D1PDATA)
#define GNSSDF_D2CDATA        (1<<GNSSENTRY_D2CDATA)
#define GNSSDF_D2PDATA        (1<<GNSSENTRY_D2PDATA)
#define GNSSDF_S1CDATA        (1<<GNSSENTRY_S1CDATA)
#define GNSSDF_S1PDATA        (1<<GNSSENTRY_S1PDATA)
#define GNSSDF_S2CDATA        (1<<GNSSENTRY_S2CDATA)
#define GNSSDF_S2PDATA        (1<<GNSSENTRY_S2PDATA)

#define RINEXENTRY_C1DATA     0
#define RINEXENTRY_C2DATA     1
#define RINEXENTRY_P1DATA     2
#define RINEXENTRY_P2DATA     3
#define RINEXENTRY_L1DATA     4
#define RINEXENTRY_L2DATA     5
#define RINEXENTRY_D1DATA     6
#define RINEXENTRY_D2DATA     7
#define RINEXENTRY_S1DATA     8
#define RINEXENTRY_S2DATA     9
#define RINEXENTRY_NUMBER     10

#define LIGHTSPEED         2.99792458e8    /* m/sec */
#define GPS_FREQU_L1       1575420000.0  /* Hz */
#define GPS_FREQU_L2       1227600000.0  /* Hz */
#define GPS_WAVELENGTH_L1  (LIGHTSPEED / GPS_FREQU_L1) /* m */
#define GPS_WAVELENGTH_L2  (LIGHTSPEED / GPS_FREQU_L2) /* m */

#define GLO_FREQU_L1_BASE  1602000000.0  /* Hz */
#define GLO_FREQU_L2_BASE  1246000000.0  /* Hz */
#define GLO_FREQU_L1_STEP      562500.0  /* Hz */
#define GLO_FREQU_L2_STEP      437500.0  /* Hz */
#define GLO_FREQU_L1(a)      (GLO_FREQU_L1_BASE+(a)*GLO_FREQU_L1_STEP)
#define GLO_FREQU_L2(a)      (GLO_FREQU_L2_BASE+(a)*GLO_FREQU_L2_STEP)
#define GLO_WAVELENGTH_L1(a) (LIGHTSPEED / GLO_FREQU_L1(a)) /* m */
#define GLO_WAVELENGTH_L2(a) (LIGHTSPEED / GLO_FREQU_L2(a)) /* m */

/* Additional flags for the data field, which tell us more. */
#define GNSSDF_LOCKLOSSL1     (1<<29)  /* lost lock on L1 */
#define GNSSDF_LOCKLOSSL2     (1<<30)  /* lost lock on L2 */

struct gnssdata {
  int    flags;              /* GPSF_xxx */
  int    week;               /* week number of GPS date */
  int    numsats;
  double timeofweek;         /* milliseconds in GPS week */
  double measdata[24][GNSSENTRY_NUMBER];  /* data fields */ 
  int    dataflags[24];      /* GPSDF_xxx */
  int    satellites[24];     /* SV - IDs */
  int    snrL1[24];          /* Important: all the 5 SV-specific fields must */
  int    snrL2[24];          /* have the same SV-order */
};

struct RTCM3ParserData {
  unsigned char Message[2048]; /* input-buffer */
  int    MessageSize;   /* current buffer size */
  int    NeedBytes;     /* bytes wanted for next run */
  int    SkipBytes;     /* bytes to skip in next round */
  int    GPSWeek;
  int    GPSTOW;        /* in seconds */
  struct gnssdata Data;
  struct gnssdata DataNew;
  int    size;
  int    lastlockl1[64];
  int    lastlockl2[64];
  int    datapos[RINEXENTRY_NUMBER];
  int    dataflag[RINEXENTRY_NUMBER];
  int    datapos2[RINEXENTRY_NUMBER];
  int    dataflag2[RINEXENTRY_NUMBER];
  int    numdatatypes;
  int    validwarning;
  int    init;
  int    startflags;
  const char * headerfile;
};

#ifndef PRINTFARG
#ifdef __GNUC__
#define PRINTFARG(a,b) __attribute__ ((format(printf, a, b)))
#else /* __GNUC__ */
#define PRINTFARG(a,b)
#endif /* __GNUC__ */
#endif /* PRINTFARG */

void HandleHeader(struct RTCM3ParserData *Parser);
int RTCM3Parser(struct RTCM3ParserData *handle);
void HandleByte(struct RTCM3ParserData *Parser, unsigned int byte);
void PRINTFARG(1,2) RTCM3Error(const char *fmt, ...);
void PRINTFARG(1,2) RTCM3Text(const char *fmt, ...);

#endif /* RTCM3TORINEX_H */
