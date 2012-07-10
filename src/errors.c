/*
 * $Id: errors.c 692 2004-05-21 17:41:09Z skaar $
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

/*
 * --------------------------------------------------------------------
 * Error handling
 * --------------------------------------------------------------------
 */

#include "cf.defs.h"
#include "cf.extern.h"

void
FatalError(char *s)
{
    fprintf (stderr, "%s:%s:%s\n", g_vprefix, g_vcurrentfile, s);
    g_silent = true;
    ReleaseCurrentLock();
    closelog();
    exit(1);
}

/* ----------------------------------------------------------------- */

void
Warning(char *s)
{
    if (g_warnings) {
        fprintf(stderr, "%s:%s:%d: Warning: %s\n",
                g_vprefix, g_vcurrentfile, g_linenumber, s);
    }
}

/* ----------------------------------------------------------------- */

void
ResetLine(char *s)
{
    int c;
    int v;
    char *p;

    v = 0;
    while (isdigit((int)*s)) {
        v = 10*v + *s++ - '0';
    }
    g_linenumber = v - 1;

    c = *s++;
    while (c == ' ') {
        c = *s++;
    }

    if (c == '"') {
        p = g_vcurrentfile;
        c = *s++;
        while (c && c != '"') {
            *p++ = c;
            c = *s++;
        }
    *p = '\0';
    }
}
