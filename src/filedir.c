/*
 * $Id: filedir.c 739 2004-05-23 06:29:11Z skaar $
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

#ifdef DARWIN
#include <sys/attr.h>
#endif

/* ----------------------------------------------------------------- */

/* This assumes that the dir lies under mountpattern */

int
IsHomeDir(char *name)
{
    char *sp;
    struct Item *ip;
    int slashes;

    if (name == NULL || strlen(name) == 0) {
        return false;
    }

    if (g_vmountlist == NULL) {
        return (false);
    }

    for (ip = g_vhomepatlist; ip != NULL; ip=ip->next) {
        slashes = 0;

        for (sp = ip->name; *sp != '\0'; sp++) {
            if (*sp == '/') {
                slashes++;
            }
        }

        for (sp = name+strlen(name); (*(sp-1) != '/') &&
                (sp >= name) && (slashes >= 0); sp--) {
            if (*sp == '/') {
                slashes--;
            }
        }

        /* Full comparison */

        if (WildMatch(ip->name,sp)) {
            Debug("IsHomeDir(true)\n");
            return(true);
        }
    }

    Debug("IsHomeDir(false)\n");
    return(false);
}


/* ----------------------------------------------------------------- */

int
EmptyDir(char *path)
{
    DIR *dirh;
    struct dirent *dirp;
    int count = 0;

    Debug2("cfagent: EmptyDir(%s)\n",path);

    if ((dirh = opendir(path)) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't open directory %s\n",path);
        CfLog(cfverbose,g_output,"opendir");
        return true;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name,path,NULL)) {
            continue;
        }

        count++;
    }

    closedir(dirh);

    return (!count);
}

/* ----------------------------------------------------------------- */

int
RecursiveCheck(char *name, mode_t plus, mode_t minus,
        enum fileactions action,
        struct UidList *uidlist, struct GidList *gidlist,
        int recurse, int rlevel, struct File *ptr, struct stat *sb)
{
    DIR *dirh;
    int goback;
    struct dirent *dirp;
    char pcwd[CF_BUFSIZE];
    struct stat statbuf;

    if (recurse == -1) {
        return false;
    }

    if (rlevel > CF_RECURSION_LIMIT) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "WARNING: Very deep nesting of directories "
                "(>%d deep): %s (Aborting files)", rlevel, name);
        CfLog(cferror,g_output,"");
        return false;
    }

    memset(pcwd,0,CF_BUFSIZE);

    Debug("RecursiveCheck(%s,+%o,-%o)\n",name,plus,minus);

    if (!DirPush(name,sb)) {
        return false;
    }

    if ((dirh = opendir(".")) == NULL) {
        if (lstat(name,&statbuf) != -1) {
            CheckExistingFile("*",name,plus,minus,action,
                    uidlist,gidlist,&statbuf,ptr,ptr->acl_aliases);
        }
        return true;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name,name,NULL)) {
            continue;
        }

        if (IgnoreFile(name,dirp->d_name,ptr->ignores)) {
            continue;
        }

        /* Assemble pathname */
        strcpy(pcwd,name);
        AddSlash(pcwd);

        if (BufferOverflow(pcwd,dirp->d_name)) {
            closedir(dirh);
            return true;
        }

        strcat(pcwd,dirp->d_name);

        if (lstat(dirp->d_name,&statbuf) == -1) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "RecursiveCheck was looking at %s when this happened:\n",
                    pcwd);
            CfLog(cferror,g_output,"lstat");
            continue;
        }

        if (g_travlinks) {
            if (lstat(dirp->d_name,&statbuf) == -1) {
                snprintf(g_output,CF_BUFSIZE*2,"Can't stat %s\n",pcwd);
                CfLog(cferror,g_output,"stat");
                continue;
            }

            if (S_ISLNK(statbuf.st_mode) && (statbuf.st_mode != getuid())) {
                snprintf(g_output, CF_BUFSIZE, 
                        "File %s is an untrusted link. cfagent will "
                        "not follow it with a destructive operation (tidy)",
                        pcwd);
                continue;
            }

            if (stat(dirp->d_name,&statbuf) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "RecursiveCheck was working on %s when "
                        "this happened:\n", pcwd);
                CfLog(cferror,g_output,"stat");
                continue;
            }
        }

        if (ptr->xdev =='y' && DeviceChanged(statbuf.st_dev)) {
            Verbose("Skipping %s on different device\n",pcwd);
            continue;
        }

        /* should we ignore links? */
        if (S_ISLNK(statbuf.st_mode)) {
            CheckExistingFile("*",pcwd,plus,minus,action,
                    uidlist,gidlist,&statbuf,ptr,ptr->acl_aliases);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (IsMountedFileSystem(&statbuf,pcwd,rlevel)) {
                continue;
            } else {
                if ((ptr->recurse > 1) || (ptr->recurse == CF_INF_RECURSE)) {
                    CheckExistingFile("*",pcwd,plus,minus,action,
                            uidlist,gidlist,&statbuf,ptr,ptr->acl_aliases);
                    goback = RecursiveCheck(pcwd,plus,minus,action,
                            uidlist,gidlist,recurse-1,rlevel+1,ptr,&statbuf);
                    DirPop(goback,name,sb);
                } else {
                    CheckExistingFile("*",pcwd,plus,minus,action,
                            uidlist,gidlist,&statbuf,ptr,ptr->acl_aliases);
                }
            }
        } else {
            CheckExistingFile("*",pcwd,plus,minus,action,
                    uidlist,gidlist,&statbuf,ptr,ptr->acl_aliases);
        }
    }

    closedir(dirh);
    return true;
}


/* ----------------------------------------------------------------- */

void
CheckExistingFile(char *cf_findertype, char *file,
        mode_t plus, mode_t minus, enum fileactions action,
        struct UidList *uidlist, struct GidList *gidlist,
        struct stat *dstat, struct File *ptr, struct Item *acl_aliases)
{
    mode_t newperm = dstat->st_mode, maskvalue;
    int amroot = true, fixmode = true, docompress=false;
    unsigned char digest[EVP_MAX_MD_SIZE+1];

#if defined HAVE_CHFLAGS
    u_long newflags;
#endif

    /* This makes the DEFAULT modes absolute */
    maskvalue = umask(0);

    if (action == compress) {
        docompress = true;
        if (ptr != NULL) {
            AddMultipleClasses(ptr->defines);
        }
        action = fixall;    /* Fix any permissions which are set */
    }

    Debug("%s: Checking fs-object %s\n",g_vprefix,file);

#if defined HAVE_CHFLAGS
    if (ptr != NULL) {
        Debug("CheckExistingFile(+%o,-%o,+%o,-%o)\n",
                plus,minus,ptr->plus_flags,ptr->minus_flags);
    } else {
        Debug("CheckExistingFile(+%o,-%o)\n",plus,minus);
    }
#else
    Debug("CheckExistingFile(+%o,-%o)\n",plus,minus);
#endif

    if (ptr != NULL) {
        if (IgnoredOrExcluded(files,file,ptr->inclusions,ptr->exclusions)) {
            Debug("Skipping excluded file %s\n",file);
            umask(maskvalue);
            return;
        }

        if (!FileObjectFilter(file,dstat,ptr->filters,files)) {
            Debug("Skipping filtered file %s\n",file);
            umask(maskvalue);
            return;
        }

        if (action == alert) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "Alert specified on file %s (m=%o,o=%d,g=%d)",
                    file,(dstat->st_mode & 07777),
                    dstat->st_uid,dstat->st_gid);
            CfLog(cferror,g_output,"");
            return;
        }
    }

    if (!IsPrivileged()) {
        amroot = false;
    }

    /* directories must have x set if r set, regardless  */

    newperm = (dstat->st_mode & 07777);
    newperm |= plus;
    newperm &= ~minus;

    if (S_ISREG(dstat->st_mode) && (action == fixdirs || action == warndirs)) {
        Debug("Regular file, returning...\n");
        umask(maskvalue);
        return;
    }

    if (S_ISDIR(dstat->st_mode)) {
        if (action == fixplain || action == warnplain) {
            umask(maskvalue);
            return;
        }

        Debug("Directory...fixing x bits\n");

        if (newperm & S_IRUSR) {
            newperm  |= S_IXUSR;
        }

        if (newperm & S_IRGRP) {
            newperm |= S_IXGRP;
        }

        if (newperm & S_IROTH) {
            newperm |= S_IXOTH;
        }
    }

    if (dstat->st_uid == 0 && (dstat->st_mode & S_ISUID)) {
        if (newperm & S_ISUID) {
            if (! IsItemIn(g_vsetuidlist,file)) {
                if (amroot) {
                    snprintf(g_output, CF_BUFSIZE*2,
                            "NEW SETUID root PROGRAM %s\n", file);
                    CfLog(cfinform,g_output,"");
                }
                PrependItem(&g_vsetuidlist,file,NULL);
            }
        } else {
            switch (action) {
            case fixall:
            case fixdirs:
            case fixplain:
                snprintf(g_output,CF_BUFSIZE*2,
                        "Removing setuid (root) flag from %s...\n\n",file);
                CfLog(cfinform,g_output,"");

                if (ptr != NULL) {
                    AddMultipleClasses(ptr->defines);
                }
                break;
            case warnall:
            case warndirs:
            case warnplain:
                if (amroot) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "WARNING setuid (root) flag on %s...\n\n",file);
                    CfLog(cferror,g_output,"");
                }
                break;

            }
        }
    }

    if (dstat->st_uid == 0 && (dstat->st_mode & S_ISGID)) {
        if (newperm & S_ISGID) {
            if (! IsItemIn(g_vsetuidlist,file)) {
                if (S_ISDIR(dstat->st_mode)) {
                    /* setgid directory */
                } else {
                    if (amroot) {
                        snprintf(g_output,CF_BUFSIZE*2,
                                "NEW SETGID root PROGRAM %s\n",file);
                        CfLog(cferror,g_output,"");
                    }
                    PrependItem(&g_vsetuidlist,file,NULL);
                }
            }
        } else {
            switch (action) {
            case fixall:
            case fixdirs:
            case fixplain:
                snprintf(g_output,CF_BUFSIZE*2,
                        "Removing setgid (root) flag from %s...\n\n",file);
                CfLog(cfinform,g_output,"");
                if (ptr != NULL) {
                    AddMultipleClasses(ptr->defines);
                }
                break;
            case warnall:
            case warndirs:
            case warnplain:
                snprintf(g_output,CF_BUFSIZE*2,
                        "WARNING setgid (root) flag on %s...\n\n",file);
                CfLog(cferror,g_output,"");
                break;

            default:
                break;
            }
        }
    }

#ifdef DARWIN
    if (CheckFinderType(file, action, cf_findertype, dstat)) {
        if (ptr != NULL) {
            AddMultipleClasses(ptr->defines);
        }
    }
#endif

    if (CheckOwner(file,action,uidlist,gidlist,dstat)) {
        if (ptr != NULL) {
            AddMultipleClasses(ptr->defines);
        }
    }

    if ((ptr != NULL) && S_ISREG(dstat->st_mode) && (ptr->checksum != 'n')) {
        Debug("Checking checksum integrity of %s\n",file);

        memset(digest,0,EVP_MAX_MD_SIZE+1);
        ChecksumFile(file,digest,ptr->checksum);

        if (!g_dontdo) {
            if (ChecksumChanged(file,digest,cferror,false,ptr->checksum)) { }
        }
    }

    /* No point in checking permission on a link */
    if (S_ISLNK(dstat->st_mode)) {
        if (ptr != NULL) {
            KillOldLink(file,ptr->defines);
        } else {
            KillOldLink(file,NULL);
        }
        umask(maskvalue);
        return;
    }

    if (stat(file,dstat) == -1) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Can't stat file %s while looking at permissions "
                "(was not copied?)\n", file);
        CfLog(cfverbose,g_output,"stat");
        umask(maskvalue);
        return;
    }

    if (CheckACLs(file,action,acl_aliases)) {
        if (ptr != NULL) {
            AddMultipleClasses(ptr->defines);
        }
    }

#ifndef HAVE_CHFLAGS

    /* file okay */
    if (((newperm & 07777) == (dstat->st_mode & 07777)) && (action != touch)) {
        Debug("File okay, newperm = %o, stat = %o\n",
                (newperm & 07777),(dstat->st_mode & 07777));
        if (docompress) {
            CompressFile(file);
        }

        if (ptr != NULL) {
            AddMultipleClasses(ptr->elsedef);
        }

        umask(maskvalue);
        return;
    }
#else
    /* file okay */
    if (((newperm & 07777) == (dstat->st_mode & 07777)) && (action != touch)) {
        Debug("File okay, newperm = %o, stat = %o\n",
                (newperm & 07777),(dstat->st_mode & 07777));
        fixmode = false;
   }
#endif

    if (fixmode) {
        Debug("Trying to fix mode...\n");

        switch (action) {
        case linkchildren:

        case warnplain:
            if (S_ISREG(dstat->st_mode)) {
                snprintf(g_output,CF_BUFSIZE*2,"%s has permission %o\n",
                        file,dstat->st_mode & 07777);
                CfLog(cferror,g_output,"");
                snprintf(g_output, CF_BUFSIZE*2, "[should be %o]\n",
                        newperm & 07777);
                CfLog(cferror,g_output,"");
            }
            break;
        case warndirs:
            if (S_ISDIR(dstat->st_mode)) {
                snprintf(g_output,CF_BUFSIZE*2,"%s has permission %o\n",
                        file,dstat->st_mode & 07777);
                CfLog(cferror,g_output,"");
                snprintf(g_output, CF_BUFSIZE*2, "[should be %o]\n",
                        newperm & 07777);
                CfLog(cferror,g_output,"");
            }
            break;
        case warnall:
            snprintf(g_output,CF_BUFSIZE*2,"%s has permission %o\n",
                    file,dstat->st_mode & 07777);
            CfLog(cferror,g_output,"");
            snprintf(g_output,CF_BUFSIZE*2,"[should be %o]\n",newperm & 07777);
            CfLog(cferror,g_output,"");
            break;

        case fixplain:
            if (S_ISREG(dstat->st_mode)) {
                if (! g_dontdo) {
                    if (chmod (file,newperm & 07777) == -1) {
                        snprintf(g_output, CF_BUFSIZE*2, 
                                "chmod failed on %s\n", file);
                        CfLog(cferror,g_output,"chmod");
                        break;
                    }
                }

                snprintf(g_output,CF_BUFSIZE*2,
                        "Plain file %s had permission %o, changed it to %o\n",
                    file,dstat->st_mode & 07777,newperm & 07777);
                CfLog(cfinform,g_output,"");

                if (ptr != NULL) {
                    AddMultipleClasses(ptr->defines);
                }
            }
            break;

        case fixdirs:
            if (S_ISDIR(dstat->st_mode)) {
                if (! g_dontdo) {
                    if (chmod (file,newperm & 07777) == -1) {
                        snprintf(g_output, CF_BUFSIZE*2,
                                "chmod failed on %s\n", file);
                        CfLog(cferror,g_output,"chmod");
                        break;
                    }
                }

                snprintf(g_output,CF_BUFSIZE*2,
                        "Directory %s had permission %o, changed it to %o\n",
                file,dstat->st_mode & 07777,newperm & 07777);
                CfLog(cfinform,g_output,"");

                if (ptr != NULL) {
                    AddMultipleClasses(ptr->defines);
                }
            }
            break;

        case fixall:
            if (! g_dontdo) {
                if (chmod (file,newperm & 07777) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,"chmod failed on %s\n",file);
                    CfLog(cferror,g_output,"chmod");
                    break;
                }
            }

            snprintf(g_output,CF_BUFSIZE*2,
                    "Object %s had permission %o, changed it to %o\n",
                file,dstat->st_mode & 07777,newperm & 07777);
            CfLog(cfinform,g_output,"");

            if (ptr != NULL) {
                AddMultipleClasses(ptr->defines);
            }
            break;

        case touch:
            if (! g_dontdo) {
                if (chmod (file,newperm & 07777) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,"chmod failed on %s\n",file);
                    CfLog(cferror,g_output,"chmod");
                    break;
                }
                utime (file,NULL);
            }
            if (ptr != NULL) {
                AddMultipleClasses(ptr->defines);
            }
            break;
        default:
            FatalError("cfagent: internal error CheckExistingFile(): "
                    "illegal file action\n");
        }
    }


#if defined HAVE_CHFLAGS  /* BSD special flags */

    if (ptr != NULL) {
        newflags = (dstat->st_flags & CHFLAGS_MASK) ;
        newflags |= ptr->plus_flags;
        newflags &= ~(ptr->minus_flags);

        /* file okay */
        if ((newflags & CHFLAGS_MASK) == (dstat->st_flags & CHFLAGS_MASK)) {
            Debug("File okay, flags = %o, current = %o\n",
                    (newflags & CHFLAGS_MASK),(dstat->st_flags & CHFLAGS_MASK));
        } else {
            Debug("Fixing %s, newflags = %o, flags = %o\n",file,
                    (newflags & CHFLAGS_MASK),(dstat->st_flags & CHFLAGS_MASK));

            switch (action) {
            case linkchildren:

            case warnplain:
                if (S_ISREG(dstat->st_mode)) {
                    snprintf(g_output,CF_BUFSIZE*2,"%s has flags %o\n",
                            file,dstat->st_flags & CHFLAGS_MASK);
                    CfLog(cferror,g_output,"");
                    snprintf(g_output,CF_BUFSIZE*2,"[should be %o]\n",
                            newflags & CHFLAGS_MASK);
                    CfLog(cferror,g_output,"");
                }
                break;
            case warndirs:
                if (S_ISDIR(dstat->st_mode)) {
                    snprintf(g_output,CF_BUFSIZE*2,"%s has flags %o\n",
                            file,dstat->st_mode & CHFLAGS_MASK);
                    CfLog(cferror,g_output,"");
                    snprintf(g_output,CF_BUFSIZE*2,"[should be %o]\n",
                            newflags & CHFLAGS_MASK);
                    CfLog(cferror,g_output,"");
                }
                break;
            case warnall:
                snprintf(g_output,CF_BUFSIZE*2,"%s has flags %o\n",
                        file,dstat->st_mode & CHFLAGS_MASK);
                CfLog(cferror,g_output,"");
                snprintf(g_output,CF_BUFSIZE*2,"[should be %o]\n",
                        newflags & CHFLAGS_MASK);
                CfLog(cferror,g_output,"");
                break;

            case fixplain:

                if (S_ISREG(dstat->st_mode)) {
                    if (! g_dontdo) {
                        if (chflags (file,newflags & CHFLAGS_MASK) == -1) {
                            snprintf(g_output,CF_BUFSIZE*2,
                                    "chflags failed on %s\n",file);
                            CfLog(cferror,g_output,"chflags");
                            break;
                        }
                    }

                    snprintf(g_output,CF_BUFSIZE*2,
                            "%s had flags %o, changed it to %o\n",
                            file,dstat->st_flags & CHFLAGS_MASK,
                            newflags & CHFLAGS_MASK);
                    CfLog(cfinform,g_output,"");

                    if (ptr != NULL) {
                        AddMultipleClasses(ptr->defines);
                    }
                }
                break;

            case fixdirs:
                if (S_ISDIR(dstat->st_mode)) {
                    if (! g_dontdo) {
                        if (chflags (file,newflags & CHFLAGS_MASK) == -1) {
                            snprintf(g_output,CF_BUFSIZE*2,
                                    "chflags failed on %s\n",file);
                            CfLog(cferror,g_output,"chflags");
                            break;
                        }
                    }

                    snprintf(g_output,CF_BUFSIZE*2,
                            "%s had flags %o, changed it to %o\n",
                            file,dstat->st_flags & CHFLAGS_MASK,
                            newflags & CHFLAGS_MASK);
                    CfLog(cfinform,g_output,"");

                    if (ptr != NULL) {
                        AddMultipleClasses(ptr->defines);
                    }
                }
                break;

            case fixall:
                if (! g_dontdo) {
                    if (chflags (file,newflags & CHFLAGS_MASK) == -1) {
                        snprintf(g_output,CF_BUFSIZE*2,
                                "chflags failed on %s\n",file);
                        CfLog(cferror,g_output,"chflags");
                        break;
                    }
                }

                snprintf(g_output,CF_BUFSIZE*2,
                        "%s had flags %o, changed it to %o\n",
                        file,dstat->st_flags & CHFLAGS_MASK,
                        newflags & CHFLAGS_MASK);
                CfLog(cfinform,g_output,"");

                if (ptr != NULL) {
                    AddMultipleClasses(ptr->defines);
                }
                break;

            case touch:
                if (! g_dontdo) {
                    if (chflags (file,newflags & CHFLAGS_MASK) == -1) {
                        snprintf(g_output,CF_BUFSIZE*2,
                                "chflags failed on %s\n",file);
                        CfLog(cferror,g_output,"chflags");
                        break;
                    }
                    utime (file,NULL);
                }
                if (ptr != NULL) {
                    AddMultipleClasses(ptr->defines);
                }
                break;

            default:
                FatalError("cfagent: internal error CheckExistingFile(): "
                        "illegal file action\n");
            }
        }
    }
#endif

    if (docompress) {
        CompressFile(file);
        if (ptr != NULL) {
            AddMultipleClasses(ptr->defines);
        }
    }

    umask(maskvalue);
    Debug("CheckExistingFile(Done)\n");
}

/* ----------------------------------------------------------------- */

void
CheckCopiedFile(char *cf_findertype, char *file, mode_t plus, mode_t minus,
        enum fileactions action, struct UidList *uidlist,
        struct GidList *gidlist, struct stat *dstat,
        struct stat *sstat, struct File *ptr, struct Item *acl_aliases)
{
    mode_t newplus,newminus;

    /* 
     * plus/minus must be relative to source file, not to perms of newly
     * created file!
     */

    Debug("CheckCopiedFile(%s,+%o,-%o)\n",file,plus,minus);

    newplus = (sstat->st_mode & 07777) | plus;
    newminus = ~(newplus & ~minus) & 07777;

    CheckExistingFile(cf_findertype,file,newplus,newminus,fixall,
            uidlist,gidlist,dstat,NULL,acl_aliases);
}

#ifdef DARWIN
/* ----------------------------------------------------------------- */

int
CheckFinderType(char * file,enum fileactions action,
        char * cf_findertype, struct stat * statbuf)
{
    /* Code modeled after hfstar's extract.c */
    typedef struct t_fndrinfo {
        long fdType;
        long fdCreator;
        short fdFlags;
        short fdLocationV;
        short fdLocationH;
        short fdFldr;
        short fdIconID;
        short fdUnused[3];
        char fdScript;
        char fdXFlags;
        short fdComment;
        long fdPutAway;
    } FInfo;

    struct attrlist attrs;
    struct {
        long ssize;
        struct timespec created;
        struct timespec modified;
        struct timespec changed;
        struct timespec backup;
        FInfo fi;
    } fndrInfo;

    int retval;

    Debug("CheckFinderType of %s for %s\n", file, cf_findertype);

    /* No checking - no action */
    if (strncmp(cf_findertype, "*", CF_BUFSIZE) == 0 ||
            strncmp(cf_findertype, "", CF_BUFSIZE) == 0) {
        Debug("CheckFinderType not needed\n");
        return 0;
    }

    attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrs.reserved = 0;
    attrs.commonattr = ATTR_CMN_CRTIME | ATTR_CMN_MODTIME |
        ATTR_CMN_CHGTIME | ATTR_CMN_BKUPTIME | ATTR_CMN_FNDRINFO;
    attrs.volattr = 0;
    attrs.dirattr = 0;
    attrs.fileattr = 0;
    attrs.forkattr = 0;

    memset(&fndrInfo, 0, sizeof(fndrInfo));

    getattrlist(file, &attrs, &fndrInfo, sizeof(fndrInfo),0);

    /* Need to update Finder Type field */
    if (fndrInfo.fi.fdType != *(long *)cf_findertype) {
        fndrInfo.fi.fdType = *(long *)cf_findertype;

        /* Determine action to take */

        if (S_ISLNK(statbuf->st_mode) || S_ISDIR(statbuf->st_mode)) {
            printf("CheckFinderType: Wrong file type for %s -- "
                    "skipping\n", file);
            return 0;
        }

        if (S_ISREG(statbuf->st_mode) && action == fixdirs) {
            printf("CheckFinderType: Wrong file type for %s -- "
                    "skipping\n", file);
            return 0;
        }

        switch (action) {
        /* Fix it */
        case fixplain:
        case fixdirs:
        case fixall:
        case touch:

            if (g_dontdo) { /* well, unless we shouldn't fix it */
                printf("CheckFinderType: Dry run. No action taken.\n");
                return 0;
            }

            /* setattrlist does not take back in the long ssize */
            retval = setattrlist(file, &attrs, &fndrInfo.created,
                    4*sizeof(struct timespec) + sizeof(FInfo), 0);

            Debug("CheckFinderType setattrlist returned %d\n", retval);

            if (retval >= 0) {
                printf("Setting Finder Type code of %s to %s\n",
                        file, cf_findertype);
            }
            else {
                printf("Setting Finder Type code of %s to %s failed!!\n",
                        file, cf_findertype);
            }

            return retval;

        /* Just warn */
        case linkchildren:
        case warnall:
        case warndirs:
        case warnplain:
            printf("Warning: FinderType does not match -- not fixing.\n");
            return 0;

        default:
            printf("Should never get here. Aroo?\n");
            return 0;
        }
    }

    else {
        Debug("CheckFinderType matched\n");
        return 0;
    }
}


#endif
/* ----------------------------------------------------------------- */

int
CheckOwner(char *file, enum fileactions action, struct UidList *uidlist,
        struct GidList *gidlist, struct stat *statbuf)
{
    struct passwd *pw;
    struct group *gp;
    struct UidList *ulp, *unknownulp;
    struct GidList *glp, *unknownglp;
    short uidmatch = false, gidmatch = false;
    uid_t uid = CF_SAME_OWNER;
    gid_t gid = CF_SAME_GROUP;

    Debug("CheckOwner: %d\n",statbuf->st_uid);

    for (ulp = uidlist; ulp != NULL; ulp=ulp->next) {
        Debug(" uid %d\n",ulp->uid);

        /* means not found while parsing */
        if (ulp->uid == CF_UNKNOWN_OWNER) {

            /* Will only match one */
            unknownulp = MakeUidList (ulp->uidname);
            if (unknownulp != NULL && statbuf->st_uid == unknownulp->uid) {
                uid = unknownulp->uid;
                uidmatch = true;
                break;
            }
        }

        /* "same" matches anything */
        if (ulp->uid == CF_SAME_OWNER || statbuf->st_uid == ulp->uid) {
            uid = ulp->uid;
            uidmatch = true;
            break;
        }
    }

    for (glp = gidlist; glp != NULL; glp=glp->next) {
        /* means not found while parsing */
        if (glp->gid == CF_UNKNOWN_GROUP) {

            /* Will only match one */
            unknownglp = MakeGidList (glp->gidname);
            if (unknownglp != NULL && statbuf->st_gid == unknownglp->gid) {
                gid = unknownglp->gid;
                gidmatch = true;
                break;
            }
        }
        /* "same" matches anything */
        if (glp->gid == CF_SAME_GROUP || statbuf->st_gid == glp->gid) {
            gid = glp->gid;
            gidmatch = true;
            break;
        }
    }


    if (uidmatch && gidmatch) {
        return false;
    } else {
        if (! uidmatch) {
            for (ulp = uidlist; ulp != NULL; ulp=ulp->next) {
                if (uidlist->uid != CF_UNKNOWN_OWNER) {

                    /* default is first (not unknown) item in list */
                    uid = uidlist->uid;
                    break;
                }
            }
        }

        if (! gidmatch) {
            for (glp = gidlist; glp != NULL; glp=glp->next) {
                if (gidlist->gid != CF_UNKNOWN_GROUP) {

                    /* default is first (not unknown) item in list */
                    gid = gidlist->gid;
                    break;
                }
            }
        }

        if (S_ISLNK(statbuf->st_mode) && (action == fixdirs ||
                    action == fixplain)) {
            Debug2("File %s incorrect type (link), skipping...\n",file);
            return false;
        }

        if ((S_ISREG(statbuf->st_mode) && action == fixdirs) ||
                (S_ISDIR(statbuf->st_mode) && action == fixplain)) {
            Debug2("File %s incorrect type, skipping...\n",file);
            return false;
        }

        switch (action) {
        case fixplain:
        case fixdirs:
        case fixall:
        case touch:
            if (g_verbose || g_debug || g_d2) {
                if (uid == CF_SAME_OWNER && gid == CF_SAME_GROUP) {
                    printf("%s:   touching %s\n",g_vprefix,file);
                } else {
                    if (uid != CF_SAME_OWNER) {
                        Debug("(Change owner to uid %d if possible)\n",uid);
                    }

                    if (gid != CF_SAME_GROUP) {
                        Debug("Change group to gid %d if possible)\n",gid);
                    }
                }
            }

            if (! g_dontdo && S_ISLNK(statbuf->st_mode)) {
#ifdef HAVE_LCHOWN
                Debug("Using LCHOWN function\n");
                if (lchown(file,uid,gid) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Cannot set ownership on link %s!\n",file);
                    CfLog(cflogonly,g_output,"lchown");
                } else {
                    return true;
                }
#endif
            } else if (! g_dontdo) {
                if (!uidmatch) {
                    snprintf(g_output,CF_BUFSIZE,
                            "Owner of %s was %d, setting to %d",
                            file,statbuf->st_uid,uid);
                    CfLog(cfinform,g_output,"");
                }

                if (!gidmatch) {
                    snprintf(g_output,CF_BUFSIZE,
                            "Group of %s was %d, setting to %d",
                            file,statbuf->st_gid,gid);
                    CfLog(cfinform,g_output,"");
                }

                if (!S_ISLNK(statbuf->st_mode)) {
                    if (chown(file,uid,gid) == -1) {
                        snprintf(g_output,CF_BUFSIZE*2,
                                "Cannot set ownership on file %s!\n",file);
                        CfLog(cflogonly,g_output,"chown");
                    } else {
                        return true;
                    }
                }
            }
            break;

        case linkchildren:
        case warnall:
        case warndirs:
        case warnplain:
            if ((pw = getpwuid(statbuf->st_uid)) == NULL) {
                snprintf(g_output, CF_BUFSIZE*2, "File %s is not owned "
                        "by anybody in the passwd database\n", file);
                CfLog(cferror,g_output,"");
                snprintf(g_output,CF_BUFSIZE*2,"(uid = %d,gid = %d)\n",
                        statbuf->st_uid,statbuf->st_gid);
                CfLog(cferror,g_output,"");
                break;
            }

            if ((gp = getgrgid(statbuf->st_gid)) == NULL) {
                snprintf(g_output, CF_BUFSIZE*2, "File %s is not owned "
                        "by any group in group database\n", file);
                CfLog(cferror,g_output,"");
                break;
            }

            snprintf(g_output,CF_BUFSIZE*2,
                    "File %s is owned by [%s], group [%s]\n",
                    file,pw->pw_name,gp->gr_name);
            CfLog(cferror,g_output,"");
            break;
        }
    }
    return false;
}


/* ----------------------------------------------------------------- */

int
CheckHomeSubDir(char *testpath, char *tidypath, int recurse)
{
    char *subdirstart, *sp1, *sp2;
    char buffer[CF_BUFSIZE];
    int homelen;

    if (strncmp(tidypath,"home/",5) == 0) {
        strcpy(buffer,testpath);

        for (ChopLastNode(buffer); strlen(buffer) != 0; ChopLastNode(buffer)) {
            if (IsHomeDir(buffer)) {
                break;
            }
        }

        homelen = strlen(buffer);

        /* No homedir */
        if (homelen == 0) {
            return false;
        }

        Debug2("CheckHomeSubDir(%s,%s)\n",testpath,tidypath);

        /* Ptr to start of subdir */
        subdirstart = tidypath + 4;

        strcpy(buffer,testpath);

        /* Filename only */
        ChopLastNode(buffer);

        /* skip user name dir */
        for (sp1 = buffer + homelen +1; *sp1 != '/' && *sp1 != '\0'; sp1++) { }

        sp2 = subdirstart;

        if (strncmp(sp1,sp2,strlen(sp2)) != 0) {
            return false;
        }

        Debug2("CheckHomeSubDir(true)\n");
        return(true);
    }
    return true;
}

/* ----------------------------------------------------------------- */

/* True if file2 is newer than file 1 */
int
FileIsNewer(char *file1, char *file2)
{
    struct stat statbuf1, statbuf2;

    if (stat(file2,&statbuf2) == -1) {
        CfLog(cferror,"","stat");
        return false;
    }

    if (stat(file1,&statbuf1) == -1) {
        CfLog(cferror,"","stat");
        return true;
    }

    if (statbuf1.st_mtime < statbuf2.st_mtime) {
        return true;
    } else {
        return false;
    }
}


/*
 * --------------------------------------------------------------------
 * Used by files and tidy modules
 * --------------------------------------------------------------------
 */

int
IgnoreFile(char *pathto, char *name, struct Item *ignores)
{
    struct Item *ip;

    Debug("IgnoreFile(%s,%s)\n",pathto,name);

    if (name == NULL || strlen(name) == 0) {
        return false;
    }

    strncpy(g_vbuff,pathto,CF_BUFSIZE-1);
    AddSlash(g_vbuff);
    strcat(g_vbuff,name);

    if (ignores != NULL) {
        if (IsWildItemIn(ignores,g_vbuff)) {
            Debug("cfagent: Ignoring private abs path [%s][%s]\n",pathto,name);
            return true;
        }

        if (IsWildItemIn(ignores,name)) {
            Debug("cfagent: Ignoring private pattern [%s][%s]\n",pathto,name);
            return true;
        }
    }

    for (ip = g_vignore; ip != NULL; ip=ip->next) {
        if (IsExcluded(ip->classes)) {
            continue;
        }

        if (*(ip->name) == '/') {
            if (strcmp(g_vbuff,ip->name) == 0) {
                Debug("cfagent: Ignoring global abs path [%s][%s]\n",
                        pathto,name);
                return true;
            }
        } else {
            if (WildMatch(ip->name,name)) {
                Debug("cfagent: Ignoring global pattern [%s][%s]\n",
                        pathto,name);
                return true;
            }
        }
    }
    return false;
}

/* ----------------------------------------------------------------- */

void
CompressFile(char *file)
{
    char comm[CF_BUFSIZE];
    FILE *pp;

    if (strlen(g_compresscommand) == 0) {
        CfLog(cferror,"CompressCommand variable is not defined","");
        return;
    }

    snprintf(comm,CF_BUFSIZE,"%s %s",g_compresscommand,file);

    snprintf(g_output,CF_BUFSIZE*2,"Compressing file %s",file);
    CfLog(cfinform,g_output,"");

    if ((pp=cfpopen(comm,"r")) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"Compression command failed on %s",file);
        CfLog(cfverbose,g_output,"");
    }

    while (!feof(pp)) {
        ReadLine(g_vbuff,CF_BUFSIZE,pp);
        CfLog(cfinform,g_vbuff,"");
    }

    cfpclose(pp);
}
