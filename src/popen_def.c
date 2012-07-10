/*
 * $Id: popen_def.c 682 2004-05-20 22:46:52Z skaar $
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

extern pid_t *CHILD;
extern int    MAXFD; /* Max number of simultaneous pipes */

/* ----------------------------------------------------------------- */

int
cfpclose_def(FILE *pp,char *defines,char *elsedef)
{
    int fd, status, wait_result;
    pid_t pid;

    Debug("cfpclose_def(pp)\n");

    /* popen hasn't been called */
    if (CHILD == NULL)  {
        return -1;
    }

    fd = fileno(pp);

    if ((pid = CHILD[fd]) == 0) {
        return -1;
    }

    CHILD[fd] = 0;

    if (fclose(pp) == EOF) {
        return -1;
    }

    Debug("cfpopen_def - Waiting for process %d\n",pid);

#ifdef HAVE_WAITPID

    while(waitpid(pid,&status,0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }

    if (status == 0) {
        AddMultipleClasses(defines);
    } else {
        AddMultipleClasses(elsedef);
        Debug("Child returned status %d\n",status);
    }

    return status;

#else

    while ((wait_result = wait(&status)) != pid) {
        if (wait_result <= 0) {
            snprintf(g_output,CF_BUFSIZE,"Wait for child failed\n");
            CfLog(cfinform,g_output,"wait");
            return -1;
        }
    }

    if (WIFSIGNALED(status)) {
        return -1;
    }

    if (! WIFEXITED(status)) {
        return -1;
    }

    if (WEXITSTATUS(status) == 0) {
        AddMultipleClasses(defines);
    } else {
        AddMultipleClasses(elsedef);
    }

    return (WEXITSTATUS(status));
#endif
}

