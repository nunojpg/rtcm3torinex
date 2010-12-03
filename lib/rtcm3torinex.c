/*
  Converter for RTCM3 data to RINEX.
  $Id$
  Copyright (C) 2005-2008 by Dirk Stöcker <stoecker@alberding.eu>

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

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef NO_RTCM3_MAIN
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#ifndef sparc
#include <stdint.h>
#endif

#ifndef isinf
#define isinf(x) 0
#endif

#include "rtcm3torinex.h"

/* CVS revision and version */
static char revisionstr[] = "$Revision$";

#ifndef COMPILEDATE
#define COMPILEDATE " built " __DATE__
#endif

static uint32_t CRC24(long size, const unsigned char *buf)
{
  uint32_t crc = 0;
  int i;

  while(size--)
  {
    crc ^= (*buf++) << (16);
    for(i = 0; i < 8; i++)
    {
      crc <<= 1;
      if(crc & 0x1000000)
        crc ^= 0x01864cfb;
    }
  }
  return crc;
}

static int GetMessage(struct RTCM3ParserData *handle)
{
  unsigned char *m, *e;
  int i;

  m = handle->Message+handle->SkipBytes;
  e = handle->Message+handle->MessageSize;
  handle->NeedBytes = handle->SkipBytes = 0;
  while(e-m >= 3)
  {
    if(m[0] == 0xD3)
    {
      handle->size = ((m[1]&3)<<8)|m[2];
      if(e-m >= handle->size+6)
      {
        if((uint32_t)((m[3+handle->size]<<16)|(m[3+handle->size+1]<<8)
        |(m[3+handle->size+2])) == CRC24(handle->size+3, m))
        {
          handle->SkipBytes = handle->size;
          break;
        }
        else
          ++m;
      }
      else
      {
        handle->NeedBytes = handle->size+6;
        break;
      }
    }
    else
      ++m;
  }
  if(e-m < 3)
    handle->NeedBytes = 3;

  /* copy buffer to front */
  i = m - handle->Message;
  if(i && m < e)
    memmove(handle->Message, m, (size_t)(handle->MessageSize-i));
  handle->MessageSize -= i;

  return !handle->NeedBytes;
}

#define LOADBITS(a) \
{ \
  while((a) > numbits) \
  { \
    if(!size--) break; \
    bitfield = (bitfield<<8)|*(data++); \
    numbits += 8; \
  } \
}

/* extract bits from data stream
   b = variable to store result, a = number of bits */
#define GETBITS(b, a) \
{ \
  LOADBITS(a) \
  b = (bitfield<<(64-numbits))>>(64-(a)); \
  numbits -= (a); \
}

/* extract bits from data stream
   b = variable to store result, a = number of bits */
#define GETBITSFACTOR(b, a, c) \
{ \
  LOADBITS(a) \
  b = ((bitfield<<(sizeof(bitfield)*8-numbits))>>(sizeof(bitfield)*8-(a)))*(c); \
  numbits -= (a); \
}

/* extract floating value from data stream
   b = variable to store result, a = number of bits */
#define GETFLOAT(b, a, c) \
{ \
  LOADBITS(a) \
  b = ((double)((bitfield<<(64-numbits))>>(64-(a))))*(c); \
  numbits -= (a); \
}

/* extract signed floating value from data stream
   b = variable to store result, a = number of bits */
#define GETFLOATSIGN(b, a, c) \
{ \
  LOADBITS(a) \
  b = ((double)(((int64_t)(bitfield<<(64-numbits)))>>(64-(a))))*(c); \
  numbits -= (a); \
}

/* extract bits from data stream
   b = variable to store result, a = number of bits */
#define GETBITSSIGN(b, a) \
{ \
  LOADBITS(a) \
  b = ((int64_t)(bitfield<<(64-numbits)))>>(64-(a)); \
  numbits -= (a); \
}

#define GETFLOATSIGNM(b, a, c) \
{ int l; \
  LOADBITS(a) \
  l = (bitfield<<(64-numbits))>>(64-1); \
  b = ((double)(((bitfield<<(64-(numbits-1))))>>(64-(a-1))))*(c); \
  numbits -= (a); \
  if(l) b *= -1.0; \
}

#define SKIPBITS(b) { LOADBITS(b) numbits -= (b); }

/* extract byte-aligned byte from data stream,
   b = variable to store size, s = variable to store string pointer */
#define GETSTRING(b, s) \
{ \
  b = *(data++); \
  s = (char *) data; \
  data += b; \
  size -= b+1; \
}

struct leapseconds { /* specify the day of leap second */
  int day;        /* this is the day, where 23:59:59 exists 2 times */
  int month;      /* not the next day! */
  int year;
  int taicount;
};
static const int months[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
static const struct leapseconds leap[] = {
/*{31, 12, 1971, 11},*/
/*{31, 12, 1972, 12},*/
/*{31, 12, 1973, 13},*/
/*{31, 12, 1974, 14},*/
/*{31, 12, 1975, 15},*/
/*{31, 12, 1976, 16},*/
/*{31, 12, 1977, 17},*/
/*{31, 12, 1978, 18},*/
/*{31, 12, 1979, 19},*/
{30, 06, 1981,20},
{30, 06, 1982,21},
{30, 06, 1983,22},
{30, 06, 1985,23},
{31, 12, 1987,24},
{31, 12, 1989,25},
{31, 12, 1990,26},
{30, 06, 1992,27},
{30, 06, 1993,28},
{30, 06, 1994,29},
{31, 12, 1995,30},
{30, 06, 1997,31},
{31, 12, 1998,32},
{31, 12, 2005,33},
{31, 12, 2008,34},
{0,0,0,0} /* end marker */
};
#define LEAPSECONDS     15 /* only needed for approx. time */
#define GPSLEAPSTART    19 /* 19 leap seconds existed at 6.1.1980 */

static int longyear(int year, int month)
{
  if(!(year % 4) && (!(year % 400) || (year % 100)))
  {
    if(!month || month == 2)
      return 1;
  }
  return 0;
}

int gnumleap(int year, int month, int day)
{
  int ls = 0;
  const struct leapseconds *l;

  for(l = leap; l->taicount && year >= l->year; ++l)
  {
    if(year > l->year || month > l->month || (month == l->month && day > l->day))
       ls = l->taicount - GPSLEAPSTART;
  }
  return ls;
}

/* Convert Moscow time into UTC (fixnumleap == 1) or GPS (fixnumleap == 0) */
void updatetime(int *week, int *secOfWeek, int mSecOfWeek, int fixnumleap)
{
  int y,m,d,k,l, nul;
  unsigned int j = *week*(7*24*60*60) + *secOfWeek + 5*24*60*60+3*60*60;
  int glo_daynumber = 0, glo_timeofday;
  for(y = 1980; j >= (unsigned int)(k = (l = (365+longyear(y,0)))*24*60*60)
  + gnumleap(y+1,1,1); ++y)
  {
    j -= k; glo_daynumber += l;
  }
  for(m = 1; j >= (unsigned int)(k = (l = months[m]+longyear(y, m))*24*60*60)
  + gnumleap(y, m+1, 1); ++m)
  {
    j -= k; glo_daynumber += l;
  }
  for(d = 1; j >= 24UL*60UL*60UL + gnumleap(y, m, d+1); ++d)
    j -= 24*60*60;
  glo_daynumber -= 16*365+4-d;
  nul = gnumleap(y, m, d);
  glo_timeofday = j-nul;

  if(mSecOfWeek < 5*60*1000 && glo_timeofday > 23*60*60)
    *secOfWeek += 24*60*60;
  else if(glo_timeofday < 5*60 && mSecOfWeek > 23*60*60*1000)
    *secOfWeek -= 24*60*60;
  *secOfWeek += mSecOfWeek/1000-glo_timeofday;
  if(fixnumleap)
    *secOfWeek -= nul;
  if(*secOfWeek < 0) {*secOfWeek += 24*60*60*7; --*week; }
  if(*secOfWeek >= 24*60*60*7) {*secOfWeek -= 24*60*60*7; ++*week; }
}

int RTCM3Parser(struct RTCM3ParserData *handle)
{
  int ret=0;

#ifdef NO_RTCM3_MAIN
  if(GetMessage(handle)) /* don't repeat */
#else
  while(!ret && GetMessage(handle))
#endif /* NO_RTCM3_MAIN */
  {
    /* using 64 bit integer types, as it is much easier than handling
    the long datatypes in 32 bit */
    uint64_t numbits = 0, bitfield = 0;
    int size = handle->size, type;
    int syncf, old = 0;
    unsigned char *data = handle->Message+3;

    GETBITS(type,12)
#ifdef NO_RTCM3_MAIN
    handle->blocktype = type;
#endif /* NO_RTCM3_MAIN */
    switch(type)
    {
#ifdef NO_RTCM3_MAIN
    default:
      ret = type;
      break;
    case 1005: case 1006:
      {
        SKIPBITS(22)
        GETBITSSIGN(handle->antX, 38)
        SKIPBITS(2)
        GETBITSSIGN(handle->antY, 38)
        SKIPBITS(2)
        GETBITSSIGN(handle->antZ, 38)
        if(type == 1006)
          GETBITS(handle->antH, 16)
        ret = type;
      }
      break;
    case 1007: case 1008: case 1033:
      {
        char *antenna;
        int antnum;

        SKIPBITS(12)
        GETSTRING(antnum,antenna)
        memcpy(handle->antenna, antenna, antnum);
        handle->antenna[antnum] = 0;
        ret = type;
      }
      break;
    case 1013:
      {
        SKIPBITS(12);
        GETBITS(handle->modjulday, 16);
        GETBITS(handle->secofday, 17);
        SKIPBITS(5);
        GETBITS(handle->leapsec, 8);
        ret = 1013;
      }
      break;
#endif /* NO_RTCM3_MAIN */
    case 1019:
      {
        struct gpsephemeris *ge;
        int sv, i;

        ge = &handle->ephemerisGPS;
        memset(ge, 0, sizeof(*ge));

        GETBITS(sv, 6)
        ge->satellite = (sv < 40 ? sv : sv+80);
        GETBITS(ge->GPSweek, 10)
        ge->GPSweek += 1024;
        GETBITS(ge->URAindex, 4)
        GETBITS(sv, 2)
        if(sv & 1)
          ge->flags |= GPSEPHF_L2PCODE;
        if(sv & 2)
          ge->flags |= GPSEPHF_L2CACODE;
        GETFLOATSIGN(ge->IDOT, 14, R2R_PI/(double)(1<<30)/(double)(1<<13))
        GETBITS(ge->IODE, 8)
        GETBITS(ge->TOC, 16)
        ge->TOC <<= 4;
        GETFLOATSIGN(ge->clock_driftrate, 8, 1.0/(double)(1<<30)/(double)(1<<25))
        GETFLOATSIGN(ge->clock_drift, 16, 1.0/(double)(1<<30)/(double)(1<<13))
        GETFLOATSIGN(ge->clock_bias, 22, 1.0/(double)(1<<30)/(double)(1<<1))
        GETBITS(ge->IODC, 10)
        GETFLOATSIGN(ge->Crs, 16, 1.0/(double)(1<<5))
        GETFLOATSIGN(ge->Delta_n, 16, R2R_PI/(double)(1<<30)/(double)(1<<13))
        GETFLOATSIGN(ge->M0, 32, R2R_PI/(double)(1<<30)/(double)(1<<1))
        GETFLOATSIGN(ge->Cuc, 16, 1.0/(double)(1<<29))
        GETFLOAT(ge->e, 32, 1.0/(double)(1<<30)/(double)(1<<3))
        GETFLOATSIGN(ge->Cus, 16, 1.0/(double)(1<<29))
        GETFLOAT(ge->sqrt_A, 32, 1.0/(double)(1<<19))
        GETBITS(ge->TOE, 16)
        ge->TOE <<= 4;

        GETFLOATSIGN(ge->Cic, 16, 1.0/(double)(1<<29))
        GETFLOATSIGN(ge->OMEGA0, 32, R2R_PI/(double)(1<<30)/(double)(1<<1))
        GETFLOATSIGN(ge->Cis, 16, 1.0/(double)(1<<29))
        GETFLOATSIGN(ge->i0, 32, R2R_PI/(double)(1<<30)/(double)(1<<1))
        GETFLOATSIGN(ge->Crc, 16, 1.0/(double)(1<<5))
        GETFLOATSIGN(ge->omega, 32, R2R_PI/(double)(1<<30)/(double)(1<<1))
        GETFLOATSIGN(ge->OMEGADOT, 24, R2R_PI/(double)(1<<30)/(double)(1<<13))
        GETFLOATSIGN(ge->TGD, 8, 1.0/(double)(1<<30)/(double)(1<<1))
        GETBITS(ge->SVhealth, 6)
        GETBITS(sv, 1)
        if(sv)
          ge->flags |= GPSEPHF_L2PCODEDATA;

        i = ((int)ge->GPSweek - (int)handle->GPSWeek)*7*24*60*60
        + ((int)ge->TOE - (int)handle->GPSTOW) - 2*60*60;
        if(i > 5*60*60 && i < 8*60*60)
        {
          handle->GPSTOW = ge->TOE;
          handle->GPSWeek = ge->GPSweek;
        }
        ret = 1019;
      }
      break;
    case 1045:
      {
        struct galileoephemeris *ge;
        int sv;

        ge = &handle->ephemerisGALILEO;
        memset(ge, 0, sizeof(*ge));

        GETBITS(sv, 6)
        ge->satellite = sv;
        GETBITS(ge->Week, 12)
        GETBITS(ge->IODnav, 10)
        GETBITS(ge->SISA, 8)
        GETFLOATSIGN(ge->IDOT, 14, R2R_PI/(double)(1<<30)/(double)(1<<13))
        GETBITSFACTOR(ge->TOC, 14, 60)
        GETFLOATSIGN(ge->clock_driftrate, 6, 1.0/(double)(1<<30)/(double)(1<<29))
        GETFLOATSIGN(ge->clock_drift, 21, 1.0/(double)(1<<30)/(double)(1<<16))
        GETFLOATSIGN(ge->clock_bias, 31, 1.0/(double)(1<<30)/(double)(1<<4))
        GETFLOATSIGN(ge->Crs, 16, 1.0/(double)(1<<5))
        GETFLOATSIGN(ge->Delta_n, 16, R2R_PI/(double)(1<<30)/(double)(1<<13))
        GETFLOATSIGN(ge->M0, 32, R2R_PI/(double)(1<<30)/(double)(1<<1))
        GETFLOATSIGN(ge->Cuc, 16, 1.0/(double)(1<<29))
        GETFLOAT(ge->e, 32, 1.0/(double)(1<<30)/(double)(1<<3))
        GETFLOATSIGN(ge->Cus, 16, 1.0/(double)(1<<29))
        GETFLOAT(ge->sqrt_A, 32, 1.0/(double)(1<<19))
        GETBITSFACTOR(ge->TOE, 14, 60)
        GETFLOATSIGN(ge->Cic, 16, 1.0/(double)(1<<29))
        GETFLOATSIGN(ge->OMEGA0, 32, R2R_PI/(double)(1<<30)/(double)(1<<1))
        GETFLOATSIGN(ge->Cis, 16, 1.0/(double)(1<<29))
        GETFLOATSIGN(ge->i0, 32, R2R_PI/(double)(1<<30)/(double)(1<<1))
        GETFLOATSIGN(ge->Crc, 16, 1.0/(double)(1<<5))
        GETFLOATSIGN(ge->omega, 32, R2R_PI/(double)(1<<30)/(double)(1<<1))
        GETFLOATSIGN(ge->OMEGADOT, 24, R2R_PI/(double)(1<<30)/(double)(1<<13))
        GETFLOATSIGN(ge->BGD_1_5A, 10, 1.0/(double)(1<<30)/(double)(1<<2))
        GETBITS(ge->E5aHS, 2)
        GETBITS(sv, 1)
        if(sv)
          ge->flags |= GALEPHF_E5ADINVALID;
        ret = 1045;
      }
      break;
    case 1020:
      {
        struct glonassephemeris *ge;
        int i;

        ge = &handle->ephemerisGLONASS;
        memset(ge, 0, sizeof(*ge));

        ge->flags |= GLOEPHF_PAVAILABLE;
        GETBITS(ge->almanac_number, 6)
        GETBITS(i, 5)
        ge->frequency_number = i-7;
        if(ge->almanac_number >= 1 && ge->almanac_number <= PRN_GLONASS_NUM)
          handle->GLOFreq[ge->almanac_number-1] = 100+ge->frequency_number;
        GETBITS(i, 1)
        if(i)
          ge->flags |= GLOEPHF_ALMANACHEALTHY;
        GETBITS(i, 1)
        if(i)
          ge->flags |= GLOEPHF_ALMANACHEALTHOK;
        GETBITS(i, 2)
        if(i & 1)
          ge->flags |= GLOEPHF_P10TRUE;
        if(i & 2)
          ge->flags |= GLOEPHF_P11TRUE;
        GETBITS(i, 5)
        ge->tk = i*60*60;
        GETBITS(i, 6)
        ge->tk += i*60;
        GETBITS(i, 1)
        ge->tk += i*30;
        GETBITS(i, 1)
        if(i)
          ge->flags |= GLOEPHF_UNHEALTHY;
        GETBITS(i, 1)
        if(i)
          ge->flags |= GLOEPHF_P2TRUE;
        GETBITS(i, 7)
        ge->tb = i*15*60;
        GETFLOATSIGNM(ge->x_velocity, 24, 1.0/(double)(1<<20))
        GETFLOATSIGNM(ge->x_pos, 27, 1.0/(double)(1<<11))
        GETFLOATSIGNM(ge->x_acceleration, 5, 1.0/(double)(1<<30))
        GETFLOATSIGNM(ge->y_velocity, 24, 1.0/(double)(1<<20))
        GETFLOATSIGNM(ge->y_pos, 27, 1.0/(double)(1<<11))
        GETFLOATSIGNM(ge->y_acceleration, 5, 1.0/(double)(1<<30))
        GETFLOATSIGNM(ge->z_velocity, 24, 1.0/(double)(1<<20))
        GETFLOATSIGNM(ge->z_pos, 27, 1.0/(double)(1<<11))
        GETFLOATSIGNM(ge->z_acceleration, 5, 1.0/(double)(1<<30))
        GETBITS(i, 1)
        if(i)
          ge->flags |= GLOEPHF_P3TRUE;
        GETFLOATSIGNM(ge->gamma, 11, 1.0/(double)(1<<30)/(double)(1<<10))
        SKIPBITS(3) /* GLONASS-M P, GLONASS-M ln (third string) */
        GETFLOATSIGNM(ge->tau, 22, 1.0/(double)(1<<30)) /* GLONASS tau n(tb) */
        SKIPBITS(5) /* GLONASS-M delta tau n(tb) */
        GETBITS(ge->E, 5)
        /* GETBITS(b, 1) / * GLONASS-M P4 */
        /* GETBITS(b, 4) / * GLONASS-M Ft */
        /* GETBITS(b, 11) / * GLONASS-M Nt */
        /* GETBITS(b, 2) / * GLONASS-M M */
        /* GETBITS(b, 1) / * GLONASS-M The Availability of Additional Data */
        /* GETBITS(b, 11) / * GLONASS-M Na */
        /* GETFLOATSIGNM(b, 32, 1.0/(double)(1<<30)/(double)(1<<1)) / * GLONASS tau c */
        /* GETBITS(b, 5) / * GLONASS-M N4 */
        /* GETFLOATSIGNM(b, 22, 1.0/(double)(1<<30)/(double)(1<<1)) / * GLONASS-M tau GPS */
        /* GETBITS(b, 1) / * GLONASS-M ln (fifth string) */
        ge->GPSWeek = handle->GPSWeek;
        ge->GPSTOW = handle->GPSTOW;
        ret = 1020;
      }
      break;
    case 1001: case 1002: case 1003: case 1004:
      if(handle->GPSWeek)
      {
        int lastlockl1[64];
        int lastlockl2[64];
        struct gnssdata *gnss;
        int i, numsats, wasamb=0;

        for(i = 0; i < 64; ++i)
          lastlockl1[i] = lastlockl2[i] = 0;

        gnss = &handle->DataNew;

        SKIPBITS(12) /* id */
        GETBITS(i,30)
        if(i/1000 < (int)handle->GPSTOW - 86400)
          ++handle->GPSWeek;
        handle->GPSTOW = i/1000;
        if(gnss->week && (gnss->timeofweek != i || gnss->week
        != handle->GPSWeek))
        {
          handle->Data = *gnss;
          memset(gnss, 0, sizeof(*gnss));
          old = 1;
        }
        gnss->timeofweek = i;
        gnss->week = handle->GPSWeek;

        GETBITS(syncf,1) /* sync */
        GETBITS(numsats,5)
        SKIPBITS(4) /* smind, smint */

        while(numsats-- && gnss->numsats < GNSS_MAXSATS)
        {
          int sv, code, l1range, c,l,s,ce,le,se,amb=0;
          int fullsat, num;

          GETBITS(sv, 6)
          fullsat = sv < 40 ? sv : sv+80;
          for(num = 0; num < gnss->numsats
          && fullsat != gnss->satellites[num]; ++num)
            ;

          if(num == gnss->numsats)
            gnss->satellites[gnss->numsats++] = fullsat;

          /* L1 */
          GETBITS(code, 1);
          if(code)
          {
            c = GNSSDF_P1DATA;  ce = GNSSENTRY_P1DATA;
            l = GNSSDF_L1PDATA; le = GNSSENTRY_L1PDATA;
            s = GNSSDF_S1PDATA; se = GNSSENTRY_S1PDATA;
          }
          else
          {
            c = GNSSDF_C1DATA;  ce = GNSSENTRY_C1DATA;
            l = GNSSDF_L1CDATA; le = GNSSENTRY_L1CDATA;
            s = GNSSDF_S1CDATA; se = GNSSENTRY_S1CDATA;
          }
          GETBITS(l1range, 24);
          GETBITSSIGN(i, 20);
          if((i&((1<<20)-1)) != 0x80000)
          {
            gnss->dataflags[num] |= (c|l);
            gnss->measdata[num][ce] = l1range*0.02;
            gnss->measdata[num][le] = l1range*0.02+i*0.0005;
          }
          GETBITS(i, 7);
          lastlockl1[sv] = i;
          if(handle->lastlockGPSl1[sv] > i || i == 0)
            gnss->dataflags2[num] |= GNSSDF2_LOCKLOSSL1;
          if(type == 1002 || type == 1004)
          {
            GETBITS(amb,8);
            if(amb && (gnss->dataflags[num] & c))
            {
              gnss->measdata[num][ce] += amb*299792.458;
              gnss->measdata[num][le] += amb*299792.458;
              ++wasamb;
            }
            GETBITS(i, 8);
            if(i)
            {
              gnss->dataflags[num] |= s;
              gnss->measdata[num][se] = i*0.25;
              i /= 4*4;
              if(i > 9) i = 9;
              else if(i < 1) i = 1;
              gnss->snrL1[num] = i;
            }
          }
          gnss->measdata[num][le] /= GPS_WAVELENGTH_L1;
          if(type == 1003 || type == 1004)
          {
            /* L2 */
            GETBITS(code,2);
            if(code)
            {
              c = GNSSDF_P2DATA;  ce = GNSSENTRY_P2DATA;
              l = GNSSDF_L2PDATA; le = GNSSENTRY_L2PDATA;
              s = GNSSDF_S2PDATA; se = GNSSENTRY_S2PDATA;
              if(code >= 2)
                gnss->dataflags2[num] |= GNSSDF2_XCORRL2;
            }
            else
            {
              c = GNSSDF_C2DATA;  ce = GNSSENTRY_C2DATA;
              l = GNSSDF_L2CDATA; le = GNSSENTRY_L2CDATA;
              s = GNSSDF_S2CDATA; se = GNSSENTRY_S2CDATA;
            }
            GETBITSSIGN(i,14);
            if((i&((1<<14)-1)) != 0x2000)
            {
              gnss->dataflags[num] |= c;
              gnss->measdata[num][ce] = l1range*0.02+i*0.02
              +amb*299792.458;
            }
            GETBITSSIGN(i,20);
            if((i&((1<<20)-1)) != 0x80000)
            {
              gnss->dataflags[num] |= l;
              gnss->measdata[num][le] = l1range*0.02+i*0.0005
              +amb*299792.458;
            }
            GETBITS(i,7);
            lastlockl2[sv] = i;
            if(handle->lastlockGPSl2[sv] > i || i == 0)
              gnss->dataflags2[num] |= GNSSDF2_LOCKLOSSL2;
            if(type == 1004)
            {
              GETBITS(i, 8);
              if(i)
              {
                gnss->dataflags[num] |= s;
                gnss->measdata[num][se] = i*0.25;
                i /= 4*4;
                if(i > 9) i = 9;
                else if(i < 1) i = 1;
                gnss->snrL2[num] = i;
              }
            }
            gnss->measdata[num][le] /= GPS_WAVELENGTH_L2;
          }
        }
        for(i = 0; i < 64; ++i)
        {
          handle->lastlockGPSl1[i] = lastlockl1[i];
          handle->lastlockGPSl2[i] = lastlockl2[i];
        }
        if(!syncf && !old)
        {
          handle->Data = *gnss;
          memset(gnss, 0, sizeof(*gnss));
        }
        if(!syncf || old)
        {
          if(wasamb) /* not RINEX compatible without */
            ret = 1;
          else
            ret = 2;
        }
#ifdef NO_RTCM3_MAIN
        else
          ret = type;
#endif /* NO_RTCM3_MAIN */
      }
      break;
    case 1009: case 1010: case 1011: case 1012:
      {
        int lastlockl1[64];
        int lastlockl2[64];
        struct gnssdata *gnss;
        int i, numsats;
        int wasamb=0;

        for(i = 0; i < 64; ++i)
          lastlockl1[i] = lastlockl2[i] = 0;

        gnss = &handle->DataNew;

        SKIPBITS(12) /* id */;
        GETBITS(i,27) /* tk */

        updatetime(&handle->GPSWeek, &handle->GPSTOW, i, 0); /* Moscow -> GPS */
        i = handle->GPSTOW*1000;
        if(gnss->week && (gnss->timeofweek != i || gnss->week
        != handle->GPSWeek))
        {
          handle->Data = *gnss;
          memset(gnss, 0, sizeof(*gnss));
          old = 1;
        }

        gnss->timeofweek = i;
        gnss->week = handle->GPSWeek;

        GETBITS(syncf,1) /* sync */
        GETBITS(numsats,5)

        SKIPBITS(4) /* smind, smint */

        while(numsats-- && gnss->numsats < GNSS_MAXSATS)
        {
          int sv, code, l1range, c,l,s,ce,le,se,amb=0;
          int freq;
          int fullsat, num;

          GETBITS(sv, 6)
          fullsat = sv-1 + PRN_GLONASS_START;
          for(num = 0; num < gnss->numsats
          && fullsat != gnss->satellites[num]; ++num)
            ;

          if(num == gnss->numsats)
            gnss->satellites[gnss->numsats++] = fullsat;

          /* L1 */
          GETBITS(code, 1)
          GETBITS(freq, 5)

          if(sv >= 1 && sv <= PRN_GLONASS_NUM)
            handle->GLOFreq[sv-1] = 100+freq-7;

          if(code)
          {
            c = GNSSDF_P1DATA;  ce = GNSSENTRY_P1DATA;
            l = GNSSDF_L1PDATA; le = GNSSENTRY_L1PDATA;
            s = GNSSDF_S1PDATA; se = GNSSENTRY_S1PDATA;
          }
          else
          {
            c = GNSSDF_C1DATA;  ce = GNSSENTRY_C1DATA;
            l = GNSSDF_L1CDATA; le = GNSSENTRY_L1CDATA;
            s = GNSSDF_S1CDATA; se = GNSSENTRY_S1CDATA;
          }
          GETBITS(l1range, 25)
          GETBITSSIGN(i, 20)
          if((i&((1<<20)-1)) != 0x80000)
          {
            /* Handle this like GPS. Actually for GLONASS L1 range is always
               valid. To be on the save side, we handle it as invalid like we
               do for GPS and also remove range in case of 0x80000. */
            gnss->dataflags[num] |= (c|l);
            gnss->measdata[num][ce] = l1range*0.02;
            gnss->measdata[num][le] = l1range*0.02+i*0.0005;
          }
          GETBITS(i, 7)
          lastlockl1[sv] = i;
          if(handle->lastlockGLOl1[sv] > i || i == 0)
            gnss->dataflags2[num] |= GNSSDF2_LOCKLOSSL1;
          if(type == 1010 || type == 1012)
          {
            GETBITS(amb,7)
            if(amb && (gnss->dataflags[num] & c))
            {
              gnss->measdata[num][ce] += amb*599584.916;
              gnss->measdata[num][le] += amb*599584.916;
              ++wasamb;
            }
            GETBITS(i, 8)
            if(i)
            {
              gnss->dataflags[num] |= s;
              gnss->measdata[num][se] = i*0.25;
              i /= 4*4;
              if(i > 9) i = 9;
              else if(i < 1) i = 1;
              gnss->snrL1[num] = i;
            }
          }
          gnss->measdata[num][le] /= GLO_WAVELENGTH_L1(freq-7);
          if(type == 1011 || type == 1012)
          {
            /* L2 */
            GETBITS(code,2)
            if(code)
            {
              c = GNSSDF_P2DATA;  ce = GNSSENTRY_P2DATA;
              l = GNSSDF_L2PDATA; le = GNSSENTRY_L2PDATA;
              s = GNSSDF_S2PDATA; se = GNSSENTRY_S2PDATA;
            }
            else
            {
              c = GNSSDF_C2DATA;  ce = GNSSENTRY_C2DATA;
              l = GNSSDF_L2CDATA; le = GNSSENTRY_L2CDATA;
              s = GNSSDF_S2CDATA; se = GNSSENTRY_S2CDATA;
            }
            GETBITSSIGN(i,14)
            if((i&((1<<14)-1)) != 0x2000)
            {
              gnss->dataflags[num] |= c;
              gnss->measdata[num][ce] = l1range*0.02+i*0.02
              +amb*599584.916;
            }
            GETBITSSIGN(i,20)
            if((i&((1<<20)-1)) != 0x80000)
            {
              gnss->dataflags[num] |= l;
              gnss->measdata[num][le] = l1range*0.02+i*0.0005
              +amb*599584.916;
            }
            GETBITS(i,7)
            lastlockl2[sv] = i;
            if(handle->lastlockGLOl2[sv] > i || i == 0)
              gnss->dataflags2[num] |= GNSSDF2_LOCKLOSSL2;
            if(type == 1012)
            {
              GETBITS(i, 8)
              if(i)
              {
                gnss->dataflags[num] |= s;
                gnss->measdata[num][se] = i*0.25;
                i /= 4*4;
                if(i > 9) i = 9;
                else if(i < 1) i = 1;
                gnss->snrL2[num] = i;
              }
            }
            gnss->measdata[num][le] /= GLO_WAVELENGTH_L2(freq-7);
          }
          if(!sv || sv > 24) /* illegal, remove it again */
            --gnss->numsats;
        }
        for(i = 0; i < 64; ++i)
        {
          handle->lastlockGLOl1[i] = lastlockl1[i];
          handle->lastlockGLOl2[i] = lastlockl2[i];
        }
        if(!syncf && !old)
        {
          handle->Data = *gnss;
          memset(gnss, 0, sizeof(*gnss));
        }
        if(!syncf || old)
        {
          if(wasamb) /* not RINEX compatible without */
            ret = 1;
          else
            ret = 2;
        }
#ifdef NO_RTCM3_MAIN
        else
          ret = type;
#endif /* NO_RTCM3_MAIN */
      }
      break;
    case 1071: case 1081: case 1091:
    case 1072: case 1082: case 1092:
    case 1073: case 1083: case 1093:
    case 1074: case 1084: case 1094:
    case 1075: case 1085: case 1095:
    case 1076: case 1086: case 1096:
    case 1077: case 1087: case 1097:
      if(handle->GPSWeek)
      {
        struct CodeData {
          int typeR;
          int typeP;
          int typeD;
          int typeS;
          int lock;
          double wl;
          const char *code; /* currently unused */
        };
        struct CodeData gps[RTCM3_MSM_NUMSIG] =
        {
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C1DATA,GNSSENTRY_L1CDATA,GNSSENTRY_D1CDATA,
          GNSSENTRY_S1CDATA,GNSSDF2_LOCKLOSSL1,GPS_WAVELENGTH_L1,"1C"},
          {GNSSENTRY_P1DATA,GNSSENTRY_L1PDATA,GNSSENTRY_D1PDATA,
          GNSSENTRY_S1PDATA,GNSSDF2_LOCKLOSSL1,GPS_WAVELENGTH_L1,"1P"},
          {GNSSENTRY_P1DATA,GNSSENTRY_L1PDATA,GNSSENTRY_D1PDATA,
          GNSSENTRY_S1PDATA,GNSSDF2_LOCKLOSSL1,GPS_WAVELENGTH_L1,"1W"},
          {GNSSENTRY_P1DATA,GNSSENTRY_L1PDATA,GNSSENTRY_D1PDATA,
          GNSSENTRY_S1PDATA,GNSSDF2_LOCKLOSSL1,GPS_WAVELENGTH_L1,"1Y"},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C2DATA,GNSSENTRY_L2CDATA,GNSSENTRY_D2CDATA,
          GNSSENTRY_S2CDATA,GNSSDF2_LOCKLOSSL2,GPS_WAVELENGTH_L2,"2C"},
          {GNSSENTRY_P2DATA,GNSSENTRY_L2PDATA,GNSSENTRY_D2PDATA,
          GNSSENTRY_S2PDATA,GNSSDF2_LOCKLOSSL2,GPS_WAVELENGTH_L2,"2P"},
          {GNSSENTRY_P2DATA,GNSSENTRY_L2PDATA,GNSSENTRY_D2PDATA,
          GNSSENTRY_S2PDATA,GNSSDF2_LOCKLOSSL2,GPS_WAVELENGTH_L2,"2W"},
          {GNSSENTRY_P2DATA,GNSSENTRY_L2PDATA,GNSSENTRY_D2PDATA,
          GNSSENTRY_S2PDATA,GNSSDF2_LOCKLOSSL2,GPS_WAVELENGTH_L2,"2Y"},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C2DATA,GNSSENTRY_L2CDATA,GNSSENTRY_D2CDATA,
          GNSSENTRY_S2CDATA,GNSSDF2_LOCKLOSSL2,GPS_WAVELENGTH_L2,"2S"},
          {GNSSENTRY_C2DATA,GNSSENTRY_L2CDATA,GNSSENTRY_D2CDATA,
          GNSSENTRY_S2CDATA,GNSSDF2_LOCKLOSSL2,GPS_WAVELENGTH_L2,"2L"},
          {GNSSENTRY_C2DATA,GNSSENTRY_L2CDATA,GNSSENTRY_D2CDATA,
          GNSSENTRY_S2CDATA,GNSSDF2_LOCKLOSSL2,GPS_WAVELENGTH_L2,"2X"},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C5DATA,GNSSENTRY_L5DATA,GNSSENTRY_D5DATA,
          GNSSENTRY_S5DATA,GNSSDF2_LOCKLOSSL5,GPS_WAVELENGTH_L5,"5I"},
          {GNSSENTRY_C5DATA,GNSSENTRY_L5DATA,GNSSENTRY_D5DATA,
          GNSSENTRY_S5DATA,GNSSDF2_LOCKLOSSL5,GPS_WAVELENGTH_L5,"5Q"},
          {GNSSENTRY_C5DATA,GNSSENTRY_L5DATA,GNSSENTRY_D5DATA,
          GNSSENTRY_S5DATA,GNSSDF2_LOCKLOSSL5,GPS_WAVELENGTH_L5,"5X"}
        };
        /* NOTE: Uses 0.0, 1.0 for wavelength as sat index dependence is done later! */
        struct CodeData glo[RTCM3_MSM_NUMSIG] =
        {
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C1DATA,GNSSENTRY_L1CDATA,GNSSENTRY_D1CDATA,
          GNSSENTRY_S1CDATA,GNSSDF2_LOCKLOSSL1,0.0,"1C"},
          {GNSSENTRY_P1DATA,GNSSENTRY_L1PDATA,GNSSENTRY_D1PDATA,
          GNSSENTRY_S1PDATA,GNSSDF2_LOCKLOSSL1,0.0,"1P"},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C2DATA,GNSSENTRY_L2CDATA,GNSSENTRY_D2CDATA,
          GNSSENTRY_S2CDATA,GNSSDF2_LOCKLOSSL2,1.0,"2C"},
          {GNSSENTRY_P2DATA,GNSSENTRY_L2PDATA,GNSSENTRY_D2PDATA,
          GNSSENTRY_S2PDATA,GNSSDF2_LOCKLOSSL2,1.0,"2P"},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0},
          {0,0,0,0,0,0,0}
        };
        struct CodeData gal[RTCM3_MSM_NUMSIG] =
        {
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C1DATA,GNSSENTRY_L1CDATA,GNSSENTRY_D1CDATA,
          GNSSENTRY_S1CDATA,GNSSDF2_LOCKLOSSL1,GAL_WAVELENGTH_E1,"1C"},
          {GNSSENTRY_C1DATA,GNSSENTRY_L1CDATA,GNSSENTRY_D1CDATA,
          GNSSENTRY_S1CDATA,GNSSDF2_LOCKLOSSL1,GAL_WAVELENGTH_E1,"1A"},
          {GNSSENTRY_C1DATA,GNSSENTRY_L1CDATA,GNSSENTRY_D1CDATA,
          GNSSENTRY_S1CDATA,GNSSDF2_LOCKLOSSL1,GAL_WAVELENGTH_E1,"1B"},
          {GNSSENTRY_C1DATA,GNSSENTRY_L1CDATA,GNSSENTRY_D1CDATA,
          GNSSENTRY_S1CDATA,GNSSDF2_LOCKLOSSL1,GAL_WAVELENGTH_E1,"1X"},
          {GNSSENTRY_C1DATA,GNSSENTRY_L1CDATA,GNSSENTRY_D1CDATA,
          GNSSENTRY_S1CDATA,GNSSDF2_LOCKLOSSL1,GAL_WAVELENGTH_E1,"1Z"},
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C6DATA,GNSSENTRY_L6DATA,GNSSENTRY_D6DATA,
          GNSSENTRY_S6DATA,GNSSDF2_LOCKLOSSE6,GAL_WAVELENGTH_E6,"6I"},
          {GNSSENTRY_C6DATA,GNSSENTRY_L6DATA,GNSSENTRY_D6DATA,
          GNSSENTRY_S6DATA,GNSSDF2_LOCKLOSSE6,GAL_WAVELENGTH_E6,"6Q"},
          {GNSSENTRY_C6DATA,GNSSENTRY_L6DATA,GNSSENTRY_D6DATA,
          GNSSENTRY_S6DATA,GNSSDF2_LOCKLOSSE6,GAL_WAVELENGTH_E6,"6I"},
          {GNSSENTRY_C6DATA,GNSSENTRY_L6DATA,GNSSENTRY_D6DATA,
          GNSSENTRY_S6DATA,GNSSDF2_LOCKLOSSE6,GAL_WAVELENGTH_E6,"6Q"},
          {GNSSENTRY_C6DATA,GNSSENTRY_L6DATA,GNSSENTRY_D6DATA,
          GNSSENTRY_S6DATA,GNSSDF2_LOCKLOSSE6,GAL_WAVELENGTH_E6,"6X"},
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C5BDATA,GNSSENTRY_L5BDATA,GNSSENTRY_D5BDATA,
          GNSSENTRY_S5BDATA,GNSSDF2_LOCKLOSSE5B,GAL_WAVELENGTH_E5B,"7I"},
          {GNSSENTRY_C5BDATA,GNSSENTRY_L5BDATA,GNSSENTRY_D5BDATA,
          GNSSENTRY_S5BDATA,GNSSDF2_LOCKLOSSE5B,GAL_WAVELENGTH_E5B,"7Q"},
          {GNSSENTRY_C5BDATA,GNSSENTRY_L5BDATA,GNSSENTRY_D5BDATA,
          GNSSENTRY_S5BDATA,GNSSDF2_LOCKLOSSE5B,GAL_WAVELENGTH_E5B,"7X"},
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C5ABDATA,GNSSENTRY_L5ABDATA,GNSSENTRY_D5ABDATA,
          GNSSENTRY_S5ABDATA,GNSSDF2_LOCKLOSSE5AB,GAL_WAVELENGTH_E5AB,"8I"},
          {GNSSENTRY_C5ABDATA,GNSSENTRY_L5ABDATA,GNSSENTRY_D5ABDATA,
          GNSSENTRY_S5ABDATA,GNSSDF2_LOCKLOSSE5AB,GAL_WAVELENGTH_E5AB,"8Q"},
          {GNSSENTRY_C5ABDATA,GNSSENTRY_L5ABDATA,GNSSENTRY_D5ABDATA,
          GNSSENTRY_S5ABDATA,GNSSDF2_LOCKLOSSE5AB,GAL_WAVELENGTH_E5AB,"8X"},
          {0,0,0,0,0,0,0},
          {GNSSENTRY_C5DATA,GNSSENTRY_L5DATA,GNSSENTRY_D5DATA,
          GNSSENTRY_S5DATA,GNSSDF2_LOCKLOSSL5,GAL_WAVELENGTH_E5A,"5I"},
          {GNSSENTRY_C5DATA,GNSSENTRY_L5DATA,GNSSENTRY_D5DATA,
          GNSSENTRY_S5DATA,GNSSDF2_LOCKLOSSL5,GAL_WAVELENGTH_E5A,"5Q"},
          {GNSSENTRY_C5DATA,GNSSENTRY_L5DATA,GNSSENTRY_D5DATA,
          GNSSENTRY_S5DATA,GNSSDF2_LOCKLOSSL5,GAL_WAVELENGTH_E5A,"5X"},
        };

        int sys = RTCM3_MSM_GPS, i=0, count, j, old = 0, wasnoamb = 0;
        int syncf, sigmask, numsat = 0, numsig = 0, numcells;
        uint64_t satmask, cellmask, ui;
        double rrmod[RTCM3_MSM_NUMSAT];
        int rrint[RTCM3_MSM_NUMSAT], rdop[RTCM3_MSM_NUMSAT];
        int ll[RTCM3_MSM_NUMCELLS];
        double cnr[RTCM3_MSM_NUMCELLS];
        double cp[RTCM3_MSM_NUMCELLS], psr[RTCM3_MSM_NUMCELLS],
        dop[RTCM3_MSM_NUMCELLS];
        struct gnssdata *gnss = &handle->DataNew;

        SKIPBITS(12)
        if(type >= 1091)
          sys = RTCM3_MSM_GALILEO;
        else if(type >= 1081)
          sys = RTCM3_MSM_GLONASS;

        switch(sys)
        {
        case RTCM3_MSM_GALILEO: /* use DF004 instead of DF248 */
        case RTCM3_MSM_GPS:
          GETBITS(i,30)
          if(i/1000 < (int)handle->GPSTOW - 86400)
            ++handle->GPSWeek;
          handle->GPSTOW = i/1000;
          break;
        case RTCM3_MSM_GLONASS:
          SKIPBITS(3)
          GETBITS(i,27) /* tk */

          updatetime(&handle->GPSWeek, &handle->GPSTOW, i, 0); /* Moscow -> GPS */
          i = handle->GPSTOW*1000;
          break;
        }

        if(gnss->week && (gnss->timeofweek != i || gnss->week
        != handle->GPSWeek))
        {
          handle->Data = *gnss;
          memset(gnss, 0, sizeof(*gnss));
          old = 1;
        }
        gnss->timeofweek = i;
        gnss->week = handle->GPSWeek;

        GETBITS(syncf, 1)
        if(((type % 10) == 6) || ((type % 10) == 7))
          SKIPBITS(3)
        GETBITS(satmask, RTCM3_MSM_NUMSAT)

        /* http://gurmeetsingh.wordpress.com/2008/08/05/fast-bit-counting-routines/ */
        for(ui = satmask; ui; ui &= (ui - 1) /* remove rightmost bit */)
          ++numsat;
        GETBITS(sigmask, RTCM3_MSM_NUMSIG)
        for(i = sigmask; i; i &= (i - 1) /* remove rightmost bit */)
          ++numsig;
        i = numsat*numsig;
        GETBITS(cellmask, (unsigned)i)

        switch(type % 10)
        {
        case 1: case 2: case 3:
          ++wasnoamb;
          for(j = numsat; j--;)
            GETFLOAT(rrmod[j], 10, 1/1024.0)
          break;
        case 4: case 6:
          for(j = numsat; j--;)
            GETBITS(rrint[j], 8)
          for(j = numsat; j--;)
            GETFLOAT(rrmod[j], 10, 1/1024.0)
          break;
        case 5: case 7:
          for(j = numsat; j--;)
            GETBITS(rrint[j], 8)
          for(j = numsat; j--;)
            GETFLOAT(rrmod[j], 10, 1/1024.0)
          for(j = numsat; j--;)
            GETBITSSIGN(rdop[j], 14)
          break;
        }

        numcells = numsat*numsig;
        if(numcells <= RTCM3_MSM_NUMCELLS)
        {
          switch(type % 10)
          {
          case 1:
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(psr[count], 15, 0.02)
            break;
          case 2:
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(cp[count], 20, 1/256.0)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETBITS(ll[count], 4)
            break;
          case 3:
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(psr[count], 15, 0.02)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(cp[count], 20, 1/256.0)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETBITS(ll[count], 4)
            break;
          case 4:
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(psr[count], 15, 0.02)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(cp[count], 20, 1/256.0)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETBITS(ll[count], 4)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETBITS(cnr[count], 6)
            break;
          case 5:
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(psr[count], 15, 0.02)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(cp[count], 20, 1/256.0)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETBITS(ll[count], 4)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOAT(cnr[count], 6, 1.0)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(dop[count], 15, 0.0001)
            break;
          case 6:
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(psr[count], 20, 0.001)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(cp[count], 22, 1/1024.0)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETBITS(ll[count], 10)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOAT(cnr[count], 10, 0.1)
          case 7:
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(psr[count], 20, 0.001)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(cp[count], 22, 1/1024.0)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETBITS(ll[count], 10)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOAT(cnr[count], 10, 0.1)
            for(count = numcells; count--;)
              if(cellmask & (UINT64(1)<<count))
                GETFLOATSIGN(dop[count], 15, 0.0001)
            break;
          }
          i = RTCM3_MSM_NUMSAT;
          j = -1;
          for(count = numcells; count--;)
          {
            while(j >= 0 && !(sigmask&(1<<--j)))
              ;
            if(j < 0)
            {
              while(!(satmask&(UINT64(1)<<(--i)))) /* next satellite */
                ;
              j = RTCM3_MSM_NUMSIG;
              while(!(sigmask&(1<<--j)))
                ;
              --numsat;
            }
            if(cellmask & (UINT64(1)<<count))
            {
              struct CodeData cd = {0,0,0,0,0,0,0};
              double wl = 0.0;
              switch(sys)
              {
              case RTCM3_MSM_GPS: cd = gps[RTCM3_MSM_NUMSIG-j-1];
                wl = cd.wl;
                break;
              case RTCM3_MSM_GLONASS: cd = glo[RTCM3_MSM_NUMSIG-j-1];
                {
                  int k = handle->GLOFreq[RTCM3_MSM_NUMSAT-i-1];
                  if(k)
                  {
                    if(cd.wl == 0.0)
                      wl = GLO_WAVELENGTH_L1(k-100);
                    else if(cd.wl == 1.0)
                      wl = GLO_WAVELENGTH_L2(k-100);
                  }
                }
                break;
              case RTCM3_MSM_GALILEO: cd = gal[RTCM3_MSM_NUMSIG-j-1];
                wl = cd.wl;
                break;
              }
              if(cd.lock && wl) /* lock cannot have a valid zero value */
              {
                int fullsat = sys+RTCM3_MSM_NUMSAT-i-1, num;
                /* FIXME: workaround for GIOVE */
                if(fullsat >= PRN_GALILEO_START && fullsat <= PRN_GALILEO_START+2)
                  fullsat += PRN_GIOVE_START-PRN_GALILEO_START;
                for(num = 0; num < gnss->numsats
                && fullsat != gnss->satellites[num]; ++num)
                  ;

                if(num == gnss->numsats)
                  gnss->satellites[gnss->numsats++] = fullsat;

                switch(type % 10)
                {
                case 1:
                  if(psr[count] > -327.68)
                  {
                    gnss->measdata[num][cd.typeR] = psr[count]
                    +(rrmod[numsat])*LIGHTSPEED/1000.0;
                    gnss->dataflags[num] |= (1<<cd.typeR);
                  }
                  break;
                case 2:
                  if(wl && cp[count] > -2048.0)
                  {
                    gnss->measdata[num][cd.typeP] = cp[count]
                    +(rrmod[numsat])*LIGHTSPEED/1000.0/wl;
                    if(handle->lastlockmsm[j][i] != ll[count])
                    {
                      gnss->dataflags2[num] |= cd.lock;
                      handle->lastlockmsm[j][i] = ll[count];
                    }
                    gnss->dataflags[num] |= (1<<cd.typeP);
                  }
                  break;
                case 3:
                  if(psr[count] > -327.68)
                  {
                    gnss->measdata[num][cd.typeR] = psr[count]
                    +(rrmod[numsat])*LIGHTSPEED/1000.0;
                    gnss->dataflags[num] |= (1<<cd.typeR);
                  }

                  if(wl && cp[count] > -2048.0)
                  {
                    gnss->measdata[num][cd.typeP] = cp[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0/wl;
                    if(handle->lastlockmsm[j][i] != ll[count])
                    {
                      gnss->dataflags2[num] |= cd.lock;
                      handle->lastlockmsm[j][i] = ll[count];
                    }
                    gnss->dataflags[num] |= (1<<cd.typeP);
                  }
                  break;
                case 4:
                  if(psr[count] > -327.68)
                  {
                    gnss->measdata[num][cd.typeR] = psr[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0;
                    gnss->dataflags[num] |= (1<<cd.typeR);
                  }

                  if(wl && cp[count] > -2048.0)
                  {
                    gnss->measdata[num][cd.typeP] = cp[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0/wl;
                    if(handle->lastlockmsm[j][i] != ll[count])
                    {
                      gnss->dataflags2[num] |= cd.lock;
                      handle->lastlockmsm[j][i] = ll[count];
                    }
                    gnss->dataflags[num] |= (1<<cd.typeP);
                  }

                  gnss->measdata[num][cd.typeS] = cnr[count];
                    gnss->dataflags[num] |= (1<<cd.typeS);
                  break;
                case 5:
                  if(psr[count] > -327.68)
                  {
                    gnss->measdata[num][cd.typeR] = psr[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0;
                    gnss->dataflags[num] |= (1<<cd.typeR);
                  }

                  if(wl && cp[count] > -2048.0)
                  {
                    gnss->measdata[num][cd.typeP] = cp[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0/wl;
                    if(handle->lastlockmsm[j][i] != ll[count])
                    {
                      gnss->dataflags2[num] |= cd.lock;
                      handle->lastlockmsm[j][i] = ll[count];
                    }
                    gnss->dataflags[num] |= (1<<cd.typeP);
                  }

                  gnss->measdata[num][cd.typeS] = cnr[count];
                    gnss->dataflags[num] |= (1<<cd.typeS);

                  if(dop[count] > -1.6384)
                  {
                    gnss->measdata[num][cd.typeD] = dop[count]+rdop[numsat];
                    gnss->dataflags[num] |= (1<<cd.typeD);
                  }
                  break;
                case 6:
                  if(psr[count] > -524.288)
                  {
                    gnss->measdata[num][cd.typeR] = psr[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0;
                    gnss->dataflags[num] |= (1<<cd.typeR);
                  }

                  if(wl && cp[count] > -2055.0)
                  {
                    gnss->measdata[num][cd.typeP] = cp[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0/wl;
                    if(handle->lastlockmsm[j][i] != ll[count])
                    {
                      gnss->dataflags2[num] |= cd.lock;
                      handle->lastlockmsm[j][i] = ll[count];
                    }
                    gnss->dataflags[num] |= (1<<cd.typeP);
                  }

                  gnss->measdata[num][cd.typeS] = cnr[count];
                    gnss->dataflags[num] |= (1<<cd.typeS);
                  break;
                case 7:
                  if(psr[count] > -524.288)
                  {
                    gnss->measdata[num][cd.typeR] = psr[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0;
                    gnss->dataflags[num] |= (1<<cd.typeR);
                  }

                  if(wl && cp[count] > -2055.0)
                  {
                    gnss->measdata[num][cd.typeP] = cp[count]
                    +(rrmod[numsat]+rrint[numsat])*LIGHTSPEED/1000.0/wl;
                    if(handle->lastlockmsm[j][i] != ll[count])
                    {
                      gnss->dataflags2[num] |= cd.lock;
                      handle->lastlockmsm[j][i] = ll[count];
                    }
                    gnss->dataflags[num] |= (1<<cd.typeP);
                  }

                  gnss->measdata[num][cd.typeS] = cnr[count];
                    gnss->dataflags[num] |= (1<<cd.typeS);

                  if(dop[count] > -1.6384)
                  {
                    gnss->measdata[num][cd.typeD] = dop[count]+rdop[numsat];
                    gnss->dataflags[num] |= (1<<cd.typeD);
                  }
                  break;
                }
              }
            }
          }
        }
        if(!syncf && !old)
        {
          handle->Data = *gnss;
          memset(gnss, 0, sizeof(*gnss));
        }
        if(!syncf || old)
        {
          if(!wasnoamb) /* not RINEX compatible without */
            ret = 1;
          else
            ret = 2;
        }
#ifdef NO_RTCM3_MAIN
        else
          ret = type;
#endif /* NO_RTCM3_MAIN */
      }
      break;
    }
  }
  return ret;
}

struct Header
{
  const char *version;
  const char *pgm;
  const char *marker;
  const char *markertype;
  const char *observer;
  const char *receiver;
  const char *antenna;
  const char *position;
  const char *antennaposition;
  const char *wavelength;
  const char *typesofobs; /* should not be modified outside */
  const char *typesofobsG; /* should not be modified outside */
  const char *typesofobsR; /* should not be modified outside */
  const char *typesofobsS; /* should not be modified outside */
  const char *typesofobsE; /* should not be modified outside */
  const char *timeoffirstobs; /* should not be modified outside */
};

#define MAXHEADERLINES 50
#define MAXHEADERBUFFERSIZE 4096
struct HeaderData
{
  union
  {
    struct Header named;
    const char *unnamed[MAXHEADERLINES];
  } data;
  int  numheaders;
};

void converttime(struct converttimeinfo *c, int week, int tow)
{
  int i, k, doy, j; /* temporary variables */
  j = week*(7*24*60*60) + tow + 5*24*60*60;
  for(i = 1980; j >= (k = (365+longyear(i,0))*24*60*60); ++i)
    j -= k;
  c->year = i;
  doy = 1+ (j / (24*60*60));
  j %= (24*60*60);
  c->hour = j / (60*60);
  j %= (60*60);
  c->minute = j / 60;
  c->second = j % 60;
  j = 0;
  for(i = 1; j + (k = months[i] + longyear(c->year,i)) < doy; ++i)
    j += k;
  c->month = i;
  c->day = doy - j;
}

#ifndef NO_RTCM3_MAIN
void RTCM3Error(const char *fmt, ...)
{
  va_list v;
  va_start(v, fmt);
  vfprintf(stderr, fmt, v);
  va_end(v);
}
#endif

void RTCM3Text(const char *fmt, ...)
{
  va_list v;
  va_start(v, fmt);
  vprintf(fmt, v);
  va_end(v);
}

static void fixrevision(void)
{
  if(revisionstr[0] == '$')
  {
    char *a;
    int i=sizeof(RTCM3TORINEX_VERSION); /* set version to 1.<revision> */
    strcpy(revisionstr, RTCM3TORINEX_VERSION ".");
    for(a = revisionstr+11; *a && *a != ' '; ++a)
      revisionstr[i++] = *a;
    revisionstr[i] = 0;
  }
}

static int HandleRunBy(char *buffer, int buffersize, const char **u,
int rinex3)
{
  const char *user;
  time_t t;
  struct tm * t2;

#ifdef NO_RTCM3_MAIN
  fixrevision();
#endif

  user= getenv("USER");
  if(!user) user = "";
  t = time(&t);
  t2 = gmtime(&t);
  if(u) *u = user;
  return 1+snprintf(buffer, buffersize,
  rinex3 ?
  "RTCM3TORINEX %-7.7s%-20.20s%04d%02d%02d %02d%02d%02d UTC "
  "PGM / RUN BY / DATE" :
  "RTCM3TORINEX %-7.7s%-20.20s%04d-%02d-%02d %02d:%02d    "
  "PGM / RUN BY / DATE", revisionstr, user, 1900+t2->tm_year,
  t2->tm_mon+1, t2->tm_mday, t2->tm_hour, t2->tm_min, t2->tm_sec);
}

#ifdef NO_RTCM3_MAIN
#define NUMSTARTSKIP 1
#else
#define NUMSTARTSKIP 3
#endif

void HandleHeader(struct RTCM3ParserData *Parser)
{
#ifdef NO_RTCM3_MAIN
  int i;
  if(Parser->allflags == 0)
    Parser->allflags = ~0;
  if(Parser->rinex3)
  {
#define CHECKFLAGSNEW(a, b, c) \
    if(Parser->allflags & GNSSDF_##b##DATA) \
    { \
      Parser->dataflag##a[Parser->numdatatypes##a] = GNSSDF_##b##DATA; \
      Parser->datapos##a[Parser->numdatatypes##a] = GNSSENTRY_##b##DATA; \
      ++Parser->numdatatypes##a; \
    }

    CHECKFLAGSNEW(GPS, C1,  C1C)
    CHECKFLAGSNEW(GPS, L1C, L1C)
    CHECKFLAGSNEW(GPS, D1C, D1C)
    CHECKFLAGSNEW(GPS, S1C, S1C)
    CHECKFLAGSNEW(GPS, P1,  C1P)
    CHECKFLAGSNEW(GPS, L1P, L1P)
    CHECKFLAGSNEW(GPS, D1P, D1P)
    CHECKFLAGSNEW(GPS, S1P, S1P)
    CHECKFLAGSNEW(GPS, P2,  C2P)
    CHECKFLAGSNEW(GPS, L2P, L2P)
    CHECKFLAGSNEW(GPS, D2P, D2P)
    CHECKFLAGSNEW(GPS, S2P, S2P)
    CHECKFLAGSNEW(GPS, C2,  C2X)
    CHECKFLAGSNEW(GPS, L2C, L2X)
    CHECKFLAGSNEW(GPS, D2C, D2X)
    CHECKFLAGSNEW(GPS, S2C, S2X)
    CHECKFLAGSNEW(GLO, C1,  C1C)
    CHECKFLAGSNEW(GLO, L1C, L1C)
    CHECKFLAGSNEW(GLO, D1C, D1C)
    CHECKFLAGSNEW(GLO, S1C, S1C)
    CHECKFLAGSNEW(GLO, P1,  C1P)
    CHECKFLAGSNEW(GLO, L1P, L1P)
    CHECKFLAGSNEW(GLO, D1P, D1P)
    CHECKFLAGSNEW(GLO, S1P, S1P)
    CHECKFLAGSNEW(GLO, P2,  C2P)
    CHECKFLAGSNEW(GLO, L2P, L2P)
    CHECKFLAGSNEW(GLO, D2P, D2P)
    CHECKFLAGSNEW(GLO, S2P, S2P)
    CHECKFLAGSNEW(GLO, C2,  C2C)
    CHECKFLAGSNEW(GLO, L2C, L2C)
    CHECKFLAGSNEW(GLO, D2C, D2C)
    CHECKFLAGSNEW(GLO, S2C, S2C)
  }
  else
  {
#define CHECKFLAGS(a, b) \
    if(Parser->allflags & GNSSDF_##a##DATA) \
    { \
      if(data[RINEXENTRY_##b##DATA]) \
      { \
        Parser->dataflagGPS[data[RINEXENTRY_##b##DATA]-1] = GNSSDF_##a##DATA; \
        Parser->dataposGPS[data[RINEXENTRY_##b##DATA]-1] = GNSSENTRY_##a##DATA; \
      } \
      else \
      { \
        Parser->dataflag[Parser->numdatatypesGPS] = GNSSDF_##a##DATA; \
        Parser->datapos[Parser->numdatatypesGPS] = GNSSENTRY_##a##DATA; \
        data[RINEXENTRY_##b##DATA] = ++Parser->numdatatypesGPS; \
      } \
    }

    int data[RINEXENTRY_NUMBER];
    for(i = 0; i < RINEXENTRY_NUMBER; ++i) data[i] = 0;

    CHECKFLAGS(C1,C1)
    CHECKFLAGS(C2,C2)
    CHECKFLAGS(P1,P1)
    CHECKFLAGS(P2,P2)
    CHECKFLAGS(L1C,L1)
    CHECKFLAGS(L1P,L1)
    CHECKFLAGS(L2C,L2)
    CHECKFLAGS(L2P,L2)
    CHECKFLAGS(D1C,D1)
    CHECKFLAGS(D1P,D1)
    CHECKFLAGS(D2C,D2)
    CHECKFLAGS(D2P,D2)
    CHECKFLAGS(S1C,S1)
    CHECKFLAGS(S1P,S1)
    CHECKFLAGS(S2C,S2)
    CHECKFLAGS(S2P,S2)
    CHECKFLAGS(C5,C5)
    CHECKFLAGS(L5,L5)
    CHECKFLAGS(D5,D5)
    CHECKFLAGS(S5,S5)
    CHECKFLAGS(C5AB,C5AB)
    CHECKFLAGS(L5AB,L5AB)
    CHECKFLAGS(D5AB,D5AB)
    CHECKFLAGS(S5AB,S5AB)
    CHECKFLAGS(C5B,C5B)
    CHECKFLAGS(L5B,L5B)
    CHECKFLAGS(D5B,D5B)
    CHECKFLAGS(S5B,S5B)
    CHECKFLAGS(C6,C6)
    CHECKFLAGS(L6,L6)
    CHECKFLAGS(D6,D6)
    CHECKFLAGS(S6,S6)
  }
#else /* NO_RTCM3_MAIN */
  struct HeaderData hdata;
  char thebuffer[MAXHEADERBUFFERSIZE];
  char *buffer = thebuffer;
  size_t buffersize = sizeof(thebuffer);
  int i;

  memset(&hdata, 0, sizeof(hdata));

  hdata.data.named.version = buffer;
  i = 1+snprintf(buffer, buffersize,
  "%9.2f           OBSERVATION DATA    M (Mixed)"
  "           RINEX VERSION / TYPE", Parser->rinex3 ? 3.0 : 2.11);
  buffer += i; buffersize -= i;

  {
    const char *str;
    hdata.data.named.pgm = buffer;
    i = HandleRunBy(buffer, buffersize, &str, Parser->rinex3);
    buffer += i; buffersize -= i;
    hdata.data.named.observer = buffer;
    i = 1+snprintf(buffer, buffersize,
    "%-20.20s                                        "
    "OBSERVER / AGENCY", str);
    buffer += i; buffersize -= i;
  }

  hdata.data.named.marker =
  "RTCM3TORINEX                                                "
  "MARKER NAME";

  hdata.data.named.markertype =  !Parser->rinex3 ? 0 :
  "GEODETIC                                                    "
  "MARKER TYPE";

  hdata.data.named.receiver =
  "                                                            "
  "REC # / TYPE / VERS";

  hdata.data.named.antenna =
  "                                                            "
  "ANT # / TYPE";

  hdata.data.named.position =
  "         .0000         .0000         .0000                  "
  "APPROX POSITION XYZ";

  hdata.data.named.antennaposition =
  "         .0000         .0000         .0000                  "
  "ANTENNA: DELTA H/E/N";

  hdata.data.named.wavelength = Parser->rinex3 ? 0 :
  "     1     1                                                "
  "WAVELENGTH FACT L1/2";

  if(Parser->rinex3)
  {
#define CHECKFLAGSNEW(a, b, c) \
    if(flags & GNSSDF_##b##DATA) \
    { \
      Parser->dataflag##a[Parser->numdatatypes##a] = GNSSDF_##b##DATA; \
      Parser->datapos##a[Parser->numdatatypes##a] = GNSSENTRY_##b##DATA; \
      ++Parser->numdatatypes##a; \
      snprintf(tbuffer+tbufferpos, sizeof(tbuffer)-tbufferpos, " %-3s", #c); \
      tbufferpos += 4; \
    }

    int flags = Parser->startflags;
    char tbuffer[6*RINEXENTRY_NUMBER+1];
    int tbufferpos = 0;
    for(i = 0; i < Parser->Data.numsats; ++i)
      flags |= Parser->Data.dataflags[i];

    CHECKFLAGSNEW(GPS, C1,  C1C)
    CHECKFLAGSNEW(GPS, L1C, L1C)
    CHECKFLAGSNEW(GPS, D1C, D1C)
    CHECKFLAGSNEW(GPS, S1C, S1C)
    CHECKFLAGSNEW(GPS, P1,  C1W)
    CHECKFLAGSNEW(GPS, L1P, L1W)
    CHECKFLAGSNEW(GPS, D1P, D1W)
    CHECKFLAGSNEW(GPS, S1P, S1W)

    hdata.data.named.typesofobsS = buffer;
    i = 1+snprintf(buffer, buffersize,
    "S  %3d%-52.52s  SYS / # / OBS TYPES", Parser->numdatatypesGPS, tbuffer);
    buffer += i; buffersize -= i;

    CHECKFLAGSNEW(GPS, P2,  C2P)
    CHECKFLAGSNEW(GPS, L2P, L2P)
    CHECKFLAGSNEW(GPS, D2P, D2P)
    CHECKFLAGSNEW(GPS, S2P, S2P)
    CHECKFLAGSNEW(GPS, C2,  C2X)
    CHECKFLAGSNEW(GPS, L2C, L2X)
    CHECKFLAGSNEW(GPS, D2C, D2X)
    CHECKFLAGSNEW(GPS, S2C, S2X)
    CHECKFLAGSNEW(GPS, C5,  C5)
    CHECKFLAGSNEW(GPS, L5,  L5)
    CHECKFLAGSNEW(GPS, D5,  D5)
    CHECKFLAGSNEW(GPS, S5,  S5)

    hdata.data.named.typesofobsG = buffer;
    i = 1+snprintf(buffer, buffersize,
    "G  %3d%-52.52s  SYS / # / OBS TYPES", Parser->numdatatypesGPS, tbuffer);
    if(Parser->numdatatypesGPS>13)
    {
      i += snprintf(buffer+i-1, buffersize,
      "\n      %-52.52s  SYS / # / OBS TYPES", tbuffer+13*4);
    }
    buffer += i; buffersize -= i;

    tbufferpos = 0;

    CHECKFLAGSNEW(GLO, C1,  C1C)
    CHECKFLAGSNEW(GLO, L1C, L1C)
    CHECKFLAGSNEW(GLO, D1C, D1C)
    CHECKFLAGSNEW(GLO, S1C, S1C)
    CHECKFLAGSNEW(GLO, P1,  C1P)
    CHECKFLAGSNEW(GLO, L1P, L1P)
    CHECKFLAGSNEW(GLO, D1P, D1P)
    CHECKFLAGSNEW(GLO, S1P, S1P)
    CHECKFLAGSNEW(GLO, P2,  C2P)
    CHECKFLAGSNEW(GLO, L2P, L2P)
    CHECKFLAGSNEW(GLO, D2P, D2P)
    CHECKFLAGSNEW(GLO, S2P, S2P)
    CHECKFLAGSNEW(GLO, C2,  C2C)
    CHECKFLAGSNEW(GLO, L2C, L2C)
    CHECKFLAGSNEW(GLO, D2C, D2C)
    CHECKFLAGSNEW(GLO, S2C, S2C)

    hdata.data.named.typesofobsR = buffer;
    i = 1+snprintf(buffer, buffersize,
    "R  %3d%-52.52s  SYS / # / OBS TYPES", Parser->numdatatypesGLO, tbuffer);
    if(Parser->numdatatypesGLO>13)
    {
      i += snprintf(buffer+i-1, buffersize,
      "\n      %-52.52s  SYS / # / OBS TYPES", tbuffer+13*4);
    }
    buffer += i; buffersize -= i;

    tbufferpos = 0;

    CHECKFLAGSNEW(GAL, C1,   C1)
    CHECKFLAGSNEW(GAL, L1C,  L1)
    CHECKFLAGSNEW(GAL, D1C,  D1)
    CHECKFLAGSNEW(GAL, S1C,  S1)
    CHECKFLAGSNEW(GAL, C6,   C6)
    CHECKFLAGSNEW(GAL, L6,   L6)
    CHECKFLAGSNEW(GAL, D6,   D6)
    CHECKFLAGSNEW(GAL, S6,   S6)
    CHECKFLAGSNEW(GAL, C5,   C5)
    CHECKFLAGSNEW(GAL, L5,   L5)
    CHECKFLAGSNEW(GAL, D5,   D5)
    CHECKFLAGSNEW(GAL, S5,   S5)
    CHECKFLAGSNEW(GAL, C5B,  C7)
    CHECKFLAGSNEW(GAL, L5B,  L7)
    CHECKFLAGSNEW(GAL, D5B,  D7)
    CHECKFLAGSNEW(GAL, S5B,  S7)
    CHECKFLAGSNEW(GAL, C5AB, C8)
    CHECKFLAGSNEW(GAL, L5AB, L8)
    CHECKFLAGSNEW(GAL, D5AB, D8)
    CHECKFLAGSNEW(GAL, S5AB, S8)

    hdata.data.named.typesofobsE = buffer;
    i = 1+snprintf(buffer, buffersize,
    "E  %3d%-52.52s  SYS / # / OBS TYPES", Parser->numdatatypesGAL, tbuffer);
    if(Parser->numdatatypesGAL>13)
    {
      i += snprintf(buffer+i-1, buffersize,
      "\n      %-52.52s  SYS / # / OBS TYPES", tbuffer+13*4);
    }
    buffer += i; buffersize -= i;
  }
  else
  {
#define CHECKFLAGS(a, b) \
    if(flags & GNSSDF_##a##DATA) \
    { \
      if(data[RINEXENTRY_##b##DATA]) \
      { \
        Parser->dataflagGPS[data[RINEXENTRY_##b##DATA]-1] = GNSSDF_##a##DATA; \
        Parser->dataposGPS[data[RINEXENTRY_##b##DATA]-1] = GNSSENTRY_##a##DATA; \
      } \
      else \
      { \
        Parser->dataflag[Parser->numdatatypesGPS] = GNSSDF_##a##DATA; \
        Parser->datapos[Parser->numdatatypesGPS] = GNSSENTRY_##a##DATA; \
        data[RINEXENTRY_##b##DATA] = ++Parser->numdatatypesGPS; \
        snprintf(tbuffer+tbufferpos, sizeof(tbuffer)-tbufferpos, "    "#b); \
        tbufferpos += 6; \
      } \
    }

    int flags = Parser->startflags;
    int data[RINEXENTRY_NUMBER];
    char tbuffer[6*RINEXENTRY_NUMBER+1];
    int tbufferpos = 0;
    for(i = 0; i < RINEXENTRY_NUMBER; ++i)
      data[i] = 0;
    for(i = 0; i < Parser->Data.numsats; ++i)
      flags |= Parser->Data.dataflags[i];

    CHECKFLAGS(C1,C1)
    CHECKFLAGS(C2,C2)
    CHECKFLAGS(P1,P1)
    CHECKFLAGS(P2,P2)
    CHECKFLAGS(L1C,L1)
    CHECKFLAGS(L1P,L1)
    CHECKFLAGS(L2C,L2)
    CHECKFLAGS(L2P,L2)
    CHECKFLAGS(D1C,D1)
    CHECKFLAGS(D1P,D1)
    CHECKFLAGS(D2C,D2)
    CHECKFLAGS(D2P,D2)
    CHECKFLAGS(S1C,S1)
    CHECKFLAGS(S1P,S1)
    CHECKFLAGS(S2C,S2)
    CHECKFLAGS(S2P,S2)
    CHECKFLAGS(C5,C5)
    CHECKFLAGS(L5,L5)
    CHECKFLAGS(D5,D5)
    CHECKFLAGS(S5,S5)
    CHECKFLAGS(C5AB,C5AB)
    CHECKFLAGS(L5AB,L5AB)
    CHECKFLAGS(D5AB,D5AB)
    CHECKFLAGS(S5AB,S5AB)
    CHECKFLAGS(C5B,C5B)
    CHECKFLAGS(L5B,L5B)
    CHECKFLAGS(D5B,D5B)
    CHECKFLAGS(S5B,S5B)
    CHECKFLAGS(C6,C6)
    CHECKFLAGS(L6,L6)
    CHECKFLAGS(D6,D6)
    CHECKFLAGS(S6,S6)

    hdata.data.named.typesofobs = buffer;
    i = 1+snprintf(buffer, buffersize,
    "%6d%-54.54s# / TYPES OF OBSERV", Parser->numdatatypesGPS, tbuffer);
    if(Parser->numdatatypesGPS>9)
    {
      i += snprintf(buffer+i-1, buffersize,
      "\n      %-54.54s# / TYPES OF OBSERV", tbuffer+9*6);
    }
    buffer += i; buffersize -= i;
  }

  {
    struct converttimeinfo cti;
    converttime(&cti, Parser->Data.week,
    (int)floor(Parser->Data.timeofweek/1000.0));
    hdata.data.named.timeoffirstobs = buffer;
    i = 1+snprintf(buffer, buffersize,
    "  %4d    %2d    %2d    %2d    %2d   %10.7f     GPS         "
    "TIME OF FIRST OBS", cti.year, cti.month, cti.day, cti.hour,
    cti.minute, cti.second + fmod(Parser->Data.timeofweek/1000.0,1.0));

    buffer += i; buffersize -= i;
  }

  hdata.numheaders = 15;

  if(Parser->headerfile)
  {
    FILE *fh;
    if((fh = fopen(Parser->headerfile, "r")))
    {
      size_t siz;
      char *lastblockstart;
      if((siz = fread(buffer, 1, buffersize-1, fh)) > 0)
      {
        buffer[siz] = '\n';
        if(siz == buffersize)
        {
          RTCM3Error("Header file is too large. Only %d bytes read.",
          (int)siz);
        }
        /* scan the file line by line and enter the entries in the list */
        /* warn for "# / TYPES OF OBSERV" and "TIME OF FIRST OBS" */
        /* overwrites entries, except for comments */
        lastblockstart = buffer;
        for(i = 0; i < (int)siz; ++i)
        {
          if(buffer[i] == '\n')
          { /* we found a line */
            char *end;
            while(buffer[i+1] == '\r')
              ++i; /* skip \r in case there are any */
            end = buffer+i;
            while(*end == '\t' || *end == ' ' || *end == '\r' || *end == '\n')
              *(end--) = 0;
            if(end-lastblockstart < 60+5) /* short line */
              RTCM3Error("Short Header line '%s' ignored.\n", lastblockstart);
            else
            {
              int pos;
              if(!strcmp("COMMENT", lastblockstart+60))
                pos = hdata.numheaders;
              else
              {
                for(pos = 0; pos < hdata.numheaders; ++pos)
                {
                  if(!strcmp(hdata.data.unnamed[pos]+60, lastblockstart+60))
                    break;
                }
                if(!strcmp("# / TYPES OF OBSERV", lastblockstart+60)
                || !strcmp("TIME OF FIRST OBS", lastblockstart+60))
                {
                  RTCM3Error("Overwriting header '%s' is dangerous.\n",
                  lastblockstart+60);
                }
              }
              if(pos >= MAXHEADERLINES)
              {
                RTCM3Error("Maximum number of header lines of %d reached.\n",
                MAXHEADERLINES);
              }
              else if(!strcmp("END OF HEADER", lastblockstart+60))
              {
                RTCM3Error("End of header ignored.\n");
              }
              else
              {
                hdata.data.unnamed[pos] = lastblockstart;
                if(pos == hdata.numheaders)
                  ++hdata.numheaders;
              }
            }
            lastblockstart = buffer+i+1;
          }
        }
      }
      else
      {
        RTCM3Error("Could not read data from headerfile '%s'.\n",
        Parser->headerfile);
      }
      fclose(fh);
    }
    else
    {
      RTCM3Error("Could not open header datafile '%s'.\n",
      Parser->headerfile);
    }
  }

  for(i = 0; i < hdata.numheaders; ++i)
  {
    if(hdata.data.unnamed[i] && hdata.data.unnamed[i][0])
      RTCM3Text("%s\n", hdata.data.unnamed[i]);
  }
  RTCM3Text("                                                            "
  "END OF HEADER\n");
#endif
}

static void ConvLine(FILE *file, const char *fmt, ...)
{
  char buffer[100], *b;
  va_list v;
  va_start(v, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, v);
  for(b = buffer; *b; ++b)
  {
    if(*b == 'e') *b = 'D';
  }
  fprintf(file, "%s", buffer);
  va_end(v);
}

void HandleByte(struct RTCM3ParserData *Parser, unsigned int byte)
{
  Parser->Message[Parser->MessageSize++] = byte;
  if(Parser->MessageSize >= Parser->NeedBytes)
  {
    int r;
    while((r = RTCM3Parser(Parser)))
    {
      if(r == 1020 || r == 1019)
      {
        FILE *file = 0;

        if(Parser->rinex3 && !(file = Parser->gpsfile))
        {
          const char *n = Parser->gpsephemeris ? Parser->gpsephemeris : Parser->glonassephemeris;
          if(n)
          {
            if(!(Parser->gpsfile = fopen(n, "w")))
            {
              RTCM3Error("Could not open ephemeris output file.\n");
            }
            else
            {
              char buffer[100];
              fprintf(Parser->gpsfile,
              "%9.2f%11sN: GNSS NAV DATA    M: Mixed%12sRINEX VERSION / TYPE\n", 3.0, "", "");
              HandleRunBy(buffer, sizeof(buffer), 0, Parser->rinex3);
              fprintf(Parser->gpsfile, "%s\n%60sEND OF HEADER\n", buffer, "");
            }
            Parser->gpsephemeris = 0;
            Parser->glonassephemeris = 0;
            file = Parser->gpsfile;
          }
        }
        else
        {
          if(r == 1020)
          {
            if(Parser->glonassephemeris)
            {
              if(!(Parser->glonassfile = fopen(Parser->glonassephemeris, "w")))
              {
                RTCM3Error("Could not open GLONASS ephemeris output file.\n");
              }
              else
              {
                char buffer[100];
                fprintf(Parser->glonassfile,
                "%9.2f%11sG: GLONASS NAV DATA%21sRINEX VERSION / TYPE\n", 2.1, "", "");
                HandleRunBy(buffer, sizeof(buffer), 0, Parser->rinex3);
                fprintf(Parser->glonassfile, "%s\n%60sEND OF HEADER\n", buffer, "");
              }
              Parser->glonassephemeris = 0;
            }
            file = Parser->glonassfile;
          }
          else if(r == 1019)
          {
            if(Parser->gpsephemeris)
            {
              if(!(Parser->gpsfile = fopen(Parser->gpsephemeris, "w")))
              {
                RTCM3Error("Could not open GPS ephemeris output file.\n");
              }
              else
              {
                char buffer[100];
                fprintf(Parser->gpsfile,
                "%9.2f%11sN: GPS NAV DATA%25sRINEX VERSION / TYPE\n", 2.1, "", "");
                HandleRunBy(buffer, sizeof(buffer), 0, Parser->rinex3);
                fprintf(Parser->gpsfile, "%s\n%60sEND OF HEADER\n", buffer, "");
              }
              Parser->gpsephemeris = 0;
            }
            file = Parser->gpsfile;
          }
        }
        if(file)
        {
          if(r == 1020)
          {
            struct glonassephemeris *e = &Parser->ephemerisGLONASS;
            int w = e->GPSWeek, tow = e->GPSTOW, i;
            struct converttimeinfo cti;

            updatetime(&w, &tow, e->tb*1000, 1);  /* Moscow - > UTC */
            converttime(&cti, w, tow);

            i = e->tk-3*60*60; if(i < 0) i += 86400;

            if(Parser->rinex3)
              ConvLine(file, "R%02d %04d %02d %02d %02d %02d %02d%19.12e%19.12e%19.12e\n",
              e->almanac_number, cti.year, cti.month, cti.day, cti.hour, cti.minute,
              cti.second, -e->tau, e->gamma, (double) i);
            else
              ConvLine(file, "%02d %02d %02d %02d %02d %02d%5.1f%19.12e%19.12e%19.12e\n",
              e->almanac_number, cti.year%100, cti.month, cti.day, cti.hour, cti.minute,
              (double) cti.second, -e->tau, e->gamma, (double) i);
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n", e->x_pos,
            e->x_velocity, e->x_acceleration, (e->flags & GLOEPHF_UNHEALTHY) ? 1.0 : 0.0);
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n", e->y_pos,
            e->y_velocity, e->y_acceleration, (double) e->frequency_number);
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n", e->z_pos,
            e->z_velocity, e->z_acceleration, (double) e->E);
          }
          else /* if(r == 1019) */
          {
            struct gpsephemeris *e = &Parser->ephemerisGPS;
            double d;                 /* temporary variable */
            unsigned long int i;       /* temporary variable */
            struct converttimeinfo cti;
            converttime(&cti, e->GPSweek, e->TOC);

            if(Parser->rinex3)
              ConvLine(file, "G%02d %04d %02d %02d %02d %02d %02d%19.12e%19.12e%19.12e\n",
              e->satellite, cti.year, cti.month, cti.day, cti.hour,
              cti.minute, cti.second, e->clock_bias, e->clock_drift,
              e->clock_driftrate);
            else
              ConvLine(file, "%02d %02d %02d %02d %02d %02d%05.1f%19.12e%19.12e%19.12e\n",
              e->satellite, cti.year%100, cti.month, cti.day, cti.hour,
              cti.minute, (double) cti.second, e->clock_bias, e->clock_drift,
              e->clock_driftrate);
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n", (double)e->IODE,
            e->Crs, e->Delta_n, e->M0);
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n", e->Cuc,
            e->e, e->Cus, e->sqrt_A);
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n",
            (double) e->TOE, e->Cic, e->OMEGA0, e->Cis);
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n", e->i0,
            e->Crc, e->omega, e->OMEGADOT);
            d = 0;
            i = e->flags;
            if(i & GPSEPHF_L2CACODE)
              d += 2.0;
            if(i & GPSEPHF_L2PCODE)
              d += 1.0;
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n", e->IDOT, d,
            (double) e->GPSweek, i & GPSEPHF_L2PCODEDATA ? 1.0 : 0.0);
            if(e->URAindex <= 6) /* URA index */
              d = ceil(10.0*pow(2.0, 1.0+((double)e->URAindex)/2.0))/10.0;
            else
              d = ceil(10.0*pow(2.0, ((double)e->URAindex)/2.0))/10.0;
            /* 15 indicates not to use satellite. We can't handle this special
               case, so we create a high "non"-accuracy value. */
            ConvLine(file, "   %19.12e%19.12e%19.12e%19.12e\n", d,
            ((double) e->SVhealth), e->TGD, ((double) e->IODC));

            ConvLine(file, "   %19.12e%19.12e\n", ((double)e->TOW), 0.0);
            /* TOW */
          }
        }
      }
      else if (r == 1 || r == 2)
      {
        int i, j, o;
        struct converttimeinfo cti;

        if(Parser->init < NUMSTARTSKIP) /* skip first epochs to detect correct data types */
        {
          ++Parser->init;

          if(Parser->init == NUMSTARTSKIP)
            HandleHeader(Parser);
          else
          {
            for(i = 0; i < Parser->Data.numsats; ++i)
              Parser->startflags |= Parser->Data.dataflags[i];
            continue;
          }
        }
        if(r == 2 && !Parser->validwarning)
        {
          RTCM3Text("No valid RINEX! All values are modulo 299792.458!"
          "           COMMENT\n");
          Parser->validwarning = 1;
        }

        converttime(&cti, Parser->Data.week,
        (int)floor(Parser->Data.timeofweek/1000.0));
        if(Parser->rinex3)
        {
          RTCM3Text("> %04d %02d %02d %02d %02d%11.7f  0%3d\n",
          cti.year, cti.month, cti.day, cti.hour, cti.minute, cti.second
          + fmod(Parser->Data.timeofweek/1000.0,1.0), Parser->Data.numsats);
          for(i = 0; i < Parser->Data.numsats; ++i)
          {
            int glo = 0, gal = 0;
            if(Parser->Data.satellites[i] <= PRN_GPS_END)
              RTCM3Text("G%02d", Parser->Data.satellites[i]);
            else if(Parser->Data.satellites[i] >= PRN_GLONASS_START
            && Parser->Data.satellites[i] <= PRN_GLONASS_END)
            {
              RTCM3Text("R%02d", Parser->Data.satellites[i] - (PRN_GLONASS_START-1));
              glo = 1;
            }
            else if(Parser->Data.satellites[i] >= PRN_GALILEO_START
            && Parser->Data.satellites[i] <= PRN_GALILEO_END)
            {
              RTCM3Text("E%02d", Parser->Data.satellites[i] - (PRN_GALILEO_START-1));
              gal = 1;
            }
            else if(Parser->Data.satellites[i] >= PRN_GIOVE_START
            && Parser->Data.satellites[i] <= PRN_GIOVE_END)
            {
              RTCM3Text("E%02d", Parser->Data.satellites[i] - (PRN_GIOVE_START-PRN_GIOVE_OFFSET));
              gal = 1;
            }
            else if(Parser->Data.satellites[i] >= PRN_WAAS_START
            && Parser->Data.satellites[i] <= PRN_WAAS_END)
              RTCM3Text("S%02d", Parser->Data.satellites[i] - PRN_WAAS_START+20);
            else
              RTCM3Text("%3d", Parser->Data.satellites[i]);

            if(glo)
            {
              for(j = 0; j < Parser->numdatatypesGLO; ++j)
              {
                int df = Parser->dataflagGLO[j];
                int pos = Parser->dataposGLO[j];
                if((Parser->Data.dataflags[i] & df)
                && !isnan(Parser->Data.measdata[i][pos])
                && !isinf(Parser->Data.measdata[i][pos]))
                {
                  char lli = ' ';
                  char snr = ' ';
                  if(df & (GNSSDF_L1CDATA|GNSSDF_L1PDATA))
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL1)
                      lli = '1';
                    snr = '0'+Parser->Data.snrL1[i];
                  }
                  if(df & (GNSSDF_L2CDATA|GNSSDF_L2PDATA))
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL2)
                      lli = '1';
                    snr = '0'+Parser->Data.snrL2[i];
                  }
                  RTCM3Text("%14.3f%c%c",
                  Parser->Data.measdata[i][pos],lli,snr);
                }
                else
                { /* no or illegal data */
                  RTCM3Text("                ");
                }
              }
            }
            else if(gal)
            {
              for(j = 0; j < Parser->numdatatypesGAL; ++j)
              {
                int df = Parser->dataflagGAL[j];
                int pos = Parser->dataposGAL[j];
                if((Parser->Data.dataflags[i] & df)
                && !isnan(Parser->Data.measdata[i][pos])
                && !isinf(Parser->Data.measdata[i][pos]))
                {
                  char lli = ' ';
                  char snr = ' ';
                  if(df & (GNSSDF_L1CDATA|GNSSDF_L1PDATA))
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL1)
                      lli = '1';
                    snr = '0'+Parser->Data.snrL1[i];
                  }
                  if(df & GNSSDF_L6DATA)
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSE6)
                      lli = '1';
                    snr = ' ';
                  }
                  if(df & GNSSDF_L5DATA)
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL5)
                      lli = '1';
                    snr = ' ';
                  }
                  if(df & GNSSDF_L5BDATA)
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSE5B)
                      lli = '1';
                    snr = ' ';
                  }
                  if(df & GNSSDF_L5ABDATA)
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSE5AB)
                      lli = '1';
                    snr = ' ';
                  }
                  RTCM3Text("%14.3f%c%c",
                  Parser->Data.measdata[i][pos],lli,snr);
                }
                else
                { /* no or illegal data */
                  RTCM3Text("                ");
                }
              }
            }
            else
            {
              for(j = 0; j < Parser->numdatatypesGPS; ++j)
              {
                int df = Parser->dataflagGPS[j];
                int pos = Parser->dataposGPS[j];
                if((Parser->Data.dataflags[i] & df)
                && !isnan(Parser->Data.measdata[i][pos])
                && !isinf(Parser->Data.measdata[i][pos]))
                {
                  char lli = ' ';
                  char snr = ' ';
                  if(df & (GNSSDF_L1CDATA|GNSSDF_L1PDATA))
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL1)
                      lli = '1';
                    snr = '0'+Parser->Data.snrL1[i];
                  }
                  if(df & (GNSSDF_L2CDATA|GNSSDF_L2PDATA))
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL2)
                      lli = '1';
                    snr = '0'+Parser->Data.snrL2[i];
                  }
                  if(df & GNSSDF_L5DATA)
                  {
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL5)
                      lli = '1';
                    snr = ' ';
                  }
                  RTCM3Text("%14.3f%c%c",
                  Parser->Data.measdata[i][pos],lli,snr);
                }
                else
                { /* no or illegal data */
                  RTCM3Text("                ");
                }
              }
            }
            RTCM3Text("\n");
          }
        }
        else
        {
          RTCM3Text(" %02d %2d %2d %2d %2d %10.7f  0%3d",
          cti.year%100, cti.month, cti.day, cti.hour, cti.minute, cti.second
          + fmod(Parser->Data.timeofweek/1000.0,1.0), Parser->Data.numsats);
          for(i = 0; i < 12 && i < Parser->Data.numsats; ++i)
          {
            if(Parser->Data.satellites[i] <= PRN_GPS_END)
              RTCM3Text("G%02d", Parser->Data.satellites[i]);
            else if(Parser->Data.satellites[i] >= PRN_GLONASS_START
            && Parser->Data.satellites[i] <= PRN_GLONASS_END)
              RTCM3Text("R%02d", Parser->Data.satellites[i]
              - (PRN_GLONASS_START-1));
            else if(Parser->Data.satellites[i] >= PRN_WAAS_START
            && Parser->Data.satellites[i] <= PRN_WAAS_END)
              RTCM3Text("S%02d", Parser->Data.satellites[i]
              - PRN_WAAS_START+20);
            else if(Parser->Data.satellites[i] >= PRN_GALILEO_START
            && Parser->Data.satellites[i] <= PRN_GALILEO_END)
              RTCM3Text("E%02d", Parser->Data.satellites[i]
              - (PRN_GALILEO_START-1));
            else if(Parser->Data.satellites[i] >= PRN_GIOVE_START
            && Parser->Data.satellites[i] <= PRN_GIOVE_END)
              RTCM3Text("E%02d", Parser->Data.satellites[i]
              - (PRN_GIOVE_START-PRN_GIOVE_OFFSET));
            else
              RTCM3Text("%3d", Parser->Data.satellites[i]);
          }
          RTCM3Text("\n");
          o = 12;
          j = Parser->Data.numsats - 12;
          while(j > 0)
          {
            RTCM3Text("                                ");
            for(i = o; i < o+12 && i < Parser->Data.numsats; ++i)
            {
              if(Parser->Data.satellites[i] <= PRN_GPS_END)
                RTCM3Text("G%02d", Parser->Data.satellites[i]);
              else if(Parser->Data.satellites[i] >= PRN_GLONASS_START
              && Parser->Data.satellites[i] <= PRN_GLONASS_END)
                RTCM3Text("R%02d", Parser->Data.satellites[i]
                - (PRN_GLONASS_START-1));
              else if(Parser->Data.satellites[i] >= PRN_WAAS_START
              && Parser->Data.satellites[i] <= PRN_WAAS_END)
                RTCM3Text("S%02d", Parser->Data.satellites[i]
                - PRN_WAAS_START+20);
              else if(Parser->Data.satellites[i] >= PRN_GALILEO_START
              && Parser->Data.satellites[i] <= PRN_GALILEO_END)
                RTCM3Text("E%02d", Parser->Data.satellites[i]
                - (PRN_GALILEO_START-1));
              else if(Parser->Data.satellites[i] >= PRN_GIOVE_START
              && Parser->Data.satellites[i] <= PRN_GIOVE_END)
                RTCM3Text("E%02d", Parser->Data.satellites[i]
                - (PRN_GIOVE_START-PRN_GIOVE_OFFSET));
              else
                RTCM3Text("%3d", Parser->Data.satellites[i]);
            }
            RTCM3Text("\n");
            j -= 12;
            o += 12;
          }
          for(i = 0; i < Parser->Data.numsats; ++i)
          {
            for(j = 0; j < Parser->numdatatypesGPS; ++j)
            {
              int v = 0;
              int df = Parser->dataflag[j];
              int pos = Parser->datapos[j];
              if((Parser->Data.dataflags[i] & df)
              && !isnan(Parser->Data.measdata[i][pos])
              && !isinf(Parser->Data.measdata[i][pos]))
              {
                v = 1;
              }
              else
              {
                df = Parser->dataflagGPS[j];
                pos = Parser->dataposGPS[j];

                if((Parser->Data.dataflags[i] & df)
                && !isnan(Parser->Data.measdata[i][pos])
                && !isinf(Parser->Data.measdata[i][pos]))
                {
                  v = 1;
                }
              }

              if(!v)
              { /* no or illegal data */
                RTCM3Text("                ");
              }
              else
              {
                char lli = ' ';
                char snr = ' ';
                if(df & (GNSSDF_L1CDATA|GNSSDF_L1PDATA))
                {
                  if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL1)
                    lli = '1';
                  snr = '0'+Parser->Data.snrL1[i];
                }
                if(df & (GNSSDF_L2CDATA|GNSSDF_L2PDATA))
                {
                  if(Parser->Data.dataflags2[i]
                  & (GNSSDF2_LOCKLOSSL2|GNSSDF2_XCORRL2))
                  {
                    lli = '0';
                    if(Parser->Data.dataflags2[i] & GNSSDF2_LOCKLOSSL2)
                      lli += 1;
                    if(Parser->Data.dataflags2[i] & GNSSDF2_XCORRL2)
                      lli += 4;
                  }
                  snr = '0'+Parser->Data.snrL2[i];
                }
                if((df & GNSSDF_P2DATA) && (Parser->Data.dataflags2[i]
                & GNSSDF2_XCORRL2))
                  lli = '4';
                RTCM3Text("%14.3f%c%c",
                Parser->Data.measdata[i][pos],lli,snr);
              }
              if(j%5 == 4 || j == Parser->numdatatypesGPS-1)
                RTCM3Text("\n");
            }
          }
        }
      }
    }
  }
}

#ifndef NO_RTCM3_MAIN
static char datestr[]     = "$Date$";

/* The string, which is send as agent in HTTP request */
#define AGENTSTRING "NTRIP NtripRTCM3ToRINEX"

#define MAXDATASIZE 1000 /* max number of bytes we can get at once */

static const char encodingTable [64] = {
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
  'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
  'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
  'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

/* does not buffer overrun, but breaks directly after an error */
/* returns the number of required bytes */
static int encode(char *buf, int size, const char *user, const char *pwd)
{
  unsigned char inbuf[3];
  char *out = buf;
  int i, sep = 0, fill = 0, bytes = 0;

  while(*user || *pwd)
  {
    i = 0;
    while(i < 3 && *user) inbuf[i++] = *(user++);
    if(i < 3 && !sep)    {inbuf[i++] = ':'; ++sep; }
    while(i < 3 && *pwd)  inbuf[i++] = *(pwd++);
    while(i < 3)         {inbuf[i++] = 0; ++fill; }
    if(out-buf < size-1)
      *(out++) = encodingTable[(inbuf [0] & 0xFC) >> 2];
    if(out-buf < size-1)
      *(out++) = encodingTable[((inbuf [0] & 0x03) << 4)
               | ((inbuf [1] & 0xF0) >> 4)];
    if(out-buf < size-1)
    {
      if(fill == 2)
        *(out++) = '=';
      else
        *(out++) = encodingTable[((inbuf [1] & 0x0F) << 2)
                 | ((inbuf [2] & 0xC0) >> 6)];
    }
    if(out-buf < size-1)
    {
      if(fill >= 1)
        *(out++) = '=';
      else
        *(out++) = encodingTable[inbuf [2] & 0x3F];
    }
    bytes += 4;
  }
  if(out-buf < size)
    *out = 0;
  return bytes;
}

static int stop = 0;

struct Args
{
  const char *server;
  const char *port;
  int         mode;
  int         timeout;
  int         rinex3;
  const char *user;
  const char *password;
  const char *proxyhost;
  const char *proxyport;
  const char *nmea;
  const char *data;
  const char *headerfile;
  const char *gpsephemeris;
  const char *glonassephemeris;
};

/* option parsing */
#ifdef NO_LONG_OPTS
#define LONG_OPT(a)
#else
#define LONG_OPT(a) a
static struct option opts[] = {
{ "data",             required_argument, 0, 'd'},
{ "server",           required_argument, 0, 's'},
{ "password",         required_argument, 0, 'p'},
{ "port",             required_argument, 0, 'r'},
{ "timeout",          required_argument, 0, 't'},
{ "header",           required_argument, 0, 'f'},
{ "user",             required_argument, 0, 'u'},
{ "gpsephemeris",     required_argument, 0, 'E'},
{ "glonassephemeris", required_argument, 0, 'G'},
{ "rinex3",           no_argument,       0, '3'},
{ "proxyport",        required_argument, 0, 'R'},
{ "proxyhost",        required_argument, 0, 'S'},
{ "nmea",             required_argument, 0, 'n'},
{ "mode",             required_argument, 0, 'M'},
{ "help",             no_argument,       0, 'h'},
{0,0,0,0}};
#endif
#define ARGOPT "-d:s:p:r:t:f:u:E:G:M:S:R:n:h3"

enum MODE { HTTP = 1, RTSP = 2, NTRIP1 = 3, AUTO = 4, END };

static const char *geturl(const char *url, struct Args *args)
{
  static char buf[1000];
  static char *Buffer = buf;
  static char *Bufend = buf+sizeof(buf);

  if(strncmp("ntrip:", url, 6))
    return "URL must start with 'ntrip:'.";
  url += 6; /* skip ntrip: */

  if(*url != '@' && *url != '/')
  {
    /* scan for mountpoint */
    args->data = Buffer;
    while(*url && *url != '@' &&  *url != ';' &&*url != '/' && Buffer != Bufend)
      *(Buffer++) = *(url++);
    if(Buffer == args->data)
      return "Mountpoint required.";
    else if(Buffer >= Bufend-1)
      return "Parsing buffer too short.";
    *(Buffer++) = 0;
  }

  if(*url == '/') /* username and password */
  {
    ++url;
    args->user = Buffer;
    while(*url && *url != '@' && *url != ';' && *url != ':' && Buffer != Bufend)
      *(Buffer++) = *(url++);
    if(Buffer == args->user)
      return "Username cannot be empty.";
    else if(Buffer >= Bufend-1)
      return "Parsing buffer too short.";
    *(Buffer++) = 0;

    if(*url == ':') ++url;

    args->password = Buffer;
    while(*url && *url != '@' && *url != ';' && Buffer != Bufend)
      *(Buffer++) = *(url++);
    if(Buffer == args->password)
      return "Password cannot be empty.";
    else if(Buffer >= Bufend-1)
      return "Parsing buffer too short.";
    *(Buffer++) = 0;
  }

  if(*url == '@') /* server */
  {
    ++url;
    if(*url != '@' && *url != ':')
    {
      args->server = Buffer;
      while(*url && *url != '@' && *url != ':' && *url != ';' && Buffer != Bufend)
        *(Buffer++) = *(url++);
      if(Buffer == args->server)
        return "Servername cannot be empty.";
      else if(Buffer >= Bufend-1)
        return "Parsing buffer too short.";
      *(Buffer++) = 0;
    }

    if(*url == ':')
    {
      ++url;
      args->port = Buffer;
      while(*url && *url != '@' && *url != ';' && Buffer != Bufend)
        *(Buffer++) = *(url++);
      if(Buffer == args->port)
        return "Port cannot be empty.";
      else if(Buffer >= Bufend-1)
        return "Parsing buffer too short.";
      *(Buffer++) = 0;
    }

    if(*url == '@') /* proxy */
    {
      ++url;
      args->proxyhost = Buffer;
      while(*url && *url != ':' && *url != ';' && Buffer != Bufend)
        *(Buffer++) = *(url++);
      if(Buffer == args->proxyhost)
        return "Proxy servername cannot be empty.";
      else if(Buffer >= Bufend-1)
        return "Parsing buffer too short.";
      *(Buffer++) = 0;

      if(*url == ':')
      {
        ++url;
        args->proxyport = Buffer;
        while(*url && *url != ';' && Buffer != Bufend)
          *(Buffer++) = *(url++);
        if(Buffer == args->proxyport)
          return "Proxy port cannot be empty.";
        else if(Buffer >= Bufend-1)
          return "Parsing buffer too short.";
        *(Buffer++) = 0;
      }
    }
  }
  if(*url == ';') /* NMEA */
  {
    args->nmea = ++url;
    while(*url)
      ++url;
  }

  return *url ? "Garbage at end of server string." : 0;
}

static int getargs(int argc, char **argv, struct Args *args)
{
  int res = 1;
  int getoptr;
  int help = 0;
  char *t;

  args->server = "www.euref-ip.net";
  args->port = "2101";
  args->timeout = 60;
  args->user = "";
  args->password = "";
  args->data = 0;
  args->headerfile = 0;
  args->gpsephemeris = 0;
  args->glonassephemeris = 0;
  args->rinex3 = 0;
  args->nmea = 0;
  args->proxyhost = 0;
  args->proxyport = "2101";
  args->mode = AUTO;
  help = 0;

  do
  {

#ifdef NO_LONG_OPTS
    switch((getoptr = getopt(argc, argv, ARGOPT)))
#else
    switch((getoptr = getopt_long(argc, argv, ARGOPT, opts, 0)))
#endif
    {
    case 's': args->server = optarg; break;
    case 'u': args->user = optarg; break;
    case 'p': args->password = optarg; break;
    case 'd': args->data = optarg; break;
    case 'f': args->headerfile = optarg; break;
    case 'E': args->gpsephemeris = optarg; break;
    case 'G': args->glonassephemeris = optarg; break;
    case 'r': args->port = optarg; break;
    case '3': args->rinex3 = 1; break;
    case 'S': args->proxyhost = optarg; break;
    case 'n': args->nmea = optarg; break;
    case 'R': args->proxyport = optarg; break;
    case 'h': help=1; break;
    case 'M':
      args->mode = 0;
      if (!strcmp(optarg,"n") || !strcmp(optarg,"ntrip1"))
        args->mode = NTRIP1;
      else if(!strcmp(optarg,"h") || !strcmp(optarg,"http"))
        args->mode = HTTP;
      else if(!strcmp(optarg,"r") || !strcmp(optarg,"rtsp"))
        args->mode = RTSP;
      else if(!strcmp(optarg,"a") || !strcmp(optarg,"auto"))
        args->mode = AUTO;
      else args->mode = atoi(optarg);
      if((args->mode == 0) || (args->mode >= END))
      {
        fprintf(stderr, "Mode %s unknown\n", optarg);
        res = 0;
      }
      break;
    case 't':
      args->timeout = strtoul(optarg, &t, 10);
      if((t && *t) || args->timeout < 0)
        res = 0;
      break;

    case 1:
      {
        const char *err;
        if((err = geturl(optarg, args)))
        {
          RTCM3Error("%s\n\n", err);
          res = 0;
        }
      }
      break;
    case -1: break;
    }
  } while(getoptr != -1 || !res);

  datestr[0] = datestr[7];
  datestr[1] = datestr[8];
  datestr[2] = datestr[9];
  datestr[3] = datestr[10];
  datestr[5] = datestr[12];
  datestr[6] = datestr[13];
  datestr[8] = datestr[15];
  datestr[9] = datestr[16];
  datestr[4] = datestr[7] = '-';
  datestr[10] = 0;

  if(args->gpsephemeris && args->glonassephemeris && args->rinex3)
  {
    RTCM3Error("RINEX3 produces a combined ephemeris file, but 2 files were specified.\n"
    "Please specify only one navigation file.\n");
    res = 0;
  }
  else if(!res || help)
  {
    RTCM3Error("Version %s (%s) GPL" COMPILEDATE
    "\nUsage: %s -s server -u user ...\n"
    " -d " LONG_OPT("--data             ") "the requested data set\n"
    " -f " LONG_OPT("--headerfile       ") "file for RINEX header information\n"
    " -s " LONG_OPT("--server           ") "the server name or address\n"
    " -p " LONG_OPT("--password         ") "the login password\n"
    " -r " LONG_OPT("--port             ") "the server port number (default 2101)\n"
    " -t " LONG_OPT("--timeout          ") "timeout in seconds (default 60)\n"
    " -u " LONG_OPT("--user             ") "the user name\n"
    " -E " LONG_OPT("--gpsephemeris     ") "output file for GPS ephemeris data\n"
    " -G " LONG_OPT("--glonassephemeris ") "output file for GLONASS ephemeris data\n"
    " -3 " LONG_OPT("--rinex3           ") "output RINEX type 3 data\n"
    " -S " LONG_OPT("--proxyhost        ") "proxy name or address\n"
    " -R " LONG_OPT("--proxyport        ") "proxy port, optional (default 2101)\n"
    " -n " LONG_OPT("--nmea             ") "NMEA string for sending to server\n"
    " -M " LONG_OPT("--mode             ") "mode for data request\n"
    "     Valid modes are:\n"
    "     1, h, http     NTRIP Version 2.0 Caster in TCP/IP mode\n"
    "     2, r, rtsp     NTRIP Version 2.0 Caster in RTSP/RTP mode\n"
    "     3, n, ntrip1   NTRIP Version 1.0 Caster\n"
    "     4, a, auto     automatic detection (default)\n"
    "or using an URL:\n%s ntrip:data[/user[:password]][@[server][:port][@proxyhost[:proxyport]]][;nmea]\n"
    , revisionstr, datestr, argv[0], argv[0]);
    exit(1);
  }
  return res;
}

/* let the output complete a block if necessary */
static void signalhandler(int sig)
{
  if(!stop)
  {
    RTCM3Error("Stop signal number %d received. "
    "Trying to terminate gentle.\n", sig);
    stop = 1;
    alarm(1);
  }
}

#ifndef WINDOWSVERSION
static void WaitMicro(int mic)
{
  struct timeval tv;
  tv.tv_sec = mic/1000000;
  tv.tv_usec = mic%1000000;
#ifdef DEBUG
  fprintf(stderr, "Waiting %d micro seconds\n", mic);
#endif
  select(0, 0, 0, 0, &tv);
}
#else /* WINDOWSVERSION */
void WaitMicro(int mic)
{
   Sleep(mic/1000);
}
#endif /* WINDOWSVERSION */

#define ALARMTIME   (2*60)

/* for some reason we had to abort hard (maybe waiting for data */
#ifdef __GNUC__
static __attribute__ ((noreturn)) void signalhandler_alarm(
int sig __attribute__((__unused__)))
#else /* __GNUC__ */
static void signalhandler_alarm(int sig)
#endif /* __GNUC__ */
{
  RTCM3Error("Programm forcefully terminated.\n");
  exit(1);
}

int main(int argc, char **argv)
{
  struct Args args;
  struct RTCM3ParserData Parser;

  setbuf(stdout, 0);
  setbuf(stdin, 0);
  setbuf(stderr, 0);

  fixrevision();

  signal(SIGINT, signalhandler);
  signal(SIGALRM,signalhandler_alarm);
  signal(SIGQUIT,signalhandler);
  signal(SIGTERM,signalhandler);
  signal(SIGPIPE,signalhandler);
  memset(&Parser, 0, sizeof(Parser));
  {
    time_t tim;
    tim = time(0) - ((10*365+2+5)*24*60*60+LEAPSECONDS);
    Parser.GPSWeek = tim/(7*24*60*60);
    Parser.GPSTOW = tim%(7*24*60*60);
  }

  if(getargs(argc, argv, &args))
  {
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct sockaddr_in their_addr; /* connector's address information */
    struct hostent *he;
    struct servent *se;
    const char *server, *port, *proxyserver = 0;
    char proxyport[6];
    char *b;
    long i;
    struct timeval tv;

    alarm(ALARMTIME);

    Parser.headerfile = args.headerfile;
    Parser.glonassephemeris = args.glonassephemeris;
    Parser.gpsephemeris = args.gpsephemeris;
    Parser.rinex3 = args.rinex3;

    if(args.proxyhost)
    {
      int p;
      if((i = strtol(args.port, &b, 10)) && (!b || !*b))
        p = i;
      else if(!(se = getservbyname(args.port, 0)))
      {
        RTCM3Error("Can't resolve port %s.", args.port);
        exit(1);
      }
      else
      {
        p = ntohs(se->s_port);
      }
      snprintf(proxyport, sizeof(proxyport), "%d", p);
      port = args.proxyport;
      proxyserver = args.server;
      server = args.proxyhost;
    }
    else
    {
      server = args.server;
      port = args.port;
    }

    memset(&their_addr, 0, sizeof(struct sockaddr_in));
    if((i = strtol(port, &b, 10)) && (!b || !*b))
      their_addr.sin_port = htons(i);
    else if(!(se = getservbyname(port, 0)))
    {
      RTCM3Error("Can't resolve port %s.", port);
      exit(1);
    }
    else
    {
      their_addr.sin_port = se->s_port;
    }
    if(!(he=gethostbyname(server)))
    {
      RTCM3Error("Server name lookup failed for '%s'.\n", server);
      exit(1);
    }
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror("socket");
      exit(1);
    }

    tv.tv_sec  = args.timeout;
    tv.tv_usec = 0;
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval) ) == -1)
    {
      RTCM3Error("Function setsockopt: %s\n", strerror(errno));
      exit(1);
    }

    their_addr.sin_family = AF_INET;
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);

    if(args.data && args.mode == RTSP)
    {
      struct sockaddr_in local;
      int sockudp, localport;
      int cseq = 1;
      socklen_t len;

      if((sockudp = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
      {
        perror("socket");
        exit(1);
      }
      /* fill structure with local address information for UDP */
      memset(&local, 0, sizeof(local));
      local.sin_family = AF_INET;
      local.sin_port = htons(0);
      local.sin_addr.s_addr = htonl(INADDR_ANY);
      len = sizeof(local);
      /* bind() in order to get a random RTP client_port */
      if((bind(sockudp, (struct sockaddr *)&local, len)) < 0)
      {
        perror("bind");
        exit(1);
      }
      if((getsockname(sockudp, (struct sockaddr*)&local, &len)) != -1)
      {
        localport = ntohs(local.sin_port);
      }
      else
      {
        perror("local access failed");
        exit(1);
      }
      if(connect(sockfd, (struct sockaddr *)&their_addr,
      sizeof(struct sockaddr)) == -1)
      {
        perror("connect");
        exit(1);
      }
      i=snprintf(buf, MAXDATASIZE-40, /* leave some space for login */
      "SETUP rtsp://%s%s%s/%s RTSP/1.0\r\n"
      "CSeq: %d\r\n"
      "Ntrip-Version: Ntrip/2.0\r\n"
      "Ntrip-Component: Ntripclient\r\n"
      "User-Agent: %s/%s\r\n"
      "Transport: RTP/GNSS;unicast;client_port=%u\r\n"
      "Authorization: Basic ",
      args.server, proxyserver ? ":" : "", proxyserver ? args.port : "",
      args.data, cseq++, AGENTSTRING, revisionstr, localport);
      if(i > MAXDATASIZE-40 || i < 0) /* second check for old glibc */
      {
        RTCM3Error("Requested data too long\n");
        exit(1);
      }
      i += encode(buf+i, MAXDATASIZE-i-4, args.user, args.password);
      if(i > MAXDATASIZE-4)
      {
        RTCM3Error("Username and/or password too long\n");
        exit(1);
      }
      buf[i++] = '\r';
      buf[i++] = '\n';
      buf[i++] = '\r';
      buf[i++] = '\n';
      if(args.nmea)
      {
        int j = snprintf(buf+i, MAXDATASIZE-i, "%s\r\n", args.nmea);
        if(j >= 0 && j < MAXDATASIZE-i)
          i += j;
        else
        {
          RTCM3Error("NMEA string too long\n");
          exit(1);
        }
      }
      if(send(sockfd, buf, (size_t)i, 0) != i)
      {
        perror("send");
        exit(1);
      }
      if((numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) != -1)
      {
        if(numbytes >= 17 && !strncmp(buf, "RTSP/1.0 200 OK\r\n", 17))
        {
          int serverport = 0, session = 0;
          const char *portcheck = "server_port=";
          const char *sessioncheck = "session: ";
          int l = strlen(portcheck)-1;
          int j=0;
          for(i = 0; j != l && i < numbytes-l; ++i)
          {
            for(j = 0; j < l && tolower(buf[i+j]) == portcheck[j]; ++j)
              ;
          }
          if(i == numbytes-l)
          {
            RTCM3Error("No server port number found\n");
            exit(1);
          }
          else
          {
            i+=l;
            while(i < numbytes && buf[i] >= '0' && buf[i] <= '9')
              serverport = serverport * 10 + buf[i++]-'0';
            if(buf[i] != '\r' && buf[i] != ';')
            {
              RTCM3Error("Could not extract server port\n");
              exit(1);
            }
          }
          l = strlen(sessioncheck)-1;
          j=0;
          for(i = 0; j != l && i < numbytes-l; ++i)
          {
            for(j = 0; j < l && tolower(buf[i+j]) == sessioncheck[j]; ++j)
              ;
          }
          if(i == numbytes-l)
          {
            RTCM3Error("No session number found\n");
            exit(1);
          }
          else
          {
            i+=l;
            while(i < numbytes && buf[i] >= '0' && buf[i] <= '9')
              session = session * 10 + buf[i++]-'0';
            if(buf[i] != '\r')
            {
              RTCM3Error("Could not extract session number\n");
              exit(1);
            }
          }

          i = snprintf(buf, MAXDATASIZE,
          "PLAY rtsp://%s%s%s/%s RTSP/1.0\r\n"
          "CSeq: %d\r\n"
          "Session: %d\r\n"
          "\r\n",
          args.server, proxyserver ? ":" : "", proxyserver ? args.port : "",
          args.data, cseq++, session);

          if(i > MAXDATASIZE || i < 0) /* second check for old glibc */
          {
            RTCM3Error("Requested data too long\n");
            exit(1);
          }
          if(send(sockfd, buf, (size_t)i, 0) != i)
          {
            perror("send");
            exit(1);
          }
          if((numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) != -1)
          {
            if(numbytes >= 17 && !strncmp(buf, "RTSP/1.0 200 OK\r\n", 17))
            {
              struct sockaddr_in addrRTP;
              /* fill structure with caster address information for UDP */
              memset(&addrRTP, 0, sizeof(addrRTP));
              addrRTP.sin_family = AF_INET;
              addrRTP.sin_port   = htons(serverport);
              their_addr.sin_addr = *((struct in_addr *)he->h_addr);
              len = sizeof(addrRTP);
              int ts = 0;
              int sn = 0;
              int ssrc = 0;
              int init = 0;
              int u, v, w;
              while(!stop && (i = recvfrom(sockudp, buf, 1526, 0,
              (struct sockaddr*) &addrRTP, &len)) > 0)
              {
                alarm(ALARMTIME);
                if(i >= 12+1 && (unsigned char)buf[0] == (2 << 6) && buf[1] == 0x60)
                {
                  u= ((unsigned char)buf[2]<<8)+(unsigned char)buf[3];
                  v = ((unsigned char)buf[4]<<24)+((unsigned char)buf[5]<<16)
                  +((unsigned char)buf[6]<<8)+(unsigned char)buf[7];
                  w = ((unsigned char)buf[8]<<24)+((unsigned char)buf[9]<<16)
                  +((unsigned char)buf[10]<<8)+(unsigned char)buf[11];

                  if(init)
                  {
                    int z;
                    if(u < -30000 && sn > 30000) sn -= 0xFFFF;
                    if(ssrc != w || ts > v)
                    {
                      RTCM3Error("Illegal UDP data received.\n");
                      exit(1);
                    }
                    if(u > sn) /* don't show out-of-order packets */
                    for(z = 12; z < i && !stop; ++z)
                      HandleByte(&Parser, (unsigned int) buf[z]);
                  }
                  sn = u; ts = v; ssrc = w; init = 1;
                }
                else
                {
                  RTCM3Error("Illegal UDP header.\n");
                  exit(1);
                }
              }
            }
            i = snprintf(buf, MAXDATASIZE,
            "TEARDOWN rtsp://%s%s%s/%s RTSP/1.0\r\n"
            "CSeq: %d\r\n"
            "Session: %d\r\n"
            "\r\n",
            args.server, proxyserver ? ":" : "", proxyserver ? args.port : "",
            args.data, cseq++, session);

            if(i > MAXDATASIZE || i < 0) /* second check for old glibc */
            {
              RTCM3Error("Requested data too long\n");
              exit(1);
            }
            if(send(sockfd, buf, (size_t)i, 0) != i)
            {
              perror("send");
              exit(1);
            }
          }
          else
          {
            RTCM3Error("Could not start data stream.\n");
            exit(1);
          }
        }
        else
        {
          RTCM3Error("Could not setup initial control connection.\n");
          exit(1);
        }
      }
      else
      {
        perror("recv");
        exit(1);
      }
    }
    else
    {
      if(connect(sockfd, (struct sockaddr *)&their_addr,
      sizeof(struct sockaddr)) == -1)
      {
        perror("connect");
        exit(1);
      }
      if(!args.data)
      {
        i = snprintf(buf, MAXDATASIZE,
        "GET %s%s%s%s/ HTTP/1.0\r\n"
        "Host: %s\r\n%s"
        "User-Agent: %s/%s\r\n"
        "Connection: close\r\n"
        "\r\n"
        , proxyserver ? "http://" : "", proxyserver ? proxyserver : "",
        proxyserver ? ":" : "", proxyserver ? proxyport : "",
        args.server, args.mode == NTRIP1 ? "" : "Ntrip-Version: Ntrip/2.0\r\n",
        AGENTSTRING, revisionstr);
      }
      else
      {
        i=snprintf(buf, MAXDATASIZE-40, /* leave some space for login */
        "GET %s%s%s%s/%s HTTP/1.0\r\n"
        "Host: %s\r\n%s"
        "User-Agent: %s/%s\r\n"
        "Connection: close\r\n"
        "Authorization: Basic "
        , proxyserver ? "http://" : "", proxyserver ? proxyserver : "",
        proxyserver ? ":" : "", proxyserver ? proxyport : "",
        args.data, args.server,
        args.mode == NTRIP1 ? "" : "Ntrip-Version: Ntrip/2.0\r\n",
        AGENTSTRING, revisionstr);
        if(i > MAXDATASIZE-40 || i < 0) /* second check for old glibc */
        {
          RTCM3Error("Requested data too long\n");
          exit(1);
        }
        i += encode(buf+i, MAXDATASIZE-i-4, args.user, args.password);
        if(i > MAXDATASIZE-4)
        {
          RTCM3Error("Username and/or password too long\n");
          exit(1);
        }
        buf[i++] = '\r';
        buf[i++] = '\n';
        buf[i++] = '\r';
        buf[i++] = '\n';
        if(args.nmea)
        {
          int j = snprintf(buf+i, MAXDATASIZE-i, "%s\r\n", args.nmea);
          if(j >= 0 && j < MAXDATASIZE-i)
            i += j;
          else
          {
            RTCM3Error("NMEA string too long\n");
            exit(1);
          }
        }
      }
      if(send(sockfd, buf, (size_t)i, 0) != i)
      {
        perror("send");
        exit(1);
      }
      if(args.data)
      {
        int k = 0;
        int chunkymode = 0;
        int starttime = time(0);
        int lastout = starttime;
        int totalbytes = 0;
        int chunksize = 0;

        while(!stop && (numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) != -1)
        {
          if(numbytes > 0)
            alarm(ALARMTIME);
          else
          {
            WaitMicro(100);
            continue;
          }
          if(!k)
          {
            if(numbytes > 17 && (!strncmp(buf, "HTTP/1.1 200 OK\r\n", 17)
            || !strncmp(buf, "HTTP/1.0 200 OK\r\n", 17)))
            {
              const char *datacheck = "Content-Type: gnss/data\r\n";
              const char *chunkycheck = "Transfer-Encoding: chunked\r\n";
              int l = strlen(datacheck)-1;
              int j=0;
              for(i = 0; j != l && i < numbytes-l; ++i)
              {
                for(j = 0; j < l && buf[i+j] == datacheck[j]; ++j)
                  ;
              }
              if(i == numbytes-l)
              {
                RTCM3Error("No 'Content-Type: gnss/data' found\n");
                exit(1);
              }
              l = strlen(chunkycheck)-1;
              j=0;
              for(i = 0; j != l && i < numbytes-l; ++i)
              {
                for(j = 0; j < l && buf[i+j] == chunkycheck[j]; ++j)
                  ;
              }
              if(i < numbytes-l)
                chunkymode = 1;
            }
            else if(numbytes < 12 || strncmp("ICY 200 OK\r\n", buf, 12))
            {
              RTCM3Error("Could not get the requested data: ");
              for(k = 0; k < numbytes && buf[k] != '\n' && buf[k] != '\r'; ++k)
              {
                RTCM3Error("%c", isprint(buf[k]) ? buf[k] : '.');
              }
              RTCM3Error("\n");
              exit(1);
            }
            else if(args.mode != NTRIP1)
            {
              if(args.mode != AUTO)
              {
                RTCM3Error("NTRIP version 2 HTTP connection failed%s.\n",
                args.mode == AUTO ? ", falling back to NTRIP1" : "");
              }
              if(args.mode == HTTP)
                exit(1);
            }
            ++k;
          }
          else
          {
            if(chunkymode)
            {
              int stop = 0;
              int pos = 0;
              while(!stop && pos < numbytes)
              {
                switch(chunkymode)
                {
                case 1: /* reading number starts */
                  chunksize = 0;
                  ++chunkymode; /* no break */
                case 2: /* during reading number */
                  i = buf[pos++];
                  if(i >= '0' && i <= '9') chunksize = chunksize*16+i-'0';
                  else if(i >= 'a' && i <= 'f') chunksize = chunksize*16+i-'a'+10;
                  else if(i >= 'A' && i <= 'F') chunksize = chunksize*16+i-'A'+10;
                  else if(i == '\r') ++chunkymode;
                  else if(i == ';') chunkymode = 5;
                  else stop = 1;
                  break;
                case 3: /* scanning for return */
                  if(buf[pos++] == '\n') chunkymode = chunksize ? 4 : 1;
                  else stop = 1;
                  break;
                case 4: /* output data */
                  i = numbytes-pos;
                  if(i > chunksize) i = chunksize;
                  {
                    int z;
                    for(z = 0; z < i && !stop; ++z)
                      HandleByte(&Parser, (unsigned int) buf[pos+z]);
                  }
                  totalbytes += i;
                  chunksize -= i;
                  pos += i;
                  if(!chunksize)
                    chunkymode = 1;
                  break;
                case 5:
                  if(i == '\r') chunkymode = 3;
                  break;
                }
              }
              if(stop)
              {
                RTCM3Error("Error in chunky transfer encoding\n");
                break;
              }
            }
            else
            {
              totalbytes += numbytes;
              {
                int z;
                for(z = 0; z < numbytes && !stop; ++z)
                  HandleByte(&Parser, (unsigned int) buf[z]);
              }
            }
            if(totalbytes < 0) /* overflow */
            {
              totalbytes = 0;
              starttime = time(0);
              lastout = starttime;
            }
          }
        }
      }
      else
      {
        while(!stop && (numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) > 0)
        {
          alarm(ALARMTIME);
          fwrite(buf, (size_t)numbytes, 1, stdout);
        }
      }
      close(sockfd);
    }
  }
  return 0;
}
#endif /* NO_RTCM3_MAIN */
