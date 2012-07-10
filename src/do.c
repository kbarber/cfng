/*
 * $Id: do.c 761 2004-05-25 22:25:46Z skaar $
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

void
GetHomeInfo(void)
{
    DIR *dirh;
    struct dirent *dirp;
    struct Item *ip;
    char path[CF_BUFSIZE], mountitem[CF_BUFSIZE];

    if (!IsPrivileged())                            {
        Debug("Not root, so skipping GetHomeInfo()\n");
        return;
    }

    if (!MountPathDefined()) {
        return;
    }


    for (ip = g_vmountlist; ip != NULL; ip = ip->next) {
        if (IsExcluded(ip->classes)) {
            continue;
        }

        if ((dirh = opendir(ip->name)) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "INFO: Host %s seems to have no (additional) "
                    "local disks except the OS\n", g_vdefaultbinserver.name);
            CfLog(cfverbose, g_output, "");
            snprintf(g_output, CF_BUFSIZE*2,
                    "      mounted under %s\n\n", ip->name);
            CfLog(cfverbose, g_output,"");
            return;
        }

        for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
            if (!SensibleFile(dirp->d_name, ip->name, NULL)) {
                continue;
            }

            strcpy(g_vbuff, ip->name);
            AddSlash(g_vbuff);
            strcat(g_vbuff, dirp->d_name);

            if (IsHomeDir(g_vbuff)) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Host defines a home directory %s\n", g_vbuff);
                CfLog(cfverbose, g_output, "");
            } else {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Host defines a potential mount point %s\n", g_vbuff);
                CfLog(cfverbose, g_output,"");
            }

            snprintf(path, CF_BUFSIZE, "%s%s", ip->name, dirp->d_name);
            snprintf(mountitem, CF_BUFSIZE, "%s:%s",
                    g_vdefaultbinserver.name,path);

            if (! IsItemIn(g_vmounted, mountitem)) {
                if ( g_mountcheck && ! RequiredFileSystemOkay(path) 
                        && g_verbose) {
                    snprintf(g_output, CF_BUFSIZE*2, 
                            "Found a mountpoint %s but there was\n", path);
                    CfLog(cfinform, g_output, "");
                    CfLog(cfinform, "nothing mounted on it.\n\n", "");
                }
            }
        }
        closedir(dirh);
    }
}

/* ----------------------------------------------------------------- */

/* 
 * This is, in fact, the most portable way to read the mount info!
 * Depressing, isn't it? 
 */

void
GetMountInfo(void)
{
    FILE *pp;
    char buf1[CF_BUFSIZE], buf2[CF_BUFSIZE], buf3[CF_BUFSIZE];
    char host[CF_MAXVARSIZE], mounton[CF_BUFSIZE];
    int i;

    if (!GetLock(ASUniqueName("mountinfo"), "", 1,
                g_vexpireafter, g_vuqname, g_cfstarttime)) {
        return;
    }

    /* sscanf(g_vmountcomm[g_vsystemhardclass],"%s",buf1); */
    /* Old BSD scanf crashes here! Why!? workaround: */

    for (i=0; g_vmountcomm[g_vsystemhardclass][i] != ' '; i++) {
        buf1[i] =  g_vmountcomm[g_vsystemhardclass][i];
    }

    buf1[i] = '\0';

    signal(SIGALRM,(void *)TimeOut);
    alarm(g_rpctimeout);

    if ((pp = cfpopen(buf1,"r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "%s: Can't open %s\n", g_vprefix, buf1);
        CfLog(cferror, g_output, "popen");
        return;
    }

    do {
        g_vbuff[0] = buf1[0] = buf2[0] = buf3[0] = '\0';

        /* abortable */
        if (ferror(pp))  {
            g_gotmountinfo = false;
            CfLog(cferror, "Error getting mount info\n", "ferror");
            break;
        }

        ReadLine(g_vbuff, CF_BUFSIZE, pp);

        /* abortable */
        if (ferror(pp))  {
            g_gotmountinfo = false;
            CfLog(cferror, "Error getting mount info\n", "ferror");
            break;
        }

        sscanf(g_vbuff, "%s%s%s", buf1, buf2, buf3);

        if (g_vbuff[0] == '\n') {
            break;
        }

        if (strstr(g_vbuff, "not responding")) {
            printf("%s: %s\n", g_vprefix, g_vbuff);
        }

        if (strstr(g_vbuff, "be root")) {
            CfLog(cferror, "Mount access is denied. You must be root.\n", "");
            CfLog(cferror, "Use the -n option to run safely.", "");
        }

        if (strstr(g_vbuff, "retrying") || strstr(g_vbuff, "denied") ||
                strstr(g_vbuff, "backgrounding")) {
            continue;
        }

        if (strstr(g_vbuff, "exceeded") || strstr(g_vbuff, "busy")) {
            continue;
        }

        if (strstr(g_vbuff, "RPC")) {
            if (! g_silent) {
                CfLog(cfinform, "There was an RPC timeout. "
                        "Aborting mount operations.\n", "");
                CfLog(cfinform, "Session failed while trying to talk "
                        "to remote host\n", "");
                snprintf(g_output, CF_BUFSIZE*2, "%s\n", g_vbuff);
                CfLog(cfinform, g_output, "");
            }

            g_gotmountinfo = false;
            ReleaseCurrentLock();
            cfpclose(pp);
            return;
        }

        switch (g_vsystemhardclass) {
        case darwin:
        case sun4:
        case sun3:
        case ultrx:
        case irix:
        case irix4:
        case irix64:
        case linuxx:
        case GnU:
        case unix_sv:
        case freebsd:
        case netbsd:
        case openbsd:
        case bsd_i:
        case nextstep:
        case bsd4_3:
        case newsos:
        case aos:
        case osf:
        case crayos:
            if (buf1[0] == '/') {
                strcpy(host, g_vdefaultbinserver.name);
                strcpy(mounton, buf3);
            } else {
                sscanf(buf1, "%[^:]", host);
                strcpy(mounton, buf3);
            }
            break;
        case solaris:
        case solarisx86:
        case hp:
            if (buf3[0] == '/') {
                strcpy(host, g_vdefaultbinserver.name);
                strcpy(mounton, buf1);
            } else {
                sscanf(buf3, "%[^:]", host);
                strcpy(mounton, buf1);
            }
            break;
        case aix:
                /* skip header */

            if (buf1[0] == '/') {
                strcpy(host, g_vdefaultbinserver.name);
                strcpy(mounton, buf2);
            } else {
                strcpy(host, buf1);
                strcpy(mounton, buf3);
                }
            break;
        case cfnt:
            strcpy(mounton, buf2);
            strcpy(host, buf1);
            break;
        case unused1:
        case unused2:
        case unused3:
                break;
        case cfsco: CfLog(cferror,
                            "Don't understand SCO mount format,  no data", "");
        default:
            printf("cfng software error: case %d = %s\n",
                    g_vsystemhardclass, g_classtext[g_vsystemhardclass]);
            FatalError("System error in GetMountInfo - no such class!");
        }

        InstallMountedItem(host, mounton);
    }

    while (!feof(pp));

    alarm(0);
    signal(SIGALRM, SIG_DFL);
    ReleaseCurrentLock();
    cfpclose(pp);
}

/* ----------------------------------------------------------------- */

void
MakePaths(void)
{
    struct File *ptr;
    struct Item *ip1, *ip2;
    char pathbuff[CF_BUFSIZE], basename[CF_EXPANDSIZE];

    for (ptr = g_vmakepath; ptr != NULL; ptr=ptr->next) {
        if (IsExcluded(ptr->classes)) {
            continue;
        }

        if (ptr->done == 'y' || strcmp(ptr->scope, g_contextid)) {
            continue;
        } else {
            ptr->done = 'y';
        }

        ResetOutputRoute(ptr->log, ptr->inform);

        /* home/subdir */
        if (strncmp(ptr->path, "home/", 5) == 0) {
            if (*(ptr->path+4) != '/') {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Illegal use of home in directories: %s\n", ptr->path);
                CfLog(cferror, g_output, "");
                continue;
            }

            for (ip1 = g_vhomepatlist; ip1 != NULL; ip1=ip1->next) {
                for (ip2 = g_vmountlist; ip2 != NULL; ip2=ip2->next) {
                    if (IsExcluded(ip2->classes)) {
                        continue;
                    }

                    pathbuff[0]='\0';
                    basename[0]='\0';

                    strcpy(pathbuff, ip2->name);
                    AddSlash(pathbuff);
                    strcat(pathbuff, ip1->name);
                    AddSlash(pathbuff);
                    strcat(pathbuff, "*/");
                    strcat(pathbuff, ptr->path+5);

                    ExpandWildCardsAndDo(pathbuff, basename,
                            DirectoriesWrapper, ptr);
                }
            }
        } else {
            Verbose("MakePath(%s)\n", ptr->path);
            pathbuff[0]='\0';
            basename[0]='\0';

            ExpandWildCardsAndDo(ptr->path, basename, DirectoriesWrapper, ptr);
        }
        ResetOutputRoute('d', 'd');
    }
}

/* ----------------------------------------------------------------- */

/* <binserver> should expand to a best fit filesys */
void
MakeChildLinks(void)
{
    struct Link *lp;
    struct Item *ip;
    int matched, varstring;
    char to[CF_EXPANDSIZE], from[CF_EXPANDSIZE], path[CF_EXPANDSIZE];
    struct stat statbuf;
    short saveenforce;
    short savesilent;

    if (g_nolinks) {
        return;
    }

    g_action = links;

    for (lp = g_vchlink; lp != NULL; lp = lp->next) {
        if (IsExcluded(lp->classes)) {
            continue;
        }

        if (lp->done == 'y' || strcmp(lp->scope, g_contextid)) {
            continue;
        } else {
            lp->done = 'y';
        }

        /* Unique ID for copy locking */
        snprintf(g_vbuff, CF_BUFSIZE, "%.50s.%.50s", lp->from, lp->to);

        if (!GetLock(ASUniqueName("link"), CanonifyName(g_vbuff),
                    lp->ifelapsed, lp->expireafter, g_vuqname, g_cfstarttime)) {
            continue;
        }

        ExpandVarstring(lp->from, from, NULL);
        ExpandVarstring(lp->to, to, NULL);

        saveenforce = g_enforcelinks;
        g_enforcelinks = g_enforcelinks || (lp->force == 'y');

        savesilent = g_silent;
        g_silent = g_silent || lp->silent;

        ResetOutputRoute(lp->log, lp->inform);

        matched = varstring = false;

        for(ip = g_vbinservers; ip != NULL && (!matched); ip = ip->next) {
            path[0] = '\0';

            /* linkchildren */
            if (strcmp(to, "linkchildren") == 0) {
                if (stat(from, &statbuf) == -1) {
                    snprintf(g_output, CF_BUFSIZE*2,
                            "Makechildlinks() can't stat %s\n", from);
                    CfLog(cferror, g_output, "stat");
                    ResetOutputRoute('d', 'd');
                    continue;
                }
                LinkChildren(from, lp->type, &statbuf, 0, 0, 
                        lp->inclusions, lp->exclusions, lp->copy,
                        lp->nofile, lp);
                break;
            }

            varstring = ExpandVarbinserv(to, path, ip->name);

            if (lp->recurse != 0) {
                matched = RecursiveLink(lp, from, path, lp->recurse);
            } else if (LinkChildFiles(from, path, lp->type, lp->inclusions,
                        lp->exclusions, lp->copy, lp->nofile, lp)) {
                matched = true;
            } else if (! varstring) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Error while trying to childlink %s -> %s\n", 
                        from, path);
                CfLog(cferror, g_output, "");
                snprintf(g_output, CF_BUFSIZE*2,
                        "The directory %s does not exist. Can't link.\n", 
                        path);
                CfLog(cferror, g_output, "");
            }

            /* don't iterate over binservers if not var */
            if (! varstring) {
                ReleaseCurrentLock();
                break;
            }
        }

        g_enforcelinks = saveenforce;
        g_silent = savesilent;
        ResetOutputRoute('d', 'd');

        if (matched == false && ip == NULL) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "ChildLink didn't find any server to match %s -> %s\n",
                    from, to);
            CfLog(cferror, g_output, "");
        }

        ReleaseCurrentLock();
    }
}

/* ----------------------------------------------------------------- */

/* <binserver> should expand to a best fit filesys */
void
MakeLinks(void)
{
    struct Link *lp;
    char from[CF_EXPANDSIZE], to[CF_EXPANDSIZE], path[CF_EXPANDSIZE];
    struct Item *ip;
    int matched, varstring;
    short saveenforce;
    short savesilent;
    int (*linkfiles)(char *from, char *to, struct Item *inclusions, 
            struct Item *exclusions, struct Item *copy, 
            short int nofile, struct Link *ptr);

    if (g_nolinks) {
        return;
    }

    g_action = links;

    for (lp = g_vlink; lp != NULL; lp = lp->next) {
        if (IsExcluded(lp->classes)) {
            continue;
        }

        if (lp->done == 'y' || strcmp(lp->scope, g_contextid)) {
            continue;
        } else {
            lp->done = 'y';
        }

        /* Unique ID for copy locking */
        snprintf(g_vbuff, CF_BUFSIZE, "%.50s.%.50s", lp->from, lp->to);

        if (!GetLock(ASUniqueName("link"), CanonifyName(g_vbuff),
                    lp->ifelapsed, lp->expireafter, g_vuqname,
                    g_cfstarttime)) {
            continue;
        }

        ExpandVarstring(lp->from, from, NULL);
        ExpandVarstring(lp->to, to, NULL);

        ResetOutputRoute(lp->log, lp->inform);

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
            printf("%s: internal error, link type was [%c]\n",
                    g_vprefix, lp->type);
            ReleaseCurrentLock();
            continue;
        }

        saveenforce = g_enforcelinks;
        g_enforcelinks = g_enforcelinks || (lp->force == 'y');

        savesilent = g_silent;
        g_silent = g_silent || lp->silent;

        matched = varstring = false;

        for( ip = g_vbinservers; ip != NULL && (!matched); ip = ip->next) {
            path[0] = '\0';

            varstring = ExpandVarbinserv(to,path,ip->name);

            if ((*linkfiles)(from, path, lp->inclusions, lp->exclusions,
                        lp->copy, lp->nofile, lp)) {
                matched = true;
            } else if (! varstring) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Error while trying to link %s -> %s\n", from, path);
                CfLog(cfinform, g_output, "");
            }

            /* don't iterate over binservers if not var */
            if (! varstring) {
                break;
            }
        }

        g_enforcelinks = saveenforce;
        g_silent = savesilent;

        ResetOutputRoute('d', 'd');

        if (matched == false && ip == NULL) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Links didn't find any file to match %s -> %s\n", from, to);
            CfLog(cferror, g_output, "");
        }

        ReleaseCurrentLock();
    }
}

/* ----------------------------------------------------------------- */

void
MailCheck(void)
{
    char mailserver[CF_BUFSIZE];
    char mailhost[CF_MAXVARSIZE];
    char rmailpath[CF_MAXVARSIZE];
    char lmailpath[CF_MAXVARSIZE];


    if (!GetLock("Mailcheck", CanonifyName(g_vfstab[g_vsystemhardclass]), 0,
                g_vexpireafter, g_vuqname, g_cfstarttime)) {
        return;
    }

    if (g_vmailserver[0] == '\0') {
        FatalError("Program does not define a mailserver for this host");
    }

    if (!IsPrivileged())                            {
        CfLog(cferror, "Only root can alter the mail configuration.\n", "");
        ReleaseCurrentLock();
        return;
    }

    sscanf (g_vmailserver, "%[^:]:%s", mailhost, rmailpath);

    if (g_vmailserver[0] == '\0') {
        CfLog(cfinform, "\n%s: Host has no defined mailserver!\n", "");
        ReleaseCurrentLock();
        return;
    }

    /* Is this the mailserver ?*/
    if (strcmp(g_vdefaultbinserver.name, mailhost) == 0) {
        ExpiredUserCheck(rmailpath, false);
        ReleaseCurrentLock();
        return;
    }

    snprintf(lmailpath, CF_BUFSIZE, "%s:%s", 
            mailhost, g_vmaildir[g_vsystemhardclass]);


    /* Remote file system mounted on */
    /* local mail dir - correct      */
    if (IsItemIn(g_vmounted, lmailpath)) {
        Verbose("%s: Mail spool area looks ok\n", g_vprefix);
        ReleaseCurrentLock();
        return;
    }

    strcpy(mailserver, g_vmaildir[g_vsystemhardclass]);
    AddSlash(mailserver);
    strcat(mailserver, ".");

    /* Check directory is in place */
    MakeDirectoriesFor(mailserver, 'n');

    if (IsItemIn(g_vmounted, g_vmailserver)) {
        if (!g_silent) {
            Verbose("%s: Warning - the mail directory seems to be "
                    "mounted as on\n", g_vprefix);
            Verbose("%s: the remote mailserver and not on the correct "
                    "local directory\n", g_vprefix);
            Verbose("%s: Should strictly mount on %s\n",
                    g_vprefix, g_vmaildir[g_vsystemhardclass]);
        }
        ReleaseCurrentLock();
        return;
    }

    if (MatchStringInFstab("mail")) {
        if (!g_silent) {
            Verbose("%s: Warning - the mail directory seems to be mounted\n",
                    g_vprefix);
            Verbose("%s: in a funny way. I can find the string <mail> in %s\n",
                    g_vprefix, g_vfstab[g_vsystemhardclass]);
            Verbose("%s: but nothing is mounted on %s\n\n",
                    g_vprefix, g_vmaildir[g_vsystemhardclass]);
        }
        ReleaseCurrentLock();
        return;
    }

    printf("\n%s: Trying to mount %s\n", g_vprefix, g_vmailserver);

    if (! g_dontdo) {
        AddToFstab(mailhost, rmailpath, g_vmaildir[g_vsystemhardclass], 
                "rw", NULL, false);
    } else {
        printf("%s: Need to mount %s:%s on %s\n", 
                g_vprefix, mailhost, rmailpath, mailserver);
    }

    ReleaseCurrentLock();
}

/* ----------------------------------------------------------------- */

void
ExpiredUserCheck(char *spooldir, int always)
{
    Verbose("%s: Checking for expired users in %s\n", g_vprefix, spooldir);

    if (always || (strncmp(g_vmailserver, g_vfqname, 
                    strlen(g_vmailserver)) != 0)) {
        DIR *dirh;
        struct dirent *dirp;
        struct stat statbuf;

        if ((dirh = opendir(spooldir)) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Can't open spool directory %s", spooldir);
            CfLog(cfverbose, g_output, "opendir");
            return;
        }

        for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
            if (!SensibleFile(dirp->d_name, spooldir, NULL)) {
                continue;
            }

            strcpy(g_vbuff, spooldir);
            AddSlash(g_vbuff);
            strcat(g_vbuff, dirp->d_name);

            if (stat(g_vbuff, &statbuf) != -1) {
                if (getpwuid(statbuf.st_uid) == NULL) {
                    if (TrueVar("WarnNonOwnerMail") ||
                            TrueVar("WarnNonOwnerFiles")) {
                        snprintf(g_output,  CF_BUFSIZE*2, 
                                "File %s in spool dir %s is not owned "
                                "by any user",  dirp->d_name, spooldir);
                        CfLog(cferror, g_output, "");
                    }

                    if (TrueVar("DeleteNonOwnerMail") ||
                            TrueVar("DeleteNonOwnerFiles")) {
                        if (g_dontdo) {
                            printf("%s: Delete file %s\n", g_vprefix, g_vbuff);
                        } else {
                            snprintf(g_output,  CF_BUFSIZE*2,  
                                    "Deleting file %s in spool dir %s not "
                                    "owned by any user",  
                                    dirp->d_name, spooldir);
                            CfLog(cferror, g_output, "");

                            if (unlink(g_vbuff) == -1) {
                                CfLog(cferror, "", "unlink");
                            }
                        }
                    }
                }
            }

            if (strstr(dirp->d_name, "lock") || strstr(dirp->d_name, ".tmp")) {
                Verbose("Ignoring non-user file %s\n", dirp->d_name);
                continue;
            }

            if (getpwnam(dirp->d_name) == NULL) {
                if (TrueVar("WarnNonUserMail") || TrueVar("WarnNonUserFiles")) {
                    snprintf(g_output, CF_BUFSIZE*2, 
                            "File %s in spool dir %s is not "
                            "the name of a user", 
                            dirp->d_name, spooldir);
                    CfLog(cferror, g_output, "");
                }

                if (TrueVar("DeleteNonUserMail") ||
                        TrueVar("DeleteNonUserFiles")) {
                    if (g_dontdo) {
                        printf("%s: Delete file %s\n", g_vprefix, g_vbuff);
                    } else {
                        snprintf(g_output,  CF_BUFSIZE*2,  
                                "Deleting file %s in spool dir %s "
                                "(not a username)",  dirp->d_name, spooldir);
                        CfLog(cferror, g_output, "");

                        if (unlink(g_vbuff) == -1) {
                            CfLog(cferror, "", "unlink");
                        }
                    }
                }
            }
        }
        closedir(dirh);
        Verbose("%s: Done checking spool directory %s\n", g_vprefix, spooldir);
    }
}

/* ----------------------------------------------------------------- */

void
MountFileSystems(void)
{
    FILE *pp;
    int fd;
    struct stat statbuf;

    if (! g_gotmountinfo || g_dontdo) {
        return;
    }

    if (!IsPrivileged())                            {
        CfLog(cferror, "Only root can mount filesystems.\n", "");
        return;
    }


    if (!GetLock(ASUniqueName("domount"), "", g_vifelapsed, 
                g_vexpireafter, g_vuqname, g_cfstarttime)) {
        return;
    }

    if (g_vsystemhardclass == cfnt) {
        /* This is a shell script. Make sure it hasn't been compromised. */
        if (stat("/etc/fstab", &statbuf) == -1) {
            if ((fd = creat("/etc/fstab", 0755)) > 0) {
                write(fd, "#!/bin/sh\n\n", 10);
                close(fd);
            } else {
                if (statbuf.st_mode & (S_IWOTH | S_IWGRP)) {
                    CfLog(cferror, "File /etc/fstab was insecure. "
                            "Cannot mount filesystems.\n", "");
                    g_gotmountinfo = false;
                    return;
                }
            }
        }
    }

    signal(SIGALRM, (void *)TimeOut);
    alarm(g_rpctimeout);

    if ((pp = cfpopen(g_vmountcomm[g_vsystemhardclass], "r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, "Failed to open pipe from %s\n", 
                g_vmountcomm[g_vsystemhardclass]);
        CfLog(cferror, g_output, "popen");
        ReleaseCurrentLock();
        return;
    }

    while (!feof(pp)) {
        /* abortable */
        if (ferror(pp)) {
            CfLog(cfinform, "Error mounting filesystems\n", "ferror");
            break;
        }

        ReadLine(g_vbuff, CF_BUFSIZE, pp);

        /* abortable */
        if (ferror(pp)) {
            CfLog(cfinform, "Error mounting filesystems\n", "ferror");
            break;
        }

        if (strstr(g_vbuff, "already mounted") || strstr(g_vbuff, "exceeded") ||
                strstr(g_vbuff, "determined")) {
            continue;
        }

        if (strstr(g_vbuff, "not supported")) {
            continue;
        }

        if (strstr(g_vbuff, "denied") || strstr(g_vbuff, "RPC")) {
            CfLog(cfinform, "There was a mount error,  trying to mount one "
                    "of the filesystems on this host.\n", "");
            snprintf(g_output, CF_BUFSIZE*2, "%s\n", g_vbuff);
            CfLog(cfinform, g_output, "");
            g_gotmountinfo = false;
            break;
        }

        if (strstr(g_vbuff, "trying") && !strstr(g_vbuff, "NFS version 2")) {
            CfLog(cferror, "Aborted because MountFileSystems() went "
                    "into a retry loop.\n", "");
            g_gotmountinfo = false;
            break;
        }
    }

    alarm(0);
    signal(SIGALRM, SIG_DFL);
    cfpclose(pp);
    ReleaseCurrentLock();
}

/* ----------------------------------------------------------------- */

void
CheckRequired(void)
{
    struct Disk *rp;
    struct Item *ip;
    int matched = 0, varstring = 0, missing = 0;
    char path[CF_EXPANDSIZE], expand[CF_EXPANDSIZE];

    g_action=required;

    for (rp = g_vrequired; rp != NULL; rp = rp->next) {
        if (IsExcluded(rp->classes)) {
            continue;
        }

        if (rp->done == 'y' || strcmp(rp->scope, g_contextid)) {
            continue;
        } else {
            rp->done = 'y';
        }

        if (!GetLock(ASUniqueName("disks"), rp->name, rp->ifelapsed, 
                    rp->expireafter, g_vuqname, g_cfstarttime)) {
            continue;
        }

        ResetOutputRoute(rp->log, rp->inform);
        matched = varstring = false;

        for(ip = g_vbinservers; ip != NULL && (!matched); ip = ip->next) {

            ExpandVarstring(rp->name, expand, NULL);
            varstring = ExpandVarbinserv(expand, path, ip->name);

            /* simple or reduced item */
            if (RequiredFileSystemOkay(path)) {
                Verbose("Filesystem %s looks sensible\n", path);
                matched = true;

                if (rp->freespace == -1) {
                    AddMultipleClasses(rp->elsedef);
                }
            } else if (! varstring) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "The file %s does not exist or is suspicious.\n\n",
                        path);
                CfLog(cferror, g_output, "");

                /* Define the class if there was no freespace option. */
                if (rp->freespace == -1) {
                    AddMultipleClasses(rp->define);
                }
            }

            /* don't iterate over binservers if not var */
            if (! varstring) {
                break;
            }
        }

        if ((rp->freespace != -1)) {
            /* HvB : Bas van der Vlies */
            if (!CheckFreeSpace(path, rp)) {
                Verbose("Free space below %d, defining %s\n",
                        rp->freespace, rp->define);
                AddMultipleClasses(rp->define);
            } else {
                Verbose("Free space above %d, defining %s\n",
                        rp->freespace, rp->elsedef);
                AddMultipleClasses(rp->elsedef);
            }
        }

        if (matched == false && ip == NULL) {
            printf(" didn't find any file to match the required "
                    "filesystem %s\n", rp->name);
            missing++;
        }

        ReleaseCurrentLock();
    }

    if (missing) {
        time_t tloc;;

        if ((tloc = time((time_t *)NULL)) == -1) {
            printf("Couldn't read system clock\n");
        }
        snprintf(g_output, CF_BUFSIZE*2, "MESSAGE at %s\n\n", ctime(&tloc));
        CfLog(cferror, g_output, "");
        snprintf(g_output, CF_BUFSIZE*2, 
                "There are %d required file(system)s missing on host <%s>\n", 
                missing, g_vdefaultbinserver.name);
        CfLog(cferror, g_output, "");
        CfLog(cferror, "even after all mounts have been attempted.\n", "");
        CfLog(cferror, "This may be caused by a failure to mount "
                "a network filesystem (check exports)\n", "");
        snprintf(g_output,  CF_BUFSIZE*2, 
                "or because no valid server was specified in "
                "the program %s\n\n",  g_vinputfile);
        CfLog(cferror, g_output, "");

        ResetOutputRoute('d', 'd');
    }

    /* Look for any statistical gathering to be scheduled ... */

    /* This is too heavy to allow without locks */
    if (g_ignorelock) {
        return;
    }

    Verbose("Checking for filesystem scans...\n");

    for (rp = g_vrequired; rp != NULL; rp = rp->next) {
        if (IsExcluded(rp->classes)) {
            continue;
        }

        if (rp->scanarrivals != 'y') {
            continue;
        }

        ResetOutputRoute(rp->log, rp->inform);

        for(ip = g_vbinservers; ip != NULL && (!matched); ip = ip->next) {
            struct stat statbuf;
            DBT key, value;
            DB *dbp = NULL;
            DB_ENV *dbenv = NULL;
            char database[CF_MAXVARSIZE], canon[CF_MAXVARSIZE];
            int ifelapsed = rp->ifelapsed;

            if (ifelapsed < CF_WEEK) {
                Verbose("IfElapsed time is too short for these data - "
                        "changes only slowly\n");
                ifelapsed = CF_WEEK;
            }

            path[0] = expand[0] = '\0';

            ExpandVarstring(rp->name, expand, NULL);
            varstring = ExpandVarbinserv(expand, path, ip->name);

            if (lstat(path, &statbuf) == -1) {
                continue;
            }

            if (!GetLock(ASUniqueName("diskscan"), 
                        CanonifyName(rp->name), ifelapsed, 
                        rp->expireafter, g_vuqname, g_cfstarttime)) {
                continue;
            }

            snprintf(canon, CF_MAXVARSIZE-1, "%s", CanonifyName(path));
            snprintf(database, CF_MAXVARSIZE-1, "%s/scan:%s.db", 
                    g_vlockdir, canon);

            Verbose("Scanning filesystem %s for arrival processes...to %s\n",
                    path, database);

            unlink(database);

            if ((errno = db_create(&dbp, dbenv, 0)) != 0) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Couldn't open checksum database %s\n", g_checksumdb);
                CfLog(cferror, g_output, "db_open");
                return;
            }

#ifdef CF_OLD_DB
            if ((errno = dbp->open(dbp, database, NULL, DB_BTREE, 
                            DB_CREATE, 0644)) != 0)
#else
            if ((errno = dbp->open(dbp, NULL, database, NULL, DB_BTREE, 
                            DB_CREATE, 0644)) != 0)
#endif
            {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Couldn't open database %s\n", database);
                CfLog(cferror, g_output, "db_open");
                dbp->close(dbp, 0);
                continue;
            }


            chmod(database, 0644);
            RegisterRecursionRootDevice(statbuf.st_dev);
            ScanFileSystemArrivals(path, 0, &statbuf, dbp);

            dbp->close(dbp, 0);
            ReleaseCurrentLock();
        }
    }
}

/* ----------------------------------------------------------------- */

/* 
 * Here we start by scanning for any absolute path wildcards. After
 * that's finished, we go snooping around the homedirs.
 */

void
TidyFiles(void)
{
    char basename[CF_EXPANDSIZE], pathbuff[CF_BUFSIZE];
    struct TidyPattern *tlp;
    struct Tidy *tp;
    struct Item *ip1, *ip2;
    struct stat statbuf;
    int homesearch = false, pathsearch = false;

    Banner("Tidying Spool Directories");

    for (ip1 = g_spooldirlist; ip1 != NULL; ip1=ip1->next) {
        if (!IsExcluded(ip1->classes)) {
            ExpiredUserCheck(ip1->name, true);
        }
    }

    Banner("Tidying by directory");

    for (tp = g_vtidy; tp != NULL; tp=tp->next) {
        if (strncmp(tp->path, "home", 4)==0) {
            for (tlp = tp->tidylist; tlp != NULL; tlp=tlp->next) {
                if (!IsExcluded(tlp->classes)) {
                    homesearch = 1;
                    break;
                }
            }
            continue;
        }

        pathsearch = false;

        for (tlp = tp->tidylist; tlp != NULL; tlp=tlp->next) {
            if (IsExcluded(tlp->classes)) {
                continue;
            }
            pathsearch = true;
        }

        if (pathsearch && (tp->done == 'n')) {
            Debug("\nTidying from base directory %s\n", tp->path);
            basename[0] = '\0';
            ExpandWildCardsAndDo(tp->path, basename, TidyWrapper, tp);
            tp->done = 'y';
        } else {
            Debug("\nNo patterns active in base directory %s\n", tp->path);
        }
    }

    Debug2("End PATHTIDY:\n");

    Banner("Tidying home directories");

    /* 
     * If there are "home" wildcards Don't go rummaging around the disks
     */
    if (!homesearch) {
        Verbose("No home patterns to search\n");
        return;
    }

    if (!IsPrivileged())                            {
        CfLog(cferror, "Only root can delete others' files.\n", "");
        return;
    }

    if (!MountPathDefined()) {
        return;
    }

    for (ip1 = g_vhomepatlist; ip1 != NULL; ip1=ip1->next) {
        for (ip2 = g_vmountlist; ip2 != NULL; ip2=ip2->next) {
            if (IsExcluded(ip2->classes)) {
                continue;
            }
            pathbuff[0]='\0';
            basename[0]='\0';
            strcpy(pathbuff, ip2->name);
            AddSlash(pathbuff);
            strcat(pathbuff, ip1->name);

            ExpandWildCardsAndDo(pathbuff, basename, 
                    RecHomeTidyWrapper, NULL);
        }
    }

    Verbose("Done with home directories\n");
}

/* ----------------------------------------------------------------- */

void
Scripts(void)
{
    struct ShellComm *ptr;
    char line[CF_BUFSIZE];
    char comm[20], *sp;
    char execstr[CF_EXPANDSIZE];
    char chdir_buf[CF_EXPANDSIZE];
    char chroot_buf[CF_EXPANDSIZE];
    int print;
    mode_t maskval = 0;
    FILE *pp;
    int preview = false;

    for (ptr = g_vscript; ptr != NULL; ptr=ptr->next) {
        preview = (ptr->preview == 'y');

        if (IsExcluded(ptr->classes)) {
            continue;
        }

        if (ptr->done == 'y' || strcmp(ptr->scope, g_contextid)) {
            continue;
        } else {
            ptr->done = 'y';
        }

        ResetOutputRoute(ptr->log, ptr->inform);

        if (!GetLock(ASUniqueName("shellcommand"), 
                    CanonifyName(ptr->name), ptr->ifelapsed, 
                    ptr->expireafter, g_vuqname, g_cfstarttime)) {
            continue;
        }

        ExpandVarstring(ptr->name, execstr, NULL);

        snprintf(g_output, CF_BUFSIZE*2, 
                "Executing script %s...(timeout=%d,uid=%d,gid=%d)\n",
                execstr, ptr->timeout, ptr->uid, ptr->gid);
        CfLog(cfinform, g_output, "");

        if (g_dontdo && preview != 'y') {
            printf("%s: execute script %s\n", g_vprefix, execstr);
        } else {
            for (sp = execstr; *sp != ' ' && *sp != '\0'; sp++) { }

            if (sp - 10 >= execstr) {
                sp -= 10;   /* copy 15 most relevant characters of command */
            } else {
                sp = execstr;
            }

            memset(comm, 0, 20);
            strncpy(comm, sp, 15);

            if (ptr->timeout != 0) {
                signal(SIGALRM, (void *)TimeOut);
                alarm(ptr->timeout);
            }

            Verbose("(Setting umask to %o)\n", ptr->umask);
            maskval = umask(ptr->umask);

            if (ptr->umask == 0) {
                snprintf(g_output, CF_BUFSIZE*2, 
                    "Programming %s running with umask 0! Use umask= to set\n",
                    execstr);
                CfLog(cfsilent, g_output, "");
            }

            ExpandVarstring(ptr->chdir, chdir_buf, "");
            ExpandVarstring(ptr->chroot, chroot_buf, "");

            switch (ptr->useshell) {
            case 'y':
                pp = cfpopen_shsetuid(execstr, "r", ptr->uid, ptr->gid, 
                        chdir_buf, chroot_buf);
                break;
            default:
                pp = cfpopensetuid(execstr, "r", ptr->uid, ptr->gid, 
                        chdir_buf, chroot_buf);
                break;
            }

            if (pp == NULL) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Couldn't open pipe to command %s\n", execstr);
                CfLog(cferror, g_output, "popen");
                ResetOutputRoute('d', 'd');
                ReleaseCurrentLock();
                continue;
            }

            while (!feof(pp)) {
                /* abortable */
                if (ferror(pp))  {
                    snprintf(g_output, CF_BUFSIZE*2, 
                            "Shell command pipe %s\n", execstr);
                    CfLog(cferror, g_output, "ferror");
                    break;
                }

                ReadLine(line, CF_BUFSIZE-1, pp);

                if (strstr(line, "cfengine-die")) {
                    break;
                }

                /* abortable */
                if (ferror(pp)) {
                    snprintf(g_output, CF_BUFSIZE*2, 
                            "Shell command pipe %s\n", execstr);
                    CfLog(cferror, g_output, "ferror");
                    break;
                }

                if (preview == 'y') {
                    /*
                     * Preview script - try to parse line as log
                     * message.  If line does not parse, then log as
                     * error.
                     */

                    int i;
                    int level = cferror;
                    char *message = line;

                    /*
                     * Table matching cfoutputlevel enums to log
                     * prefixes.
                     */

                    char *prefixes[] =
                    {
                        ":silent:",
                        ":inform:",
                        ":verbose:",
                        ":editverbose:",
                        ":error:",
                        ":logonly:",
                    };
                    int precount = sizeof(prefixes)/sizeof(char *);

                    if (line[0] == ':') {
                        /*
                         * Line begins with colon - see if it matches a
                         * log prefix.
                         */

                        for (i=0; i<precount; i++) {
                            int prelen = 0;
                            prelen = strlen(prefixes[i]);
                            if (strncmp(line, prefixes[i], prelen) == 0) {
                                /*
                                 * Found log prefix - set logging level,
                                 * and remove the prefix from the log
                                 * message.
                                 */
                                level = i;
                                message += prelen;
                                break;
                            }
                        }
                    }

                    snprintf(g_output, CF_BUFSIZE, "%s (preview of %s)\n", 
                            message, comm);
                    CfLog(level, g_output, "");
                } else {
                    /*
                     * Dumb script - echo non-empty lines to standard output.
                     */

                    print = false;

                    for (sp = line; *sp != '\0'; sp++) {
                        if (! isspace((int)*sp)) {
                            print = true;
                            break;
                        }
                    }

                    if (print) {
                        printf("%s:%s: %s\n", g_vprefix, comm, line);
                    }
                }
            }

            cfpclose_def(pp, ptr->defines, ptr->elsedef);
        }

        if (ptr->timeout != 0) {
            alarm(0);
            signal(SIGALRM, SIG_DFL);
        }

        umask(maskval);

        snprintf(g_output, CF_BUFSIZE*2, "Finished script %s\n", execstr);
        CfLog(cfinform, g_output, "");

        ResetOutputRoute('d', 'd');
        ReleaseCurrentLock();
    }
}

/* ----------------------------------------------------------------- */

void
GetSetuidLog(void)
{
    struct Item *filetop = NULL;
    struct Item *ip;
    FILE *fp;
    char *sp;

    /* Ignore this if not root */
    if (!IsPrivileged()) {
        return;
    }

    if ((fp = fopen(g_vsetuidlog, "r")) == NULL) {

    } else {
        while (!feof(fp)) {
            ReadLine(g_vbuff, CF_BUFSIZE, fp);

            if (strlen(g_vbuff) == 0) {
                continue;
            }

            if ((ip = (struct Item *)malloc (sizeof(struct Item))) == NULL) {
                perror("malloc");
                FatalError("GetSetuidList() couldn't allocate memory #1");
            }

            if ((sp = malloc(strlen(g_vbuff)+2)) == NULL) {
                perror("malloc");
                FatalError("GetSetuidList() couldn't allocate memory #2");
            }

            if (filetop == NULL) {
                g_vsetuidlist = filetop = ip;
            } else {
                filetop->next = ip;
            }

            Debug2("SETUID-LOG: %s\n", g_vbuff);

            strcpy(sp, g_vbuff);
            ip->name = sp;
            ip->next = NULL;
            filetop = ip;
        }
        fclose(fp);
    }
}

/* ----------------------------------------------------------------- */

/* Check through file systems */
void
CheckFiles(void)
{
    struct File *ptr;
    char ebuff[CF_EXPANDSIZE];
    short savetravlinks = g_travlinks;
    short savekilloldlinks = g_killoldlinks;

    if (g_travlinks && (g_verbose || g_debug || g_d2)) {
        printf("(Default in switched to purge stale links...)\n");
    }

    for (ptr = g_vfile; ptr != NULL; ptr=ptr->next) {
        if (IsExcluded(ptr->classes)) {
            continue;
        }

        if (ptr->done == 'y' || strcmp(ptr->scope, g_contextid)) {
            continue;
        } else {
            ptr->done = 'y';
        }

        g_travlinks = savetravlinks;

        if (ptr->travlinks == 'T') {
            g_travlinks = true;
        } else if (ptr->travlinks == 'F') {
            g_travlinks = false;
        } else if (ptr->travlinks == 'K') {
            g_killoldlinks = true;
        }

        ResetOutputRoute(ptr->log, ptr->inform);

        if (strncmp(ptr->path, "home", 4) == 0) {
            CheckHome(ptr);
            continue;
        }

        Verbose("Checking file(s) in %s\n", ptr->path);

        ebuff[0] = '\0';

        ExpandWildCardsAndDo(ptr->path, ebuff, CheckFileWrapper, ptr);

        ResetOutputRoute('d', 'd');
        g_travlinks = savetravlinks;
        g_killoldlinks = savekilloldlinks;
    }
}

/* ----------------------------------------------------------------- */

void
SaveSetuidLog(void)
{
    FILE *fp;
    struct Item *ip;


    /* Ignore this if not root */
    if (!IsPrivileged()) {
        return;
    }

    if (! g_dontdo) {
        if ((fp = fopen(g_vsetuidlog, "w")) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2, "Can't open %s for writing\n", 
                    g_vsetuidlog);
            CfLog(cferror, g_output, "fopen");
            return;
        }

        Verbose("Saving the setuid log in %s\n", g_vsetuidlog);

        for (ip = g_vsetuidlist; ip != NULL; ip=ip->next) {
            if (!isspace((int)*(ip->name)) && strlen(ip->name) != 0) {
                fprintf(fp, "%s\n", ip->name);
                Debug2("SAVE-SETUID-LOG: %s\n", ip->name);
            }
        }

        fclose(fp);
        chmod(g_vsetuidlog, 0600);
    }
}

/* ----------------------------------------------------------------- */

void
DisableFiles(void)
{
    struct Disable *dp;
    struct stat statbuf;
    char workname[CF_EXPANDSIZE], path[CF_BUFSIZE];

    for (dp = g_vdisablelist; dp != NULL; dp=dp->next) {
        if (IsExcluded(dp->classes)) {
            continue;
        }

        if (dp->done == 'y' || strcmp(dp->scope, g_contextid)) {
            continue;
        } else {
            dp->done = 'y';
        }

        if (!GetLock(ASUniqueName("disable"), CanonifyName(dp->name), 
                    dp->ifelapsed, dp->expireafter, g_vuqname, 
                    g_cfstarttime)) {
            continue;
        }

        ExpandVarstring(dp->name, workname, NULL);

        ResetOutputRoute(dp->log, dp->inform);

        if (lstat(workname, &statbuf) == -1) {
            Verbose("Filetype %s,  %s is not there - ok\n", 
                    dp->type, workname);
            AddMultipleClasses(dp->elsedef);
            ReleaseCurrentLock();
            continue;
        }


        if (S_ISDIR(statbuf.st_mode)) {
            if ((strcmp(dp->type, "file") != 0) &&
                    (strcmp(dp->type, "link") != 0)) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Warning %s is a directory.\n", workname);
                CfLog(cferror, g_output, "");
                CfLog(cferror, 
                        "I refuse to rename/delete a directory!\n\n", "");
            } else {
                Verbose("Filetype %s, %s is not there - ok\n",
                        dp->type, workname);
            }
            ResetOutputRoute('d', 'd');
            ReleaseCurrentLock();
            continue;
        }

        Verbose("Disable checking %s\n", workname);

        if (S_ISLNK(statbuf.st_mode)) {
            if (strcmp(dp->type, "file") == 0) {
                Verbose("%s: %s is a link, not disabling\n",
                        g_vprefix, workname);
                ResetOutputRoute('d', 'd');
                ReleaseCurrentLock();
                continue;
            }

            memset(g_vbuff, 0, CF_BUFSIZE);

            if (readlink(workname, g_vbuff, CF_BUFSIZE-1) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "DisableFiles() can't read link %s\n", workname);
                CfLog(cferror, g_output, "readlink");
                ResetOutputRoute('d', 'd');
                ReleaseCurrentLock();
                continue;
            }

            if (dp->action == 'd') {
                printf("%s: Deleting link %s -> %s\n",
                        g_vprefix,  workname,  g_vbuff);

                if (! g_dontdo) {
                    if (unlink(workname) == -1) {
                        snprintf(g_output, CF_BUFSIZE*2, 
                                "Error while unlinking %s\n", workname);
                        CfLog(cferror, g_output, "unlink");
                        ResetOutputRoute('d', 'd');
                        ReleaseCurrentLock();
                        continue;
                    }

                    AddMultipleClasses(dp->defines);
                }
            } else {
                snprintf(g_output,  CF_BUFSIZE,  
                        "Warning - file %s exists\n",  workname);
                CfLog(cferror, g_output, "");
            }
        } else {
            if (! S_ISREG(statbuf.st_mode)) {
                Verbose("%s: %s is not a plain file - won't disable\n", 
                        g_vprefix, workname);
                ResetOutputRoute('d', 'd');
                ReleaseCurrentLock();
                continue;
            }

            if (strcmp(dp->type, "link") == 0) {
                Verbose("%s: %s is a file, not disabling\n",
                        g_vprefix, workname);
                ResetOutputRoute('d', 'd');
                ReleaseCurrentLock();
                continue;
            }

            if (stat(workname, &statbuf) == -1) {
                CfLog(cferror, "Internal; error in Disable\n", "");
                ResetOutputRoute('d', 'd');
                ReleaseCurrentLock();
                return;
            }

            if (dp->size != CF_NOSIZE) {
                switch (dp->comp) {
                case '<':
                    if (statbuf.st_size < dp->size) {
                        Verbose("cfng: %s is smaller than %d bytes\n", 
                                workname, dp->size);
                        break;
                    }
                    Verbose("Size is okay\n");
                    ResetOutputRoute('d', 'd');
                    ReleaseCurrentLock();
                    continue;

                case '=':
                    if (statbuf.st_size == dp->size) {
                        Verbose("cfng: %s is equal to %d bytes\n",
                                workname, dp->size);
                        break;
                    }
                    Verbose("Size is okay\n");
                    ResetOutputRoute('d', 'd');
                    ReleaseCurrentLock();
                    continue;

                default:
                    if (statbuf.st_size > dp->size) {
                        Verbose("cfng: %s is larger than %d bytes\n",
                                workname, dp->size);
                        break;
                    }
                    Verbose("Size is okay\n");
                    ResetOutputRoute('d', 'd');
                    ReleaseCurrentLock();
                    continue;
                }
            }

            if (dp->rotate == 0) {
                if (strlen(dp->destination) > 0) {
                    if (IsFileSep(dp->destination[0])) {
                        strncpy(path, dp->destination, CF_BUFSIZE-1);
                    } else {
                        strcpy(path, workname);
                        ChopLastNode(path);
                        AddSlash(path);

                        if (BufferOverflow(path, dp->destination)) {
                            snprintf(g_output, CF_BUFSIZE*2, 
                                    "Buffer overflow occurred while "
                                    "renaming %s\n", workname);
                            CfLog(cferror, g_output, "");
                            ResetOutputRoute('d', 'd');
                            ReleaseCurrentLock();
                            continue;
                        }
                        strcat(path, dp->destination);
                    }
                } else {
                    strcpy(path, workname);
                    strcat(path, ".cfdisabled");
                }

                snprintf(g_output, CF_BUFSIZE*2, 
                        "Disabling/renaming file %s to %s\n", workname, path);
                CfLog(cfinform, g_output, "");

                if (! g_dontdo) {
                    chmod(workname, (mode_t)0600);

                    if (! IsItemIn(g_vreposlist, path)) {
                        if (dp->action == 'd') {
                            if (rename(workname, path) == -1) {
                                snprintf(g_output, CF_BUFSIZE*2, 
                                        "Error occurred while renaming %s\n",
                                        workname);
                                CfLog(cferror, g_output, "rename");
                                ResetOutputRoute('d', 'd');
                                ReleaseCurrentLock();
                                continue;
                            }

                            if (Repository(path, dp->repository)) {
                                unlink(path);
                            }

                            AddMultipleClasses(dp->defines);
                        } else {
                            snprintf(g_output, CF_BUFSIZE, 
                                    "Warning - file %s exists "
                                    "(need to disable)", workname);
                            CfLog(cferror, g_output, "");
                        }
                    }
                }
            } else if (dp->rotate == CF_TRUNCATE) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Truncating (emptying) %s\n", workname);
                CfLog(cfinform, g_output, "");

                if (dp->action == 'd') {
                    if (! g_dontdo) {
                        TruncateFile(workname);
                        AddMultipleClasses(dp->defines);
                    }
                } else {
                    snprintf(g_output, CF_BUFSIZE, 
                            "File %s needs emptying", workname);
                    CfLog(cferror, g_output, "");
                }
            } else {
                snprintf(g_output, CF_BUFSIZE*2, "Rotating files %s by %d\n", 
                        workname, dp->rotate);
                CfLog(cfinform, g_output, "");

                if (dp->action == 'd') {
                    if (!g_dontdo) {
                        RotateFiles(workname, dp->rotate);
                        AddMultipleClasses(dp->defines);
                    } else {
                        snprintf(g_output, CF_BUFSIZE, 
                                "File %s needs rotating/emptying", workname);
                        CfLog(cferror, g_output, "");
                    }
                }
            }
        }
        ResetOutputRoute('d', 'd');
        ReleaseCurrentLock();
    }
}


/* ----------------------------------------------------------------- */

void
MountHomeBinServers(void)
{
    struct Mountables *mp;
    char host[CF_MAXVARSIZE];
    char mountdir[CF_BUFSIZE];
    char maketo[CF_BUFSIZE];

    /*
    * HvB: Bas van der Vlies
    */
    char mountmode[CF_BUFSIZE];

    if (! g_gotmountinfo) {
        CfLog(cfinform, "Incomplete mount info due to RPC failure.\n", "");
        snprintf(g_output, CF_BUFSIZE*2, 
                "%s will not be modified on this pass!\n\n", 
                g_vfstab[g_vsystemhardclass]);
        CfLog(cfinform, g_output, "");
        return;
    }

    if (!IsPrivileged())                            {
        CfLog(cferror, "Only root can mount filesystems.\n", "");
        return;
    }

    Banner("Checking home and binservers");

    for (mp = g_vmountables; mp != NULL; mp=mp->next) {
        sscanf(mp->filesystem, "%[^:]:%s", host, mountdir);

        if (mp->done == 'y' || strcmp(mp->scope, g_contextid)) {
            continue;
        } else {
            mp->done = 'y';
        }

        Debug("Mount: checking %s\n", mp->filesystem);

        strcpy(maketo, mountdir);

        if (maketo[strlen(maketo)-1] == '/') {
            strcat(maketo, ".");
        } else {
            strcat(maketo, "/.");
        }

        /* A host never mounts itself nfs */
        if (strcmp(host, g_vdefaultbinserver.name) == 0) {
            Debug("Skipping host %s\n", host);
            continue;
        }

        /* HvB: Bas van der Vlies */
        if ( mp->readonly ) {
            strcpy(mountmode, "ro");
        } else {
            strcpy(mountmode, "rw");
        }

        if (IsHomeDir(mountdir)) {
            if (!IsItemIn(g_vmounted, mp->filesystem) &&
                    IsClassedItemIn(g_vhomeservers, host)) {
                MakeDirectoriesFor(maketo, 'n');
                AddToFstab(host, mountdir, mountdir, mountmode, 
                        mp->mountopts, false);
            } else if (IsClassedItemIn(g_vhomeservers, host)) {
                AddToFstab(host, mountdir, mountdir, mountmode, 
                        mp->mountopts, true);
            }
        } else {
            if (!IsItemIn(g_vmounted, mp->filesystem) &&
                    IsClassedItemIn(g_vbinservers, host)) {
                MakeDirectoriesFor(maketo, 'n');
                AddToFstab(host, mountdir, mountdir, mountmode, 
                        mp->mountopts, false);
            }
            else if (IsClassedItemIn(g_vbinservers, host)) {
                AddToFstab(host, mountdir, mountdir, mountmode, 
                        mp->mountopts, true);
            }
        }
    }
}


/* ----------------------------------------------------------------- */

void
MountMisc(void)
{
    struct MiscMount *mp;
    char host[CF_MAXVARSIZE];
    char mountdir[CF_BUFSIZE];
    char maketo[CF_BUFSIZE];
    char mtpt[CF_BUFSIZE];

    if (! g_gotmountinfo) {
        CfLog(cfinform, "Incomplete mount info due to RPC failure.\n", "");
        snprintf(g_output, CF_BUFSIZE*2, 
                "%s will not be modified on this pass!\n\n", 
                g_vfstab[g_vsystemhardclass]);
        CfLog(cfinform, g_output, "");
        return;
    }

    if (!IsPrivileged()) {
        CfLog(cferror, "Only root can mount filesystems.\n", "");
        return;
    }

    Banner("Checking miscellaneous mountables:");

    for (mp = g_vmiscmount; mp != NULL; mp=mp->next) {
        sscanf(mp->from, "%[^:]:%s", host, mountdir);

        if (mp->done == 'y' || strcmp(mp->scope, g_contextid)) {
            continue;
        } else {
            mp->done = 'y';
        }

        strcpy(maketo, mp->onto);

        if (maketo[strlen(maketo)-1] == '/') {
            strcat(maketo, ".");
        } else {
            strcat(maketo, "/.");
        }

        /* A host never mounts itself nfs */
        if (strcmp(host, g_vdefaultbinserver.name) == 0) {
            continue;
        }

        snprintf(mtpt, CF_BUFSIZE, "%s:%s", host, mp->onto);

        if (!IsItemIn(g_vmounted, mtpt)) {
            MakeDirectoriesFor(maketo, 'n');
            AddToFstab(host, mountdir, mp->onto, mp->options, NULL, false);
        } else {
            AddToFstab(host, mountdir, mp->onto, mp->options, NULL, true);
        }
    }
}

/* ----------------------------------------------------------------- */

void
Unmount(void)
{
    struct UnMount *ptr;
    char comm[CF_BUFSIZE];
    char fs[CF_BUFSIZE];
    struct Item *filelist, *item;
    struct stat statbuf;
    FILE *pp;

    if (!IsPrivileged())                            {
        CfLog(cferror, "Only root can unmount filesystems.\n", "");
        return;
    }

    filelist = NULL;

    if (! LoadItemList(&filelist, g_vfstab[g_vsystemhardclass])) {
        snprintf(g_output, CF_BUFSIZE*2, "Couldn't open %s!\n", 
                g_vfstab[g_vsystemhardclass]);
        CfLog(cferror, g_output, "");
        return;
    }

    g_numberofedits = 0;

    for (ptr=g_vunmount; ptr != NULL; ptr=ptr->next) {
        if (IsExcluded(ptr->classes)) {
            continue;
        }

        if (ptr->done == 'y' || strcmp(ptr->scope, g_contextid)) {
            continue;
        } else {
            ptr->done = 'y';
        }

        if (!GetLock(ASUniqueName("unmount"), CanonifyName(ptr->name), 
                    ptr->ifelapsed, ptr->expireafter, g_vuqname, 
                    g_cfstarttime)) {
            continue;
        }

        fs[0] = '\0';

        sscanf(ptr->name, "%*[^:]:%s", fs);

        if (strlen(fs) == 0) {
            ReleaseCurrentLock();
            continue;
        }

        snprintf(g_output,  CF_BUFSIZE*2,  "Unmount filesystem %s on %s\n", 
                fs, ptr->name);
        CfLog(cfverbose, g_output, "");

        if (strcmp(fs, "/") == 0 || strcmp(fs, "/usr") == 0) {
            CfLog(cfinform, "Request to unmount / or /usr is refused!\n", "");
            ReleaseCurrentLock();
            continue;
        }

        if (IsItemIn(g_vmounted, ptr->name) && (! g_dontdo)) {
            snprintf(comm,  CF_BUFSIZE,  "%s %s", 
                    g_vunmountcomm[g_vsystemhardclass], fs);

            if ((pp = cfpopen(comm, "r")) == NULL) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Failed to open pipe from %s\n", 
                        g_vunmountcomm[g_vsystemhardclass]);
                CfLog(cferror, g_output, "");
                ReleaseCurrentLock();
                return;
            }

            ReadLine(g_vbuff, CF_BUFSIZE, pp);

            if (strstr(g_vbuff, "busy") || strstr(g_vbuff, "Busy")) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "umount warned that the device under %s\n", ptr->name);
                CfLog(cfinform, g_output, "");
                CfLog(cfinform, "was busy. Cannot unmount that device.\n", "");
                /* don't delete the mount dir when unmount's failed */
                ptr->deletedir = 'n';
            } else {
                snprintf(g_output, CF_BUFSIZE*2, "Unmounting %s\n", ptr->name);
                CfLog(cfinform, g_output, "");
                /* update mount record */
                DeleteItemStarting(&g_vmounted, ptr->name);  
            }

            cfpclose(pp);
        }

        if (ptr->deletedir == 'y') {
            if (stat(fs, &statbuf) != -1) {
                if ( ! S_ISDIR(statbuf.st_mode)) {
                    snprintf(g_output, CF_BUFSIZE*2, 
                            "Warning! %s was not a directory.\n", fs);
                    CfLog(cfinform, g_output, "");
                    CfLog(cfinform, "(Unmount) will not delete this!\n", "");
                    KillOldLink(fs, NULL);
                } else if (! g_dontdo) {
                    if (rmdir(fs) == -1) {
                        snprintf(g_output, CF_BUFSIZE*2, 
                                "Unable to remove the directory %s\n", fs);
                        CfLog(cferror, g_output, "rmdir");
                    } else {
                        snprintf(g_output,  CF_BUFSIZE*2, 
                                "Removing directory %s\n", 
                                ptr->name);
                        CfLog(cfinform, g_output, "");
                    }
                }
            }
        }

        if (ptr->deletefstab == 'y') {
            if (g_vsystemhardclass == aix) {
                strcpy (g_vbuff, fs);
                strcat (g_vbuff, ":");

                item = LocateNextItemContaining(filelist, g_vbuff);

                if (item == NULL || item->next == NULL) {
                    snprintf(g_output, CF_BUFSIZE*2, "Bad format in %s\n", 
                            g_vfstab[aix]);
                    CfLog(cferror, g_output, "");
                    continue;
                }

                DeleteItem(&filelist, item->next);

                while (strstr(item->next->name, "=")) {
                    /* DeleteItem(NULL) is harmless */
                    DeleteItem(&filelist, item->next);
                }
            } else {
                Debug("Trying to delete filesystem %s from list\n", ptr->name);

                /* ensure name is not just a substring */
                if (g_vsystemhardclass == ultrx)   {
                    strcpy (g_vbuff, ptr->name);
                    strcat (g_vbuff, ":");
                    DeleteItemContaining(&filelist, g_vbuff);
                } else {
                    switch (g_vsystemhardclass) {
                    case unix_sv:
                    case solarisx86:
                    case solaris:
                        /* find fs in proper context
                         * ("<host>:<remotepath> <-> <fs> ") */
                        snprintf(g_vbuff, CF_BUFSIZE, 
                                "[^:]+:[^ \t]+[ \t]+[^ \t]+[ \t]+%s[ \t]", fs);
                        break;
                    default:
                        /* find fs in proper context
                         * ("<host>:<remotepath> <fs> ") */
                        snprintf(g_vbuff, CF_BUFSIZE, 
                                "[^:]+:[^ \t]+[ \t]+%s[ \t]", fs);
                        break;
                    }
                    item = LocateItemContainingRegExp(filelist, g_vbuff);
                    DeleteItem(&filelist, item);
                }
            }
        }

        ReleaseCurrentLock();
    }

    if ((! g_dontdo) && (g_numberofedits > 0)) {
        SaveItemList(filelist, g_vfstab[g_vsystemhardclass], g_vrepository);
    }

    DeleteItemList(filelist);
}

/* ----------------------------------------------------------------- */

void
EditFiles(void)
{
    struct Edit *ptr;
    struct stat statbuf;

    Debug("Editfiles()\n");

    for (ptr=g_veditlist; ptr!=NULL; ptr=ptr->next) {
        if (ptr->done == 'y' || strcmp(ptr->scope, g_contextid)) {
            continue;
        }

        if (strncmp(ptr->fname, "home", 4) == 0) {
            DoEditHomeFiles(ptr);
        } else {
            if (lstat(ptr->fname, &statbuf) != -1) {
                if (S_ISDIR(statbuf.st_mode)) {
                    DoRecursiveEditFiles(ptr->fname, ptr->recurse, 
                            ptr, &statbuf);
                } else {
                    WrapDoEditFile(ptr, ptr->fname);
                }
            } else {
                DoEditFile(ptr, ptr->fname);
            }
        }
    }
    g_editverbose = false;
}

/* ----------------------------------------------------------------- */

void
CheckResolv(void)
{
    struct Item *filebase = NULL, *referencefile = NULL;
    struct Item *ip;
    char ch;
    int fd, existed = true;

    Verbose("Checking config in %s\n", g_vresolvconf[g_vsystemhardclass]);

    if (g_vdomain == NULL) {
        CfLog(cferror, "Domain name not specified. "
                "Can't configure resolver\n", "");
        return;
    }

    if (!IsPrivileged())                            {
        CfLog(cferror, "Only root can configure the resolver.\n", "");
        return;
    }

    if (! LoadItemList(&referencefile, g_vresolvconf[g_vsystemhardclass])) {
        snprintf(g_output, CF_BUFSIZE*2, "Trying to create %s\n", 
                g_vresolvconf[g_vsystemhardclass]);
        CfLog(cfinform, g_output, "");
        existed = false;

        if ((fd = creat(g_vresolvconf[g_vsystemhardclass], 0644)) == -1) {
            snprintf(g_output, CF_BUFSIZE*2, "Unable to create file %s\n", 
                    g_vresolvconf[g_vsystemhardclass]);
            CfLog(cferror, g_output, "creat");
            return;
        } else {
            close(fd);
        }
    }

    if (existed) {
        LoadItemList(&filebase, g_vresolvconf[g_vsystemhardclass]);
    }

    for (ip = filebase; ip != NULL; ip=ip->next) {
        if (strlen(ip->name) == 0) {
            continue;
        }

        ch = *(ip->name+strlen(ip->name)-2);

        if (isspace((int)ch)) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Deleted line %s ended with a space in %s.\n", 
                    ip->name, g_vresolvconf[g_vsystemhardclass]);
            CfLog(cfinform, g_output, "");
            CfLog(cfinform, "The resolver doesn't understand this.\n", "");
            DeleteItem(&filebase, ip);
        }
        else if (isspace((int)*(ip->name))) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Deleted line %s started with a space in %s.\n", 
                    ip->name, g_vresolvconf[g_vsystemhardclass]);
            CfLog(cfinform, g_output, "");
            CfLog(cfinform, "The resolver doesn't understand this.\n", "");
            DeleteItem(&filebase, ip);
        }
    }

    if (strcmp(g_vdomain, "undefined.domain") != 0) {
        DeleteItemStarting(&filebase, "domain");
    }

    while(DeleteItemStarting(&filebase, "search")) { }

    if (OptionIs(g_contextid, "EmptyResolvConf",  true)) {
        while (DeleteItemStarting(&filebase, "nameserver ")) { }
    }

    EditItemsInResolvConf(g_vresolve, &filebase);

    if (strcmp(g_vdomain, "undefined.domain") != 0) {
        snprintf(g_vbuff, CF_BUFSIZE, "domain %s", ToLowerStr(g_vdomain));
        PrependItem(&filebase, g_vbuff, NULL);
    }

    if (g_dontdo) {
        printf("Check %s for editing\n", g_vresolvconf[g_vsystemhardclass]);
    } else if (!ItemListsEqual(filebase, referencefile)) {
        SaveItemList(filebase, g_vresolvconf[g_vsystemhardclass],
                g_vrepository);
        chmod(g_vresolvconf[g_vsystemhardclass], g_defaultsystemmode);
    } else {
        Verbose("cfng: %s is okay\n", g_vresolvconf[g_vsystemhardclass]);
    }

    DeleteItemList(filebase);
    DeleteItemList(referencefile);
}


/* ----------------------------------------------------------------- */

void
MakeImages(void)
{
    struct Image *ip;
    struct Item *svp;
    struct stat statbuf;
    struct servent *serverent;
    int savesilent;
    char path[CF_EXPANDSIZE];
    char destination[CF_EXPANDSIZE];
    char server[CF_EXPANDSIZE];
    char listserver[CF_EXPANDSIZE];

    if ((serverent = getservbyname(CFENGINE_SERVICE, "tcp")) == NULL) {
        CfLog(cfverbose, "Remember to register cfengine in "
                "/etc/services: cfengine 5308/tcp\n", "getservbyname");
        g_portnumber = htons((unsigned short)5308);
    } else {
        /* already in network order */
        g_portnumber = (unsigned short)(serverent->s_port);
    }

    /* order servers */
    for (svp = g_vserverlist; svp != NULL; svp=svp->next) {
        /* Global input/output channel */
        g_conn = NewAgentConn();  

        if (g_conn == NULL) {
            return;
        }

        ExpandVarstring(svp->name, listserver, NULL);

        for (ip = g_vimage; ip != NULL; ip=ip->next) {
            ExpandVarstring(ip->server, server, NULL);
            ExpandVarstring(ip->path, path, NULL);
            ExpandVarstring(ip->destination, destination, NULL);

            /*
             * Group together similar hosts so we can do multiple
             * transactions in one connection.
             */
            if (strcmp(listserver,server) != 0) {
                continue;
            }

            if (IsExcluded(ip->classes)) {
                continue;
            }

            if (ip->done == 'y' || strcmp(ip->scope, g_contextid)) {
                continue;
            } else {
                ip->done = 'y';
            }

            Verbose("Checking copy from %s:%s to %s\n",
                    server, path, destination);

            if (!OpenServerConnection(ip)) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Unable to establish connection with %s (failover)\n", 
                        listserver);
                CfLog(cfinform, g_output, "");
                AddMultipleClasses(ip->failover);
                continue;
            }

            if (g_authenticated) {
                Debug("Authentic connection verified\n");
            }

            g_imagebackup = true;

            savesilent = g_silent;

            if (strcmp(ip->action, "silent") == 0) {
                g_silent = true;
            }

            ResetOutputRoute(ip->log, ip->inform);

            if (cfstat(path, &statbuf, ip) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, "Can't stat %s in copy\n", path);
                CfLog(cfinform, g_output, "");
                ReleaseCurrentLock();
                g_silent = savesilent;
                ResetOutputRoute('d', 'd');
                continue;
            }

            /* Unique ID for copy locking */
            snprintf(g_vbuff, CF_BUFSIZE, "%.50s.%.50s", path, destination);

            if (!GetLock(ASUniqueName("copy"), CanonifyName(g_vbuff), 
                        ip->ifelapsed, ip->expireafter, g_vuqname, 
                        g_cfstarttime)) {
                g_silent = savesilent;
                ResetOutputRoute('d','d');
                continue;
            }

            g_imagebackup = ip->backup;

            if (strncmp(destination, "home", 4) == 0) {
                /* Don't send home backups to repository */
                g_homecopy = true;
                CheckHomeImages(ip);
                g_homecopy = false;
            } else {
                if (S_ISDIR(statbuf.st_mode)) {
                    if (ip->purge == 'y') {
                        Verbose("%s: (Destination purging enabled)\n", 
                                g_vprefix);
                    }
                    RegisterRecursionRootDevice(statbuf.st_dev);
                    RecursiveImage(ip, path, destination, ip->recurse);
                } else {
                    if (! MakeDirectoriesFor(destination, 'n')) {
                        ReleaseCurrentLock();
                        g_silent = savesilent;
                        ResetOutputRoute('d', 'd');
                        continue;
                    }

                    CheckImage(path, destination, ip);
                }
            }

            ReleaseCurrentLock();
            g_silent = savesilent;
            ResetOutputRoute('d', 'd');
        }

        CloseServerConnection();
        DeleteAgentConn(g_conn);
    }
}

/* ----------------------------------------------------------------- */

void
ConfigureInterfaces(void)
{
    struct Interface *ifp;

    Banner("Network interface configuration");

    if (GetLock("netconfig", g_vifdev[g_vsystemhardclass], g_vifelapsed, 
                g_vexpireafter, g_vuqname, g_cfstarttime)) {
        if (strlen(g_vnetmask) != 0) {
            IfConf(g_vifdev[g_vsystemhardclass], g_vipaddress, g_vnetmask,
                    g_vbroadcast);
        }

        SetDefaultRoute();
        ReleaseCurrentLock();
    }

    for (ifp = g_viflist; ifp != NULL; ifp=ifp->next) {
        if (!GetLock("netconfig", ifp->ifdev, g_vifelapsed, g_vexpireafter, 
                    g_vuqname, g_cfstarttime)) {
            continue;
        }

        if (ifp->done == 'y' || strcmp(ifp->scope, g_contextid)) {
            continue;
        } else {
            ifp->done = 'y';
        }

        IfConf(ifp->ifdev, ifp->ipaddress, ifp->netmask, ifp->broadcast);
        SetDefaultRoute();
        ReleaseCurrentLock();
    }
}

/* ----------------------------------------------------------------- */

void
CheckTimeZone(void)
{
    struct Item *ip;
    char tz[CF_MAXVARSIZE];

    if (g_vtimezone == NULL) {
        CfLog(cferror, "Program does not define a timezone", "");
        return;
    }

    for (ip = g_vtimezone; ip != NULL; ip=ip->next) {
#ifdef NT

        tzset();
        strcpy(tz, timezone());

#else
#ifndef AOS
#ifndef SUN4

        tzset();
        strcpy(tz, tzname[0]);

#else

        if ((tloc = time((time_t *)NULL)) == -1) {
            printf("Couldn't read system clock\n\n");
        }
        strcpy(tz, localtime(&tloc)->tm_zone);

#endif /* SUN4 */
#endif /* AOS  */
#endif /* NT */

        if (TZCheck(tz, ip->name)) {
            return;
        }
    }

    snprintf(g_output, CF_BUFSIZE*2, 
            "Time zone was %s which is not in the list of acceptable values",
            tz);

    CfLog(cferror, g_output, "");
}

/* ----------------------------------------------------------------- */

void
CheckProcesses(void)
{
    struct Process *pp;
    struct Item *procdata = NULL;
    char *psopts = g_vpsopts[g_vsystemhardclass];


    if (!LoadProcessTable(&procdata, psopts)) {
        CfLog(cferror, "Unable to read the process table\n", "");
        return;
    }

    for (pp = g_vproclist; pp != NULL; pp=pp->next) {
        if (IsExcluded(pp->classes)) {
            continue;
        }

        if (pp->done == 'y' || strcmp(pp->scope, g_contextid) != 0) {
            continue;
        } else {
            pp->done = 'y';
        }

        snprintf(g_vbuff, CF_BUFSIZE-1, "proc-%s-%s", pp->expr, pp->restart);

        if (!GetLock(ASUniqueName("processes"), CanonifyName(g_vbuff), 
                    pp->ifelapsed, pp->expireafter, g_vuqname, g_cfstarttime)) {
            continue;
        }

        ResetOutputRoute(pp->log, pp->inform);

        if (strcmp(pp->expr, "SetOptionString") == 0) {
            psopts = pp->restart;
            DeleteItemList(procdata);
            procdata = NULL;
            if (!LoadProcessTable(&procdata, psopts)) {
                CfLog(cferror, "Unable to read the process table\n", "");
            }
        } else {
            DoProcessCheck(pp, procdata);
        }

        ResetOutputRoute('d', 'd');
        ReleaseCurrentLock();
    }
}

/* ----------------------------------------------------------------- */

void
CheckPackages(void)
{
    struct Package *ptr;
    enum cmpsense result;
    int match = 0;


    for (ptr = g_vpkg; ptr != NULL; ptr=ptr->next) {
        if (ptr->done == 'y' || strcmp(ptr->scope, g_contextid) != 0) {
            continue;
        } else {
            ptr->done = 'y';
        }

        if (IsExcluded(ptr->classes)) {
            continue;
        }

        if (!GetLock(ASUniqueName("packages"), CanonifyName(ptr->name), 
                    ptr->ifelapsed, ptr->expireafter, g_vuqname, 
                    g_cfstarttime)) {
            continue;
        }

        switch(ptr->pkgmgr) {
        case pkgmgr_rpm:
            match = RPMPackageCheck(ptr->name, ptr->ver, ptr->cmp);
            break;
        case pkgmgr_dpkg:
            match = DPKGPackageCheck(ptr->name, ptr->ver, ptr->cmp);
            break;
        default:
            /* UGH!  This should *never* happen.  GetPkgMgr() and
                * InstallPackagesItem() should have caught this before it
                * was ever installed!!!
                * */
            snprintf(g_output, CF_BUFSIZE, "Internal error! "
                    "Tried to check package %s in an unknown "
                    "database: %d.  This should never happen!\n", 
                    ptr->name, ptr->pkgmgr);
            CfLog(cferror, g_output, "");
            break;
        }

        if (match) {
            AddMultipleClasses(ptr->defines);
        } else {
            AddMultipleClasses(ptr->elsedef);
        }

        ptr->done = 'y';
        ReleaseCurrentLock();
    }
}

/* ----------------------------------------------------------------- */

void
DoMethods(void)
{
    struct Method *ptr;
    struct Item *ip;
    char label[CF_BUFSIZE];
    unsigned char digest[EVP_MAX_MD_SIZE+1];

    Banner("Dispatching new methods");

    for (ptr = g_vmethods; ptr != NULL; ptr=ptr->next) {
        if (ptr->done == 'y' || strcmp(ptr->scope, g_contextid) != 0) {
            continue;
        } else {
            ptr->done = 'y';
        }

        ChecksumList(ptr->send_args, digest, 'm');
        snprintf(label, CF_BUFSIZE-1, "%s/rpc_in/localhost_localhost_%s_%s", 
                g_vlockdir, ptr->name, ChecksumPrint('m', digest));

        if (!GetLock(ASUniqueName("methods-dispatch"), CanonifyName(label), 
                    ptr->ifelapsed, ptr->expireafter, g_vuqname, 
                    g_cfstarttime)) {
            continue;
        }

        DispatchNewMethod(ptr);

        ptr->done = 'y';
        ReleaseCurrentLock();
    }

    Banner("Evaluating incoming methods that policy accepts...");

    for (ip = GetPendingMethods(CF_METHODEXEC); ip != NULL; ip=ip->next) {
        /* Call child process to execute method*/
        EvaluatePendingMethod(ip->name);
    }

    DeleteItemList(ip);

    Banner("Fetching replies to finished methods");

    for (ip = GetPendingMethods(CF_METHODREPLY); ip != NULL; ip=ip->next) {
        if (ParentLoadReplyPackage(ip->name)) { }
    }

    DeleteItemList(ip);
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

int
RequiredFileSystemOkay(char *name)
{
    struct stat statbuf, localstat;
    DIR *dirh;
    struct dirent *dirp;
    long sizeinbytes = 0, filecount = 0;
    char buff[CF_BUFSIZE];

    Debug("Checking required filesystem %s\n", name);

    if (stat(name, &statbuf) == -1) {
        return(false);
    }

    if (S_ISLNK(statbuf.st_mode)) {
        KillOldLink(name, NULL);
        return(true);
    }

    if (S_ISDIR(statbuf.st_mode)) {
        if ((dirh = opendir(name)) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Can't open directory %s which checking required/disk\n", 
                    name);
            CfLog(cferror, g_output, "opendir");
            return false;
        }

        for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
            if (!SensibleFile(dirp->d_name, name, NULL)) {
                continue;
            }

            filecount++;

            strcpy(buff, name);

            if (buff[strlen(buff)] != '/') {
                strcat(buff, "/");
            }

            strcat(buff, dirp->d_name);

            if (lstat(buff, &localstat) == -1) {
                if (S_ISLNK(localstat.st_mode)) {
                    KillOldLink(buff, NULL);
                    continue;
                }

                snprintf(g_output, CF_BUFSIZE*2, 
                        "Can't stat %s in required/disk\n", buff);
                CfLog(cferror, g_output, "lstat");
                continue;
            }

            sizeinbytes += localstat.st_size;
        }

        closedir(dirh);

        if (sizeinbytes < 0) {
            Verbose("Internal error: count of byte size was less than zero!\n");
            return true;
        }

        if (sizeinbytes < g_sensiblefssize) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "File system %s is suspiciously small! (%d bytes)\n", 
                    name, sizeinbytes);
            CfLog(cferror, g_output, "");
            return(false);
        }

        if (filecount < g_sensiblefilecount) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Filesystem %s has only %d files/directories.\n", 
                    name, filecount);
            CfLog(cferror, g_output, "");
            return(false);
        }
    }
    return(true);
}


/* ----------------------------------------------------------------- */

int
ScanFileSystemArrivals(char *name, int rlevel, struct stat *sb, DB *dbp)
{
    DIR *dirh;
    int goback;
    struct dirent *dirp;
    char pcwd[CF_BUFSIZE];
    struct stat statbuf;

    if (rlevel > CF_RECURSION_LIMIT) {
        snprintf(g_output, CF_BUFSIZE*2, "WARNING: Very deep nesting "
                "of directories (>%d deep): %s (Aborting files)",
                rlevel, name);
        CfLog(cferror, g_output, "");
        return false;
    }

    memset(pcwd, 0, CF_BUFSIZE);

    Debug("ScanFileSystemArrivals(%s)\n", name);

    if (!DirPush(name, sb)) {
        return false;
    }

    if ((dirh = opendir(".")) == NULL) {
        return true;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name, name, NULL)) {
            continue;
        }

        /* Assemble pathname */
        strcpy(pcwd, name);
        AddSlash(pcwd);

        if (BufferOverflow(pcwd, dirp->d_name)) {
            closedir(dirh);
            return true;
        }

        strcat(pcwd, dirp->d_name);

        if (lstat(dirp->d_name, &statbuf) == -1) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "RecursiveCheck was looking at %s when this happened:\n",
                    pcwd);
            CfLog(cferror, g_output, "lstat");
            continue;
        }

        if (DeviceChanged(statbuf.st_dev)) {
            Verbose("Skipping %s on different device\n", pcwd);
            continue;
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (IsMountedFileSystem(&statbuf, pcwd, rlevel)) {
                continue;
            } else {
                RecordFileSystemArrivals(dbp, sb->st_mtime);
                goback = ScanFileSystemArrivals(pcwd, rlevel+1, &statbuf, dbp);
                DirPop(goback, name, sb);
            }
        } else {
            RecordFileSystemArrivals(dbp, sb->st_mtime);
        }
    }
    closedir(dirh);
    return true;
}

/* ----------------------------------------------------------------- */

void
InstallMountedItem(char *host, char *mountdir)
{
    char buf[CF_BUFSIZE];

    strcpy (buf, host);
    strcat (buf, ":");
    strcat (buf, mountdir);

    if (IsItemIn(g_vmounted, buf)) {
        if (! g_silent || !g_warnings) {
            if (!strstr(buf, "swap")) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "WARNING mount item %s\n", buf);
                CfLog(cferror, g_output, "");
                CfLog(cferror, "is mounted multiple times!\n", "");
            }
        }
    }

    AppendItem(&g_vmounted, buf, NULL);
}

/* ----------------------------------------------------------------- */


void AddToFstab(char *host, char *rmountpt, char *mountpt,
        char *mode, char *options, int ismounted)
{
    char fstab[CF_BUFSIZE];
    char *opts;
    FILE *fp;

    Debug("AddToFstab(%s)\n", mountpt);


    if ( options != NULL) {
        opts = options;
    } else {
        opts = g_vmountopts[g_vsystemhardclass];
    }

    switch (g_vsystemhardclass) {
    case osf:
    case bsd4_3:
    case irix:
    case irix4:
    case irix64:
    case sun3:
    case aos:
    case nextstep:
    case newsos:
    case sun4:
        snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s %s\t%s,%s 0 0",
                host, rmountpt, mountpt, g_vnfstype, mode, opts);
        break;
    case crayos:
        snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s %s\t%s,%s",
                host, rmountpt, mountpt, ToUpperStr(g_vnfstype), mode, opts);
        break;
    case ultrx:
        snprintf(fstab, CF_BUFSIZE, "%s@%s:%s:%s:0:0:%s:%s",
                rmountpt, host, mountpt, mode, g_vnfstype, opts);
        break;
    case hp:
        snprintf(fstab, CF_BUFSIZE, "%s:%s %s \t %s \t %s,%s 0 0",
                host, rmountpt, mountpt, g_vnfstype, mode, opts);
        break;
    case aix:
        snprintf(fstab, CF_BUFSIZE, "%s:\n\tdev\t= %s\n\ttype\t= "
                "%s\n\tvfs\t= %s\n\tnodename\t= %s\n\tmount\t= true\n"
                "\toptions\t= %s,%s\n\taccount\t= false\n\n",
                mountpt, rmountpt, g_vnfstype, g_vnfstype,
                host, mode, opts);
        break;
    case GnU:
    case linuxx:
        snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s \t %s \t %s,%s",
                host, rmountpt, mountpt, g_vnfstype, mode, opts);
        break;
    case netbsd:
    case openbsd:
    case bsd_i:
    case freebsd:
        snprintf(fstab, CF_BUFSIZE, "%s:%s \t %s \t %s \t %s,%s 0 0",
                host, rmountpt, mountpt, g_vnfstype, mode, opts);
        break;
    case unix_sv:
    case solarisx86:
    case solaris:
        snprintf(fstab, CF_BUFSIZE,"%s:%s - %s %s - yes %s,%s",
                host, rmountpt, mountpt, g_vnfstype, mode, opts);
        break;
    case cfnt:
        snprintf(fstab, CF_BUFSIZE, "/bin/mount %s:%s %s",
                host,rmountpt,mountpt);
        break;
    case cfsco:
        CfLog(cferror, "Don't understand filesystem format on SCO, no data","");
        break;
    case unused1:
    case unused2:
    case unused3:
    default:
        FatalError("AddToFstab(): unknown hard class detected!\n");
        break;
    }

    if (MatchStringInFstab(mountpt)) {
        /* if the fstab entry has changed, remove the old entry and update */
        if (!MatchStringInFstab(fstab)) {

            struct UnMount *saved_VUNMOUNT = g_vunmount;
            char mountspec[MAXPATHLEN];
            struct Item *mntentry = NULL;
            struct UnMount cleaner;

            snprintf(g_output, CF_BUFSIZE*2, 
                    "Removing \"%s\" entry from %s to allow update:\n", 
                    mountpt, g_vfstab[g_vsystemhardclass]);
            CfLog(cfinform, g_output, "");
            CfLog(cfinform, "---------------------------------------------------", "");

            /* delete current fstab entry and unmount if necessary */

            snprintf(mountspec, CF_BUFSIZE, ".+:%s", mountpt);
            mntentry = LocateItemContainingRegExp(g_vmounted, mountspec);

            if (mntentry) {
                /* extract current host */
                sscanf(mntentry->name, "%[^:]:", mountspec);
                strcat(mountspec, ":");
                strcat(mountspec, mountpt);
            } else {
                /* mountpt isn't mounted,  so Unmount can use dummy host name */
                snprintf(mountspec, CF_BUFSIZE, "host:%s", mountpt);
            }

            /* delete current fstab entry and unmount if necessary
             * (don't rmdir) */
            cleaner.name        = mountspec;
            cleaner.classes     = NULL;
            cleaner.deletedir   = 'n';
            cleaner.deletefstab = 'y';
            cleaner.force       = 'n';
            cleaner.done        = 'n';
            cleaner.scope       = g_contextid;
            cleaner.next        = NULL;

            g_vunmount = &cleaner;
            Unmount();
            g_vunmount = saved_VUNMOUNT;
            CfLog(cfinform,"---------------------------------------------------","");
        } else {
            /* 
             * no need to update fstab - this mount entry is already
             * there warn if entry's already in the fstab but hasn't
             * been mounted 
             */
            if (!ismounted && !g_silent && !strstr(mountpt, "cdrom")) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Warning the file system %s seems to be in %s\n", 
                        mountpt, g_vfstab[g_vsystemhardclass]);
                CfLog(cfinform, g_output, "");
                snprintf(g_output, CF_BUFSIZE*2, 
                        "already, but I was not able to mount it.\n");
                CfLog(cfinform, g_output, "");
                snprintf(g_output,  CF_BUFSIZE*2,  
                        "Check the exports file on host %s? "
                        "Check for file with same name as dir?\n", host);
                CfLog(cfinform, g_output, "");
            }

            return;
        }
    }

    if (g_dontdo) {
        printf("%s: add filesystem to %s\n",
                g_vprefix, g_vfstab[g_vsystemhardclass]);
        printf("%s: %s\n",g_vprefix,fstab);
    } else {
        if ((fp = fopen(g_vfstab[g_vsystemhardclass],"a")) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2, "Can't open %s for appending\n", 
                    g_vfstab[g_vsystemhardclass]);
            CfLog(cferror, g_output, "fopen");
            ReleaseCurrentLock();
            return;
        }

        snprintf(g_output, CF_BUFSIZE*2, "Adding filesystem to %s\n", 
                g_vfstab[g_vsystemhardclass]);
        CfLog(cfinform, g_output, "");
        snprintf(g_output, CF_BUFSIZE*2, "%s\n", fstab);
        CfLog(cfinform, g_output, "");
        fprintf(fp, "%s\n", fstab);
        fclose(fp);

        chmod(g_vfstab[g_vsystemhardclass], g_defaultsystemmode);
    }
}


/* ----------------------------------------------------------------- */

int CheckFreeSpace (char *file, struct Disk *disk_ptr)
{
    struct stat statbuf;
    int free;
    int kilobytes;

    if (stat(file, &statbuf) == -1) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Couldn't stat %s checking diskspace\n", file);
        CfLog(cferror, g_output, "");
        return true;
    }

    /* HvB : Bas van der Vlies
        if force is specified then skip this check if this
        is on the file server.
    */
    if ( disk_ptr->force != 'y' ) {
        if (IsMountedFileSystem(&statbuf, file, 1)) {
            return true;
        }
    }

    kilobytes = disk_ptr->freespace;

    /* percentage */
    if (kilobytes < 0) {
        free = GetDiskUsage(file, cfpercent);
        kilobytes = -1 * kilobytes;
        if (free < kilobytes) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Free disk space is under %d%% for partition\n", kilobytes);
            CfLog(cfinform, g_output, "");
            snprintf(g_output,  CF_BUFSIZE*2, "containing %s (%d%% free)\n", 
                    file,  free);
            CfLog(cfinform, g_output, "");
            return false;
        }
    } else {
        free = GetDiskUsage(file, cfabs);

        if (free < kilobytes) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Disk space under %d kB for partition\n", kilobytes);
            CfLog(cfinform, g_output, "");
            snprintf(g_output, CF_BUFSIZE*2, 
                    "containing %s (%d kB free)\n", file, free);
            CfLog(cfinform, g_output, "");
            return false;
        }
    }

    return true;
}

/* ----------------------------------------------------------------- */

/* iterate check over homedirs */

void
CheckHome(struct File *ptr)
{
    struct Item *ip1, *ip2;
    char basename[CF_EXPANDSIZE],pathbuff[CF_BUFSIZE];

    Debug("CheckHome(%s)\n", ptr->path);

    if (!IsPrivileged()) {
        CfLog(cferror, "Only root can check others' files.\n", "");
        return;
    }

    if (!MountPathDefined()) {
        return;
    }

    for (ip1 = g_vhomepatlist; ip1 != NULL; ip1=ip1->next) {
        for (ip2 = g_vmountlist; ip2 != NULL; ip2=ip2->next) {
            if (IsExcluded(ip2->classes)) {
                continue;
            }
            pathbuff[0]='\0';
            basename[0]='\0';
            strcpy(pathbuff, ip2->name);
            AddSlash(pathbuff);
            strcat(pathbuff, ip1->name);
            AddSlash(pathbuff);

            /* home/subdir */
            if (strncmp(ptr->path, "home/", 5) == 0) {
                strcat(pathbuff, "*");
                AddSlash(pathbuff);

                if (*(ptr->path+4) != '/') {
                    snprintf(g_output, CF_BUFSIZE*2, 
                            "Illegal use of home in files: %s\n", ptr->path);
                    CfLog(cferror, g_output, "");
                    return;
                } else {
                    strcat(pathbuff, ptr->path+5);
                }

                ExpandWildCardsAndDo(pathbuff, basename, RecFileCheck, ptr);
            } else {
                ExpandWildCardsAndDo(pathbuff, basename, RecFileCheck, ptr);
            }
        }
    }
}

/* ----------------------------------------------------------------- */

void
EditItemsInResolvConf(struct Item *from, struct Item **list)
{
    char buf[CF_MAXVARSIZE], work[CF_EXPANDSIZE];

    if ((from != NULL) && !IsDefinedClass(from->classes)) {
        return;
    }

    if (from == NULL) {
        return;
    } else {
        ExpandVarstring(from->name, work, "");
        EditItemsInResolvConf(from->next, list);
        if (isdigit((int)*(work))) {
            snprintf(buf, CF_BUFSIZE, "nameserver %s", work);
        } else {
            strcpy(buf, work);
        }

        DeleteItemMatching(list, buf); /* del+prep = move to head of list */
        PrependItem(list, buf, NULL);
        return;
    }
}


/* ----------------------------------------------------------------- */

int
TZCheck(char *tzsys, char *tzlist)
{
    if (strncmp(tzsys, "GMT", 3) == 0) {
        return (strncmp(tzsys, tzlist, 5) == 0); /* e.g. GMT+1 */
    } else {
        return (strncmp(tzsys, tzlist, 3) == 0); /* e.g. MET or CET */
    }
}

/* ----------------------------------------------------------------- */

/* 
 * This function recursively expands a path containing wildcards and
 * executes the function pointed to by function for each matching file
 * or directory.
 */
void
ExpandWildCardsAndDo(char *wildpath, char *buffer,
        void (*function)(char *path, void *ptr), void *argptr)
{
    char *rest, extract[CF_BUFSIZE], construct[CF_BUFSIZE];
    char varstring[CF_EXPANDSIZE],cleaned[CF_BUFSIZE], *work;
    struct stat statbuf;
    DIR *dirh;
    struct dirent *dp;
    int count, isdir = false, i, j;

    varstring[0] = '\0';

    memset(cleaned, 0, CF_BUFSIZE);

    for (i = j = 0; wildpath[i] != '\0'; i++, j++) {
        if ((i > 0) && (wildpath[i] == '/') && (wildpath[i-1] == '/')) {
            j--;
        }
        cleaned[j] = wildpath[i];
    }

    ExpandVarstring(cleaned, varstring, NULL);
    work = varstring;

    Debug2("ExpandWildCardsAndDo(%s=%s)\n", cleaned, work);

    extract[0] = '\0';

    if (*work == '/') {
        work++;
        isdir = true;
    }

    sscanf(work, "%[^/]", extract);
    rest = work + strlen(extract);

    if (strlen(extract) == 0) {
        if (isdir) {
            strcat(buffer, "/");
        }
        (*function)(buffer, argptr);
        return;
    }

    if (! IsWildCard(extract)) {
        strcat(buffer, "/");
        if (BufferOverflow(buffer, extract)) {
            snprintf(g_output, CF_BUFSIZE*2, "Culprit %s\n", extract);
            CfLog(cferror, g_output, "");
            exit(0);
        }
        strcat(buffer, extract);
        ExpandWildCardsAndDo(rest, buffer, function, argptr);
        return;
    } else {
        strcat(buffer, "/");

        if ((dirh=opendir(buffer)) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2, "Can't open dir: %s\n", buffer);
            CfLog(cferror, g_output, "opendir");
            return;
        }

        count = 0;
        strcpy(construct, buffer);                 /* save relative path */

        for (dp = readdir(dirh); dp != 0; dp = readdir(dirh)) {
            if (!SensibleFile(dp->d_name, buffer, NULL)) {
                continue;
            }

            count++;
            strcpy(buffer, construct);
            strcat(buffer, dp->d_name);

            if (stat(buffer, &statbuf) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, "Can't stat %s\n\n", buffer);
                CfLog(cferror, g_output, "stat");
                continue;
            }

            if ((S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode)) &&
                    WildMatch(extract, dp->d_name)) {
                ExpandWildCardsAndDo(rest, buffer, function, argptr);
            }
        }

        if (count == 0) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "No directories matching %s in %s\n", extract, buffer);
            CfLog(cfinform, g_output, "");
            return;
        }
        closedir(dirh);
    }
}


/* ----------------------------------------------------------------- */

/* True if file path in /. */
int
TouchDirectory(struct File *ptr)
{
    char *sp;

    if (ptr->action == touch) {
        sp = ptr->path+strlen(ptr->path)-2;

        if (strcmp(sp, "/.") == 0) {
            return(true);
        } else {
            return false;
        }
    } else {
        return false;
    }
}


/* ----------------------------------------------------------------- */

void
RecFileCheck(char *startpath, void *vp)
{
    struct File *ptr;
    struct stat sb;

    ptr = (struct File *)vp;

    Verbose("%s: Checking files in %s...\n", g_vprefix, startpath);

    if (!GetLock(ASUniqueName("files"), startpath, ptr->ifelapsed, 
                ptr->expireafter, g_vuqname, g_cfstarttime)) {
        return;
    }

    if (stat(startpath, &sb) == -1) {
        snprintf(g_output, CF_BUFSIZE, 
                "Directory %s cannot be accessed in files", startpath);
        CfLog(cfinform, g_output, "stat");
        return;
    }

    CheckExistingFile("*", startpath, ptr->plus, ptr->minus, ptr->action, 
            ptr->uid, ptr->gid, &sb, ptr, ptr->acl_aliases);

    RecursiveCheck(startpath, ptr->plus, ptr->minus, ptr->action, ptr->uid, 
            ptr->gid, ptr->recurse, 0, ptr, &sb);

    ReleaseCurrentLock();
}


/*
 * --------------------------------------------------------------------
 * Level 3
 * --------------------------------------------------------------------
 */

void
RecordFileSystemArrivals(DB *dbp, time_t mtime)
{
    DBT key, value;
    char *keyval = strdup(ConvTimeKey(ctime(&mtime)));
    double new = 0, old = 0;

    Debug("Record fs hit at %s\n", keyval);
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));

    key.data = keyval;
    key.size = strlen(keyval)+1;

    if ((errno = dbp->get(dbp, NULL, &key, &value, 0)) != 0) {
        if (errno != DB_NOTFOUND) {
            dbp->err(dbp, errno, NULL);
            free(keyval);
            return;
        }

        old = 0.0;
    } else {
        bcopy(value.data, &old, sizeof(double));
    }

    /* 
     * Arbitrary counting scale (x+(x+1))/2 becomes like principal value
     * / weighted av
     */
    new = old + 0.5; 

    key.data = keyval;
    key.size = strlen(keyval)+1;

    value.data = (void *) &new;
    value.size = sizeof(double);

    if ((errno = dbp->put(dbp, NULL, &key, &value, 0)) != 0) {
        CfLog(cferror, "db->put failed", "db->put");
    }

    free(keyval);
}


/*
 * --------------------------------------------------------------------
 * Toolkit fstab
 * --------------------------------------------------------------------
 */

int
MatchStringInFstab(char *str)
{
    FILE *fp;

    if ((fp = fopen(g_vfstab[g_vsystemhardclass],"r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Can't open %s for reading\n", g_vfstab[g_vsystemhardclass]);
        CfLog(cferror, g_output, "fopen");
        return true; /* write nothing */
    }

    while (!feof(fp)) {
        ReadLine(g_vbuff, CF_BUFSIZE, fp);

        if (g_vbuff[0] == '#') {
            continue;
        }

        if (strstr(g_vbuff, str)) {
            fclose(fp);
            return true;
        }
    }

    fclose(fp);
    return(false);
}
