/*
  Converter for RTCM3 data to RINEX.
  $Id: rtcm3torinex.c,v 1.2 2006/01/13 08:25:10 stoecker Exp $
  Copyright (C) 2005-2006 by Dirk Stoecker <stoecker@euronav.de>

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
#include <getopt.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* The string, which is send as agent in HTTP request */
#define AGENTSTRING "NTRIP NtripRTCM3ToRINEX"

#define MAXDATASIZE 1000 /* max number of bytes we can get at once */

/* CVS revision and version */
static char revisionstr[] = "$Revision: 1.2 $";
static char datestr[]     = "$Date: 2006/01/13 08:25:10 $";
static int stop = 0;

/* unimportant, only for approx. time needed */
#define LEAPSECONDS 14

struct converttimeinfo {
  int second;    /* seconds of GPS time [0..59] */
  int minute;    /* minutes of GPS time [0..59] */
  int hour;      /* hour of GPS time [0..24] */
  int day;       /* day of GPS time [1..28..30(31)*/
  int month;     /* month of GPS time [1..12]*/
  int year;      /* year of GPS time [1980..] */
};

#define PRN_GPS_START             1
#define PRN_GPS_END               32
#define PRN_GLONASS_START         38
#define PRN_GLONASS_END           61

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
  int    size;
  int    lastlockl1[64];
  int    lastlockl2[64];
  int    datapos[RINEXENTRY_NUMBER];
  int    dataflag[RINEXENTRY_NUMBER];
  int    numdatatypes;
  int    validwarning;
  int    init;
  const char * headerfile;
};

struct Args
{
  const char *server;
  int         port;
  const char *user;
  const char *password;
  const char *data;
  const char *headerfile;
};

/* option parsing */
#ifdef NO_LONG_OPTS
#define LONG_OPT(a)
#else
#define LONG_OPT(a) a
static struct option opts[] = {
{ "data",       required_argument, 0, 'd'},
{ "server",     required_argument, 0, 's'},
{ "password",   required_argument, 0, 'p'},
{ "port",       required_argument, 0, 'r'},
{ "header",     required_argument, 0, 'f'},
{ "user",       required_argument, 0, 'u'},
{ "help",       no_argument,       0, 'h'},
{0,0,0,0}};
#endif
#define ARGOPT "d:hp:r:s:u:f:"

static int getargs(int argc, char **argv, struct Args *args)
{
  int res = 1;
  int getoptr;
  int help = 0;
  char *t;

  args->server = "www.euref-ip.net";
  args->port = 80;
  args->user = "";
  args->password = "";
  args->data = 0;
  args->headerfile = 0;
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
    case 'h': help=1; break;
    case 'r': 
      args->port = strtoul(optarg, &t, 10);
      if((t && *t) || args->port < 1 || args->port > 65535)
        res = 0;
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

  if(!res || help)
  {
    fprintf(stderr, "Version %s (%s) GPL\nUsage: %s -s server -u user ...\n"
    " -d " LONG_OPT("--data       ") "the requested data set\n"
    " -f " LONG_OPT("--headerfile ") "file for RINEX header information\n"
    " -s " LONG_OPT("--server     ") "the server name or address\n"
    " -p " LONG_OPT("--password   ") "the login password\n"
    " -r " LONG_OPT("--port       ") "the server port number (default 80)\n"
    " -u " LONG_OPT("--user       ") "the user name\n"
    , revisionstr, datestr, argv[0]);
    exit(1);
  }
  return res;
}

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
    memmove(handle->Message, m, handle->MessageSize-i);
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
#define GETBITSSIGN(b, a) \
{ \
  LOADBITS(a) \
  b = ((int64_t)(bitfield<<(64-numbits)))>>(64-(a)); \
  numbits -= (a); \
}

#define SKIPBITS(b) { LOADBITS(b) numbits -= (b); }

static int RTCM3Parser(struct RTCM3ParserData *handle)
{
  int ret=0;

  while(!ret && GetMessage(handle))
  {
    /* using 64 bit integer types, as it is much easier than handling
    the long datatypes in 32 bit */
    uint64_t numbits = 0, bitfield = 0;
    int size = handle->size, type;
    unsigned char *data = handle->Message+3;

    GETBITS(type,12)
    switch(type)
    {
    case 1004:
      if(handle->GPSWeek)
      {
        int lastlockl1[64];
        int lastlockl2[64];
        struct gnssdata *gnss;
        int i, num, wasamb=0;

        for(i = 0; i < 64; ++i)
          lastlockl1[i] = lastlockl2[i] = 0;

        gnss = &handle->Data;
        memset(gnss, 0, sizeof(*gnss));

        SKIPBITS(12) /* id */
        GETBITS(i,30)
        if(i/1000 < (int)handle->GPSTOW - 86400)
          ++handle->GPSWeek;
        handle->GPSTOW = i/1000;
        gnss->timeofweek = i;
        gnss->week = handle->GPSWeek;

        SKIPBITS(1) /* sync */
        GETBITS(i,5)
        gnss->numsats = i;
        SKIPBITS(4) /* smind, smint */

        for(num = 0; num < gnss->numsats; ++num)
        {
          int sv, code, l1range, c,l,s,ce,le,se,amb;

          GETBITS(sv, 6);
          gnss->satellites[num] = (sv < 40 ? sv : sv+80);
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
          if(l1range != 0x80000)
          {
            gnss->dataflags[num] |= c;
            gnss->measdata[num][ce] = l1range*0.02;
          }
          GETBITSSIGN(i, 20);
          if(i != 0x80000)
          {
            gnss->dataflags[num] |= l;
            gnss->measdata[num][le] = l1range*0.02+i*0.0005;
          }
          GETBITS(i, 7);
          lastlockl1[sv] = i;
          if(handle->lastlockl1[sv] > i)
            gnss->dataflags[num] |= GNSSDF_LOCKLOSSL1;
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
          /* L2 */
          GETBITS(code,2);
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
          GETBITSSIGN(i,14);
          if(i != 0x2000)
          {
            gnss->dataflags[num] |= c;
            gnss->measdata[num][ce] = l1range*0.02+i*0.02
            +amb*299792.458;
          }
          GETBITSSIGN(i,20);
          if(i != 0x80000)
          {
            gnss->dataflags[num] |= l;
            gnss->measdata[num][le] = l1range*0.02+i*0.0005
            +amb*299792.458;
          }
          GETBITS(i,7);
          lastlockl2[sv] = i;
          if(handle->lastlockl2[sv] > i)
            gnss->dataflags[num] |= GNSSDF_LOCKLOSSL2;
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
        for(i = 0; i < 64; ++i)
        {
          handle->lastlockl1[i] = lastlockl1[i];
          handle->lastlockl2[i] = lastlockl2[i];
        }
        if(wasamb) /* not RINEX compatible without */
          ret = 1;
        else
          ret = 2;
      }
      break;
    }
  }
  return ret;
}

static int longyear(int year, int month)
{
  if(!(year % 4) && (!(year % 400) || (year % 100)))
  {
    if(!month || month == 2)
      return 1;
  }
  return 0;
}

static void converttime(struct converttimeinfo *c, int week, int tow)
{
  /* static variables */
  static const int months[13] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

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

struct Header
{
  const char *version;
  const char *pgm;
  const char *marker;
  const char *observer;
  const char *receiver;
  const char *antenna;
  const char *position;
  const char *antennaposition;
  const char *wavelength;
  const char *typesofobs; /* should not be modified outside */
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

static void HandleHeader(struct RTCM3ParserData *Parser)
{
  struct HeaderData hdata;
  char thebuffer[MAXHEADERBUFFERSIZE];
  char *buffer = thebuffer;
  int buffersize = sizeof(thebuffer);
  int i;

  hdata.data.named.version =
  "     2.11           OBSERVATION DATA    M (Mixed)"
  "           RINEX VERSION / TYPE";

  {
    const char *str;
    time_t t;
    struct tm * t2;

    str = getenv("USER");
    if(!str) str = "";
    t = time(&t);
    t2 = gmtime(&t);
    hdata.data.named.pgm = buffer;
    i = 1+snprintf(buffer, buffersize,
    "RTCM3TORINEX %-7.7s%-20.20s%04d-%02d-%02d %02d:%02d    "
    "PGM / RUN BY / DATE",
    revisionstr, str, 1900+t2->tm_year, t2->tm_mon+1, t2->tm_mday, t2->tm_hour,
    t2->tm_min);
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
  
  hdata.data.named.wavelength =
  "     1     1                                                "
  "WAVELENGTH FACT L1/2";

  {
#define CHECKFLAGS(a, b) \
    if(flags & GNSSDF_##a##DATA \
    && !data[RINEXENTRY_##b##DATA]) \
    { \
      Parser->dataflag[Parser->numdatatypes] = GNSSDF_##a##DATA; \
      Parser->datapos[Parser->numdatatypes++] = data[RINEXENTRY_##b##DATA] \
      = GNSSENTRY_##a##DATA; \
      snprintf(tbuffer+tbufferpos, sizeof(tbuffer)-tbufferpos, "    "#b); \
      tbufferpos += 6; \
    }

    int flags = 0;
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

    hdata.data.named.typesofobs = buffer;
    i = 1+snprintf(buffer, buffersize,
    "%6i%-54.54s# / TYPES OF OBSERV", Parser->numdatatypes, tbuffer);
    if(Parser->numdatatypes>9)
    {
      i += snprintf(buffer+i-1, buffersize,
      "\n      %-54.54s# / TYPES OF OBSERV", tbuffer+9*6);
    }
    buffer += i; buffersize -= i;
  }

  {
    struct converttimeinfo cti;
    converttime(&cti, Parser->Data.week,
    floor(Parser->Data.timeofweek/1000.0));
    hdata.data.named.timeoffirstobs = buffer;
      i = 1+snprintf(buffer, buffersize,
    "  %4d    %2d    %2d    %2d    %2d   %10.7f     GPS         "
    "TIME OF FIRST OBS", cti.year%100, cti.month, cti.day, cti.hour,
    cti.minute, cti.second + fmod(Parser->Data.timeofweek/1000.0,1.0));

    buffer += i; buffersize -= i;
  }

  hdata.numheaders = 11;

  if(Parser->headerfile)
  {
    FILE *fh;
    if((fh = fopen(Parser->headerfile, "r")))
    {
      int siz;
      char *lastblockstart;
      if((siz = fread(buffer, 1, buffersize-1, fh)) > 0)
      {
        buffer[siz] = '\n';
        if(siz == buffersize)
        {
          fprintf(stderr, "Header file is too large. Only %d bytes read.",
          siz);
        }
        /* scan the file line by line and enter the entries in the list */
        /* warn for "# / TYPES OF OBSERV" and "TIME OF FIRST OBS" */
        /* overwrites entries, except for comments */
        lastblockstart = buffer;
        for(i = 0; i < siz; ++i)
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
              fprintf(stderr, "Short Header line '%s' ignored.\n", lastblockstart);
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
                  fprintf(stderr, "Overwriting header '%s' is dangerous.\n",
                  lastblockstart+60);
                }
              }
              if(pos >= MAXHEADERLINES)
              {
                fprintf(stderr,
                "Maximum number of header lines of %d reached.\n",
                MAXHEADERLINES);
              }
              else if(!strcmp("END OF HEADER", lastblockstart+60))
              {
                fprintf(stderr, "End of header ignored.\n");
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
        fprintf(stderr, "Could not read data from headerfile '%s'.\n",
        Parser->headerfile);
      }
      fclose(fh);
    }
    else
    {
      fprintf(stderr, "Could not open header datafile '%s'.\n",
      Parser->headerfile);
    }
  }

  for(i = 0; i < hdata.numheaders; ++i)
    printf("%s\n", hdata.data.unnamed[i]);
  printf("                                                            "
  "END OF HEADER\n");
}

/* let the output complete a block if necessary */
static void signalhandler(int sig)
{
  if(!stop)
  {
    fprintf(stderr, "Stop signal number %d received. "
    "Trying to terminate gentle.\n", sig);
    stop = 1;
    alarm(1);
  }
}

/* for some reason we had to abort hard (maybe waiting for data */
#ifdef __GNUC__
static __attribute__ ((noreturn)) void signalhandler_alarm(
int sig __attribute__((__unused__)))
#else /* __GNUC__ */
static void signalhandler_alarm(int sig)
#endif /* __GNUC__ */
{
  fprintf(stderr, "Programm forcefully terminated.\n");
  exit(1);
}

static void HandleByte(struct RTCM3ParserData *Parser, unsigned int byte)
{
  Parser->Message[Parser->MessageSize++] = byte;
  if(Parser->MessageSize >= Parser->NeedBytes)
  {
    int r;
    while((r = RTCM3Parser(Parser)))
    {
      int i, j, o;
      struct converttimeinfo cti;

      if(!Parser->init)
      {
        HandleHeader(Parser);
        Parser->init = 1;
      }
      if(r == 2 && !Parser->validwarning)
      {
        printf("No valid RINEX! All values are modulo 299792.458!"
        "           COMMENT\n");
        Parser->validwarning = 1;
      }

      converttime(&cti, Parser->Data.week,
      floor(Parser->Data.timeofweek/1000.0));
      printf(" %02d %2d %2d %2d %2d %10.7f  0%3d",
      cti.year%100, cti.month, cti.day, cti.hour, cti.minute, cti.second
      + fmod(Parser->Data.timeofweek/1000.0,1.0), Parser->Data.numsats);
      for(i = 0; i < 12 && i < Parser->Data.numsats; ++i)
      {
        if(Parser->Data.satellites[i] <= PRN_GPS_END)
          printf("G%02d", Parser->Data.satellites[i]);
        else if(Parser->Data.satellites[i] >= PRN_GLONASS_START
        && Parser->Data.satellites[i] <= PRN_GLONASS_END)
          printf("R%02d", Parser->Data.satellites[i] - (PRN_GLONASS_START-1));
        else
          printf("%3d", Parser->Data.satellites[i]);
      }
      printf("\n");
      o = 12;
      j = Parser->Data.numsats - 12;
      while(j > 0)
      {
        printf("                                ");
        for(i = o; i < o+12 && i < Parser->Data.numsats; ++i)
        {
          if(Parser->Data.satellites[i] <= PRN_GPS_END)
            printf("G%02d", Parser->Data.satellites[i]);
          else if(Parser->Data.satellites[i] >= PRN_GLONASS_START
          && Parser->Data.satellites[i] <= PRN_GLONASS_END)
            printf("R%02d", Parser->Data.satellites[i] - (PRN_GLONASS_START-1));
          else
            printf("%3d", Parser->Data.satellites[i]);
        }
        printf("\n");
        j -= 12;
        o += 12;
      }
      for(i = 0; i < Parser->Data.numsats; ++i)
      {
        for(j = 0; j < Parser->numdatatypes; ++j)
        {
          if(!(Parser->Data.dataflags[i] & Parser->dataflag[j])
          || isnan(Parser->Data.measdata[i][Parser->datapos[j]])
          || isinf(Parser->Data.measdata[i][Parser->datapos[j]]))
          { /* no or illegal data */
            printf("                ");
          }
          else
          {
            char lli = ' ';
            char snr = ' ';
            if(Parser->dataflag[j] & (GNSSDF_L1CDATA|GNSSDF_L1PDATA))
            {
              if(Parser->Data.dataflags[i] & GNSSDF_LOCKLOSSL1)
                lli = '1';
              snr = '0'+Parser->Data.snrL1[i];
            }
            if(Parser->dataflag[j] & (GNSSDF_L2CDATA|GNSSDF_L2PDATA))
            {
              if(Parser->Data.dataflags[i] & GNSSDF_LOCKLOSSL2)
                lli = '1';
              snr = '0'+Parser->Data.snrL2[i];
            }
            printf("%14.3f%c%c",
            Parser->Data.measdata[i][Parser->datapos[j]],lli,snr);
          }
          if(j%5 == 4 || j == Parser->numdatatypes-1)
            printf("\n");
        }
      }
    }
  }
}

int main(int argc, char **argv)
{
  struct Args args;
  struct RTCM3ParserData Parser;

  setbuf(stdout, 0);
  setbuf(stdin, 0);
  setbuf(stderr, 0);

  {
    char *a;
    int i=0;
    for(a = revisionstr+11; *a && *a != ' '; ++a)
      revisionstr[i++] = *a;
    revisionstr[i] = 0;
  }

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
    int i, sockfd, numbytes;  
    char buf[MAXDATASIZE];
    struct hostent *he;
    struct sockaddr_in their_addr; /* connector's address information */

    Parser.headerfile = args.headerfile;

    if(!(he=gethostbyname(args.server)))
    {
      perror("gethostbyname");
      exit(1);
    }
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror("socket");
      exit(1);
    }
    their_addr.sin_family = AF_INET;    /* host byte order */
    their_addr.sin_port = htons(args.port);  /* short, network byte order */
    their_addr.sin_addr = *((struct in_addr *)he->h_addr);
    memset(&(their_addr.sin_zero), '\0', 8);
    if(connect(sockfd, (struct sockaddr *)&their_addr,
    sizeof(struct sockaddr)) == -1)
    {
      perror("connect");
      exit(1);
    }

    if(!args.data)
    {
      i = snprintf(buf, MAXDATASIZE,
      "GET / HTTP/1.0\r\n"
      "User-Agent: %s/%s\r\n"
#ifdef UNUSED
      "Accept: */*\r\n"
      "Connection: close\r\n"
#endif
      "\r\n"
      , AGENTSTRING, revisionstr);
    }
    else
    {
      i=snprintf(buf, MAXDATASIZE-40, /* leave some space for login */
      "GET /%s HTTP/1.0\r\n"
      "User-Agent: %s/%s\r\n"
#ifdef UNUSED
      "Accept: */*\r\n"
      "Connection: close\r\n"
#endif
      "Authorization: Basic "
      , args.data, AGENTSTRING, revisionstr);
      if(i > MAXDATASIZE-40 && i < 0) /* second check for old glibc */
      {
        fprintf(stderr, "Requested data too long\n");
        exit(1);
      }
      i += encode(buf+i, MAXDATASIZE-i-5, args.user, args.password);
      if(i > MAXDATASIZE-5)
      {
        fprintf(stderr, "Username and/or password too long\n");
        exit(1);
      }
      snprintf(buf+i, 5, "\r\n\r\n");
      i += 5;
    }
    if(send(sockfd, buf, (size_t)i, 0) != i)
    {
      perror("send");
      exit(1);
    }
    if(args.data)
    {
      int k = 0;
      while(!stop && (numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) != -1)
      {
        if(!k)
        {
          if(numbytes < 12 || strncmp("ICY 200 OK\r\n", buf, 12))
          {
            fprintf(stderr, "Could not get the requested data: ");
            for(k = 0; k < numbytes && buf[k] != '\n' && buf[k] != '\r'; ++k)
            {
              fprintf(stderr, "%c", isprint(buf[k]) ? buf[k] : '.');
            }
            fprintf(stderr, "\n");
            exit(1);
          }
          ++k;
        }
        else
        {
          int z;
          for(z = 0; z < numbytes && !stop; ++z)
            HandleByte(&Parser, buf[z]);
        }
      }
    }
    else
    {
      while((numbytes=recv(sockfd, buf, MAXDATASIZE-1, 0)) > 0)
      {
        fwrite(buf, (size_t)numbytes, 1, stdout);
      }
    }

    close(sockfd);
  }
  return 0;
}
