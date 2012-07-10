/*
 * $Id: image.c 743 2004-05-23 07:27:32Z skaar $
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
 * File Image copying
 */


#include "cf.defs.h"
#include "cf.extern.h"

/* ----------------------------------------------------------------- */
/* Level 1                                                           */
/* ----------------------------------------------------------------- */

/* 
 * Schedule files matching pattern to be collected on next
 * actionsequence -- this function should only be called during parsing
 * since it uses covert variables that belong to the parser
 */

void
GetRemoteMethods(void)
{
    struct Item*ip;
    char client[CF_BUFSIZE];

    Banner("Looking for remote method collaborations");

    for (ip = g_vrpcpeerlist; ip != NULL; ip = ip->next) {
        strncpy(client,ip->name,64);

        if (strstr(ip->name,".")||strstr(ip->name,":")) {

        } else {
            strcat(client,".");
            strcat(client,g_vdomain);
        }

        Verbose(" Hailing remote peer %s\n",client);

        if (strcmp(client,g_vfqname) == 0) {
            /* Do not need to do this to ourselves ..  */
            continue;
        }

        NewParser();
        Verbose(" Hailing remote peer %s for messages for us\n",client);
        snprintf(g_vbuff,CF_BUFSIZE,"%s_*",Hostname2IPString(g_vfqname));

        InitializeAction();

        snprintf(g_destination, CF_BUFSIZE, "%s/rpc_in", g_vlockdir);
        snprintf(g_currentobject, CF_BUFSIZE, "%s/rpc_out", g_vlockdir);
        snprintf(g_findertype,1,"*");

        g_plusmask  = 0400;
        g_minusmask = 0377;
        g_imagebackup = 'n';
        g_encrypt = 'y';
        strcpy(g_imageaction,"fix");
        strcpy(g_classbuff,"any");
        snprintf(g_vuidname,CF_MAXVARSIZE,"%d",getuid());
        snprintf(g_vgidname,CF_MAXVARSIZE,"%d",getgid());
        g_imgcomp = '>';
        g_vrecurse = 1;
        g_pifelapsed = 0;
        g_pexpireafter = 0;
        g_copytype = g_defaultcopytype;

        InstallImageItem(g_findertype, g_currentobject, g_plusmask,
                g_minusmask, g_destination, g_imageaction, g_vuidname,
                g_vgidname, g_imgsize, g_imgcomp, g_vrecurse, 
                g_copytype, g_linktype, client);
        DeleteParser();
    }
    Verbose("\nFinished with RPC\n\n");
}

/* ----------------------------------------------------------------- */

void
RecursiveImage(struct Image *ip, char *from, char *to, int maxrecurse)
{
    struct stat statbuf, deststatbuf;
    char newfrom[CF_BUFSIZE];
    char newto[CF_BUFSIZE];
    int save_uid, save_gid, succeed;
    struct Item *namecache = NULL;
    struct Item *ptr, *ptr1;
    struct cfdirent *dirp;
    CFDIR *dirh;

    /* reached depth limit */
    if (maxrecurse == 0) {
        Debug2("MAXRECURSE ran out, quitting at level %s with endlist = %d\n",
                from,ip->next);
        return;
    }

    Debug2("RecursiveImage(%s,lev=%d,next=%d)\n",from,maxrecurse,ip->next);

    if (IgnoreFile(from,"",ip->ignores)) {
        Verbose("Ignoring directory %s\n",from);
        return;
    }

    /* Check for root dir */
    if (strlen(from) == 0) {
        from = "/";
    }

    /* Check that dest dir exists before starting */

    strncpy(newto,to,CF_BUFSIZE-2);
    AddSlash(newto);

    if (! MakeDirectoriesFor(newto,ip->forcedirs)) {
        snprintf(g_output,CF_BUFSIZE*2,
                "Unable to make directory for %s in copy: %s to %s\n",
                newto,ip->path,ip->destination);
        CfLog(cferror,g_output,"");
        return;
    }

    if ((dirh = cfopendir(from,ip)) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"copy can't open directory [%s]\n",from);
        CfLog(cfverbose,g_output,"");
        return;
    }

    for (dirp = cfreaddir(dirh,ip); dirp != NULL; dirp = cfreaddir(dirh,ip)) {
        if (!SensibleFile(dirp->d_name,from,ip)) {
            continue;
        }

        /* Do not purge this file */
        if (ip->purge == 'y') {
            AppendItem(&namecache,dirp->d_name,NULL);
        }

        if (IgnoreFile(from,dirp->d_name,ip->ignores)) {
            continue;
        }

        /* Assemble pathname */
        strncpy(newfrom,from,CF_BUFSIZE-2);
        AddSlash(newfrom);
        strncpy(newto,to,CF_BUFSIZE-2);
        AddSlash(newto);

        if (BufferOverflow(newfrom,dirp->d_name)) {
            printf(" culprit: RecursiveImage\n");
            cfclosedir(dirh);
            return;
        }

        strncat(newfrom,dirp->d_name,CF_BUFSIZE-2);

        if (BufferOverflow(newto,dirp->d_name)) {
            printf(" culprit: RecursiveImage\n");
            cfclosedir(dirh);
            return;
        }

        strcat(newto,dirp->d_name);

        if (g_travlinks || ip->linktype == 'n') {
            /* 
             * No point in checking if there are untrusted symlinks
             * here, since this is from a trusted source, by defintion 
             */
            if (cfstat(newfrom,&statbuf,ip) == -1) {
                Verbose("%s: (Can't stat %s)\n",g_vprefix,newfrom);
                continue;
            }
        } else {
            if (cflstat(newfrom,&statbuf,ip) == -1) {
                Verbose("%s: (Can't stat %s)\n",g_vprefix,newfrom);
                continue;
            }
        }

        if ((ip->xdev =='y') && DeviceChanged(statbuf.st_dev)) {
            Verbose("Skipping %s on different device\n",newfrom);
            continue;
        }

        if (!FileObjectFilter(newfrom,&statbuf,ip->filters,image)) {
            continue;
        }

        if (!S_ISDIR(statbuf.st_mode) && IgnoredOrExcluded(image,
                    dirp->d_name,ip->inclusions,ip->exclusions)) {
            continue;
        }

        if (!S_ISDIR(statbuf.st_mode)) {
            succeed = 0;
            for (ptr = g_vexcludecache; ptr != NULL; ptr=ptr->next) {

                if ((strncmp(ptr->name,newto,strlen(newto)+1) == 0)
                        && (strncmp(ptr->classes,ip->classes,
                                strlen(ip->classes)+1) == 0)) {

                    succeed = 1;
                }
            }

            if (succeed) {
                snprintf(g_output,CF_BUFSIZE*2,
                        "Skipping excluded file %s class %s\n",
                        newto,ip->classes);
                CfLog(cfverbose,g_output,"");
                continue;
            } else {
                Debug2("file %s class %s was not excluded\n",
                        newto,ip->classes);
            }
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (g_travlinks || ip->linktype == 'n') {
                CfLog(cfverbose, "Traversing directory links during "
                        "copy is too dangerous, pruned","");
                continue;
            }

            memset(&deststatbuf,0,sizeof(struct stat));
            save_uid = (ip->uid)->uid;
            save_gid = (ip->gid)->gid;

            /* Preserve uid and gid  */
            if ((ip->uid)->uid == (uid_t)-1) {
                (ip->uid)->uid = statbuf.st_uid;
            }

            if ((ip->gid)->gid == (gid_t)-1) {
                (ip->gid)->gid = statbuf.st_gid;
            }

            if (stat(newto,&deststatbuf) == -1) {
                mkdir(newto,statbuf.st_mode);
                if (stat(newto,&deststatbuf) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,"Can't stat %s\n",newto);
                    CfLog(cferror,g_output,"stat");
                    continue;
                }
            }

            CheckCopiedFile(ip->cf_findertype,newto,ip->plus,ip->minus,
                    fixall,ip->uid,ip->gid,&deststatbuf,&statbuf,
                    NULL,ip->acl_aliases);

            (ip->uid)->uid = save_uid;
            (ip->gid)->gid = save_gid;

            Verbose("Syncronizing %s -> %s\n",newfrom,newto);
            RecursiveImage(ip,newfrom,newto,maxrecurse-1);
        } else {
            CheckImage(newfrom,newto,ip);
        }
    }

    if (ip->purge == 'y') {

        /* 
         * inclusions not exclusions, since exclude from purge means
         * include
         */
        PurgeFiles(namecache,to,ip->inclusions);

        DeleteItemList(namecache);
    }

    DeleteCompressedArray(ip->inode_cache);

    ip->inode_cache = NULL;

    cfclosedir(dirh);
}

/* ----------------------------------------------------------------- */

void
CheckHomeImages(struct Image *ip)
{
    DIR *dirh, *dirh2;
    struct dirent *dirp, *dirp2;
    char username[CF_MAXVARSIZE];
    char homedir[CF_BUFSIZE],dest[CF_BUFSIZE];
    struct passwd *pw;
    struct group *gr;
    struct stat statbuf;
    struct Item *itp;
    int request_uid = ip->uid->uid;  /* save if -1 */
    int request_gid = ip->gid->gid;  /* save if -1 */

    if (!MountPathDefined()) {
        printf("%s:  mountpattern is undefined\n",g_vprefix);
        return;
    }

    if (cfstat(ip->path,&statbuf,ip) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,
                "Master file %s doesn't exist for copying\n",ip->path);
        CfLog(cferror,g_output,"");
        return;
    }

    for (itp = g_vmountlist; itp != NULL; itp=itp->next) {
        if (IsExcluded(itp->classes)) {
            continue;
        }

    if ((dirh = opendir(itp->name)) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't open directory %s\n",itp->name);
        CfLog(cfverbose,g_output,"opendir");
        return;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name,itp->name,NULL)) {
            continue;
        }

        strcpy(homedir,itp->name);
        AddSlash(homedir);
        strcat(homedir,dirp->d_name);

        if (! IsHomeDir(homedir)) {
            continue;
        }

        if ((dirh2 = opendir(homedir)) == NULL) {
            snprintf(g_output,CF_BUFSIZE*2,"Can't open directory %s\n",homedir);
            CfLog(cfverbose,g_output,"opendir");
            return;
        }

        for (dirp2 = readdir(dirh2); dirp2 != NULL; dirp2 = readdir(dirh2)) {
            if (!SensibleFile(dirp2->d_name,homedir,NULL)) {
                continue;
            }

            strcpy(username,dirp2->d_name);
            strcpy(dest,homedir);
            AddSlash(dest);
            strcat(dest,dirp2->d_name);

            if (strlen(ip->destination) > 4) {
                AddSlash(dest);

                if (strlen(ip->destination) < 6) {
                    snprintf(g_output,CF_BUFSIZE*2,"Error in home/copy to %s",
                            ip->destination);
                    CfLog(cferror,g_output,"");
                    return;
                } else {
                    strcat(dest,(ip->destination)+strlen("home/"));
                }
            }

            if (request_uid == -1) {
                if ((pw = getpwnam(username)) == NULL) {
                    Debug2("cfagent: directory corresponds to "
                            "no user %s - ignoring\n", username);
                    continue;
                } else {
                    Debug2("(Setting user id to %s)\n",pw->pw_name);
                }

                ip->uid->uid = pw->pw_uid;
            }

            if (request_gid == -1) {
                if ((pw = getpwnam(username)) == NULL) {
                    Debug2("cfagent: directory corresponds to "
                            "no user %s - ignoring\n", username);
                    continue;
                }

                if ((gr = getgrgid(pw->pw_gid)) == NULL) {
                    Debug2("cfagent: no group defined for group "
                            "id %d - ignoring\n",pw->pw_gid);
                    continue;
                } else {
                    Debug2("(Setting group id to %s)\n",gr->gr_name);
                }

                ip->gid->gid = gr->gr_gid;
            }
            CheckImage(ip->path,dest,ip);
         }
      closedir(dirh2);
      }
   closedir(dirh);
   }
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

void
CheckImage(char *source, char *destination, struct Image *ip)
{
    CFDIR *dirh;
    char sourcefile[CF_BUFSIZE];
    char sourcedir[CF_BUFSIZE];
    char destdir[CF_BUFSIZE];
    char destfile[CF_BUFSIZE];
    struct stat sourcestatbuf, deststatbuf;
    struct cfdirent *dirp;
    int save_uid, save_gid, found;

    Debug2("CheckImage (source=%s destination=%s)\n",source,destination);

    if (ip->linktype == 'n') {
        found = cfstat(source,&sourcestatbuf,ip);
    } else {
        found = cflstat(source,&sourcestatbuf,ip);
    }

    if (found == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't stat %s\n",source);
        CfLog(cferror,g_output,"");
        FlushClientCache(ip);
        return;
    }

    /* Preserve hard link structure when copying */
    if (sourcestatbuf.st_nlink > 1) {
        RegisterHardLink(sourcestatbuf.st_ino,destination,ip);
    }

    save_uid = (ip->uid)->uid;
    save_gid = (ip->gid)->gid;

    /* Preserve uid and gid  */
    if ((ip->uid)->uid == (uid_t)-1) {
        (ip->uid)->uid = sourcestatbuf.st_uid;
    }

    if ((ip->gid)->gid == (gid_t)-1) {
        (ip->gid)->gid = sourcestatbuf.st_gid;
    }

    if (S_ISDIR(sourcestatbuf.st_mode)) {
        strcpy(sourcedir,source);
        AddSlash(sourcedir);
        strcpy(destdir,destination);
        AddSlash(destdir);

        if ((dirh = cfopendir(sourcedir,ip)) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Can't open directory %s\n", sourcedir);
            CfLog(cfverbose,g_output,"opendir");
            FlushClientCache(ip);
            (ip->uid)->uid = save_uid;
            (ip->gid)->gid = save_gid;
            return;
        }

        /* Now check any overrides */

        if (stat(destdir,&deststatbuf) == -1) {
            snprintf(g_output,CF_BUFSIZE*2,"Can't stat directory %s\n",destdir);
            CfLog(cferror,g_output,"stat");
        } else {
            CheckCopiedFile(ip->cf_findertype,destdir,ip->plus,ip->minus,
                    fixall,ip->uid,ip->gid,&deststatbuf,&sourcestatbuf,NULL,
                    ip->acl_aliases);
        }

        for (dirp = cfreaddir(dirh,ip); dirp != NULL;
                dirp = cfreaddir(dirh,ip)) {

            if (!SensibleFile(dirp->d_name,sourcedir,ip)) {
                continue;
            }

            strcpy(sourcefile, sourcedir);

            if (BufferOverflow(sourcefile,dirp->d_name)) {
                FatalError("Culprit: CheckImage");
            }

            strcat(sourcefile, dirp->d_name);
            strcpy(destfile, destdir);

            if (BufferOverflow(destfile,dirp->d_name)) {
                FatalError("Culprit: CheckImage");
            }

            strcat(destfile, dirp->d_name);

            if (cflstat(sourcefile,&sourcestatbuf,ip) == -1) {
                printf("%s: Can't stat %s\n",g_vprefix,sourcefile);
                FlushClientCache(ip);
                (ip->uid)->uid = save_uid;
                (ip->gid)->gid = save_gid;
                return;
            }

            ImageCopy(sourcefile,destfile,sourcestatbuf,ip);
        }

        cfclosedir(dirh);
        FlushClientCache(ip);
        (ip->uid)->uid = save_uid;
        (ip->gid)->gid = save_gid;
        return;
    }

    strcpy(sourcefile,source);
    strcpy(destfile,destination);

    ImageCopy(sourcefile,destfile,sourcestatbuf,ip);
    (ip->uid)->uid = save_uid;
    (ip->gid)->gid = save_gid;
    FlushClientCache(ip);
}

/* ----------------------------------------------------------------- */

void
PurgeFiles(struct Item *filelist, char *directory, struct Item *inclusions)
{
    DIR *dirh;
    struct stat statbuf;
    struct dirent *dirp;
    char filename[CF_BUFSIZE];

    Debug("PurgeFiles(%s)\n",directory);

    /* If we purge with no authentication we wipe out EVERYTHING */

    if (strlen(directory) < 2) {
        snprintf(g_output,CF_BUFSIZE,
                "Purge of %s denied -- too dangerous!",directory);
        CfLog(cferror,g_output,"");
        return;
    }

    if (!g_authenticated) {
        Verbose("Not purging %s - no verified contact with server\n",directory);
        return;
    }

    if ((dirh = opendir(directory)) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't open directory %s\n",directory);
        CfLog(cfverbose,g_output,"cfopendir");
        return;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name,directory,NULL)) {
            continue;
        }

        if (! IsItemIn(filelist,dirp->d_name)) {

            strncpy(filename,directory,CF_BUFSIZE-2);
            AddSlash(filename);
            strncat(filename,dirp->d_name,CF_BUFSIZE-2);

            Debug("Checking purge %s..\n",filename);

            if (g_dontdo) {
                printf("Need to purge %s from copy dest directory\n",filename);
            } else {
                snprintf(g_output,CF_BUFSIZE*2,
                        "Purging %s in copy dest directory\n",filename);
                CfLog(cfinform,g_output,"");

                if (lstat(filename,&statbuf) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Couldn't stat %s while purging\n",filename);
                    CfLog(cfverbose,g_output,"stat");
                } else if (S_ISDIR(statbuf.st_mode)) {
                    struct Tidy tp;
                    struct TidyPattern tpat;

                    tp.maxrecurse = 2;
                    tp.done = 'n';
                    tp.tidylist = &tpat;
                    tp.next = NULL;
                    tp.path = filename;

                    /* exclude means don't purge, i.e. include here */
                    tp.exclusions = inclusions;

                    tp.ignores = NULL;

                    tpat.filters = NULL;
                    tpat.recurse = CF_INF_RECURSE;
                    tpat.age = 0;
                    tpat.size = 0;
                    tpat.pattern = strdup("*");
                    tpat.classes = strdup("any");
                    tpat.defines = NULL;
                    tpat.elsedef = NULL;
                    tpat.dirlinks = 'y';
                    tpat.travlinks = 'n';
                    tpat.rmdirs = 'y';
                    tpat.searchtype = 'a';
                    tpat.log = 'd';
                    tpat.inform = 'd';
                    tpat.next = NULL;
                    RecursiveTidySpecialArea(filename,&tp,
                            CF_INF_RECURSE,&statbuf);
                    free(tpat.pattern);
                    free(tpat.classes);

                    chdir("..");

                    if (rmdir(filename) == -1) {
                        snprintf(g_output,CF_BUFSIZE*2,
                                "Couldn't remove directory %s while purging\n",
                                filename);
                        CfLog(cfverbose,g_output,"rmdir");
                    }

                    continue;
                } else if (unlink(filename) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Couldn't unlink %s while purging\n",filename);
                    CfLog(cfverbose,g_output,"");
                }
            }
        }
    }
    closedir(dirh);
}


/*
 * --------------------------------------------------------------------
 * Level 3
 * --------------------------------------------------------------------
 */

void
ImageCopy(char *sourcefile,char *destfile,
        struct stat sourcestatbuf, struct Image *ip)
{
    char linkbuf[CF_BUFSIZE], *lastnode;
    struct stat deststatbuf;
    struct Link empty;
    struct Item *ptr, *ptr1;
    int succeed, silent = false, enforcelinks;
    mode_t srcmode = sourcestatbuf.st_mode;
    int ok_to_copy = false, found;

    Debug2("ImageCopy(%s,%s,+%o,-%o)\n",
            sourcefile,destfile,ip->plus,ip->minus);

    if ((strcmp(sourcefile,destfile) == 0) &&
            (strcmp(ip->server,"localhost") == 0)) {

        snprintf(g_output,CF_BUFSIZE*2,
                "Image loop: file/dir %s copies to itself",sourcefile);

        CfLog(cfinform,g_output,"");
        return;
    }

    empty.defines = NULL;
    empty.elsedef = NULL;

    if (IgnoredOrExcluded(image,sourcefile,ip->inclusions,ip->exclusions)) {
        return;
    }

    succeed = 0;

    for (ptr = g_vexcludecache; ptr != NULL; ptr=ptr->next) {

        if ((strncmp(ptr->name,destfile,strlen(destfile)+1) == 0) &&
                (strncmp(ptr->classes,ip->classes,
                         strlen(ip->classes)+1) == 0)) {

            succeed = 1;
        }
    }

    if (succeed) {
        snprintf(g_output,CF_BUFSIZE*2,"Skipping excluded file %s class %s\n",
                destfile,ip->classes);
        CfLog(cfverbose,g_output,"");
        return;
    } else {
        Debug2("file %s class %s was not excluded\n",destfile,ip->classes);
    }


    if (ip->linktype != 'n') {
        lastnode=ReadLastNode(sourcefile);

        if (IsWildItemIn(g_vlinkcopies,lastnode) ||
                IsWildItemIn(ip->symlink,lastnode)) {

            Verbose("cfagent: copy item %s marked for linking instead\n",
                    sourcefile);
            enforcelinks = g_enforcelinks;
            g_enforcelinks = true;

            switch (ip->linktype) {
            case 's':
                succeed = LinkFiles(destfile,sourcefile,
                        NULL,NULL,NULL,true,&empty);
                break;
            case 'r':
                succeed = RelativeLink(destfile,sourcefile,
                        NULL,NULL,NULL,true,&empty);
                break;
            case 'a':
                succeed = AbsoluteLink(destfile,sourcefile,
                        NULL,NULL,NULL,true,&empty);
                break;
            default:
                printf("%s: internal error, link type was [%c] in ImageCopy\n",
                        g_vprefix,ip->linktype);
                return;
            }

            if (succeed) {
                g_enforcelinks = enforcelinks;

                if (lstat(destfile,&deststatbuf) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,"Can't lstat %s\n",destfile);
                    CfLog(cferror,g_output,"lstat");
                } else {
                    CheckCopiedFile(ip->cf_findertype,destfile,ip->plus,
                            ip->minus,fixall,ip->uid,ip->gid,&deststatbuf,
                            &sourcestatbuf,NULL,ip->acl_aliases);
                }
            }
            return;
        }
    }

    if (strcmp(ip->action,"silent") == 0) {
        silent = true;
    }

    memset(linkbuf,0,CF_BUFSIZE);

    found = lstat(destfile,&deststatbuf);

    if (found != -1) {

        if ((S_ISLNK(deststatbuf.st_mode) && (ip->linktype == 'n')) ||
                (S_ISLNK(deststatbuf.st_mode)  &&
                 ! S_ISLNK(sourcestatbuf.st_mode))) {

            if (!S_ISLNK(sourcestatbuf.st_mode) && (ip->typecheck == 'y')) {

                printf("%s: image exists but destination type is "
                        "silly (file/dir/link doesn't match)\n", g_vprefix);
                printf("%s: source=%s, dest=%s\n",
                        g_vprefix,sourcefile,destfile);
                return;
            }
            if (g_dontdo) {

                Verbose("Need to remove old symbolic link %s to "
                        "make way for copy\n", destfile);

            } else {
                if (unlink(destfile) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Couldn't remove link %s",destfile);
                    CfLog(cferror,g_output,"unlink");
                    return;
                }
                Verbose("Removing old symbolic link %s to make way for copy\n",
                        destfile);
                found = -1;
            }
        }
    }

    if (ip->size != CF_NOSIZE) {
        switch (ip->comp) {
        case '<':
            if (sourcestatbuf.st_size > ip->size) {
                snprintf(g_output,CF_BUFSIZE*2,
                        "Source file %s is > %d bytes in copy (omitting)\n",
                        sourcefile,ip->size);
                CfLog(cfinform,g_output,"");
                return;
            }
            break;

        case '=':
            if (sourcestatbuf.st_size != ip->size) {
                snprintf(g_output,CF_BUFSIZE*2,
                        "Source file %s is not %d bytes in copy (omitting)\n",
                        sourcefile,ip->size);
                CfLog(cfinform,g_output,"");
                return;
            }
            break;

        default:
            if (sourcestatbuf.st_size < ip->size) {
                snprintf(g_output,CF_BUFSIZE*2,
                        "Source file %s is < %d bytes in copy (omitting)\n",
                        sourcefile,ip->size);
                CfLog(cfinform,g_output,"");
                return;
            }
            break;;
        }
    }


    if (found == -1) {
        if (strcmp(ip->action,"warn") == 0) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "Image file %s is non-existent\n",destfile);
            CfLog(cfinform,g_output,"");
            snprintf(g_output, CF_BUFSIZE*2, 
                    "(should be copy of %s)\n",sourcefile);
            CfLog(cfinform,g_output,"");
            return;
        }

        if (S_ISREG(srcmode)) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "%s wasn't at destination (copying)",destfile);
            if (g_dontdo) {
                printf("Need this: %s\n",g_output);
                return;
            }

            CfLog(cfverbose,g_output,"");
            snprintf(g_output,CF_BUFSIZE*2,
                    "Copying from %s:%s\n",ip->server,sourcefile);
            CfLog(cfinform,g_output,"");

            if (CopyReg(sourcefile,destfile,sourcestatbuf,deststatbuf,ip)) {
                if (stat(destfile,&deststatbuf) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,"Can't stat %s\n",destfile);
                    CfLog(cferror,g_output,"stat");
                } else {
                    CheckCopiedFile(ip->cf_findertype,destfile,
                            ip->plus,ip->minus,fixall,
                            ip->uid,ip->gid,&deststatbuf,&sourcestatbuf,
                            NULL,ip->acl_aliases);
                }

                AddMultipleClasses(ip->defines);

                for (ptr = g_vautodefine; ptr != NULL; ptr=ptr->next) {
                    if (strncmp(ptr->name,destfile,strlen(destfile)+1) == 0) {

                        snprintf(g_output,CF_BUFSIZE*2,
                                "cfagent: image %s was set to autodefine %s\n",
                                ptr->name,ptr->classes);

                        CfLog(cfinform,g_output,"");
                        AddMultipleClasses(ptr->classes);
                    }
                }

                if (g_vsinglecopy != NULL) {
                    succeed = 1;
                } else {
                    succeed = 0;
                }

                for (ptr = g_vsinglecopy; ptr != NULL; ptr=ptr->next) {
                    if ((strcmp(ptr->name,"on") != 0) &&
                            (strcmp(ptr->name,"true") != 0)) {
                        continue;
                    }

                    if (strncmp(ptr->classes,ip->classes,
                                strlen(ip->classes)+1) == 0) {

                        for (ptr1 = g_vexcludecache; ptr1 != NULL;
                                ptr1=ptr1->next) {

                            if ((strncmp(ptr1->name,destfile,
                                        strlen(destfile)+1) == 0) &&
                                    (strncmp(ptr1->classes,ip->classes,
                                        strlen(ip->classes)+1) == 0)) {

                                succeed = 0;
                            }
                        }
                    }
                }

                if (succeed) {
                    Debug("Appending image %s class %s to singlecopy list\n",
                            destfile,ip->classes);
                    AppendItem(&g_vexcludecache,destfile,ip->classes);
                }
            } else {
                AddMultipleClasses(ip->failover);
            }

            Debug2("Leaving ImageCopy\n");
            return;
        }

        if (S_ISFIFO (srcmode)) {
#ifdef HAVE_MKFIFO
            if (g_dontdo) {
                Silent("%s: Make FIFO %s\n",g_vprefix,destfile);
            } else if (mkfifo (destfile,srcmode)) {
                snprintf(g_output,CF_BUFSIZE*2,"Cannot create fifo `%s'", destfile);
                CfLog(cferror,g_output,"mkfifo");
                    return;
            }

            AddMultipleClasses(ip->defines);
#endif
        } else {
            if (S_ISBLK (srcmode) || S_ISCHR (srcmode) || S_ISSOCK (srcmode)) {
                if (g_dontdo) {
                    Silent("%s: Make BLK/CHR/SOCK %s\n",g_vprefix,destfile);
                } else if (mknod (destfile, srcmode, sourcestatbuf.st_rdev)) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Cannot create special file `%s'",destfile);
                    CfLog(cferror,g_output,"mknod");
                    return;
                }

                AddMultipleClasses(ip->defines);
            }
        }

    if (S_ISLNK(srcmode)) {
            if (cfreadlink(sourcefile,linkbuf,CF_BUFSIZE,ip) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Can't readlink %s\n",sourcefile);
                CfLog(cferror,g_output,"");
                Debug2("Leaving ImageCopy\n");
                return;
            }

            snprintf(g_output,CF_BUFSIZE*2,"Checking link from %s to %s\n",
                    destfile,linkbuf);
            CfLog(cfverbose,g_output,"");

            /* Not absolute path - must fix */
            if (ip->linktype == 'a' && linkbuf[0] != '/') {
                strcpy(g_vbuff,sourcefile);
                ChopLastNode(g_vbuff);
                AddSlash(g_vbuff);
                strncat(g_vbuff,linkbuf,CF_BUFSIZE-1);
                strncpy(linkbuf,g_vbuff,CF_BUFSIZE-1);
            }

            switch (ip->linktype) {
            case 's':
                if (*linkbuf == '.') {
                    succeed = RelativeLink(destfile,linkbuf,
                            NULL,NULL,NULL,true,&empty);
                } else {
                    succeed = LinkFiles(destfile,linkbuf,
                            NULL,NULL,NULL,true,&empty);
                }
                break;
            case 'r':
                succeed = RelativeLink(destfile,linkbuf,
                        NULL,NULL,NULL,true,&empty);
                break;
            case 'a':
                succeed = AbsoluteLink(destfile,linkbuf,
                        NULL,NULL,NULL,true,&empty);
                break;
            default:
                printf("cfagent: internal error, link type was [%c] "
                        "in ImageCopy\n", ip->linktype);
                return;
            }

            if (succeed) {
                if (lstat(destfile,&deststatbuf) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,"Can't lstat %s\n",destfile);
                    CfLog(cferror,g_output,"lstat");
                } else {
                    CheckCopiedFile(ip->cf_findertype,destfile,
                            ip->plus,ip->minus,fixall,ip->uid,ip->gid,
                            &deststatbuf,&sourcestatbuf,NULL,ip->acl_aliases);
                }
                AddMultipleClasses(ip->defines);
            }
        }
    } else {
        Debug("Destination file %s exists\n",destfile);

        if (ip->force == 'n') {
            switch (ip->type) {
            case 'c':
                if (S_ISREG(deststatbuf.st_mode) && S_ISREG(srcmode)) {
                    ok_to_copy = CompareCheckSums(sourcefile,destfile,
                            ip,&sourcestatbuf,&deststatbuf);
                } else {

                    CfLog(cfverbose,"Checksum comparison replaced "
                            "by ctime: files not regular\n","");

                    snprintf(g_output,CF_BUFSIZE*2,"%s -> %s\n",
                            sourcefile,destfile);
                    CfLog(cfinform,g_output,"");

                    ok_to_copy = (deststatbuf.st_ctime 
                            < sourcestatbuf.st_ctime) 
                        || (deststatbuf.st_mtime < sourcestatbuf.st_mtime);

                }

                if (ok_to_copy && strcmp(ip->action,"warn") == 0) {

                    snprintf(g_output, CF_BUFSIZE*2,
                            "Image file %s has a wrong MD5 checksum "
                            "(should be copy of %s)\n", destfile, sourcefile);

                    CfLog(cferror,g_output,"");
                    return;
                }
                break;

            case 'b':
                if (S_ISREG(deststatbuf.st_mode) && S_ISREG(srcmode)) {
                    ok_to_copy = CompareBinarySums(sourcefile,destfile,
                            ip,&sourcestatbuf,&deststatbuf);
                } else {
                    CfLog(cfinform, "Byte comparison replaced by ctime: "
                            "files not regular\n","");
                    snprintf(g_output, CF_BUFSIZE*2, "%s -> %s\n",
                            sourcefile, destfile);
                    CfLog(cfverbose,g_output,"");

                    ok_to_copy = (deststatbuf.st_ctime 
                            < sourcestatbuf.st_ctime)
                        ||(deststatbuf.st_mtime < sourcestatbuf.st_mtime);

                }

                if (ok_to_copy && strcmp(ip->action,"warn") == 0) {

                    snprintf(g_output, CF_BUFSIZE*2, 
                            "Image file %s has a wrong binary checksum "
                            "(should be copy of %s)\n", destfile, sourcefile);

                    CfLog(cferror,g_output,"");
                    return;
                }
                break;

            case 'm':
                ok_to_copy = (deststatbuf.st_mtime < sourcestatbuf.st_mtime);

                if (ok_to_copy && strcmp(ip->action,"warn") == 0) {

                    snprintf(g_output, CF_BUFSIZE*2, 
                           "Image file %s out of date "
                           "(should be copy of %s)\n",destfile,sourcefile);

                    CfLog(cferror,g_output,"");
                    return;
                }
                break;

            default:
                ok_to_copy = (deststatbuf.st_ctime < sourcestatbuf.st_ctime)
                    || (deststatbuf.st_mtime < sourcestatbuf.st_mtime);

                if (ok_to_copy && strcmp(ip->action,"warn") == 0) {

                    snprintf(g_output, CF_BUFSIZE*2,
                            "Image file %s out of date "
                            "(should be copy of %s)\n", destfile, sourcefile);

                    CfLog(cferror,g_output,"");
                    return;
                }
                break;
            }
        }


        if (ip->typecheck == 'y') {
            if ((S_ISDIR(deststatbuf.st_mode)  &&
                        ! S_ISDIR(sourcestatbuf.st_mode))  ||
                    (S_ISREG(deststatbuf.st_mode)  &&
                        ! S_ISREG(sourcestatbuf.st_mode))  ||
                    (S_ISBLK(deststatbuf.st_mode)  &&
                        ! S_ISBLK(sourcestatbuf.st_mode))  ||
                    (S_ISCHR(deststatbuf.st_mode)  &&
                        ! S_ISCHR(sourcestatbuf.st_mode))  ||
                    (S_ISSOCK(deststatbuf.st_mode) &&
                        ! S_ISSOCK(sourcestatbuf.st_mode)) ||
                    (S_ISFIFO(deststatbuf.st_mode) &&
                        ! S_ISFIFO(sourcestatbuf.st_mode)) ||
                    (S_ISLNK(deststatbuf.st_mode)  &&
                     ! S_ISLNK(sourcestatbuf.st_mode))) {

                printf("%s: image exists but destination type is silly "
                        "(file/dir/link doesn't match)\n",g_vprefix);
                printf("%s: source=%s, dest=%s\n",
                        g_vprefix, sourcefile, destfile);
                return;
            }
        }

        /* Always check links */
        if ((ip->force == 'y') || ok_to_copy ||
                S_ISLNK(sourcestatbuf.st_mode)) {

            if (S_ISREG(srcmode)) {
                snprintf(g_output,CF_BUFSIZE*2,
                        "Update of image %s from master %s on %s",
                        destfile,sourcefile,ip->server);

                if (g_dontdo) {
                    printf("Need: %s\n",g_output);
                    return;
                }

                CfLog(cfinform,g_output,"");

                AddMultipleClasses(ip->defines);

                for (ptr = g_vautodefine; ptr != NULL; ptr=ptr->next) {
                    if (strncmp(ptr->name,destfile,strlen(destfile)+1) == 0) {
                        snprintf(g_output,CF_BUFSIZE*2,
                                "cfagent: image %s was set to autodefine %s\n",
                                ptr->name,ptr->classes);
                        CfLog(cfinform,g_output,"");
                        AddMultipleClasses(ptr->classes);
                    }
                }

                if (CopyReg(sourcefile,destfile,sourcestatbuf,deststatbuf,ip)) {
                    if (stat(destfile,&deststatbuf) == -1) {
                        snprintf(g_output, CF_BUFSIZE*2, 
                                "Can't stat %s\n", destfile);
                        CfLog(cferror,g_output,"stat");
                    } else {
                        CheckCopiedFile(ip->cf_findertype,destfile,
                                ip->plus,ip->minus,fixall,ip->uid,ip->gid,
                                &deststatbuf,&sourcestatbuf,NULL,
                                ip->acl_aliases);
                    }

                    if (g_vsinglecopy != NULL) {
                        succeed = 1;
                    } else {
                        succeed = 0;
                    }

                    for (ptr = g_vsinglecopy; ptr != NULL; ptr=ptr->next) {
                        if (strncmp(ptr->classes,ip->classes,
                                    strlen(ip->classes)+1) == 0) {

                            for (ptr1 = g_vexcludecache; ptr1 != NULL;
                                    ptr1=ptr1->next) {

                                if ((strncmp(ptr1->name,destfile,
                                            strlen(destfile)+1) == 0) &&
                                        (strncmp(ptr1->classes,ip->classes,
                                            strlen(ip->classes)+1) == 0)) {

                                    succeed = 0;
                                }
                            }
                        }
                    }

                    if (succeed) {
                        AppendItem(&g_vexcludecache,destfile,ip->classes);
                    }
                } else {
                    AddMultipleClasses(ip->failover);
                }
                return;
            }

            if (S_ISLNK(sourcestatbuf.st_mode)) {
                if (cfreadlink(sourcefile,linkbuf,CF_BUFSIZE,ip) == -1) {
                    snprintf(g_output, CF_BUFSIZE*2,
                            "Can't readlink %s\n", sourcefile);
                    CfLog(cferror,g_output,"");
                    return;
                }

                snprintf(g_output,CF_BUFSIZE*2,"Checking link from %s to %s\n",
                        destfile,linkbuf);

                CfLog(cfverbose,g_output,"");

                enforcelinks = g_enforcelinks;
                g_enforcelinks = true;

                switch (ip->linktype) {
                case 's':
                    if (*linkbuf == '.') {
                        succeed = RelativeLink(destfile,linkbuf,
                                NULL,NULL,NULL,true,&empty);
                    } else {
                        succeed = LinkFiles(destfile,linkbuf,
                                NULL,NULL,NULL,true,&empty);
                    }
                    break;
                case 'r':
                    succeed = RelativeLink(destfile,linkbuf,
                            NULL,NULL,NULL,true,&empty);
                    break;
                case 'a':
                    succeed = AbsoluteLink(destfile,linkbuf,
                            NULL,NULL,NULL,true,&empty);
                    break;
                default:
                    printf("cfagent: internal error, link type was [%c] "
                            "in ImageCopy\n", ip->linktype);
                    return;
                }

                if (succeed) {
                    CheckCopiedFile(ip->cf_findertype,destfile,
                            ip->plus,ip->minus,fixall,ip->uid,ip->gid,
                            &deststatbuf,&sourcestatbuf,NULL,ip->acl_aliases);
                }
                g_enforcelinks = enforcelinks;
            }
        } else {
            CheckCopiedFile(ip->cf_findertype,destfile,ip->plus,
                    ip->minus,fixall,ip->uid,ip->gid,&deststatbuf,
                    &sourcestatbuf,NULL,ip->acl_aliases);

            if (g_vsinglecopy != NULL) {
                succeed = 1;
            } else {
                succeed = 0;
            }

            for (ptr = g_vsinglecopy; ptr != NULL; ptr=ptr->next) {

                if (strncmp(ptr->classes,ip->classes,
                            strlen(ip->classes)+1) == 0) {

                    for (ptr1 = g_vexcludecache; ptr1 != NULL; ptr1=ptr1->next) {

                        if ((strncmp(ptr1->name,destfile,
                                    strlen(destfile)+1) == 0)
                                && (strncmp(ptr1->classes,ip->classes,
                                    strlen(ip->classes)+1) == 0)) {

                            succeed = 0;
                        }
                    }
                }
            }

            if (succeed) {
                Debug("Appending image %s class %s to singlecopy list\n",
                        destfile,ip->classes);
                AppendItem(&g_vexcludecache,destfile,ip->classes);
            }

            Debug("Image file is up to date: %s\n",destfile);
            AddMultipleClasses(ip->elsedef);
        }
    }
}

/* ----------------------------------------------------------------- */

/* wrapper for network access */
int
cfstat(char *file, struct stat *buf, struct Image *ip)
{
    int res;

    if (ip == NULL) {
        return stat(file,buf);
    }

    if (strcmp(ip->server,"localhost") == 0) {
        res = stat(file,buf);
        CheckForHoles(buf,ip);
        return res;
    } else {
        return cf_rstat(file,buf,ip,"file");
    }
}

/* ----------------------------------------------------------------- */

/* wrapper for network access */
int
cflstat(char *file, struct stat *buf, struct Image *ip)
{
    int res;

    if (ip == NULL) {
        return lstat(file,buf);
    }

    if (strcmp(ip->server,"localhost") == 0) {
        res = lstat(file,buf);
        CheckForHoles(buf,ip);
        return res;
    } else {
        /* read cache if possible */
        return cf_rstat(file,buf,ip,"link");
    }
}

/* ----------------------------------------------------------------- */

/* wrapper for network access */

int
cfreadlink(char *sourcefile, char *linkbuf, int buffsize, struct Image *ip)
{
    struct cfstat *sp;

    memset(linkbuf,0,buffsize);

    if (strcmp(ip->server,"localhost") == 0) {
        return readlink(sourcefile,linkbuf,buffsize-1);
    }

    for (sp = ip->cache; sp != NULL; sp=sp->next) {
        if ((strcmp(ip->server,sp->cf_server) == 0) &&
                (strcmp(sourcefile,sp->cf_filename) == 0)) {
            if (sp->cf_readlink != NULL) {
                if (strlen(sp->cf_readlink)+1 > buffsize) {
                    printf("%s: readlink value is too large in cfreadlink\n",
                            g_vprefix);
                    printf("%s: [%s]]n",g_vprefix,sp->cf_readlink);
                    return -1;
                } else {
                    memset(linkbuf,0,buffsize);
                    strcpy(linkbuf,sp->cf_readlink);
                    return 0;
                }
            }
        }
    }
    return -1;
}

/* ----------------------------------------------------------------- */

CFDIR *
cfopendir(char *name, struct Image *ip)
{
    CFDIR *cf_ropendir(),*returnval;

    if (strcmp(ip->server,"localhost") == 0) {
        if ((returnval = (CFDIR *)malloc(sizeof(CFDIR))) == NULL) {
            FatalError("Can't allocate memory in cfopendir()\n");
        }

        returnval->cf_list = NULL;
        returnval->cf_listpos = NULL;
        returnval->cf_dirh = opendir(name);

        if (returnval->cf_dirh != NULL) {
            return returnval;
        } else {
            free ((char *)returnval);
            return NULL;
        }
    } else {
        return cf_ropendir(name,ip);
    }
}

/* ----------------------------------------------------------------- */

/* 
 * We need this cfdirent type to handle the weird hack used in
 * SVR4/solaris dirent structures
 */
struct cfdirent *
cfreaddir(CFDIR *cfdirh,struct Image *ip)
{
    static struct cfdirent dir;
    struct dirent *dirp;

    memset(dir.d_name,0,CF_BUFSIZE);

    if (strcmp(ip->server,"localhost") == 0) {
        dirp = readdir(cfdirh->cf_dirh);

        if (dirp == NULL) {
            return NULL;
        }

        strncpy(dir.d_name,dirp->d_name,CF_BUFSIZE-1);
        return &dir;
    } else {
        if (cfdirh->cf_listpos != NULL) {
            strncpy(dir.d_name,(cfdirh->cf_listpos)->name,CF_BUFSIZE);
            cfdirh->cf_listpos = cfdirh->cf_listpos->next;
            return &dir;
        } else {
            return NULL;
        }
    }
}

/* ----------------------------------------------------------------- */

void
cfclosedir(CFDIR *dirh)
{
    if ((dirh != NULL) && (dirh->cf_dirh != NULL)) {
        closedir(dirh->cf_dirh);
    }

    Debug("cfclosedir()\n");
    DeleteItemList(dirh->cf_list);
    free((char *)dirh);
}


/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

int
CopyReg(char *source, char *dest, struct stat sstat, struct stat dstat,
        struct Image *ip)
{
    char backup[CF_BUFSIZE];
    char new[CF_BUFSIZE], *linkable;
    int remote = false, silent, backupisdir=false, backupok=false;
    struct stat s;
#ifdef HAVE_UTIME_H
    struct utimbuf timebuf;
#endif

#ifdef DARWIN
    /* For later copy from new to dest */
    char *rsrcbuf;
    int rsrcbytesr; /* read */
    int rsrcbytesw; /* written */
    int rsrcbytesl; /* to read */
    int rsrcrd;
    int rsrcwd;

    /* Keep track of if a resrouce fork */
    char * tmpstr;
    char * forkpointer;

    int rsrcfork;
    rsrcfork=0;
#endif


    Debug2("CopyReg(%s,%s)\n",source,dest);

    if (g_dontdo) {
        printf("%s: copy from %s to %s\n",g_vprefix,source,dest);
        return false;
    }

    /* Make an assoc array of inodes used to preserve hard links */

    linkable = CompressedArrayValue(ip->inode_cache,sstat.st_ino);

    /* Preserve hard links, if possible */
    if (sstat.st_nlink > 1)  {
        if (CompressedArrayElementExists(ip->inode_cache,sstat.st_ino)
                && (strcmp(dest,linkable) != 0)) {
            unlink(dest);

            silent = g_silent;
            g_silent = true;

            DoHardLink(dest,linkable,NULL);

            g_silent = silent;
            return true;
        }
    }

    if (strcmp(ip->server,"localhost") != 0) {
        Debug("This is a remote copy from server: %s\n",ip->server);
        remote = true;
    }

#ifdef DARWIN
    /* Need to munge the "new" name */
    if (strstr(dest, _PATH_RSRCFORKSPEC)) {
        rsrcfork = 1;

        tmpstr = malloc(CF_BUFSIZE);

        /* Drop _PATH_RSRCFORKSPEC */
        strncpy(tmpstr, dest, CF_BUFSIZE);
        forkpointer = strstr(tmpstr, _PATH_RSRCFORKSPEC);
        *forkpointer = '\0';

        strncpy(new, tmpstr, CF_BUFSIZE);

        free(tmpstr);
    } else {
#endif

        if (BufferOverflow(dest,CF_NEW)) {
            printf(" culprit: CopyReg\n");
            return false;
        }
        strcpy(new,dest);

#ifdef DARWIN
    }
#endif

    strcat(new,CF_NEW);

    if (remote) {
        if (g_conn->error) {
            return false;
        }

        if (!CopyRegNet(source,new,ip,sstat.st_size)) {
            return false;
        }
    } else {
        if (!CopyRegDisk(source,new,ip)) {
            return false;
        }

        if (ip->stealth == 'y') {
#ifdef HAVE_UTIME_H
            timebuf.actime = sstat.st_atime;
            timebuf.modtime = sstat.st_mtime;
            utime(source,&timebuf);
#endif
        }
    }

    Debug("CopyReg succeeded in copying to %s to %s\n",source,new);

    if (g_imagebackup != 'n') {
        char stamp[CF_BUFSIZE];
        time_t STAMPNOW;
        STAMPNOW = time((time_t *)NULL);

        sprintf(stamp, "_%d_%s", g_cfstarttime, CanonifyName(ctime(&STAMPNOW)));

        if (BufferOverflow(dest,stamp)) {
            printf(" culprit: CopyReg\n");
            return false;
        }
        strcpy(backup,dest);

        if (g_imagebackup == 's') {
            strcat(backup,stamp);
        }

        /* rely on prior BufferOverflow() and: 
         *      strlen(CF_SAVED) * < BUFFER_MARGIN 
         */
        strcat(backup,CF_SAVED);

        if (IsItemIn(g_vreposlist,backup)) {
            return true;
        }

        /* Mainly important if there is a dir in the way */
        if (lstat(backup,&s) != -1) {
            if (S_ISDIR(s.st_mode)) {
                backupisdir = true;
                PurgeFiles(NULL,backup,NULL);
                rmdir(backup);
            }

            unlink(backup);
        }

        if (rename(dest,backup) == -1) {
            /* ignore */
        }
        /* Did the rename() succeed? NFS-safe */
        backupok = (lstat(backup,&s) != -1);
    } else {

        /* Mainly important if there is a dir in the way */
        if (stat(dest,&s) != -1) {
            if (S_ISDIR(s.st_mode)) {
                PurgeFiles(NULL,dest,NULL);
                rmdir(dest);
            }
        }
    }

    if (stat(new,&dstat) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't stat new file %s\n",new);
        CfLog(cferror,g_output,"stat");
        return false;
    }

    if (dstat.st_size != sstat.st_size) {

        snprintf(g_output, CF_BUFSIZE*2, "WARNING: new file %s seems "
                "to have been corrupted in transit (sizes %d and %d), "
                "aborting!\n", new, (int) dstat.st_size, (int) sstat.st_size);

        CfLog(cfverbose,g_output,"");
        if (backupok) {
            rename(backup,dest); /* ignore failure */
        }
        return false;
    }

    if (ip->verify == 'y') {
        Verbose("Final verification of transmission.\n");
        if (CompareCheckSums(source,new,ip,&sstat,&dstat)) {

            snprintf(g_output, CF_BUFSIZE*2, "WARNING: new file %s seems"
                    "to have been corrupted in transit, aborting!\n", new);

            CfLog(cfverbose,g_output,"");
            if (backupok) {
                rename(backup,dest); /* ignore failure */
            }
            return false;
        }
    }

#ifdef DARWIN
    /* Can't just "mv" the resource fork, unfortunately */
    if (rsrcfork) {
        rsrcrd = open(new, O_RDONLY|O_BINARY);
        rsrcwd = open(dest, O_WRONLY|O_BINARY|O_CREAT|O_TRUNC, 0600);

        if (rsrcrd == -1 || rsrcwd == -1) {
            snprintf(g_output, CF_BUFSIZE, "Open of rsrcrd/rsrcwd failed\n");
            CfLog(cfinform,g_output,"open");
            close(rsrcrd);
            close(rsrcwd);
            return(false);
        }

        rsrcbuf = malloc(CF_BUFSIZE);

        rsrcbytesr = 0;

        while(1) {
            rsrcbytesr = read(rsrcrd, rsrcbuf, CF_BUFSIZE);

            /* Ck error */
            if (rsrcbytesr == -1) {
                if (errno == EINTR) {
                    continue;
                } else {
                    snprintf(g_output, CF_BUFSIZE, "Read of rsrcrd failed\n");
                    CfLog(cfinform,g_output,"read");
                    close(rsrcrd);
                    close(rsrcwd);
                    free(rsrcbuf);
                    return(false);
                }
            } else if (rsrcbytesr == 0) {
                /* Reached EOF */
                close(rsrcrd);
                close(rsrcwd);
                free(rsrcbuf);

                unlink(new); /* Go ahead and unlink .cfnew */

                break;
            }

            rsrcbytesl = rsrcbytesr;
            rsrcbytesw = 0;

            while (rsrcbytesl > 0) {

                rsrcbytesw += write(rsrcwd, rsrcbuf, rsrcbytesl);

                /* Ck error */
                if (rsrcbytesw == -1) {
                    if (errno == EINTR) {
                        continue;
                    } else {
                        snprintf(g_output, CF_BUFSIZE, 
                                "Write of rsrcwd failed\n");
                        CfLog(cfinform,g_output,"write");

                        close(rsrcrd);
                        close(rsrcwd);
                        free(rsrcbuf);
                        return(false);
                    }
                }
                rsrcbytesl = rsrcbytesr - rsrcbytesw;
            }
        }
    } else {
#endif
        if (rename(new,dest) == -1) {

            snprintf(g_output, CF_BUFSIZE*2,
                    "Problem: could not install copy file as %s, "
                    "directory in the way?\n",dest);

            CfLog(cferror,g_output,"rename");
            if (backupok) {
                rename(backup,dest); /* ignore failure */
            }
            return false;
        }
#ifdef DARWIN
    }
#endif

    if ((g_imagebackup != 'n') && backupisdir) {
        snprintf(g_output,CF_BUFSIZE,
                "Cannot move a directory to repository, leaving at %s",backup);
        CfLog(cfinform,g_output,"");
    } else if ((g_imagebackup != 'n') && Repository(backup,ip->repository)) {
        unlink(backup);
    }

    if (ip->preservetimes == 'y') {
#ifdef HAVE_UTIME_H
        timebuf.actime = sstat.st_atime;
        timebuf.modtime = sstat.st_mtime;
        utime(dest,&timebuf);
#endif
    }

    return true;
}

/*
 * --------------------------------------------------------------------
 * Level 3
 * --------------------------------------------------------------------
 */

void
RegisterHardLink(int i,char *value,struct Image *ip)
{
    if (!FixCompressedArrayValue(i,value,&(ip->inode_cache))) {
        /* Not root hard link, remove to preserve consistency */
        if (g_dontdo) {
            Verbose("Need to remove old hard link %s to preserve "
                    "structure..\n", value);
        } else {
            Verbose("Removing old hard link %s to preserve structure..\n",
                    value);
            unlink(value);
        }
    }
}
