/*
 * $Id: granules.c 740 2004-05-23 06:34:48Z skaar $
 *
 * Copyright (C) 1995-2004 Free Software Foundation, Inc.
 *
 * This file is part of cfng, a fork of GNU cfengine. Modifications,
 * additions and other differences herein is not releated to or
 * associated with the original project.
 *
 * This file is derived from GNU cfengine - written by Mark Burgess,
 * Dept of Computing and Engineering, Oslo College, Dept. of Theoretical
 * physics, University of Oslo.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA
 */


#include "../pub/getopt.h"
#include "cf.defs.h"
#include "cf.extern.h"
#include <math.h>

#include <db.h>

/* ----------------------------------------------------------------- */

char *
ConvTimeKey(char *str)
{
    int i;
    char buf1[10],buf2[10],buf3[10],buf4[10],buf5[10],buf[10],out[10];
    static char timekey[64];

    sscanf(str,"%s %s %s %s %s",buf1,buf2,buf3,buf4,buf5);

    timekey[0] = '\0';

    /* Day */

    sprintf(timekey,"%s:",buf1);

    /* Hours */

    sscanf(buf4,"%[^:]",buf);
    sprintf(out,"Hr%s",buf);
    strcat(timekey,out);

    /* Minutes */

    sscanf(buf4,"%*[^:]:%[^:]",buf);
    sprintf(out,"Min%s",buf);
    strcat(timekey,":");

    sscanf(buf,"%d",&i);

    switch ((i / 5)) {
    case 0:
        strcat(timekey,"Min00_05");
        break;
    case 1:
        strcat(timekey,"Min05_10");
        break;
    case 2:
        strcat(timekey,"Min10_15");
        break;
    case 3:
        strcat(timekey,"Min15_20");
        break;
    case 4:
        strcat(timekey,"Min20_25");
        break;
    case 5:
        strcat(timekey,"Min25_30");
        break;
    case 6:
        strcat(timekey,"Min30_35");
        break;
    case 7:
        strcat(timekey,"Min35_40");
        break;
    case 8:
        strcat(timekey,"Min40_45");
        break;
    case 9:
        strcat(timekey,"Min45_50");
        break;
    case 10:
        strcat(timekey,"Min50_55");
        break;
    case 11:
        strcat(timekey,"Min55_00");
        break;
    }
    return timekey;
}

/* ----------------------------------------------------------------- */

char *
GenTimeKey(time_t now)
{
    char str[64];

    sprintf(str,"%s",ctime(&now));

    return ConvTimeKey(str);
}
