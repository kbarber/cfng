/*
 * $Id: sensible.c 744 2004-05-23 07:53:59Z skaar $
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

/* Files to be ignored when parsing directories */
char *VSKIPFILES[] = {
    ".",
    "..",
    "lost+found",
    ".cfng.rm",
    NULL
};

/* ----------------------------------------------------------------- */

int
SensibleFile(char *nodename,char *path,struct Image *ip)
{
    int i, suspicious = true;
    struct stat statbuf;
    unsigned char *sp, newname[CF_BUFSIZE],vbuff[CF_BUFSIZE];

    if (strlen(nodename) < 1) {
        snprintf(g_output, CF_BUFSIZE, 
                "Empty (null) filename detected in %s\n", path);
        CfLog(cferror,g_output,"");
        return false;
    }

    if (IsItemIn(g_suspiciouslist,nodename)) {
        struct stat statbuf;
        if (stat(nodename,&statbuf) != -1) {
            if (S_ISREG(statbuf.st_mode)) {
                snprintf(g_output, CF_BUFSIZE,
                        "Suspicious file %s found in %s\n", 
                        nodename, path);
                CfLog(cferror,g_output,"");
                return false;
            }
        }
    }

    if ((strcmp(nodename,"...") == 0) && (strcmp(path,"/") == 0)) {
        Verbose("DFS cell node detected in /...\n");
        return true;
    }

    for (i = 0; VSKIPFILES[i] != NULL; i++) {
        if (strcmp(nodename,VSKIPFILES[i]) == 0) {
            Debug("Filename %s/%s is classified as ignorable\n",
                    path, nodename);
            return false;
        }
    }

    if ((strcmp("[",nodename) == 0) && (strcmp("/usr/bin",path) == 0)) {
        if (g_vsystemhardclass == linuxx) {
            return true;
        }
    }

    suspicious = true;

    for (sp = nodename; *sp != '\0'; sp++) {
        if ((*sp > 31) && (*sp < 127)) {
            suspicious = false;
            break;
        }
    }

    strcpy(vbuff,path);
    AddSlash(vbuff);
    strcat(vbuff,nodename);

    if (suspicious && g_nonalphafiles) {
        snprintf(g_output, CF_BUFSIZE,
            "Suspicious filename %s in %s has no alphanumeric "
            "content (security)", CanonifyName(nodename), path);
        CfLog(cfsilent,g_output,"");
        strcpy(newname,vbuff);

        for (sp = newname+strlen(path); *sp != '\0'; sp++) {
            if ((*sp > 126) || (*sp < 33)) {
                /* Create a visible ASCII interpretation */
                *sp = 50 + (*sp / 50);
            }
        }

        strcat(newname,".cf-nonalpha");

        snprintf(g_output,CF_BUFSIZE,"Renaming file %s to %s",vbuff,newname);
        CfLog(cfsilent,g_output,"");

        if (rename(vbuff,newname) == -1) {
            CfLog(cfverbose,"Rename failed - foreign filesystem?\n","rename");
        }
        if (chmod(newname,0644) == -1) {
            CfLog(cfverbose,"Mode change failed - "
                    "foreign filesystem?\n","chmod");
        }
        return false;
    }

    if (strstr(nodename,".") && (g_extensionlist != NULL)) {
        if (cflstat(vbuff,&statbuf,ip) == -1) {
            snprintf(g_output,CF_BUFSIZE,
                    "Couldn't examine %s - foreign filesystem?\n",vbuff);
            CfLog(cfverbose,g_output,"lstat");
            return true;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (strcmp(nodename,"...") == 0) {
                Verbose("Hidden directory ... found in %s\n",path);
                return true;
            }

            for (sp = nodename+strlen(nodename)-1; *sp != '.'; sp--) { }

                /* Don't get .dir */
            if ((char *)sp != nodename) {
                sp++; /* Find file extension, look for known plain files  */

                if ((strlen(sp) > 0) && IsItemIn(g_extensionlist,sp)) {
                    snprintf(g_output, CF_BUFSIZE, 
                            "Suspicious directory %s in %s looks like "
                            "plain file with extension .%s", 
                            nodename, path, sp);
                    CfLog(cfsilent,g_output,"");
                    return false;
                }
            }
        }
    }

    /* Check for files like ".. ." */
    for (sp = nodename; *sp != '\0'; sp++) {
        if ((*sp != '.') && ! isspace(*sp)) {
            suspicious = false;
            return true;
        }
    }

    /* removed if (g_extensionlist==NULL) mb */
    if (cflstat(vbuff,&statbuf,ip) == -1) {
        snprintf(g_output,CF_BUFSIZE,"Couldn't stat %s",vbuff);
        CfLog(cfverbose,g_output,"lstat");
        return true;
    }

    /* No sense in warning about empty files */
    if (statbuf.st_size == 0 && ! (g_verbose||g_inform)) {
        return false;
    }

    snprintf(g_output, CF_BUFSIZE, "Suspicous looking file "
            "object \"%s\" masquerading as hidden file in %s\n",
            nodename, path);

    CfLog(cfsilent,g_output,"");
    Debug("Filename looks suspicious\n");

    if (S_ISLNK(statbuf.st_mode)) {
        snprintf(g_output,CF_BUFSIZE,"   %s is a symbolic link\n",nodename);
        CfLog(cfsilent,g_output,"");
    } else if (S_ISDIR(statbuf.st_mode)) {
        snprintf(g_output,CF_BUFSIZE,"   %s is a directory\n",nodename);
        CfLog(cfsilent,g_output,"");
    }

    snprintf(g_output,CF_BUFSIZE,
        "[%s] has size %ld and full mode %o\n",
        nodename,(unsigned long)(statbuf.st_size),
        (unsigned int)(statbuf.st_mode));
    CfLog(cfsilent,g_output,"");

    return true;
}


/* ----------------------------------------------------------------- */

void
RegisterRecursionRootDevice(dev_t device)
{
    Debug("Registering root device as %d\n",device);
    g_rootdevice = device;
}


/* ----------------------------------------------------------------- */

int
DeviceChanged(dev_t thisdevice)
{
    if (thisdevice == g_rootdevice) {
        return false;
    } else {
        Verbose("Device change from %d to %d\n",g_rootdevice,thisdevice);
        return true;
    }
}
