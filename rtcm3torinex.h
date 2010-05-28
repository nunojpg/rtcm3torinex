#ifndef RTCM3TORINEX_H
#define RTCM3TORINEX_H

/*
  Converter for RTCM3 data to RINEX.
  $Id$
  Copyright (C) 2005-2006 by Dirk Stöcker <stoecker@alberding.eu>

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

#define RTCM3TORINEX_VERSION "1.50"

#include <stdio.h>

#define GNSS_MAXSATS 64

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
#define GNSSDF_XCORRL2        (1<<28)  /* cross-correlated L2 */
#define GNSSDF_LOCKLOSSL1     (1<<29)  /* lost lock on L1 */
#define GNSSDF_LOCKLOSSL2     (1<<30)  /* lost lock on L2 */

struct converttimeinfo {
  int second;    /* seconds of GPS time [0..59] */
  int minute;    /* minutes of GPS time [0..59] */
  int hour;      /* hour of GPS time [0..24] */
  int day;       /* day of GPS time [1..28..30(31)*/
  int month;     /* month of GPS time [1..12]*/
  int year;      /* year of GPS time [1980..] */
};

struct gnssdata {
  int    flags;              /* GPSF_xxx */
  int    week;               /* week number of GPS date */
  int    numsats;
  double timeofweek;         /* milliseconds in GPS week */
  double measdata[GNSS_MAXSATS][GNSSENTRY_NUMBER];  /* data fields */ 
  int    dataflags[GNSS_MAXSATS];      /* GPSDF_xxx */
  int    satellites[GNSS_MAXSATS];     /* SV - IDs */
  int    channels[GNSS_MAXSATS];       /* Glonass channels - valid of Glonass SV only */
  int    snrL1[GNSS_MAXSATS];          /* Important: all the 5 SV-specific fields must */
  int    snrL2[GNSS_MAXSATS];          /* have the same SV-order */
};

#define GPSEPHF_L2PCODEDATA    (1<<0) /* set, if NAV data OFF on L2 P-code, s1w4b01 */
#define GPSEPHF_L2PCODE        (1<<1) /* set, if P-code available, s1w3b12 */
#define GPSEPHF_L2CACODE       (1<<2) /* set, if CA-code available, s1w3b11 */
#define GPSEPHF_VALIDATED      (1<<3) /* data is completely valid */

#define R2R_PI          3.1415926535898

struct gpsephemeris {
  int    flags;            /* GPSEPHF_xxx */
  int    satellite;        /*  SV ID   ICD-GPS data position */
  int    IODE;             /*          [s2w3b01-08]              */
  int    URAindex;         /*  [1..15] [s1w3b13-16]              */
  int    SVhealth;         /*          [s1w3b17-22]              */
  int    GPSweek;          /*          [s1w3b01-10]              */
  int    IODC;             /*          [s1w3b23-32,w8b01-08]     */
  int    TOW;              /*  [s]     [s1w2b01-17]              */
  int    TOC;              /*  [s]     [s1w8b09-24]              */
  int    TOE;              /*  [s]     [s2w10b1-16]              */
  double clock_bias;       /*  [s]     [s1w10b1-22, af0]         */
  double clock_drift;      /*  [s/s]   [s1w9b09-24, af1]         */
  double clock_driftrate;  /*  [s/s^2] [s1w9b01-08, af2]         */
  double Crs;              /*  [m]     [s2w3b09-24]              */
  double Delta_n;          /*  [rad/s] [s2w4b01-16 * Pi]         */
  double M0;               /*  [rad]   [s2w4b17-24,w5b01-24 * Pi]*/
  double Cuc;              /*  [rad]   [s2w6b01-16]              */
  double e;                /*          [s2w6b17-24,w6b01-24]     */
  double Cus;              /*  [rad]   [s2w8b01-16]              */
  double sqrt_A;           /*  [m^0.5] [s2w8b16-24,w9b01-24]     */
  double Cic;              /*  [rad]   [s3w3b01-16]              */
  double OMEGA0;           /*  [rad]   [s3w3b17-24,w4b01-24 * Pi]*/
  double Cis;              /*  [rad]   [s3w5b01-16]              */
  double i0;               /*  [rad]   [s3w5b17-24,w6b01-24 * Pi]*/
  double Crc;              /*  [m]     [s3w701-16]               */
  double omega;            /*  [rad]   [s3w7b17-24,w8b01-24 * Pi]*/
  double OMEGADOT;         /*  [rad/s] [s3w9b01-24 * Pi]         */
  double IDOT;             /*  [rad/s] [s3w10b9-22 * Pi]         */
  double TGD;              /*  [s]     [s1w7b17-24]              */
};

#define GLOEPHF_UNHEALTHY       (1<<0) /* set if unhealty satellite, f2b78 */
#define GLOEPHF_ALMANACHEALTHOK (1<<1) /* set if ALM health is available */
#define GLOEPHF_ALMANACHEALTHY  (1<<2) /* set if Cn word is true */
#define GLOEPHF_PAVAILABLE      (1<<3) /* set if the 3 P flags are available */
#define GLOEPHF_P10TRUE         (1<<4)
#define GLOEPHF_P11TRUE         (1<<5)
#define GLOEPHF_P2TRUE          (1<<6)
#define GLOEPHF_P3TRUE          (1<<7)

struct glonassephemeris {
  int    GPSWeek;
  int    GPSTOW;
  int    flags;              /* GLOEPHF_xxx */
  int    almanac_number;
  int    frequency_number;   /* ICD-GLONASS data position */
  int    tb;                 /* [s]     [f2b70-76] */
  int    tk;                 /* [s]     [f1b65-76] */
  int    E;                  /* [days]  [f4b49-53] */
  double tau;                /* [s]     [f4b59-80] */
  double gamma;              /*         [f3b69-79] */
  double x_pos;              /* [km]    [f1b09-35] */
  double x_velocity;         /* [km/s]  [f1b41-64] */
  double x_acceleration;     /* [km/s^2][f1b36-40] */
  double y_pos;              /* [km]    [f2b09-35] */
  double y_velocity;         /* [km/s]  [f2b41-64] */
  double y_acceleration;     /* [km/s^2][f2b36-40] */
  double z_pos;              /* [km]    [f3b09-35] */
  double z_velocity;         /* [km/s]  [f3b41-64] */
  double z_acceleration;     /* [km/s^2][f3b36-40] */
};

struct RTCM3ParserData {
  unsigned char Message[2048]; /* input-buffer */
  int    MessageSize;   /* current buffer size */
  int    NeedBytes;     /* bytes wanted for next run */
  int    SkipBytes;     /* bytes to skip in next round */
  int    GPSWeek;
  int    GPSTOW;        /* in seconds */
  struct gnssdata Data;
  struct gpsephemeris ephemerisGPS;
  struct glonassephemeris ephemerisGLONASS;
  struct gnssdata DataNew;
  int    size;
  int    lastlockl1[64];
  int    lastlockl2[64];
#ifdef NO_RTCM3_MAIN
  double antX;
  double antY;
  double antZ;
  double antH;
  char   antenna[256+1];
  int    blocktype;
#endif /* NO_RTCM3_MAIN */
  int    datapos[RINEXENTRY_NUMBER];
  int    dataflag[RINEXENTRY_NUMBER];
  /* for RINEX2 GPS and GLO are both handled in GPS */
  int    dataposGPS[RINEXENTRY_NUMBER]; /* SBAS has same entries */
  int    dataflagGPS[RINEXENTRY_NUMBER];
  int    dataposGLO[RINEXENTRY_NUMBER]; /* only used for RINEX3 */
  int    dataflagGLO[RINEXENTRY_NUMBER];
  int    numdatatypesGPS;
  int    numdatatypesGLO; /* only used for RINEX3 */
  int    validwarning;
  int    init;
  int    startflags;
  int    rinex3;
  const char * headerfile;
  const char * glonassephemeris;
  const char * gpsephemeris;
  FILE *glonassfile;
  FILE *gpsfile;
};

#ifndef PRINTFARG
#ifdef __GNUC__
#define PRINTFARG(a,b) __attribute__ ((format(printf, a, b)))
#else /* __GNUC__ */
#define PRINTFARG(a,b)
#endif /* __GNUC__ */
#endif /* PRINTFARG */

int gnumleap(int year, int month, int day);
void updatetime(int *week, int *tow, int tk, int fixnumleap);
void converttime(struct converttimeinfo *c, int week, int tow);

void HandleHeader(struct RTCM3ParserData *Parser);
int RTCM3Parser(struct RTCM3ParserData *handle);
void HandleByte(struct RTCM3ParserData *Parser, unsigned int byte);
void PRINTFARG(1,2) RTCM3Error(const char *fmt, ...);
void PRINTFARG(1,2) RTCM3Text(const char *fmt, ...);

#endif /* RTCM3TORINEX_H */
