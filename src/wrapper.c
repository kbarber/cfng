/*
 * $Id: wrapper.c 682 2004-05-20 22:46:52Z skaar $
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
 * These functions are wrappers for the real functions so that
 * we can abstract the task of parsing general wildcard paths
 * like /a*b/bla?/xyz from a single function ExpandWildCardsAndDo() 
 */

void
TidyWrapper(char *startpath,void *vp)
{
    struct Tidy *tp;
    struct stat sb;

    tp = (struct Tidy *) vp;

    Debug2("TidyWrapper(%s)\n",startpath);

    if (tp->done == 'y') {
        return;
    }

    tp->done = 'y';

    if (!GetLock(ASUniqueName("tidy"),CanonifyName(startpath),
                tp->ifelapsed,tp->expireafter,g_vuqname,g_cfstarttime)) {
        return;
    }

    if (stat(startpath,&sb) == -1) {
        snprintf(g_output,CF_BUFSIZE,
                "Tidy directory %s cannot be accessed",startpath);
        CfLog(cfinform,g_output,"stat");
        return;
    }

    RegisterRecursionRootDevice(sb.st_dev);
    RecursiveTidySpecialArea(startpath,tp,tp->maxrecurse,&sb);

    ReleaseCurrentLock();
}

/* ----------------------------------------------------------------- */

void
RecHomeTidyWrapper(char *startpath,void *vp)
{
    struct stat sb;
    struct Tidy *tp = (struct Tidy *) vp;

    Verbose("Tidying Home partition %s...\n",startpath);

    if (tp != NULL) {
        if (!GetLock(ASUniqueName("tidy"),CanonifyName(startpath),
                    tp->ifelapsed,tp->expireafter,g_vuqname,g_cfstarttime)) {
            return;
        }
    } else {
        if (!GetLock(ASUniqueName("tidy"),CanonifyName(startpath),
                    g_vifelapsed,g_vexpireafter,g_vuqname,g_cfstarttime)) {
            return;
        }
    }

    if (stat(startpath,&sb) == -1) {
        snprintf(g_output,CF_BUFSIZE,
                "Tidy directory %s cannot be accessed",startpath);
        CfLog(cfinform,g_output,"stat");
        return;
   }

    RegisterRecursionRootDevice(sb.st_dev);
    RecursiveHomeTidy(startpath,1,&sb);

    ReleaseCurrentLock();
}

/* ----------------------------------------------------------------- */

void
CheckFileWrapper(char *startpath,void *vp)
{
    struct stat statbuf;
    mode_t filemode;
    char *lastnode, lock[CF_MAXVARSIZE];
    int fd;
    struct File *ptr;

    ptr = (struct File *)vp;

    if (ptr->uid != NULL) {
        snprintf(lock,CF_MAXVARSIZE-1,"%s_%o_%o_%d",
                startpath,ptr->plus,ptr->minus,ptr->uid->uid);
    } else {
        snprintf(lock,CF_MAXVARSIZE-1,"%s_%o_%o",
                startpath,ptr->plus,ptr->minus);
    }


    if (!GetLock(ASUniqueName("files"),CanonifyName(lock),
                ptr->ifelapsed,ptr->expireafter,g_vuqname,g_cfstarttime)) {
        return;
    }

    if ((strlen(startpath) == 0) || (startpath == NULL)) {
        return;
    }

    if (ptr->action == touch && IsWildCard(ptr->path)) {
        printf("%s: Can't touch a wildcard! (%s)\n",g_vprefix,ptr->path);
        return;
    }

    for (lastnode = startpath+strlen(startpath)-1; *lastnode != '/';
            lastnode--) { }

    lastnode++;

    if (ptr->inclusions != NULL && !IsWildItemIn(ptr->inclusions,lastnode)) {
        Debug2("cfagent: skipping non-included pattern %s\n",lastnode);

        if (stat(startpath,&statbuf) != -1) {
            /* assure that we recurse into directories */
            if (!S_ISDIR(statbuf.st_mode)) {
                return;
            }
        }
    }

    if (IsWildItemIn(ptr->exclusions,lastnode)) {
        Debug2("Skipping excluded pattern file %s\n",lastnode);
        ReleaseCurrentLock();
        return;
    }

    if (stat(startpath,&statbuf) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"Cannot access file/directory %s\n",
                ptr->path);
        CfLog(cfinform,g_output,"stat");

       /* files ending in /. */
        if (TouchDirectory(ptr)) {
            MakeDirectoriesFor(startpath,'n');
            ptr->action = fixall;

            /* trunc /. */
            *(startpath+strlen(ptr->path)-2) = '\0';

            CheckExistingFile("*",startpath,ptr->plus,ptr->minus,
                    ptr->action,ptr->uid,ptr->gid,&statbuf,
                    ptr,ptr->acl_aliases);
            ReleaseCurrentLock();
            return;
        }

        /* Decide the mode for filecreation */
        filemode = g_defaultmode;
        filemode |=   ptr->plus;
        filemode &= ~(ptr->minus);

        switch (ptr->action) {
        case create:
        case touch:
            if (! g_dontdo) {
                MakeDirectoriesFor(startpath,'n');

                if ((fd = creat(ptr->path,filemode)) == -1) {
                    perror("creat");
                    AddMultipleClasses(ptr->elsedef);
                    return;
                } else {
                    AddMultipleClasses(ptr->defines);
                    close(fd);
                }

                CheckExistingFile("*",startpath,ptr->plus,ptr->minus,
                        fixall,ptr->uid,ptr->gid,&statbuf,
                        ptr,ptr->acl_aliases);
            }

            snprintf(g_output,CF_BUFSIZE*2,"Creating file %s, mode = %o\n",
                    ptr->path,filemode);
            CfLog(cfinform,g_output,"");
            break;
        case alert:
        case linkchildren:
        case warnall:
        case warnplain:
        case warndirs:
        case fixplain:
        case fixdirs:
        case fixall:
            snprintf(g_output,CF_BUFSIZE*2,
                    "File/Dir %s did not exist and was marked (%s)\n",
                    ptr->path,g_fileactiontext[ptr->action]);
            CfLog(cfinform,g_output,"");
            break;
        case compress:
            break;
        default:
            FatalError("cfagent: Internal sofware error: "
                    "Checkfiles(), bad action\n");
        }
    } else {
/*
   if (TouchDirectory(ptr)) Don't check, just touch..
      {
      ReleaseCurrentLock();
      return;
      }
*/
        if (ptr->action == create) {
            ReleaseCurrentLock();
            return;
        }


        if (ptr->action == linkchildren) {
            LinkChildren(ptr->path,
                's',
                &statbuf,
                ptr->uid->uid,ptr->gid->gid,
                ptr->inclusions,
                ptr->exclusions,
                NULL,
                0,
                NULL);
            ReleaseCurrentLock();
            return;
        }

        if (S_ISDIR(statbuf.st_mode) && (ptr->recurse != 0)) {
            if (IgnoreFile(startpath,ReadLastNode(startpath),ptr->ignores)) {
                Verbose("%s: (Skipping ignored directory %s)\n",
                        g_vprefix,startpath);
                return;
            }

            CheckExistingFile("*",startpath,ptr->plus,ptr->minus,
                    ptr->action,ptr->uid,ptr->gid,&statbuf,
                    ptr,ptr->acl_aliases);
            RegisterRecursionRootDevice(statbuf.st_dev);
            RecursiveCheck(startpath,ptr->plus,ptr->minus,ptr->action,
                    ptr->uid,ptr->gid,ptr->recurse,0,ptr,&statbuf);
        } else {
            if (lstat(startpath,&statbuf) == -1) {
                CfLog(cferror,"Unable to stat already statted object!","lstat");
                return;
            }
            CheckExistingFile("*",startpath,ptr->plus,ptr->minus,
                    ptr->action,ptr->uid,ptr->gid,&statbuf,
                    ptr,ptr->acl_aliases);
        }
    }
    ReleaseCurrentLock();
}

/* ----------------------------------------------------------------- */

void
DirectoriesWrapper(char *dir,void *vp)
{
    struct stat statbuf;
    char directory[CF_EXPANDSIZE];
    struct File *ptr;

    ptr=(struct File *)vp;

    ExpandVarstring(dir,directory,"");

    AddSlash(directory);
    strcat(directory,".");

    MakeDirectoriesFor(directory,'n');

    if (stat(directory,&statbuf) == -1) {
        ExpandVarstring(dir,directory,"");

        /* Shouldn't happen - mode 000 ??*/
        chmod(directory,0500);

        if (stat(directory,&statbuf) == -1) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "Cannot stat %s after creating it",directory);
            CfLog(cfinform,g_output,"stat");
            return;
        }
    }

    if (!GetLock(ASUniqueName("directories"),CanonifyName(directory),
                ptr->ifelapsed,ptr->expireafter,g_vuqname,g_cfstarttime)) {
        return;
    }

    CheckExistingFile("*",dir,ptr->plus,ptr->minus,ptr->action,
            ptr->uid,ptr->gid,&statbuf,ptr,ptr->acl_aliases);
    ReleaseCurrentLock();
}
