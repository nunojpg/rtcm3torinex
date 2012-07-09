#ifndef RTCM3TORINEX_H
#define RTCM3TORINEX_H

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

#define RTCM3TORINEX_VERSION "2"

#include <stdio.h>

#define NUMRTCM3PARSERSATS    144
#define GNSS_MAXSATS 64

#define PRN_GPS_START             1
#define PRN_GPS_END               32
#define PRN_GLONASS_START         38
#define PRN_GLONASS_END           61
#define PRN_GALILEO_START         71
#define PRN_GALILEO_END           100
#define PRN_SBAS_START            120
#define PRN_SBAS_END              138
#define PRN_GIOVE_START           139
#define PRN_GIOVE_END             140
#define PRN_COMPASS_START         141
#define PRN_COMPASS_END           170
#define PRN_QZSS_START            193
#define PRN_QZSS_END              202

#define PRN_GLONASS_NUM           (PRN_GLONASS_END-PRN_GLONASS_START+1)

#define RTCM3_MSM_NUMSIG      32
#define RTCM3_MSM_NUMSAT      64
#define RTCM3_MSM_NUMCELLS    64

/* system identifiers */
#define RTCM3_MSM_GPS     0
#define RTCM3_MSM_GLONASS 1
#define RTCM3_MSM_GALILEO 2
#define RTCM3_MSM_SBAS    3
#define RTCM3_MSM_QZSS    4
#define RTCM3_MSM_COMPASS 5

#define RTCM3_MSM_NUMSYS  6

#define GNSSENTRY_CODE    0
#define GNSSENTRY_PHASE   1
#define GNSSENTRY_DOPPLER 2
#define GNSSENTRY_SNR     3

#define GNSSENTRY_TYPEC1    (0<<2)
#define GNSSENTRY_TYPEC2    (1<<2)
#define GNSSENTRY_TYPEP1    (2<<2)
#define GNSSENTRY_TYPEP2    (3<<2)
#define GNSSENTRY_TYPEC5    (4<<2)
#define GNSSENTRY_TYPEC6    (5<<2)
#define GNSSENTRY_TYPEC5B   (6<<2)
#define GNSSENTRY_TYPEC5AB  (7<<2)
#define GNSSENTRY_TYPECSAIF (8<<2)
#define GNSSENTRY_TYPEC1N   (9<<2)

#define GNSSENTRY_C1DATA     (GNSSENTRY_TYPEC1+GNSSENTRY_CODE)
#define GNSSENTRY_L1CDATA    (GNSSENTRY_TYPEC1+GNSSENTRY_PHASE)
#define GNSSENTRY_D1CDATA    (GNSSENTRY_TYPEC1+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S1CDATA    (GNSSENTRY_TYPEC1+GNSSENTRY_SNR)

#define GNSSENTRY_C2DATA     (GNSSENTRY_TYPEC2+GNSSENTRY_CODE)
#define GNSSENTRY_L2CDATA    (GNSSENTRY_TYPEC2+GNSSENTRY_PHASE)
#define GNSSENTRY_D2CDATA    (GNSSENTRY_TYPEC2+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S2CDATA    (GNSSENTRY_TYPEC2+GNSSENTRY_SNR)

#define GNSSENTRY_CB1DATA    GNSSENTRY_C2DATA
#define GNSSENTRY_LB1DATA    GNSSENTRY_L2CDATA
#define GNSSENTRY_DB1DATA    GNSSENTRY_D2CDATA
#define GNSSENTRY_SB1DATA    GNSSENTRY_S2CDATA

#define GNSSENTRY_P1DATA     (GNSSENTRY_TYPEP1+GNSSENTRY_CODE)
#define GNSSENTRY_L1PDATA    (GNSSENTRY_TYPEP1+GNSSENTRY_PHASE)
#define GNSSENTRY_D1PDATA    (GNSSENTRY_TYPEP1+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S1PDATA    (GNSSENTRY_TYPEP1+GNSSENTRY_SNR)

#define GNSSENTRY_P2DATA     (GNSSENTRY_TYPEP2+GNSSENTRY_CODE)
#define GNSSENTRY_L2PDATA    (GNSSENTRY_TYPEP2+GNSSENTRY_PHASE)
#define GNSSENTRY_D2PDATA    (GNSSENTRY_TYPEP2+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S2PDATA    (GNSSENTRY_TYPEP2+GNSSENTRY_SNR)

#define GNSSENTRY_C5DATA     (GNSSENTRY_TYPEC5+GNSSENTRY_CODE)
#define GNSSENTRY_L5DATA     (GNSSENTRY_TYPEC5+GNSSENTRY_PHASE)
#define GNSSENTRY_D5DATA     (GNSSENTRY_TYPEC5+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S5DATA     (GNSSENTRY_TYPEC5+GNSSENTRY_SNR)

#define GNSSENTRY_C6DATA     (GNSSENTRY_TYPEC6+GNSSENTRY_CODE)
#define GNSSENTRY_L6DATA     (GNSSENTRY_TYPEC6+GNSSENTRY_PHASE)
#define GNSSENTRY_D6DATA     (GNSSENTRY_TYPEC6+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S6DATA     (GNSSENTRY_TYPEC6+GNSSENTRY_SNR)

#define GNSSENTRY_CB3DATA    GNSSENTRY_C6DATA
#define GNSSENTRY_LB3DATA    GNSSENTRY_L6DATA
#define GNSSENTRY_DB3DATA    GNSSENTRY_D6DATA
#define GNSSENTRY_SB3DATA    GNSSENTRY_S6DATA

#define GNSSENTRY_CLEXDATA   GNSSENTRY_C6DATA
#define GNSSENTRY_LLEXDATA   GNSSENTRY_L6DATA
#define GNSSENTRY_DLEXDATA   GNSSENTRY_D6DATA
#define GNSSENTRY_SLEXDATA   GNSSENTRY_S6DATA

#define GNSSENTRY_C5BDATA    (GNSSENTRY_TYPEC5B+GNSSENTRY_CODE)
#define GNSSENTRY_L5BDATA    (GNSSENTRY_TYPEC5B+GNSSENTRY_PHASE)
#define GNSSENTRY_D5BDATA    (GNSSENTRY_TYPEC5B+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S5BDATA    (GNSSENTRY_TYPEC5B+GNSSENTRY_SNR)

#define GNSSENTRY_CB2DATA    GNSSENTRY_C5BDATA
#define GNSSENTRY_LB2DATA    GNSSENTRY_L5BDATA
#define GNSSENTRY_DB2DATA    GNSSENTRY_D5BDATA
#define GNSSENTRY_SB2DATA    GNSSENTRY_S5BDATA

#define GNSSENTRY_C5ABDATA   (GNSSENTRY_TYPEC5AB+GNSSENTRY_CODE)
#define GNSSENTRY_L5ABDATA   (GNSSENTRY_TYPEC5AB+GNSSENTRY_PHASE)
#define GNSSENTRY_D5ABDATA   (GNSSENTRY_TYPEC5AB+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S5ABDATA   (GNSSENTRY_TYPEC5AB+GNSSENTRY_SNR)

#define GNSSENTRY_CSAIFDATA  (GNSSENTRY_TYPECSAIF+GNSSENTRY_CODE)
#define GNSSENTRY_LSAIFDATA  (GNSSENTRY_TYPECSAIF+GNSSENTRY_PHASE)
#define GNSSENTRY_DSAIFDATA  (GNSSENTRY_TYPECSAIF+GNSSENTRY_DOPPLER)
#define GNSSENTRY_SSAIFDATA  (GNSSENTRY_TYPECSAIF+GNSSENTRY_SNR)

#define GNSSENTRY_C1NDATA    (GNSSENTRY_TYPEC1N+GNSSENTRY_CODE)
#define GNSSENTRY_L1NDATA    (GNSSENTRY_TYPEC1N+GNSSENTRY_PHASE)
#define GNSSENTRY_D1NDATA    (GNSSENTRY_TYPEC1N+GNSSENTRY_DOPPLER)
#define GNSSENTRY_S1NDATA    (GNSSENTRY_TYPEC1N+GNSSENTRY_SNR)

#define GNSSENTRY_NUMBER     40 /* number of types!!! */

/* Data flags. These flags are used in the dataflags field of gpsdata structure
   and are required to determine, which data fields are filled with valid data. */
#define GNSSDF_C1DATA         (1<<GNSSENTRY_C1DATA)
#define GNSSDF_L1CDATA        (1<<GNSSENTRY_L1CDATA)
#define GNSSDF_D1CDATA        (1<<GNSSENTRY_D1CDATA)
#define GNSSDF_S1CDATA        (1<<GNSSENTRY_S1CDATA)
#define GNSSDF_C2DATA         (1<<GNSSENTRY_C2DATA)
#define GNSSDF_L2CDATA        (1<<GNSSENTRY_L2CDATA)
#define GNSSDF_D2CDATA        (1<<GNSSENTRY_D2CDATA)
#define GNSSDF_S2CDATA        (1<<GNSSENTRY_S2CDATA)
#define GNSSDF_CB1DATA        (1<<GNSSENTRY_CB1DATA)
#define GNSSDF_LB1DATA        (1<<GNSSENTRY_LB1DATA)
#define GNSSDF_DB1DATA        (1<<GNSSENTRY_DB1DATA)
#define GNSSDF_SB1DATA        (1<<GNSSENTRY_SB1DATA)
#define GNSSDF_P1DATA         (1<<GNSSENTRY_P1DATA)
#define GNSSDF_L1PDATA        (1<<GNSSENTRY_L1PDATA)
#define GNSSDF_D1PDATA        (1<<GNSSENTRY_D1PDATA)
#define GNSSDF_S1PDATA        (1<<GNSSENTRY_S1PDATA)
#define GNSSDF_P2DATA         (1<<GNSSENTRY_P2DATA)
#define GNSSDF_L2PDATA        (1<<GNSSENTRY_L2PDATA)
#define GNSSDF_D2PDATA        (1<<GNSSENTRY_D2PDATA)
#define GNSSDF_S2PDATA        (1<<GNSSENTRY_S2PDATA)

#define GNSSDF_C5DATA         (1<<GNSSENTRY_C5DATA)
#define GNSSDF_L5DATA         (1<<GNSSENTRY_L5DATA)
#define GNSSDF_D5DATA         (1<<GNSSENTRY_D5DATA)
#define GNSSDF_S5DATA         (1<<GNSSENTRY_S5DATA)
#define GNSSDF_C6DATA         (1<<GNSSENTRY_C6DATA)
#define GNSSDF_L6DATA         (1<<GNSSENTRY_L6DATA)
#define GNSSDF_D6DATA         (1<<GNSSENTRY_D6DATA)
#define GNSSDF_S6DATA         (1<<GNSSENTRY_S6DATA)
#define GNSSDF_CB3DATA        (1<<GNSSENTRY_CB3DATA)
#define GNSSDF_LB3DATA        (1<<GNSSENTRY_LB3DATA)
#define GNSSDF_DB3DATA        (1<<GNSSENTRY_DB3DATA)
#define GNSSDF_SB3DATA        (1<<GNSSENTRY_SB3DATA)
#define GNSSDF_C5BDATA        (1<<GNSSENTRY_C5BDATA)
#define GNSSDF_L5BDATA        (1<<GNSSENTRY_L5BDATA)
#define GNSSDF_D5BDATA        (1<<GNSSENTRY_D5BDATA)
#define GNSSDF_S5BDATA        (1<<GNSSENTRY_S5BDATA)
#define GNSSDF_CB2DATA        (1<<GNSSENTRY_CB2DATA)
#define GNSSDF_LB2DATA        (1<<GNSSENTRY_LB2DATA)
#define GNSSDF_DB2DATA        (1<<GNSSENTRY_DB2DATA)
#define GNSSDF_SB2DATA        (1<<GNSSENTRY_SB2DATA)
#define GNSSDF_C5ABDATA       (1<<GNSSENTRY_C5ABDATA)
#define GNSSDF_L5ABDATA       (1<<GNSSENTRY_L5ABDATA)
#define GNSSDF_D5ABDATA       (1<<GNSSENTRY_D5ABDATA)
#define GNSSDF_S5ABDATA       (1<<GNSSENTRY_S5ABDATA)

#define GNSSDF_CSAIFDATA      (1LL<<GNSSENTRY_CSAIFDATA)
#define GNSSDF_LSAIFDATA      (1LL<<GNSSENTRY_LSAIFDATA)
#define GNSSDF_DSAIFDATA      (1LL<<GNSSENTRY_DSAIFDATA)
#define GNSSDF_SSAIFDATA      (1LL<<GNSSENTRY_SSAIFDATA)

#define GNSSDF_C1NDATA        (1LL<<GNSSENTRY_C1NDATA)
#define GNSSDF_L1NDATA        (1LL<<GNSSENTRY_L1NDATA)
#define GNSSDF_D1NDATA        (1LL<<GNSSENTRY_D1NDATA)
#define GNSSDF_S1NDATA        (1LL<<GNSSENTRY_S1NDATA)

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

#define RINEXENTRY_C5DATA     10
#define RINEXENTRY_L5DATA     11
#define RINEXENTRY_D5DATA     12
#define RINEXENTRY_S5DATA     13
#define RINEXENTRY_C6DATA     14
#define RINEXENTRY_L6DATA     15
#define RINEXENTRY_D6DATA     16
#define RINEXENTRY_S6DATA     17
#define RINEXENTRY_C7DATA    18 /* 5B */
#define RINEXENTRY_L7DATA    19
#define RINEXENTRY_D7DATA    20
#define RINEXENTRY_S7DATA    21
#define RINEXENTRY_C8DATA    22 /* 5AB */
#define RINEXENTRY_L8DATA    23
#define RINEXENTRY_D8DATA    24
#define RINEXENTRY_S8DATA    25

#define RINEXENTRY_NUMBER     26

#define LIGHTSPEED         2.99792458e8    /* m/sec */
#define GPS_FREQU_L1       1575420000.0  /* Hz */
#define GPS_FREQU_L2       1227600000.0  /* Hz */
#define GPS_FREQU_L5       1176450000.0  /* Hz */
#define GPS_WAVELENGTH_L1  (LIGHTSPEED / GPS_FREQU_L1) /* m */
#define GPS_WAVELENGTH_L2  (LIGHTSPEED / GPS_FREQU_L2) /* m */
#define GPS_WAVELENGTH_L5  (LIGHTSPEED / GPS_FREQU_L5) /* m */

#define GLO_FREQU_L1_BASE  1602000000.0  /* Hz */
#define GLO_FREQU_L2_BASE  1246000000.0  /* Hz */
#define GLO_FREQU_L1_STEP      562500.0  /* Hz */
#define GLO_FREQU_L2_STEP      437500.0  /* Hz */
#define GLO_FREQU_L1(a)      (GLO_FREQU_L1_BASE+(a)*GLO_FREQU_L1_STEP)
#define GLO_FREQU_L2(a)      (GLO_FREQU_L2_BASE+(a)*GLO_FREQU_L2_STEP)
#define GLO_WAVELENGTH_L1(a) (LIGHTSPEED / GLO_FREQU_L1(a)) /* m */
#define GLO_WAVELENGTH_L2(a) (LIGHTSPEED / GLO_FREQU_L2(a)) /* m */

#define GAL_FREQU_E1       1575420000.0  /* Hz */
#define GAL_FREQU_E5A      1176450000.0  /* Hz */
#define GAL_FREQU_E5AB     1197950000.0  /* Hz */
#define GAL_FREQU_E5B      1207140000.0  /* Hz */
#define GAL_FREQU_E6       1278750000.0  /* Hz */
#define GAL_WAVELENGTH_E1     (LIGHTSPEED / GAL_FREQU_E1) /* m */
#define GAL_WAVELENGTH_E5A    (LIGHTSPEED / GAL_FREQU_E5A) /* m */
#define GAL_WAVELENGTH_E5AB   (LIGHTSPEED / GAL_FREQU_E5AB) /* m */
#define GAL_WAVELENGTH_E5B    (LIGHTSPEED / GAL_FREQU_E5B) /* m */
#define GAL_WAVELENGTH_E6     (LIGHTSPEED / GAL_FREQU_E6) /* m */

#define QZSS_FREQU_L1       1575420000.0  /* Hz */
#define QZSS_FREQU_L2       1227600000.0  /* Hz */
#define QZSS_FREQU_L5       1176450000.0  /* Hz */
#define QZSS_FREQU_LEX      1278750000.0 /* Hz */
#define QZSS_WAVELENGTH_L1  (LIGHTSPEED / QZSS_FREQU_L1) /* m */
#define QZSS_WAVELENGTH_L2  (LIGHTSPEED / QZSS_FREQU_L2) /* m */
#define QZSS_WAVELENGTH_L5  (LIGHTSPEED / QZSS_FREQU_L5) /* m */
#define QZSS_WAVELENGTH_LEX (LIGHTSPEED / QZSS_FREQU_LEX) /* m */

#define COMPASS_FREQU_B1       1561098000.0  /* Hz */
#define COMPASS_FREQU_B2       1207140000.0  /* Hz */
#define COMPASS_FREQU_B3       1268520000.0  /* Hz */
#define COMPASS_WAVELENGTH_B1  (LIGHTSPEED / COMPASS_FREQU_B1) /* m */
#define COMPASS_WAVELENGTH_B2  (LIGHTSPEED / COMPASS_FREQU_B2) /* m */
#define COMPASS_WAVELENGTH_B3  (LIGHTSPEED / COMPASS_FREQU_B3) /* m */

#define PRN_GIOVE_OFFSET 51

/* Additional flags for the data field, which tell us more. */
#define GNSSDF2_XCORRL2        (1<<0)  /* cross-correlated L2 */
#define GNSSDF2_LOCKLOSSL1     (1<<1)  /* lost lock on L1 */
#define GNSSDF2_LOCKLOSSL2     (1<<2)  /* lost lock on L2 */
#define GNSSDF2_LOCKLOSSL5     (1<<3)  /* lost lock on L5 */
#define GNSSDF2_LOCKLOSSE6     (1<<4)  /* lost lock on E6 */
#define GNSSDF2_LOCKLOSSE5B    (1<<5)  /* lost lock on E5B */
#define GNSSDF2_LOCKLOSSE5AB   (1<<6)  /* lost lock on E5AB */
#define GNSSDF2_LOCKLOSSB1     GNSSDF2_LOCKLOSSL2
#define GNSSDF2_LOCKLOSSB2     GNSSDF2_LOCKLOSSE5B
#define GNSSDF2_LOCKLOSSB3     GNSSDF2_LOCKLOSSE6
#define GNSSDF2_LOCKLOSSLEX    GNSSDF2_LOCKLOSSE6
#define GNSSDF2_LOCKLOSSSAIF   (1<<9)  /* lost lock on SAIF signal */

#define UINT64(c) c ## ULL

struct converttimeinfo {
  int second;    /* seconds of GPS time [0..59] */
  int minute;    /* minutes of GPS time [0..59] */
  int hour;      /* hour of GPS time [0..24] */
  int day;       /* day of GPS time [1..28..30(31)*/
  int month;     /* month of GPS time [1..12]*/
  int year;      /* year of GPS time [1980..] */
};

struct gnssdata {
  int    flags;              /* GNSSF_xxx */
  int    week;               /* week number of GPS date */
  int    numsats;
  double timeofweek;         /* milliseconds in GPS week */
  double measdata[GNSS_MAXSATS][GNSSENTRY_NUMBER];  /* data fields */ 
  unsigned long long dataflags[GNSS_MAXSATS];      /* GNSSDF_xxx */
  unsigned int dataflags2[GNSS_MAXSATS];     /* GNSSDF2_xxx */
  int    satellites[GNSS_MAXSATS];     /* SV - IDs */
  int    snrL1[GNSS_MAXSATS];          /* Important: all the 6 SV-specific fields must */
  int    snrL2[GNSS_MAXSATS];          /* have the same SV-order */
  const char * codetype[GNSS_MAXSATS][GNSSENTRY_NUMBER];
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

#define GALEPHF_E5ADINVALID     (1<<0) /* E5aDVS set invalid */

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

struct galileoephemeris {
  int    flags;            /* GALEPHF_xxx */
  int    satellite;        /* SV ID */
  int    IODnav;
  int    TOC;              /* [s] */
  int    TOE;              /* [s] */
  double clock_bias;       /* [s] */
  double clock_drift;      /* [s/s] */
  double clock_driftrate;  /* [s/s^2] */
  double Crs;              /* [m] */
  double Delta_n;          /* [rad/s] */
  double M0;               /* [rad] */
  double Cuc;              /* [rad] */
  double e;
  double Cus;              /* [rad] */
  double sqrt_A;           /* [m^0.5] */
  double Cic;              /* [rad] */
  double OMEGA0;           /* [rad] */
  double Cis;              /* [rad] */
  double i0;               /* [rad] */
  double Crc;              /* [m] */
  double omega;            /* [rad] */
  double OMEGADOT;         /* [rad/s] */
  double IDOT;             /* [rad/s] */
  double BGD_1_5A;         /* [s] */
  double BGD_1_5B;         /* [s] */
  int    Week;
  int    SISA;
  int    E5aHS;
};

struct DataInfo {
  long long flags[RINEXENTRY_NUMBER];
  int       pos[RINEXENTRY_NUMBER];
  int       numtypes;
  char      type[GNSSENTRY_NUMBER];
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
  struct galileoephemeris ephemerisGALILEO;
  struct glonassephemeris ephemerisGLONASS;
  struct gnssdata DataNew;
  int    GLOFreq[PRN_GLONASS_NUM]; /* frequency numbers of GLONASS + 100 */
  int    size;
  int    lastlockGPSl1[64];
  int    lastlockGPSl2[64];
  int    lastlockGLOl1[64];
  int    lastlockGLOl2[64];
  int    lastlockmsm[RTCM3_MSM_NUMCELLS][NUMRTCM3PARSERSATS];
#ifdef NO_RTCM3_MAIN
  double antX;
  double antY;
  double antZ;
  double antH;
  char   antenna[256+1];
  int    blocktype;
  int    allflags;
  int    modjulday;
  int    secofday;
  int    leapsec;
#endif /* NO_RTCM3_MAIN */
  int          pos[RINEXENTRY_NUMBER];
  long long    flags[RINEXENTRY_NUMBER];
  /* For RINEX2 only field GPS is used */
  struct DataInfo info[RTCM3_MSM_NUMSYS];
  int          validwarning;
  int          init;
  int          startflags;
  int          rinex3;
  const char * headerfile;
  const char * glonassephemeris;
  const char * gpsephemeris;
  FILE *       glonassfile;
  FILE *       gpsfile;
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
