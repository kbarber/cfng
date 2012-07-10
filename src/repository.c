/*
 * $Id: repository.c 744 2004-05-23 07:53:59Z skaar $
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
 * Repository handler
 */

#include "cf.defs.h"
#include "cf.extern.h"
#include "../pub/global.h"


/* 
 * Returns true if the file was backup up and false if not 
 */
int Repository(char *file,char *repository)
{
    char buffer[CF_BUFSIZE];
    char localrepository[CF_BUFSIZE];
    char node[CF_BUFSIZE];
    struct stat sstat, dstat;
    char *sp;
    struct Image dummy;
    short imagecopy;

    if (repository == NULL) {
        strncpy(localrepository,g_vrepository,CF_BUFSIZE);
    } else {
        if (strcmp(repository,"none") == 0 
                || strcmp(repository,"off") == 0) {
            return false;
        }
        strncpy(localrepository,repository,CF_BUFSIZE);
    }

    if (g_imagebackup == 'n') {
        return true;
    }

    if (IsItemIn(g_vreposlist,file)) {
        snprintf(g_output,CF_BUFSIZE,
            "The file %s has already been moved to the repository once.",file);
        CfLog(cfinform,g_output,"");

        snprintf(g_output, CF_BUFSIZE,
            "Multiple update will cause loss of backup. "
            "Use backup=false in copy to override.");

        CfLog(cfinform,g_output,"");
        return true;
    }

    PrependItem(&g_vreposlist,file,NULL);

    if ((strlen(localrepository) == 0) || g_homecopy) {
        return false;
    }

    Debug2("Repository(%s)\n",file);

    strcpy (node,file);

    buffer[0] = '\0';

    for (sp = node; *sp != '\0'; sp++) {
        if (*sp == '/') {
            *sp = g_reposchar;
        }
    }

    strncpy(buffer,localrepository,CF_BUFSIZE-2);
    AddSlash(buffer);

    if (BufferOverflow(buffer,node)) {
        printf("culprit: Repository()\n");
        return false;
    }

    strcat(buffer,node);

    if (!MakeDirectoriesFor(buffer,'y')) {
        snprintf(g_output,CF_BUFSIZE,"Repository (%s),testfile (%s)",
                localrepository,buffer);
    }

    if (stat(file,&sstat) == -1) {
        Debug2("Repository file %s not there\n",file);
        return true;
    }

    stat(buffer,&dstat);

    /* without this there would be loop between this and Repository */

    imagecopy = g_imagebackup;
    g_imagebackup = 'n';

    dummy.server = "localhost";
    dummy.inode_cache = NULL;
    dummy.cache = NULL;
    dummy.stealth = 'n';
    dummy.encrypt = 'n';
    dummy.preservetimes = 'n';
    dummy.repository = NULL;

    CheckForHoles(&sstat,&dummy);

    if (CopyReg(file,buffer,sstat,dstat,&dummy)) {
        g_imagebackup = imagecopy;
        return true;
    } else {
        g_imagebackup = imagecopy;
        return false;
    }
}
