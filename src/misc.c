/*
 * $Id: misc.c 755 2004-05-25 18:40:01Z skaar $
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
 * misc functions: add classes ("any", Linux Distribution classes),
 * file, directory and checkum checks. Routines for unquoting and
 * execution of external commands.
 */

#include "cf.defs.h"
#include "cf.extern.h"
#include "../pub/global.h"
#include <db.h>


/* Enter directory and check for race exploits */
int
DirPush(char *name, struct stat *sb)
{
    if (chdir(name) == -1) {
        snprintf(g_output, CF_BUFSIZE,
                "Could not change to directory %s, mode %o in tidy",
                name,sb->st_mode & 07777);
        CfLog(cfinform, g_output, "chdir");
        return false;
    } else {
        Debug("Changed directory to %s\n", name);
    }

    CheckLinkSecurity(sb, name);
    return true;
}

/* ----------------------------------------------------------------- */

/* Exit directory and check for race exploits */
void
DirPop(int goback, char *name, struct stat *sb)
{
    if (goback && g_travlinks) {
        if (chdir(name) == -1) {
            snprintf(g_output, CF_BUFSIZE,
                "Error in backing out of recursive descent securely to %s",
                name);
            CfLog(cferror, g_output, "chdir");
            HandleSignal(SIGTERM);
        }

        CheckLinkSecurity(sb, name);
    } else if (goback) {
        if (chdir("..") == -1) {
            snprintf(g_output, CF_BUFSIZE,
                "Error in backing out of recursive descent securely to %s",
                name);
            CfLog(cferror, g_output, "chdir");
            HandleSignal(SIGTERM);
        }
    }
}

/* ----------------------------------------------------------------- */

void
CheckLinkSecurity(struct stat *sb, char *name)
{
    struct stat security;

    Debug("Checking the inode and device to make sure we are "
            "where we think we are...\n");

    if (stat(".", &security) == -1) {
        snprintf(g_output, CF_BUFSIZE,
                "Could not stat directory %s after entering!",name);
        CfLog(cferror, g_output, "stat");
        return;
    }

    if ((sb->st_dev != security.st_dev) || (sb->st_ino != security.st_ino)) {

        snprintf(g_output, CF_BUFSIZE,
                "SERIOUS SECURITY ALERT: path race exploited in "
                "recursion to/from %s. Not safe for agent to continue "
                "- aborting", name);

        CfLog(cferror, g_output, "");
        HandleSignal(SIGTERM);
        /* Exits */
    }
}

/* ----------------------------------------------------------------- */

void 
TruncateFile(char *name)
{
    struct stat statbuf;
    int fd;

    if (stat(name, &statbuf) == -1) {
        Debug2("cfagent: didn't find %s to truncate\n", name);
        return;
    } else {
        /* dummy mode ignored */
        if ((fd = creat(name, 000)) == -1) {
            snprintf(g_output, CF_BUFSIZE*2, "creat(%s) failed\n", name);
            CfLog(cferror, g_output, "creat");
        } else {
            close(fd);
        }
    }
}

/* ----------------------------------------------------------------- */

int
FileSecure(char *name)
{
    struct stat statbuf;

    if (g_parseonly || !g_cfparanoid) {
        return true;
    }

    if (stat(name, &statbuf) == -1) {
        return false;
    }

    if (statbuf.st_uid != getuid()) {
        snprintf(g_output, CF_BUFSIZE*2,
                "File %s is not owned by uid %d (security exception)",
                name, getuid());
        CfLog(cferror, g_output, "");
    }

    /* Is the file writable by ANYONE except the owner ? */

    if (statbuf.st_mode & (S_IWGRP | S_IWOTH)) {

        snprintf(g_output, CF_BUFSIZE*2,
                "File %s (owner %d) is writable by others "
                "(security exception)", name, getuid());

        CfLog(cferror, g_output, "");
        return false;
    }

    return true;
}

/* ----------------------------------------------------------------- */

/* 
 * Returns false if filename never seen before, and adds a checksum to
 * the database. Returns true if checksums do not match and also updates
 * database to the new value 
 */
int
ChecksumChanged(char *filename, unsigned char digest[EVP_MAX_MD_SIZE+1],
        int warnlevel, int refresh, char type)
{
    struct stat stat1, stat2;
    int i, needupdate = false, size = 21;
    unsigned char dbvalue[EVP_MAX_MD_SIZE+1], dbdigest[EVP_MAX_MD_SIZE+1];
    DBT key,value;
    DB *dbp;
    DB_ENV *dbenv = NULL;

    Debug("ChecksumChanged: key %s with data %s\n",
            filename,ChecksumPrint(type,digest));

    /* Size of message digests */
    switch(type) {
    case 'm':
        size = CF_MD5_LEN;
        break;
    case 's':
        size = CF_SHA_LEN;
        break;
    }

    if (g_checksumdb == NULL) {
        if (g_iscfengine) {
            CfLog(cferror,"ChecksumDatabase = (<undefined>) in control:","");
            return false;
        } else {
            Debug("Direct comparison (no db)\n");
            ChecksumFile(filename, dbdigest, type);

            for (i = 0; i < size; i++) {
                if (digest[i] != dbdigest[i]) {
                    return true;
                }
            }
            return false;
        }
    }

    /* Check whether database is current wrt local file */
    if (refresh) {

        if (stat(filename, &stat1) == -1) {
            snprintf(g_output, CF_BUFSIZE*2, "Couldn't stat %s\n", filename);
            CfLog(cferror, g_output, "stat");
            return false;
        }

        if (stat(g_checksumdb, &stat2) != -1) {
            if (stat1.st_mtime > stat2.st_mtime) {

                Debug("Checksum database is older than %s..."
                        "refresh needed\n", filename);

                needupdate = true;
            } else {
                Debug("Checksum up to date..\n");
            }
        } else {
            needupdate = true;
        }
    }

    if ((errno = db_create(&dbp, dbenv, 0)) != 0) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't open checksum database %s\n", g_checksumdb);
        CfLog(cferror, g_output, "db_open");
        return false;
    }

#ifdef CF_OLD_DB
    if ((errno = dbp->open(dbp, g_checksumdb, NULL, DB_BTREE,
                    DB_CREATE, 0644)) != 0)
#else
    if ((errno = dbp->open(dbp, NULL, g_checksumdb, NULL, DB_BTREE,
                    DB_CREATE, 0644)) != 0)
#endif
    {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't open checksum database %s\n", g_checksumdb);
        CfLog(cferror, g_output, "db_open");
        dbp->close(dbp, 0);
        return false;
    }

    memset(&value, 0, sizeof(value));
    memset(&key, 0, sizeof(key));

    key.data = filename;
    key.size = strlen(filename)+1;
    value.data = dbvalue;
    value.size = size;

    if (needupdate) {
        memset(dbdigest, 0, EVP_MAX_MD_SIZE+1);
        ChecksumFile(filename, dbdigest, type);

        key.data = filename;
        key.size = strlen(filename)+1;
        value.data = (void *) dbdigest;
        value.size = size;

        Debug("Updating checksum for %s to %s\n",
                filename, ChecksumPrint(type, value.data));

        if ((errno = dbp->del(dbp, NULL, &key, 0)) != 0) {
            CfLog(cferror, "", "db_store");
        }

        key.data = filename;
        key.size = strlen(filename)+1;

        if ((errno = dbp->put(dbp, NULL, &key, &value, 0)) != 0) {
            CfLog(cferror, "put failed", "db->put");
        }
    }

    /* The filename key was found in the db */
    if ((errno = dbp->get(dbp, NULL, &key, &value, 0)) == 0) {
        Debug("Comparing %s (sent)\n", ChecksumPrint(type, digest));
        Debug("     with %s (db)\n", ChecksumPrint(type, value.data));

        memset(dbdigest, 0, EVP_MAX_MD_SIZE+1);
        bcopy(value.data, dbdigest, size);

        for (i = 0; i < size; i++) {
            if (digest[i] != dbdigest[i]) {

                Debug("Found checksum for %s in database "
                        "but it didn't match\n", filename);

                snprintf(g_output, CF_BUFSIZE*2,
                        "SECURITY ALERT: Checksum for %s changed!", filename);
                CfLog(warnlevel, g_output,"");

                if (g_checksumupdates) {
                    memset(dbdigest, 0, EVP_MAX_MD_SIZE+1);
                    ChecksumFile(filename, dbdigest, type);

                    key.data = filename;
                    key.size = strlen(filename)+1;
                    value.data = (void *) dbdigest;
                    value.size = size;

                    Verbose("Updating checksum for %s to %s\n",
                            filename, ChecksumPrint(type, value.data));

                    if ((errno = dbp->del(dbp, NULL, &key,0)) != 0) {
                        CfLog(cferror, "", "db->del");
                    }

                    key.data = filename;
                    key.size = strlen(filename)+1;

                    if ((errno = dbp->put(dbp, NULL, &key, &value, 0)) != 0) {
                        CfLog(cferror, "put failed", "db->put");
                    }
                }

                dbp->close(dbp, 0);

                /* Checksum updated but was changed */
                return true;
            }
        }

        Debug("Found checksum for %s in database and it matched\n", filename);
        dbp->close(dbp, 0);
        return false;

    } else {
        /* Key was not found, so install it */

        if (g_iscfengine) {

            snprintf(g_output, CF_BUFSIZE,
                    "File %s was not in md5 database - new file found",
                    filename);

            CfLog(cfsilent, g_output,"");
        }

        memset(dbdigest, 0, EVP_MAX_MD_SIZE+1);
        ChecksumFile(filename, dbdigest, type);

        key.data = filename;
        key.size = strlen(filename)+1;
        value.data = (void *) dbdigest;
        value.size = size;

        Debug("Storing checksum for %s in database %s\n",
                filename, ChecksumPrint(type, dbdigest));

        if ((errno = dbp->put(dbp, NULL, &key, &value, 0)) != 0) {
            CfLog(cferror, "put failed", "db->put");
        }

        dbp->close(dbp,0);

        if (g_iscfengine) {
            return false;      /* No need to warn when first installed */
        } else {
            return true;
        }
    }
}



/* 
 * Go through the database and purge records about non-existent files
 */
void
ChecksumPurge(void)
{
    DBT key,value;
    DB *dbp;
    DBC *dbcp;
    DB_ENV *dbenv = NULL;
    int ret;
    struct stat statbuf;

    if ((errno = db_create(&dbp, dbenv, 0)) != 0) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't open checksum database %s\n", g_checksumdb);
        CfLog(cferror, g_output, "db_open");
        return;
    }

#ifdef CF_OLD_DB
    if ((errno = dbp->open(dbp, g_checksumdb, NULL, DB_BTREE,
                    DB_CREATE, 0644)) != 0)
#else
    if ((errno = dbp->open(dbp, NULL, g_checksumdb, NULL, DB_BTREE,
                    DB_CREATE, 0644)) != 0)
#endif
    {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't open checksum database %s\n", g_checksumdb);
        CfLog(cferror, g_output, "db_open");
        dbp->close(dbp,0);
        return;
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        CfLog(cferror,"Error reading from checksum database","");
        dbp->err(dbp, ret, "DB->cursor");
        return;
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));

    /* Walk through the database and print out the key/data pairs. */
    while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0) {
        if (stat(key.data, &statbuf) == -1) {

            Verbose("Purging file %s from checksum db - no longer exists\n",
                    (char *)key.data);

            snprintf(g_output, CF_BUFSIZE*2,
                    "SECURITY INFO: Checksum for %s purged - "
                    "file no longer exists!", key.data);

            CfLog(cferror,g_output,"");

            if ((errno = dbp->del(dbp,NULL,&key,0)) != 0) {
                CfLog(cferror,"","db_store");
            }
        }
    }

    dbcp->c_close(dbcp);
    dbp->close(dbp,0);
}

/* ----------------------------------------------------------------- */

int
IgnoredOrExcluded(enum actions action,char *file,
        struct Item *inclusions, struct Item *exclusions)
{
    char *lastnode;

    Debug("IgnoredOrExcluded(%s)\n",file);

    if (strstr(file,"/")) {
        for (lastnode = file+strlen(file); *lastnode != '/'; lastnode--) { }
        lastnode++;
    } else {
        lastnode = file;
    }

    if ((inclusions != NULL) && !IsWildItemIn(inclusions,lastnode)) {
        Debug("cfagent: skipping non-included pattern %s\n", file);
        return true;
    }

    switch(action) {
    case image:
        if (IsWildItemIn(g_vexcludecopy, lastnode) ||
                IsWildItemIn(exclusions, lastnode)) {

            Debug("Skipping excluded pattern %s\n", file);
            return true;
        }
    case links:
        if (IsWildItemIn(g_vexcludelink, lastnode) ||
                IsWildItemIn(exclusions, lastnode)) {

            Debug("Skipping excluded pattern %s\n", file);
            return true;
        }
    default:
        if (IsWildItemIn(exclusions,lastnode)) {
            Debug("Skipping excluded pattern %s\n", file);
            return true;
        }
    }
    return false;
}


/* ----------------------------------------------------------------- */

void
Banner(char *string)
{
    Verbose("---------------------------------------------------------------------\n");
    Verbose("%s\n", string);
    Verbose("---------------------------------------------------------------------\n\n");
}

/* ----------------------------------------------------------------- */

void
DebugBinOut(char *buffer, int len)
{
    char *sp;

    Debug("BinaryBuffer[");

    for (sp = buffer; (*sp != '\0') && (sp < buffer+len); sp++) {
        Debug("%x", *sp);
    }

    Debug("]\n");
}

/* ----------------------------------------------------------------- */

char *
ChecksumPrint(char type, unsigned char digest[EVP_MAX_MD_SIZE+1])
{
    unsigned int i;
    static char buffer[EVP_MAX_MD_SIZE*4];
    int len = 16;

    switch(type) {
    case 's':
        sprintf(buffer, "SHA=  ");
        len = 20;
        break;
    case 'm':
        sprintf(buffer, "MD5=  ");
        len = 16;
        break;
    }

    for (i = 0; i < len; i++) {
        sprintf((char *)(buffer+4+2*i), "%02x", digest[i]);
    }

    return buffer;
}

/* ----------------------------------------------------------------- */

int
ShellCommandReturnsZero(char *comm)
{
    int status, i, argc;
    pid_t pid;
    char arg[CF_MAXSHELLARGS][CF_BUFSIZE];
    char **argv;

    /* Build argument array */

    for (i = 0; i < CF_MAXSHELLARGS; i++) {
        memset(arg[i],0,CF_BUFSIZE);
    }

    argc = SplitCommand(comm, arg);

    if (argc == -1) {
        snprintf(g_output, CF_BUFSIZE, "Too many arguments in %s\n", comm);
        CfLog(cferror, g_output, "");
        return false;
    }

    if ((pid = fork()) < 0) {
        FatalError("Failed to fork new process");
    } else if (pid == 0) {
        /* child */
        argv = (char **) malloc((argc+1)*sizeof(char *));

        if (argv == NULL) {
            FatalError("Out of memory");
        }

        for (i = 0; i < argc; i++) {
            argv[i] = arg[i];
        }

        argv[i] = (char *) NULL;

        if (execv(arg[0],argv) == -1) {
            yyerror("script failed");
            perror("execvp");
            exit(1);
        }
    } else {
        /* parent */

        pid_t wait_result;
        while ((wait_result = wait(&status)) != pid) {
            if (wait_result <= 0) {
                snprintf(g_output, CF_BUFSIZE, "Wait for child failed\n");
                CfLog(cfinform, g_output, "wait");
                return false;
            }
        }

        if (WIFSIGNALED(status)) {
            Debug("Script %s returned: %d\n", comm, WTERMSIG(status));
            return false;
        }

        if (! WIFEXITED(status)) {
            return false;
        }

        if (WEXITSTATUS(status) == 0) {
            Debug("Shell command returned 0\n");
            return true;
        } else {
            Debug("Shell command was non-zero\n");
            return false;
        }
       
   }
    return false;
}


/* ----------------------------------------------------------------- */

void
SetSignals(void)
{
    int i;

    g_signals[SIGHUP] = strdup("SIGHUP");
    g_signals[SIGINT] = strdup("SIGINT");
    g_signals[SIGTRAP] = strdup("SIGTRAP");
    g_signals[SIGKILL] = strdup("SIGKILL");
    g_signals[SIGPIPE] = strdup("SIGPIPE");
    g_signals[SIGCONT] = strdup("SIGCONT");
    g_signals[SIGABRT] = strdup("SIGABRT");
    g_signals[SIGSTOP] = strdup("SIGSTOP");
    g_signals[SIGQUIT] = strdup("SIGQUIT");
    g_signals[SIGTERM] = strdup("SIGTERM");
    g_signals[SIGCHLD] = strdup("SIGCHLD");
    g_signals[SIGUSR1] = strdup("SIGUSR1");
    g_signals[SIGUSR2] = strdup("SIGUSR2");
    g_signals[SIGBUS] = strdup("SIGBUS");
    g_signals[SIGSEGV] = strdup("SIGSEGV");

    for (i = 0; i < highest_signal; i++) {
        if (g_signals[i] == NULL) {
            g_signals[i] = strdup("NOSIG");
        }
    }
}

/* ----------------------------------------------------------------- */

void
SetClassesOnScript(char *execstr,char *classes,char *elseclasses,int useshell)
{
    FILE *pp;
    int print;
    char line[CF_BUFSIZE],*sp;

    switch (useshell) {
    case 'y':
        pp = cfpopen_sh(execstr, "r");
        break;
    default:
        pp = cfpopen(execstr, "r");
        break;
    }

    if (pp == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, "Couldn't open pipe to command %s\n",
                execstr);
        CfLog(cferror, g_output, "popen");
        return;
    }

    while (!feof(pp)) {
        /* abortable */
        if (ferror(pp))  {
            snprintf(g_output, CF_BUFSIZE*2, "Shell command pipe %s\n",
                    execstr);
            CfLog(cferror, g_output, "ferror");
            break;
        }

        ReadLine(line, CF_BUFSIZE, pp);

        if (strstr(line, "cfengine-die")) {
            break;
        }

        if (ferror(pp)) {
            /* abortable */
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Shell command pipe %s\n", execstr);
            CfLog(cferror, g_output, "ferror");
            break;
        }

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
            Verbose("%s:%s: %s\n", g_vprefix, execstr, line);
        }
    }

    cfpclose_def(pp, classes, elseclasses);
}

/* ----------------------------------------------------------------- */

char *
UnQuote(char *s)
{
    if (s[strlen(s)-1] == '\"') {
        s[strlen(s)-1] = '\0';
    }

    if (s[0] == '\"') {
        return s+1;
    } else {
        return s;
    }
}

/* ----------------------------------------------------------------- */

void AddListSeparator(char *s) 
{
    if (s[strlen(s)-1] != g_listseparator) {
        s[strlen(s)+1] = '\0';
        s[strlen(s)] = g_listseparator;
    }
}

/* ----------------------------------------------------------------- */

void ChopListSeparator(char *s)

{
if (s[strlen(s)-1] == g_listseparator)
   {
   s[strlen(s)-1] = '\0';
   } 
}

/* ----------------------------------------------------------------- */

void
IDClasses(void)
{
    struct stat statbuf;
    char    *sp;
    int     i = 0;

    AddClassToHeap("any");      /* This is a reserved word / wildcard */

    snprintf(g_vbuff, CF_BUFSIZE, "cfng_%s", CanonifyName(VERSION));
    AddClassToHeap(g_vbuff);

    for (sp = g_vbuff + strlen(g_vbuff); i < 2; sp--) {
        if (*sp == '_') {
            i++;
            *sp = '\0';
            AddClassToHeap(g_vbuff);
        }
    }

    if (stat("/etc/redhat-release", &statbuf) != -1) {
        Verbose("This appears to be a redhat system.\n");
        AddClassToHeap("redhat");
        linux_redhat_version();
    }

    if (stat("/etc/SuSE-release", &statbuf) != -1) {
        Verbose("\nThis appears to be a SuSE system.\n");
        AddClassToHeap("SuSE");
        linux_suse_version();
    }

    if (stat("/etc/slackware-release", &statbuf) != -1) {
        Verbose("\nThis appears to be a slackware system.\n");
        AddClassToHeap("slackware");
    }

    if (stat("/etc/debian_version", &statbuf) != -1) {
        Verbose("\nThis appears to be a debian system.\n");
        AddClassToHeap("debian");
        debian_version();
    }

    if (stat("/etc/UnitedLinux-release", &statbuf) != -1) {
        Verbose("\nThis appears to be a UnitedLinux system.\n");
        AddClassToHeap("UnitedLinux");
    }
}

/* ----------------------------------------------------------------- */

int linux_fedora_version(void)
{
#define FEDORA_ID "Fedora Core"

#define RELEASE_FLAG "release "

/* We are looking for one of the following strings...
 *
 * Fedora Core release 1 (Yarrow)
 */

#define FEDORA_REL_FILENAME "/etc/fedora-release"

    FILE *fp;

    /* The full string read in from fedora-release */
    char relstring[CF_MAXVARSIZE];
    char classbuf[CF_MAXVARSIZE];

    /* Fedora */
    char *vendor = "";
    /* Where the numerical release will be found */
    char *release = NULL;

    int major = -1;
    char strmajor[CF_MAXVARSIZE];

    /* Grab the first line from the file and then close it. */
    if ((fp = fopen(FEDORA_REL_FILENAME,"r")) == NULL) {
        return 1;
    }
    fgets(relstring, sizeof(relstring), fp);
    fclose(fp);

    Verbose("Looking for fedora core linux info...\n");

    /* First, try to grok the vendor */
    if(!strncmp(relstring, FEDORA_ID, strlen(FEDORA_ID))) {
        vendor = "fedora";
    } else {
        Verbose("Could not identify OS distro from %s\n",
                FEDORA_REL_FILENAME);
        return 2;
    }

    /* 
     * Now, grok the release.  We assume that all the strings will have
     * the word 'release' before the numerical release.
     */
    release = strstr(relstring, RELEASE_FLAG);
    if(release == NULL) {
        Verbose("Could not find a numeric OS release in %s\n",
            FEDORA_REL_FILENAME);
        return 2;
    } else {
        release += strlen(RELEASE_FLAG);
        if (sscanf(release, "%d", &major) == 1) {
            sprintf(strmajor, "%d", major);
        };
    }

    if(major != -1 && vendor != "") {
        classbuf[0] = '\0';
        strcat(classbuf, vendor);
        AddClassToHeap(classbuf);
        strcat(classbuf, "_");
        strcat(classbuf, strmajor);
        AddClassToHeap(classbuf);
    }
    return 0;
}

/* ----------------------------------------------------------------- */

int
linux_redhat_version(void)
{

#define REDHAT_ID "Red Hat Linux"
#define REDHAT_AS_ID "Red Hat Enterprise Linux AS"
#define REDHAT_AS21_ID "Red Hat Linux Advanced Server"
#define REDHAT_ES_ID "Red Hat Enterprise Linux ES"
#define REDHAT_WS_ID "Red Hat Enterprise Linux WS"
#define MANDRAKE_ID "Linux Mandrake"
#define FEDORA_ID "Fedora Core"

#define RELEASE_FLAG "release "

/* 
 * We are looking for one of the following strings...
 *
 * Red Hat Linux release 6.2 (Zoot)
 * Red Hat Linux Advanced Server release 2.1AS (Pensacola)
 * Red Hat Enterprise Linux AS release 3 (Taroon)
 * Red Hat Enterprise Linux WS release 3 (Taroon)
 * Linux Mandrake release 7.1 (helium)
 * Red Hat Enterprise Linux ES release 2.1 (Panama)
 * Fedora Core release 1 (Yarrow)
 */

#define RH_REL_FILENAME "/etc/redhat-release"

    FILE *fp;

/* The full string read in from redhat-release */
    char relstring[CF_MAXVARSIZE];
    char classbuf[CF_MAXVARSIZE];

    /* Red Hat, Mandrake */
    char *vendor = "";
    /* as (Advanced Server, Enterprise) */
    char *edition = "";
    /* Where the numerical release will be found */
    char *release = NULL;

    int major = -1;
    char strmajor[CF_MAXVARSIZE];
    int minor = -1;
    char strminor[CF_MAXVARSIZE];

    /* Grab the first line from the file and then close it. */
    if ((fp = fopen(RH_REL_FILENAME,"r")) == NULL) {
        return 1;
    }
    fgets(relstring, sizeof(relstring), fp);
    fclose(fp);

    Verbose("Looking for redhat linux info...\n");

    /* First, try to grok the vendor and the edition (if any) */
    if(!strncmp(relstring, REDHAT_ES_ID, strlen(REDHAT_ES_ID))) {
        vendor = "redhat";
        edition = "es";
    } else if(!strncmp(relstring, REDHAT_WS_ID, strlen(REDHAT_WS_ID))) {
        vendor = "redhat";
        edition = "ws";
    } else if(!strncmp(relstring, REDHAT_WS_ID, strlen(REDHAT_WS_ID))) {
        vendor = "redhat";
        edition = "ws";
    } else if(!strncmp(relstring, REDHAT_AS_ID, strlen(REDHAT_AS_ID)) ||
        !strncmp(relstring, REDHAT_AS21_ID, strlen(REDHAT_AS21_ID))) {
        vendor = "redhat";
        edition = "as";
    } else if(!strncmp(relstring, REDHAT_ID, strlen(REDHAT_ID))) {
        vendor = "redhat";
    } else if(!strncmp(relstring, MANDRAKE_ID, strlen(MANDRAKE_ID))) {
        vendor = "mandrake";
    } else if(!strncmp(relstring, FEDORA_ID, strlen(FEDORA_ID))) {
        vendor = "fedora";
    } else {
        Verbose("Could not identify OS distro from %s\n", RH_REL_FILENAME);
        return 2;
    }

    /* 
     * Now, grok the release.  For AS, we neglect the AS at the end of
     * the numerical release because we already figured out that it *is*
     * AS from the infomation above.  We assume that all the strings
     * will have the word 'release' before the numerical release.
     */
    release = strstr(relstring, RELEASE_FLAG);
    if(release == NULL) {
        Verbose("Could not find a numeric OS release in %s\n",
            RH_REL_FILENAME);
        return 2;
    } else {
        release += strlen(RELEASE_FLAG);
        if (sscanf(release, "%d.%d", &major, &minor) == 2) {
            sprintf(strmajor, "%d", major);
            sprintf(strminor, "%d", minor);
        } else if (sscanf(release, "%d", &major) == 1) {
            /* 
             * red hat 9 is *not* red hat 9.0.  and same thing with RHEL
             * AS 3
             */
            sprintf(strmajor, "%d", major);
            minor = -2;
        }
    }

    if(major != -1 && minor != -1 && vendor != "") {
        classbuf[0] = '\0';
        strcat(classbuf, vendor);
        AddClassToHeap(classbuf);
        strcat(classbuf, "_");

        if(edition != "") {
            strcat(classbuf, edition);
            AddClassToHeap(classbuf);
            strcat(classbuf, "_");
        }

        strcat(classbuf, strmajor);
        AddClassToHeap(classbuf);

        if (minor != -2) {
            strcat(classbuf, "_");
            strcat(classbuf, strminor);
            AddClassToHeap(classbuf);
        }
    }
    return 0;
}

/* ----------------------------------------------------------------- */

int
linux_suse_version(void)
{

#define SUSE_REL_FILENAME "/etc/SuSE-release"
#define SUSE_ID "SuSE Linux"
#define SUSE_RELEASE_FLAG "Linux "

    /* The full string read in from SuSE-release */
    char relstring[CF_MAXVARSIZE];
    char classbuf[CF_MAXVARSIZE];

    /* Where the numerical release will be found */
    char *release = NULL;

    int major = -1;
    char strmajor[CF_MAXVARSIZE];
    int minor = -1;
    char strminor[CF_MAXVARSIZE];

    FILE *fp;

    /* Grab the first line from the file and then close it. */
    if ((fp = fopen(SUSE_REL_FILENAME,"r")) == NULL) {
        return 1;
    }
    fgets(relstring, sizeof(relstring), fp);
    fclose(fp);

    /* 
     * Determine release version. We assume that the version follows the
     * string "SuSE Linux".
     */
    release = strstr(relstring, SUSE_RELEASE_FLAG);
    if(release == NULL) {
        Verbose("Could not find a numeric OS release in %s\n",
                SUSE_REL_FILENAME);
        return 2;
    } else {
        release += strlen(SUSE_RELEASE_FLAG);
        sscanf(release, "%d.%d", &major, &minor);
        sprintf(strmajor, "%d", major);
        sprintf(strminor, "%d", minor);
    }
    if(major != -1 && minor != -1) {
        classbuf[0] = '\0';
        strcat(classbuf, "SuSE");
        AddClassToHeap(classbuf);
        strcat(classbuf, "_");
        strcat(classbuf, strmajor);
        AddClassToHeap(classbuf);
        strcat(classbuf, "_");
        strcat(classbuf, strminor);
        AddClassToHeap(classbuf);
    }
    return 0;
}

/* ----------------------------------------------------------------- */

/* Andrew Stribblehill */
int
debian_version(void)
{

#define DEBIAN_VERSION_FILENAME "/etc/debian_version"
    int major = -1;
    int release = -1;
    char classname[CF_MAXVARSIZE] = "";
    FILE *fp;

    if ((fp = fopen(DEBIAN_VERSION_FILENAME, "r")) == NULL) {
        return 1;
    }

    Verbose("Looking for Debian version...\n");
    switch (fscanf(fp, "%d.%d", &major, &release)) {
    case 2:
        Verbose("This appears to be a Debian %u.%u system.", major, release);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u_%u", major, release);
        AddClassToHeap(classname);
        /* Fall-through */
    case 1:
        Verbose("This appears to be a Debian %u system.", major);
        snprintf(classname, CF_MAXVARSIZE, "debian_%u", major);
        AddClassToHeap(classname);
        break;
    case 0:
        Verbose("No Debian version number found.\n");
        fclose(fp);
        return 2;
    }
    fclose(fp);
    return 0;
}
