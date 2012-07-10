/*
 * $Id: df.c 682 2004-05-20 22:46:52Z skaar $
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
 * Disk usage module.
 * Contributed to cfengine by Demosthenes Skipitaris
 */


#include "../pub/getopt.h"
#include "cf.defs.h"
#include "cf.extern.h"

#ifdef HAVE_SYS_STATFS_H
 #include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

/* ----------------------------------------------------------------- */

int
GetDiskUsage(char *file,enum cfsizes type)
{
#if defined SOLARIS || defined OSF || defined UNIXWARE
    struct statvfs buf;
#elif defined ULTRIX
    struct fs_data buf;
#else
    struct statfs buf;
#endif
    u_long blocksize = 1024, total = 0, used = 0, avail = 0;
    int capacity = 0;

    memset(&buf,0, sizeof (buf));

    Verbose("Checking free space on %s\n",file);


#if defined ULTRIX
    if (getmnt (NULL, &buf, sizeof (struct fs_data), STAT_ONE, file) == -1) {
        snprintf(g_output,CF_BUFSIZE,"Couldn't get filesystem info for %s\n",file);
        CfLog(cferror,g_output,"");
        return CF_INFINITY;
    }
#elif defined SOLARIS || defined OSF || defined UNIXWARE
    if (statvfs (file, &buf) != 0) {
        snprintf(g_output,CF_BUFSIZE,"Couldn't get filesystem info for %s\n",file);
        CfLog(cferror,g_output,"");
        return CF_INFINITY;
    }
#elif defined IRIX || defined SCO || defined CFCRAY || defined UNIXWARE
    if (statfs (file, &buf, sizeof (struct statfs), 0) != 0) {
        snprintf(g_output,CF_BUFSIZE,"Couldn't get filesystem info for %s\n",file);
        CfLog(cferror,g_output,"");
        return CF_INFINITY;
    }
#else
    if (statfs (file, &buf) != 0) {
        snprintf(g_output,CF_BUFSIZE,"Couldn't get filesystem info for %s\n",file);
        CfLog(cferror,g_output,"");
        return CF_INFINITY;
    }
#endif

#if defined ULTRIX
    total = buf.fd_btot;
    used = buf.fd_btot - buf.fd_bfree;
    avail = buf.fd_bfreen;
#endif

#if defined SOLARIS
    total = buf.f_blocks * (buf.f_frsize / blocksize);
    used = (buf.f_blocks - buf.f_bfree) * (buf.f_frsize / blocksize);
    avail = buf.f_bavail * (buf.f_frsize / blocksize);
#endif

#if defined NETBSD || defined FREEBSD || defined OPENBSD || defined SUNOS || defined HPuUX || defined DARWIN
    total = buf.f_blocks;
    used = buf.f_blocks - buf.f_bfree;
    avail = buf.f_bavail;
#endif

#if defined OSF
    total = (buf.f_blocks *  buf.f_frsize) / blocksize;
    used = ((buf.f_blocks - buf.f_bfree)* (buf.f_frsize) / blocksize);
    avail = (buf.f_bavail * buf.f_frsize) / blocksize;
#endif

#if defined AIX || defined SCO || defined CFCRAY || defined LINUX
    total = buf.f_blocks * ((float)buf.f_bsize / blocksize);
    used = (buf.f_blocks - buf.f_bfree) * ((float)buf.f_bsize / blocksize);
    avail = buf.f_bfree * ((float)buf.f_bsize / blocksize);
#endif

#if defined IRIX
    /* Float fix by arjen@sara.nl */
    total = buf.f_blocks *  ((float)buf.f_bsize / blocksize);
    used = (buf.f_blocks - buf.f_bfree) * ((float)buf.f_bsize / blocksize);
    avail = buf.f_bfree *  ((float)buf.f_bsize/blocksize);
#endif

    capacity = (double) (avail) / (double) (avail + used) * 100;

    Debug2("GetDiskUsage(%s) = %d/%d\n",file,avail,capacity);

    /* Free kilobytes */
    if (type == cfabs) {
        return avail;
    } else {
        return capacity;
    }
}
