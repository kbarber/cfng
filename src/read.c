/*
 * $Id: read.c 690 2004-05-21 15:16:43Z skaar $
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

#include "cf.defs.h"
#include "cf.extern.h"

/* ----------------------------------------------------------------- */

int
ReadLine(char *buff, int size, FILE *fp)
{
    buff[0] = '\0';

    /* mark end of buffer */
    buff[size - 1] = '\0';

    if (fgets(buff, size, fp) == NULL) {

        /* EOF */
        *buff = '\0';
        return false;
    } else {
        char *tmp;

        /* remove newline */
        if ((tmp = strrchr(buff, '\n')) != NULL) {
            *tmp = '\0';
        }
    }
    return true;
}
