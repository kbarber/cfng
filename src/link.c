/*
 * $Id: link.c 743 2004-05-23 07:27:32Z skaar $
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
 * Toolkit: links
 */

int
LinkChildFiles(char *from,char *to,char type,
        struct Item *inclusions, struct Item *exclusions, struct Item *copy,
        short nofile, struct Link *ptr)
{
    DIR *dirh;
    struct dirent *dirp;
    char pcwdto[CF_BUFSIZE],pcwdfrom[CF_BUFSIZE];
    struct stat statbuf;
    int (*linkfiles)(char *from, char *to, 
            struct Item *inclusions, 
            struct Item *exclusions, 
            struct Item *copy, 
            short int nofile, 
            struct Link *ptr);

    Debug("LinkChildFiles(%s,%s)\n",from,to);

    if (stat(to,&statbuf) == -1) {
        /* no error warning, since the higher level routine uses this */
        return(false);
    }

    if ((dirh = opendir(to)) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't open directory %s\n",to);
        CfLog(cferror,g_output,"opendir");
        return false;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name,to,NULL)) {
            continue;
        }

        /* Assemble pathnames */
        strcpy(pcwdto,to);
        AddSlash(pcwdto);

        if (BufferOverflow(pcwdto,dirp->d_name)) {
            FatalError("Can't build filename in LinkChildFiles");
        }
        strcat(pcwdto,dirp->d_name);

        strcpy(pcwdfrom,from);
        AddSlash(pcwdfrom);

        if (BufferOverflow(pcwdfrom,dirp->d_name)) {
            FatalError("Can't build filename in LinkChildFiles");
        }
        strcat(pcwdfrom,dirp->d_name);

        switch (type) {
        case 's':
            linkfiles = LinkFiles;
            break;
        case 'r':
            linkfiles = RelativeLink;
            break;
        case 'a':
            linkfiles = AbsoluteLink;
            break;
        case 'h':
            linkfiles = HardLinkFiles;
            break;
        default:
            printf("Internal error, link type was [%c]\n",type);
            continue;
        }

        (*linkfiles)(pcwdfrom,pcwdto,inclusions,exclusions,copy,nofile,ptr);
    }

    closedir(dirh);
    return true;
}

/* 
 * --------------------------------------------------------------------
 * Here we try to break up the path into a part which will match the
 * last element of a mounted filesytem mountpoint and the remainder
 * after that. We parse the path backwards to get a math e.g.
 *
 *  /fys/lib/emacs ->  lib /emacs
 *                     fys /lib/emacs
 *                         /fys/lib/emacs
 *
 *  we try to match lib and fys to the binserver list e.g. /mn/anyon/fys
 *  and hope for the best. If it doesn't match, tough!
 * ---------------------------------------------------------------------
 */

void
LinkChildren(char *path,char type,struct stat *rootstat,uid_t uid,gid_t gid,
        struct Item *inclusions,struct Item *exclusions,struct Item *copy,
        short nofile,struct Link *ptr)
{
    char *sp;
    char lastlink[CF_BUFSIZE],server[CF_BUFSIZE];
    char from[CF_BUFSIZE],to[CF_BUFSIZE];
    char relpath[CF_BUFSIZE];

    char odir[CF_BUFSIZE];
    DIR *dirh;
    struct dirent *dirp;
    struct stat statbuf;
    int matched = false;
    int (*linkfiles)(char *from, char *to, struct Item *inclusions, struct Item *exclusions, struct Item *copy, short int nofile, struct Link *ptr);

    Debug("LinkChildren(%s)\n",path);

    if (! S_ISDIR(rootstat->st_mode)) {
        snprintf(g_output,CF_BUFSIZE*2,
                "File %s is not a directory: it has no children to link!\n",
                path);
        CfLog(cferror,g_output,"");
        return;
    }

    Verbose("Linking the children of %s\n",path);

    for (sp = path+strlen(path); sp != path-1; sp--) {
        if (*(sp-1) == '/') {
            relpath[0] = '\0';
            sscanf(sp,"%[^/]%s", lastlink,relpath);

            if (MatchAFileSystem(server,lastlink)) {
                strcpy(odir,server);

                if (BufferOverflow(odir,relpath)) {
                    FatalError("culprit: LinkChildren()");
                }
                strcat(odir,relpath);

                if ((dirh = opendir(odir)) == NULL) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Can't open directory %s\n",path);
                    CfLog(cferror,g_output,"opendir");
                    return;
                }

                for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
                    if (!SensibleFile(dirp->d_name,odir,NULL)) {
                        continue;
                    }

                    strcpy(from,path);
                    AddSlash(from);

                    if (BufferOverflow(from,dirp->d_name)) {
                        FatalError("culprit: LinkChildren()");
                    }

                    strcat(from,dirp->d_name);

                    strcpy(to,odir);
                    AddSlash(to);

                    if (BufferOverflow(to,dirp->d_name)) {
                        FatalError("culprit: LinkChildren()");
                    }

                    strcat(to,dirp->d_name);

                    Debug2("LinkChild from = %s to = %s\n",from,to);

                    if (stat(to,&statbuf) == -1) {
                        continue;
                    } else {
                        switch (type) {
                        case 's':
                            linkfiles = LinkFiles;
                            break;
                        case 'r':
                            linkfiles = RelativeLink;
                            break;
                        case 'a':
                            linkfiles = AbsoluteLink;
                            break;
                        case 'h':
                            linkfiles = HardLinkFiles;
                            break;
                        default:
                            snprintf(g_output,CF_BUFSIZE*2,
                                "Internal error, link type was [%c]\n",type);
                            CfLog(cferror,g_output,"");
                            continue;
                        }

                        matched = (*linkfiles)(from,to,inclusions,
                                exclusions,copy,nofile,ptr);

                        if (matched && !g_dontdo) {
                            chown(from,uid,gid);
                        }
                    }
                }

                if (matched) return;
            }
        }
    }

    snprintf(g_output,CF_BUFSIZE*2,
            "Couldn't link the children of %s to anything because no\n",path);
    CfLog(cferror,g_output,"");

    snprintf(g_output, CF_BUFSIZE*2, "file system was found to "
            "mirror it in the defined binservers list.\n");

    CfLog(cferror,g_output,"");
}

/* ----------------------------------------------------------------- */

int
RecursiveLink(struct Link *lp,char *from,char *to,int maxrecurse)
{
    struct stat statbuf;
    DIR *dirh;
    struct dirent *dirp;
    char newfrom[CF_BUFSIZE];
    char newto[CF_BUFSIZE];
    void *bug_check;
    int (*linkfiles)(char *from, char *to, struct Item *inclusions, struct Item *exclusions, struct Item *copy, short int nofile, struct Link *ptr);

    /* reached depth limit */
    if (maxrecurse == 0)  {

        Debug2("MAXRECURSE ran out, quitting at level %s with "
                "endlist = %d\n", to, lp->next);

        return false;
    }

    if (IgnoreFile(to,"",lp->ignores)) {
        Verbose("%s: Ignoring directory %s\n",g_vprefix,from);
        return false;
    }

    /* Check for root dir */
    if (strlen(to) == 0) {
        to = "/";
    }

    bug_check = lp->next;

    if ((dirh = opendir(to)) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't open directory [%s]\n",to);
        CfLog(cferror,g_output,"opendir");
        return false;
    }

    if (lp->next != bug_check) {
        printf("%s: solaris BSD compat bug: opendir wrecked "
                "the heap memory!!", g_vprefix);
        printf("%s: in copy to %s, using workaround...\n",g_vprefix,from);
        lp->next = bug_check;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
    if (!SensibleFile(dirp->d_name,to,NULL)) {
        continue;
    }

    if (IgnoreFile(to,dirp->d_name,lp->ignores)) {
        continue;
    }

    /* Assemble pathname */
    strcpy(newfrom,from);
    AddSlash(newfrom);
    strcpy(newto,to);
    AddSlash(newto);

    if (BufferOverflow(newfrom,dirp->d_name)) {
        closedir(dirh);
        return true;
    }

    strcat(newfrom,dirp->d_name);

    if (BufferOverflow(newto,dirp->d_name)) {
        closedir(dirh);
        return true;
    }

    strcat(newto,dirp->d_name);

    if (g_travlinks) {
        if (stat(newto,&statbuf) == -1) {
            snprintf(g_output,CF_BUFSIZE*2,"Can't stat %s\n",newto);
            CfLog(cfverbose,g_output,"");
            continue;
        }
    } else {
        if (lstat(newto,&statbuf) == -1) {
            snprintf(g_output,CF_BUFSIZE*2,"Can't stat %s\n",newto);
            CfLog(cfverbose,g_output,"");

            memset(g_vbuff,0,CF_BUFSIZE);
            if (readlink(newto,g_vbuff,CF_BUFSIZE-1) != -1) {
                Verbose("File is link to -> %s\n",g_vbuff);
            }
            continue;
        }
    }

    if (!FileObjectFilter(newto,&statbuf,lp->filters,links)) {
        Debug("Skipping filtered file %s\n",newto);
        continue;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        RecursiveLink(lp,newfrom,newto,maxrecurse-1);
    } else {
        switch (lp->type) {
        case 's':
            linkfiles = LinkFiles;
            break;
        case 'r':
            linkfiles = RelativeLink;
        break;
        case 'a':
            linkfiles = AbsoluteLink;
            break;
        case 'h':
            linkfiles = HardLinkFiles;
            break;
        default:
            printf("cfagent: internal error, link type was [%c]\n",lp->type);
            continue;
        }

        (*linkfiles)(newfrom,newto,lp->inclusions,lp->exclusions,
                     lp->copy,lp->nofile,lp);
        }
    }

    closedir(dirh);
    return true;
}

/* ----------------------------------------------------------------- */

/* should return true if 'to' found */
int
LinkFiles(char *from,char *to_tmp,
        struct Item *inclusions,struct Item *exclusions,struct Item *copy,
        short nofile,struct Link *ptr)
{
    struct stat buf,savebuf;
    char to[CF_BUFSIZE], linkbuf[CF_BUFSIZE];
    char saved[CF_BUFSIZE],absto[CF_BUFSIZE],*lastnode;
    struct UidList fakeuid;
    struct Image ip;
    char stamp[CF_BUFSIZE];
    time_t STAMPNOW;
    STAMPNOW = time((time_t *)NULL);

    memset(to,0,CF_BUFSIZE);
    memset(&ip,0, sizeof(ip));

    /* links without a directory reference */
    if ((*to_tmp != '/') && (*to_tmp != '.')) {
        strcpy(to,"./");
    }

    if (strlen(to_tmp)+3 > CF_BUFSIZE) {
        printf("%s: CF_BUFSIZE boundaries exceeded in LinkFiles(%s->%s)\n",
                g_vprefix,from,to_tmp);
        return false;
    }

    strcat(to,to_tmp);

    Debug2("Linkfiles(%s,%s)\n",from,to);

    for (lastnode = from+strlen(from); *lastnode != '/'; lastnode--) { }

    lastnode++;

    if (IgnoredOrExcluded(links,lastnode,inclusions,exclusions)) {
        Verbose("%s: Skipping non-included pattern %s\n",g_vprefix,from);
        return true;
    }

    if (IsWildItemIn(g_vcopylinks,lastnode) || IsWildItemIn(copy,lastnode)) {
        fakeuid.uid = CF_SAME_OWNER;
        fakeuid.next = NULL;
        ip.plus = CF_SAMEMODE;
        ip.minus = CF_SAMEMODE;
        ip.uid = &fakeuid;
        ip.gid = (struct GidList *) &fakeuid;
        ip.action = "do";
        ip.recurse = 0;
        ip.type = 't';
        ip.defines = ptr->defines;
        ip.elsedef = ptr->elsedef;
        ip.backup = true;
        ip.exclusions = NULL;
        ip.inclusions = NULL;
        ip.symlink = NULL;
        ip.classes = NULL;
        ip.plus_flags = 0;
        ip.size = CF_NOSIZE;
        ip.linktype = 's';
        ip.minus_flags = 0;
        ip.server = strdup("localhost");
        Verbose("%s: Link item %s marked for copying instead\n", 
                g_vprefix, from);
        MakeDirectoriesFor(to,'n');
        CheckImage(to,from,&ip);
        free(ip.server);
        return true;
    }

    /* relative path, must still check if exists */
    if (*to != '/') {
        Debug("Relative link destination detected: %s\n",to);
        strcpy(absto,AbsLinkPath(from,to));
        Debug("Absolute path to relative link = %s, from %s\n",absto,from);
    } else {
        strcpy(absto,to);
    }

    if (!nofile) {
        if (stat(absto,&buf) == -1) {
            /* no error warning, since the higher level routine uses this */
            return(false);
        }
    }

    Debug2("Trying to link %s -> %s (%s)\n",from,to,absto);

    if (lstat(from,&buf) == 0) {
        if (! S_ISLNK(buf.st_mode) && ! g_enforcelinks) {
            snprintf(g_output,CF_BUFSIZE*2,"Error linking %s -> %s\n",from,to);
            CfLog(cfsilent,g_output,"");

            snprintf(g_output, CF_BUFSIZE*2,
                    "Cannot make link: %s exists and is not a link! "
                    "(uid %d)\n", from, buf.st_uid);

            CfLog(cfsilent,g_output,"");
            return(true);
        }

        if (S_ISREG(buf.st_mode) && g_enforcelinks) {
            snprintf(g_output, CF_BUFSIZE*2, "Moving %s to %s%s\n",
                    from, from, CF_SAVED);
            CfLog(cfsilent,g_output,"");

            if (g_dontdo) {
                return true;
            }

            saved[0] = '\0';
            strcpy(saved,from);

            sprintf(stamp, "_%d_%s",
                    g_cfstarttime, CanonifyName(ctime(&STAMPNOW)));
            strcat(saved,stamp);

            strcat(saved,CF_SAVED);

            if (rename(from,saved) == -1) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Can't rename %s to %s\n", from,saved);
                CfLog(cferror,g_output,"rename");
                return(true);
            }

            if (Repository(saved,g_vrepository)) {
                unlink(saved);
            }
        }

        if (S_ISDIR(buf.st_mode) && g_enforcelinks) {
            snprintf(g_output,CF_BUFSIZE*2,"Moving directory %s to %s%s.dir\n",
                    from,from,CF_SAVED);
            CfLog(cfsilent,g_output,"");

            if (g_dontdo) {
                return true;
            }

            saved[0] = '\0';
            strcpy(saved,from);

            sprintf(stamp, "_%d_%s",
                    g_cfstarttime, CanonifyName(ctime(&STAMPNOW)));
            strcat(saved,stamp);

            strcat(saved,CF_SAVED);
            strcat(saved,".dir");

            if (stat(saved,&savebuf) != -1) {

                snprintf(g_output,CF_BUFSIZE*2,
                        "Couldn't save directory %s, "
                        "since %s exists already\n",
                        from,saved);

                CfLog(cferror,g_output,"");

                snprintf(g_output,CF_BUFSIZE*2,
                        "Unable to force link to "
                        "existing directory %s\n",from);

                CfLog(cferror,g_output,"");
                return true;
            }

            if (rename(from,saved) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Can't rename %s to %s\n", from,saved);
                CfLog(cferror,g_output,"rename");
                return(true);
            }
        }
    }

    memset(linkbuf,0,CF_BUFSIZE);

    if (readlink(from,linkbuf,CF_BUFSIZE-1) == -1) {
        /* link doesn't exist */
        if (! MakeDirectoriesFor(from,'n')) {

            snprintf(g_output,CF_BUFSIZE*2,
                    "Couldn't build directory tree up to %s!\n",from);

            CfLog(cfsilent,g_output,"");

            snprintf(g_output,CF_BUFSIZE*2,
                    "One element was a plain file, not a directory!\n");

            CfLog(cfsilent,g_output,"");
            return(true);
        }
    } else {
        int off1 = 0, off2 = 0;

        DeleteSlash(linkbuf);

        /* Ignore ./ at beginning */
        if (strncmp(linkbuf,"./",2) == 0) {
            off1 = 2;
        }

        if (strncmp(to,"./",2) == 0) {
            off2 = 2;
        }

        if (strcmp(linkbuf+off1,to+off2) != 0) {
            if (g_enforcelinks) {
                snprintf(g_output,CF_BUFSIZE*2,"Removing link %s\n",from);
                CfLog(cfinform,g_output,"");

                if (!g_dontdo) {
                    if (unlink(from) == -1) {
                        perror("unlink");
                        return true;
                    }

                    return DoLink(from,to,ptr->defines);
                }
            } else {

                snprintf(g_output,CF_BUFSIZE*2,
                        "Old link %s points somewhere else. Doing nothing!\n",
                        from);

                CfLog(cfsilent,g_output,"");

                snprintf(g_output, CF_BUFSIZE*2,
                        "(Link points to %s not %s)\n\n",
                        linkbuf,to);

                CfLog(cfsilent,g_output,"");
                return(true);
            }
        } else {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Link (%s->%s) exists.\n", from, to_tmp);
            CfLog(cfverbose,g_output,"");

            if (!nofile) {

                /* Check whether link points somewhere */
                KillOldLink(from,ptr->defines);

                return true;
            }

            AddMultipleClasses(ptr->elsedef);
            return(true);
        }
    }

    return DoLink(from,to,ptr->defines);
}

/* ----------------------------------------------------------------- */

int
RelativeLink(char *from, char *to,
    struct Item *inclusions, struct Item *exclusions, struct Item *copy,
    short nofile, struct Link *ptr)
{
    char *sp, *commonto, *commonfrom;
    char buff[CF_BUFSIZE],linkto[CF_BUFSIZE];
    int levels=0;

    Debug2("RelativeLink(%s,%s)\n",from,to);

    if (*to == '.') {
        return LinkFiles(from,to,inclusions,exclusions,copy,nofile,ptr);
    }

    if (!CompressPath(linkto,to)) {
        snprintf(g_output,CF_BUFSIZE*2,"Failed to link %s to %s\n",from,to);
        CfLog(cferror,g_output,"");
        return false;
    }

    commonto = linkto;
    commonfrom = from;

    if (strcmp(commonto,commonfrom) == 0) {
        CfLog(cferror,"Can't link file to itself!\n","");
        snprintf(g_output,CF_BUFSIZE*2,"(%s -> %s)\n",from,to);
        CfLog(cferror,g_output,"");
        return false;
    }

    while (*commonto == *commonfrom) {
        commonto++;
        commonfrom++;
    }

    while (!((*commonto == '/') && (*commonfrom == '/'))) {
        commonto--;
        commonfrom--;
    }

    commonto++;

    Debug("Commonto = %s, common from = %s\n",commonto,commonfrom);

    for (sp = commonfrom; *sp != '\0'; sp++) {
        if (*sp == '/') {
            levels++;
        }
    }

    Debug("LEVELS = %d\n",levels);

    memset(buff,0,CF_BUFSIZE);

    strcat(buff,"./");

    while(--levels > 0) {
        if (BufferOverflow(buff,"../")) {
            return false;
        }

        strcat(buff,"../");
    }

    if (BufferOverflow(buff,commonto)) {
        return false;
    }

    strcat(buff,commonto);

    return LinkFiles(from,buff,inclusions,exclusions,copy,nofile,ptr);
}

/* ----------------------------------------------------------------- */

int
AbsoluteLink(char *from, char *to,
    struct Item *inclusions, struct Item *exclusions, struct Item *copy,
    short nofile, struct Link *ptr)
{
    char absto[CF_BUFSIZE];
    char expand[CF_BUFSIZE];

    Debug2("AbsoluteLink(%s,%s)\n",from,to);

    if (*to == '.') {
        strcpy(g_linkto,from);
        ChopLastNode(g_linkto);
        AddSlash(g_linkto);
        strcat(g_linkto,to);
    } else {
        strcpy(g_linkto,to);
    }

    CompressPath(absto,g_linkto);

    expand[0] = '\0';

    if (!nofile) {

        /* begin at level 1 and beam out at 15 */
        if (!ExpandLinks(expand,absto,0))  {
            CfLog(cferror,"Failed to make absolute link in\n","");
            snprintf(g_output,CF_BUFSIZE*2,"%s -> %s\n",from,to);
            CfLog(cferror,g_output,"");
            return false;
        } else {
            Debug2("ExpandLinks returned %s\n",expand);
        }
    } else {
        strcpy(expand,absto);
    }

    CompressPath(g_linkto,expand);

    return LinkFiles(from,g_linkto,inclusions,exclusions,copy,nofile,ptr);
}

/* ----------------------------------------------------------------- */

int
DoLink(char *from, char *to, char *defines)
{
    if (g_dontdo) {
        printf("cfagent: Need to link files %s -> %s\n",from,to);
    } else {
        snprintf(g_output,CF_BUFSIZE*2,"Linking files %s -> %s\n",from,to);
        CfLog(cfinform,g_output,"");

        if (symlink(to,from) == -1) {
            snprintf(g_output,CF_BUFSIZE*2,"Couldn't link %s to %s\n",to,from);
            CfLog(cferror,g_output,"symlink");
            return false;
        } else {
            AddMultipleClasses(defines);
            return true;
        }
    }
    return true;
}

/* ----------------------------------------------------------------- */

void
KillOldLink(char *name, char *defs)
{
    char linkbuf[CF_BUFSIZE];
    char linkpath[CF_BUFSIZE],*sp;
    struct stat statbuf;

    Debug("KillOldLink(%s)\n",name);
    memset(linkbuf,0,CF_BUFSIZE);
    memset(linkpath,0,CF_BUFSIZE);

    if (readlink(name,linkbuf,CF_BUFSIZE-1) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,
                "(Can't read link %s while checking for deadlinks)\n",name);
        CfLog(cfverbose,g_output,"");
        return;
    }

    if (linkbuf[0] != '/') {
        strcpy(linkpath,name);    /* Get path to link */

        for (sp = linkpath+strlen(linkpath); (*sp != '/') &&
                (sp >= linkpath); sp-- ) {
            *sp = '\0';
        }
    }

    strcat(linkpath,linkbuf);
    CompressPath(g_vbuff,linkpath);

    /* link points nowhere */
    if (stat(g_vbuff,&statbuf) == -1) {
        if (g_killoldlinks || g_debug || g_d2) {

            snprintf(g_output, CF_BUFSIZE*2,
                    "%s is a link which points to %s, "
                    "but that file doesn't seem to exist\n",
                    name, g_vbuff);

            CfLog(cfverbose,g_output,"");
        }

        if (g_killoldlinks) {
            snprintf(g_output,CF_BUFSIZE*2,"Removing dead link %s\n",name);
            CfLog(cfinform,g_output,"");

            if (! g_dontdo) {

                /* May not work on a client-mounted system ! */
                unlink(name);

                AddMultipleClasses(defs);
            }
        }
    }
}

/* ----------------------------------------------------------------- */

/* should return true if 'to' found */
int
HardLinkFiles(char *from, char *to,
    struct Item *inclusions, struct Item *exclusions, struct Item *copy,
    short nofile, struct Link *ptr)
{

    struct stat frombuf,tobuf;
    char saved[CF_BUFSIZE], *lastnode;
    struct UidList fakeuid;
    struct Image ip;
    char stamp[CF_BUFSIZE];
    time_t STAMPNOW;
    STAMPNOW = time((time_t *)NULL);

    for (lastnode = from+strlen(from); *lastnode != '/'; lastnode--) { }

    lastnode++;

    if (inclusions != NULL && !IsWildItemIn(inclusions,lastnode)) {
        Verbose("%s: Skipping non-included pattern %s\n",g_vprefix,from);
        return true;
    }

    if (IsWildItemIn(g_vexcludelink,lastnode) ||
            IsWildItemIn(exclusions,lastnode)) {

        Verbose("%s: Skipping excluded pattern %s\n",g_vprefix,from);
        return true;
    }

    if (IsWildItemIn(g_vcopylinks,lastnode) || IsWildItemIn(copy,lastnode)) {
        fakeuid.uid = CF_SAME_OWNER;
        fakeuid.next = NULL;
        ip.plus = CF_SAMEMODE;
        ip.minus = CF_SAMEMODE;
        ip.uid = &fakeuid;
        ip.gid = (struct GidList *) &fakeuid;
        ip.action = "do";
        ip.recurse = 0;
        ip.type = 't';
        ip.backup = true;
        ip.plus_flags = 0;
        ip.minus_flags = 0;
        ip.exclusions = NULL;
        ip.symlink = NULL;
        Verbose("%s: Link item %s marked for copying instead\n",
                g_vprefix, from);
        CheckImage(to,from,&ip);
        return true;
    }

    if (stat(to,&tobuf) == -1) {
        /* no error warning, since the higher level routine uses this */
        return(false);
    }

    if (! S_ISREG(tobuf.st_mode)) {

        snprintf(g_output, CF_BUFSIZE*2,
                "%s: will only hard link regular files "
                "and %s is not regular\n", g_vprefix, to);

        CfLog(cfsilent,g_output,"");
        return true;
    }

    Debug2("Trying to (hard) link %s -> %s\n",from,to);

    if (stat(from,&frombuf) == -1) {
        DoHardLink(from,to,ptr->defines);
        return true;
    }

    /* 
     * Both files exist, but are they the same file? POSIX says the
     * files could be on different devices, but unix doesn't allow this
     * behaviour so the tests below are theoretical...
     */

    if (frombuf.st_ino != tobuf.st_ino && frombuf.st_dev != frombuf.st_dev) {
        Verbose("If this is POSIX, unable to determine if %s is "
                "hard link is correct\n", from);
        Verbose("since it points to a different filesystem!\n");

        if (frombuf.st_mode == tobuf.st_mode &&
                frombuf.st_size == tobuf.st_size) {

            snprintf(g_output,CF_BUFSIZE*2,
                    "Hard link (%s->%s) on different device APPEARS okay\n",
                    from,to);

            CfLog(cfverbose,g_output,"");
            AddMultipleClasses(ptr->elsedef);
            return true;
        }
    }

    if (frombuf.st_ino == tobuf.st_ino && frombuf.st_dev == frombuf.st_dev) {

        snprintf(g_output,CF_BUFSIZE*2,
                "Hard link (%s->%s) exists and is okay.\n",from,to);

        CfLog(cfverbose,g_output,"");
        AddMultipleClasses(ptr->elsedef);
        return true;
    }

    snprintf(g_output,CF_BUFSIZE*2,
            "%s does not appear to be a hard link to %s\n",from,to);

    CfLog(cfinform,g_output,"");

    if (g_enforcelinks) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Moving %s to %s.%s\n", from, from, CF_SAVED);
        CfLog(cfinform,g_output,"");

        if (g_dontdo) {
            return true;
        }

        saved[0] = '\0';
        strcpy(saved,from);

        sprintf(stamp, "_%d_%s", g_cfstarttime, 
                CanonifyName(ctime(&STAMPNOW)));
        strcat(saved,stamp);

        strcat(saved,CF_SAVED);

        if (rename(from,saved) == -1) {
            perror("rename");
            return(true);
        }

        DoHardLink(from,to,ptr->defines);
    }

    return(true);
}

/* ----------------------------------------------------------------- */

void
DoHardLink(char *from, char *to, char *defines)
{
    if (g_dontdo) {
        printf("Hardlink files %s -> %s\n\n",from,to);
    } else {
        snprintf(g_output, CF_BUFSIZE*2,
                "Hardlinking files %s -> %s\n", from, to);
        CfLog(cfinform,g_output,"");

        if (link(to,from) == -1) {
            CfLog(cferror,"","link");
        } else {
            AddMultipleClasses(defines);
        }
    }
}

/* ----------------------------------------------------------------- */

/* 
 * Expand a path contaning symbolic links, up to 4 levels of symbolic
 * links and then beam out in a hurry !  recursive
 */

int
ExpandLinks(char *dest, char *from, int level)
{
    char *sp, buff[CF_BUFSIZE];
    char node[CF_MAXLINKSIZE];
    struct stat statbuf;
    int lastnode = false;

    memset(dest,0,CF_BUFSIZE);

    Debug2("ExpandLinks(%s,%d)\n",from,level);

    if (level >= CF_MAXLINKLEVEL) {

        CfLog(cferror,
            "Too many levels of symbolic links to evaluate "
            "absolute path\n","");

        return false;
    }

    for (sp = from; *sp != '\0'; sp++) {
        if (*sp == '/') {
            continue;
        }

        sscanf(sp,"%[^/]",node);
        sp += strlen(node);

        if (*sp == '\0') {
            lastnode = true;
            }

        if (strcmp(node,".") == 0) {
            continue;
        }

        if (strcmp(node,"..") == 0) {
            if (! ChopLastNode(g_linkto)) {
                Debug("cfagent: used .. beyond top of filesystem!\n");
                return false;
            }
            continue;
        } else {
            strcat(dest,"/");
        }
        strcat(dest,node);

        /* File doesn't exist so we can stop here */
        if (lstat(dest,&statbuf) == -1) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Can't stat %s in ExpandLinks\n", dest);
            CfLog(cferror,g_output,"stat");
            return false;
        }

        if (S_ISLNK(statbuf.st_mode)) {
            memset(buff,0,CF_BUFSIZE);

            if (readlink(dest,buff,CF_BUFSIZE-1) == -1) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Expand links can't stat %s\n", dest);
                CfLog(cferror,g_output,"readlink");
                return false;
            } else {
                if (buff[0] == '.') {

                    ChopLastNode(dest);
                    AddSlash(dest);

                    if (BufferOverflow(dest,buff)) {
                        return false;
                    }
                    strcat(dest,buff);
                }
                else if (buff[0] == '/') {
                    strcpy(dest,buff);
                    DeleteSlash(dest);

                    if (strcmp(dest,from) == 0) {
                        Debug2("No links to be expanded\n");
                        return true;
                    }

                    if (!lastnode && !ExpandLinks(buff,dest,level+1)) {
                        return false;
                    }
                } else {
                    ChopLastNode(dest);
                    AddSlash(dest);
                    strcat(dest,buff);
                    DeleteSlash(dest);

                    if (strcmp(dest,from) == 0) {
                        Debug2("No links to be expanded\n");
                        return true;
                    }

                    memset(buff,0,CF_BUFSIZE);

                    if (!lastnode && !ExpandLinks(buff,dest,level+1)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

/* ----------------------------------------------------------------- */

/* 
 * Take an abolute source and a relative destination object and find the
 * absolute name of the to object
 */
char *
AbsLinkPath(char *from, char *relto)
{
    char *sp;
    int pop = 1;
    static char destination[CF_BUFSIZE];

    if (*relto == '/') {
        printf("Cfengine internal error: call to AbsLInkPath "
                "with absolute pathname\n");
        FatalError("");
    }

    strcpy(destination,from);  /* reuse to save stack space */

    for (sp = relto; *sp != '\0'; sp++) {
        if (strncmp(sp,"../",3) == 0) {
            pop++;
            sp += 2;
            continue;
        }

        if (strncmp(sp,"./",2) == 0) {
            sp += 1;
            continue;
        }

        break; /* real link */
    }

    while (pop > 0) {
        ChopLastNode(destination);
        pop--;
    }

    if (strlen(destination) == 0) {
        strcpy(destination,"/");
    } else {
        AddSlash(destination);
    }

    strcat(destination,sp);
    Debug("Reconstructed absolute linkname = %s\n",destination);
    return destination;
}
