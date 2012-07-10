/*
 * $Id: log.c 741 2004-05-23 06:55:46Z skaar $
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

extern char g_cflock[CF_BUFSIZE];

/* ----------------------------------------------------------------- */

void
CfOpenLog(void)
{
    char value[CF_BUFSIZE];
    int facility = LOG_USER;

    if (GetMacroValue(g_contextid, "SyslogFacility")) {
        strncpy(value, GetMacroValue(g_contextid, "SyslogFacility"), 32);

        if (strcmp(value, "LOG_USER") == 0) {
            facility = LOG_USER;
        }
        if (strcmp(value, "LOG_DAEMON") == 0) {
            facility = LOG_DAEMON;
        }
        if (strcmp(value, "LOG_LOCAL0") == 0) {
            facility = LOG_LOCAL0;
        }
        if (strcmp(value, "LOG_LOCAL1") == 0) {
            facility = LOG_LOCAL1;
        }
        if (strcmp(value, "LOG_LOCAL2") == 0) {
            facility = LOG_LOCAL2;
        }
        if (strcmp(value, "LOG_LOCAL3") == 0) {
            facility = LOG_LOCAL3;
        }
        if (strcmp(value, "LOG_LOCAL4") == 0) {
            facility = LOG_LOCAL4;
        }
        openlog(g_vprefix, LOG_PID | LOG_NOWAIT | LOG_ODELAY, facility);
    } else if (g_iscfengine) {
        openlog(g_vprefix, LOG_PID | LOG_NOWAIT | LOG_ODELAY, LOG_USER);
    } else {
        openlog(g_vprefix, LOG_PID | LOG_NOWAIT | LOG_ODELAY, LOG_DAEMON);
   }
}

/* ----------------------------------------------------------------- */

void CfLog(enum cfoutputlevel level, char *string, char *errstr)
{
    int endl = false;
    char *sp; 
    char buffer[1024];

    if ((string == NULL) || (strlen(string) == 0)) {
        return;
    }

    strncpy(buffer, string, 1022);
    buffer[1023] = '\0';

    /* 
     * Check for %s %m which someone might be able to insert into an
     * error message in order to get a syslog buffer overflow. Bug
     * reported by Pekka Savola 
     */
    for (sp = buffer; *sp != '\0'; sp++) {
        if ((*sp == '%') && (*(sp+1) >= 'a')) {
            *sp = '?';
        }
    }


#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)

    /* 
     * If we can't lock this could be dangerous to proceed with threaded
     * file descriptors 
     */
    if (!g_silent && (pthread_mutex_lock(&MUTEX_SYSCALL) != 0)) {
        return;
    }
    /* 
     * CfLog(cferror,"pthread_mutex_lock failed","lock");
     *    would lead to sick recursion 
     */

#endif

    switch(level) {
    case cfsilent:
        if (! g_silent || g_verbose || g_debug || g_d2) {
            ShowAction();
            printf("%s: %s", g_vprefix, buffer);
            endl = true;
        }
        break;
    case cfinform:
        if (g_silent) {
            return;
        }

        if (g_inform || g_verbose || g_debug || g_d2) {
            ShowAction();
            printf("%s: %s", g_vprefix, buffer);
            endl = true;
        }

        if (g_logging && IsPrivileged() && !g_dontdo) {
            syslog(LOG_NOTICE, "%s", buffer);

            if ((errstr != NULL) && (strlen(errstr) != 0)) {
                syslog(LOG_ERR, "%s: %s", errstr, strerror(errno));
            }
        }
        break;
    case cfverbose:
        if (g_verbose || g_debug || g_d2) {
            if ((errstr == NULL) || (strlen(errstr) > 0)) {
                ShowAction();
                printf("%s: %s\n", g_vprefix, buffer);
                printf("%s: %s", g_vprefix, errstr);
                endl = true;
            } else {
                ShowAction();
                printf("%s: %s", g_vprefix, buffer);
                endl = true;
            }
        }
        break;
    case cfeditverbose:
        if (g_editverbose || g_debug) {
            ShowAction();
            printf("%s: %s", g_vprefix, buffer);
            endl = true;
        }
        break;
    case cflogonly:
        if (g_logging && IsPrivileged() && !g_dontdo) {
            syslog(LOG_ERR, " %s", buffer);

            if ((errstr != NULL) && (strlen(errstr) > 0)) {
                syslog(LOG_ERR, " %s", errstr);
            }
        }
        break;
    case cferror:
        printf("%s: %s", g_vprefix, buffer);

        if (g_logging && IsPrivileged() && !g_dontdo) {
            syslog(LOG_ERR, " %s", buffer);
        }

        if (buffer[strlen(buffer)-1] != '\n') {
            printf("\n");
        }

        if ((errstr != NULL) && (strlen(errstr) > 0)) {
            ShowAction();
            printf("%s: %s: %s\n", g_vprefix, errstr, strerror(errno));
            endl = true;

            if (g_logging && IsPrivileged() && !g_dontdo) {
                syslog(LOG_ERR, " %s: %s", errstr, strerror(errno));
            }
        }
    }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
        /* CfLog(cferror,"pthread_mutex_unlock failed","lock");*/
    }
#endif


    if (endl && (buffer[strlen(buffer)-1] != '\n')) {
        printf("\n");
    }
}

/* ----------------------------------------------------------------- */

/* t = true, f = false, d = default */
void
ResetOutputRoute (char log, char inform)
{
    if ((log == 'y') || (log == 'n') 
            || (inform == 'y') || (inform == 'n')) {

        g_inform_save = g_inform;
        g_logging_save = g_logging;

        switch (log) {
        case 'y':
            g_logging = true;
            break;
        case 'n':
            g_logging = false;
            break;
        }

        switch (inform) {
        case 'y':
            g_inform = true;
            break;
        case 'n':
            g_inform = false;
            break;
        }
    } else {
        g_inform = g_inform_save;
        g_logging = g_logging_save;
    }
}

/* ----------------------------------------------------------------- */

void
ShowAction(void)
{
    if (g_showactions) {
        printf("%s:",g_cflock);
    }
}
