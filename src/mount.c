/*
 * $Id: mount.c 741 2004-05-23 06:55:46Z skaar $
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

/*
 * Toolkits: mount support
 */

int
MountPathDefined(void)
{
    if (g_vmountlist == NULL) {
        CfLog(cfinform,"Program does not define mountpattern\n","");
        return false;
    }

    return true;
}

/* ----------------------------------------------------------------- */


int MatchAFileSystem(char *server, char *lastlink)
{
    struct Item *mp;
    char *sp;
    char host[CF_MAXVARSIZE];

    for (mp = g_vmounted; mp != NULL; mp=mp->next) {
        sscanf (mp->name,"%[^:]",host);

        if (! IsItemIn(g_vbinservers,host)) {
            continue;
        }

        if (strcmp(host,g_vdefaultbinserver.name) == 0) {

            /* Can't link machine to itself! */
            continue;
        }

        for (sp = mp->name+strlen(mp->name); *(sp-1) != '/'; sp--) { }

        if (IsHomeDir(sp)) {
            continue;
        }

        if (strcmp(sp,lastlink) == 0) {
            strcpy(server,mp->name+strlen(host)+1);
            return(true);
        }
    }

    return(false);
}

/* ----------------------------------------------------------------- */

/* Is FS NFS mounted ? */

int
IsMountedFileSystem (struct stat *childstat,char *dir,int rlevel)
{
    struct stat parentstat;
    struct Mountables *mp;
    char host[CF_MAXVARSIZE];

    for (mp = g_vmountables; mp !=NULL; mp=mp->next) {
        if (strstr(mp->filesystem,dir)) {
            sscanf(mp->filesystem,"%[^:]",host);

            Debug("Looking at filesystem %s on %s\n",mp->filesystem,host);

            if (strncmp(host,g_vfqname,strlen(host)) == 0) {
                Verbose("Filesystem %s belongs to this host\n",dir);
                return false;
            }
        }
    }

    strcpy(g_vbuff,dir);

    if (g_vbuff[strlen(g_vbuff)-1] == '/') {
        strcat(g_vbuff,"..");
    } else {
        strcat(g_vbuff,"/..");
    }

    if (stat(g_vbuff,&parentstat) == -1) {

        Debug2("File %s couldn't stat its parent directory! "
                "Assuming permission\n",dir);
        Debug2("is denied because the file system is mounted from "
                "another host.\n");

        return(true);
    }

    /* If this is the root of a search, don't stop before we started ! */
    if (rlevel == 0) {
        Debug("NotMountedFileSystem\n");
        return false;
    }

    if (childstat->st_dev != parentstat.st_dev) {
        Debug2("[%s is on a different file system, not descending]\n",dir);
        return (true);
    }

    Debug("NotMountedFileSystem\n");
    return(false);
}
