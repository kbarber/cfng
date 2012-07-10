/*
 * $Id: init.c 741 2004-05-23 06:55:46Z skaar $
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
#include "../pub/global.h"

/* ----------------------------------------------------------------- */

void
CheckWorkDirectories(void)
{
    struct stat statbuf;
    char *sp;

    Debug("CheckWorkDirectories()\n");

    if (uname(&g_vsysname) == -1) {
        perror("uname ");
        FatalError("Uname couldn't get kernel name info!!\n");
    }

    snprintf(g_logfile, CF_BUFSIZE, "%s/cfng_setuid.log", g_vlogdir);
    g_vsetuidlog = strdup(g_logfile);

    if (!IsPrivileged()) {
        Verbose("\n(Non privileged user...)\n\n");

        if ((sp = getenv("HOME")) == NULL) {
            FatalError("You do not have a HOME variable pointing to "
                    "your home directory");
        }

        snprintf(g_vlogdir, CF_BUFSIZE, "%s/.cfagent", sp);
        snprintf(g_vlockdir, CF_BUFSIZE, "%s/.cfagent", sp);
        snprintf(g_vbuff, CF_BUFSIZE, "%s/.cfagent/test", sp);
        MakeDirectoriesFor(g_vbuff, 'y');
        snprintf(g_vbuff, CF_BUFSIZE, "%s/.cfagent/state/test", sp);
        MakeDirectoriesFor(g_vbuff, 'n');
        snprintf(g_cfprivkeyfile, CF_BUFSIZE, 
                "%s/.cfagent/keys/localhost.priv", sp);
        snprintf(g_cfpubkeyfile, CF_BUFSIZE, 
                "%s/.cfagent/keys/localhost.pub", sp);
    } else {
        snprintf(g_vbuff, CF_BUFSIZE, "%s/test", g_vlockdir);
        snprintf(g_vbuff, CF_BUFSIZE,"%s/test", g_vlogdir);
        MakeDirectoriesFor(g_vbuff, 'n');

        /*
        snprintf(g_vbuff,CF_BUFSIZE,"%s/state/test",g_vlogdir);
        MakeDirectoriesFor(g_vbuff,'n');
        */

        snprintf(g_cfprivkeyfile, CF_BUFSIZE,
                "%s/keys/localhost.priv", WORKDIR);
        snprintf(g_cfpubkeyfile, CF_BUFSIZE,
                "%s/keys/localhost.pub", WORKDIR);
    }

    Verbose("Checking integrity of the state database\n");
    snprintf(g_vbuff, CF_BUFSIZE, "%s", g_vlockdir);

    if (stat(g_vbuff, &statbuf) == -1) {
        snprintf(g_vbuff, CF_BUFSIZE, "%s", g_vlockdir);
        MakeDirectoriesFor(g_vbuff, 'n');
        chown(g_vbuff, getuid(), getgid());
        chmod(g_vbuff, (mode_t)0755);
    } else {
        if (statbuf.st_mode & 022) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "UNTRUSTED: State directory %s was not private!\n",
                    g_vlockdir, statbuf.st_mode & 0777);
            CfLog(cferror, g_output, "");
        }
    }

    Verbose("Checking integrity of the module directory\n");

    snprintf(g_vbuff, CF_BUFSIZE, "%s/modules", WORKDIR);

    if (stat(g_vbuff, &statbuf) == -1) {
        snprintf(g_vbuff, CF_BUFSIZE, "%s/modules/test", WORKDIR);
        MakeDirectoriesFor(g_vbuff, 'n');
        snprintf(g_vbuff, CF_BUFSIZE, "%s/modules", WORKDIR);
        chown(g_vbuff, getuid(), getgid());
        chmod(g_vbuff, (mode_t)0700);
    } else {
        if (statbuf.st_mode & 022) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "UNTRUSTED: Module directory %s was not private!\n",
                    g_vlockdir, statbuf.st_mode & 0777);
            CfLog(cferror, g_output, "");
        }
    }

    Verbose("Checking integrity of the input data for RPC\n");

    snprintf(g_vbuff, CF_BUFSIZE, "%s/rpc_in", g_vlockdir);

    if (stat(g_vbuff, &statbuf) == -1) {
        snprintf(g_vbuff, CF_BUFSIZE, "%s/rpc_in/test", g_vlockdir);
        MakeDirectoriesFor(g_vbuff, 'n');
        snprintf(g_vbuff, CF_BUFSIZE, "%s/rpc_in", g_vlockdir);
        chown(g_vbuff, getuid(), getgid());
        chmod(g_vbuff, (mode_t)0700);
    } else {
        if (statbuf.st_mode & 077) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "UNTRUSTED: RPC input directory %s was not private! (%o)\n",
                    g_vbuff, statbuf.st_mode & 0777);
            FatalError(g_output);
        }
    }

    Verbose("Checking integrity of the output data for RPC\n");

    snprintf(g_vbuff, CF_BUFSIZE, "%s/rpc_out", g_vlockdir);
    if (stat(g_vbuff, &statbuf) == -1) {
        snprintf(g_vbuff, CF_BUFSIZE, "%s/rpc_out/test", g_vlockdir);
        MakeDirectoriesFor(g_vbuff, 'n');
        snprintf(g_vbuff, CF_BUFSIZE, "%s/rpc_out", g_vlockdir);
        chown(g_vbuff, getuid(), getgid());
        chmod(g_vbuff, (mode_t)0700);
    } else {
        if (statbuf.st_mode & 077) {
            snprintf(g_output, CF_BUFSIZE*2,
                "UNTRUSTED: RPC output directory %s was not private! (%o)\n",
                g_vbuff, statbuf.st_mode & 0777);
            FatalError(g_output);
        }
    }

    Verbose("Checking integrity of the PKI directory\n");
    snprintf(g_vbuff, CF_BUFSIZE, "%s/keys", WORKDIR);

    if (stat(g_vbuff, &statbuf) == -1) {
        snprintf(g_vbuff, CF_BUFSIZE, "%s/keys/test", WORKDIR);
        MakeDirectoriesFor(g_vbuff, 'n');
        snprintf(g_vbuff, CF_BUFSIZE, "%s/keys", WORKDIR);
        chmod(g_vbuff, (mode_t)0700); /* Locks must be immutable to others */
    } else {
        if (statbuf.st_mode & 077) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "UNTRUSTED: Private key directory %s was not private!\n",
                    g_vlockdir, statbuf.st_mode & 0777);
            FatalError(g_output);
        }
    }

    Verbose("Making sure that locks are private...\n");
    chown(g_vlockdir, getuid(), getgid());
    chmod(g_vlockdir, (mode_t)0755); /* Locks must be immutable to others */
}


/* ----------------------------------------------------------------- */

void
ActAsDaemon(int preserve)
{
    int fd, maxfd;
#ifdef HAVE_SETSID
    setsid();
#endif
    closelog();
    fflush(NULL);
    fd = open("/dev/null", O_RDWR, 0);
    if (fd != -1) {
        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);

        if (fd > STDERR_FILENO) 
            close(fd);
    }
#ifdef HAVE_SYSCONF
    maxfd = sysconf(_SC_OPEN_MAX);
#else
# ifdef _POXIX_OPEN_MAX
    maxfd = _POSIX_OPEN_MAX;
# else
    maxfd = 1024;
# endif
#endif

    for (fd = STDERR_FILENO+1; fd < maxfd; ++fd) {
        if (fd != preserve) close(fd);
    }
}

/* ----------------------------------------------------------------- */

/* make this more reliable ... does anyone have : in hostname? */

int
IsIPV6Address(char *name)
{
    char *sp;
    int count,max = 0;

    Debug("IsIPV6Address(%s)\n", name);

    if (name == NULL) {
        return false;
    }

    count = 0;

    for (sp = name; *sp != '\0'; sp++) {
        if (isalnum((int)*sp)) {
            count++;
        } else if ((*sp != ':') && (*sp != '.')) {
            return false;
        }

        if (count > max) {
            max = count;
        } else {
            count = 0;
        }
    }

    if (max <= 2) {
        Debug("Looks more like a MAC address");
        return false;
    }

    if (strstr(name, ":") == NULL) {
        return false;
    }

    if (StrStr(name, "scope")) {
        return false;
    }

    return true;
}


/* ----------------------------------------------------------------- */

/* make this more reliable ... does anyone have : in hostname? */

int IsIPV4Address(char *name)
{
    char *sp;
    int count = 0;

    Debug("IsIPV4Address(%s)\n", name);

    if (name == NULL) {
        return false;
    }

    for (sp = name; *sp != '\0'; sp++) {
        if (!isdigit((int)*sp) && (*sp != '.')) {
            return false;
        }

        if (*sp == '.') {
            count++;
        }
    }

    if (count != 3) {
        return false;
    }

    return true;
}

/* ----------------------------------------------------------------- */

 /* Does this address belong to a local interface */
int
IsInterfaceAddress(char *adr)
{
    struct Item *ip;

    for (ip = g_ipaddresses; ip != NULL; ip = ip->next) {
        if (StrnCmp(adr, ip->name, strlen(adr)) == 0) {
            Debug("Identifying (%s) as one of my interfaces\n", adr);
            return true;
        }
    }

    Debug("(%s) is not one of my interfaces\n", adr);
    return false;
}
