/*
 * $Id: modules.c 682 2004-05-20 22:46:52Z skaar $
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

void CheckClass(char *class, char *source);

/* ----------------------------------------------------------------- */

int
CheckForModule(char *actiontxt,char *args)
{
    struct stat statbuf;
    char line[CF_BUFSIZE],command[CF_BUFSIZE];
    char name[CF_MAXVARSIZE],content[CF_BUFSIZE],*sp;
    char ebuff[CF_EXPANDSIZE];
    FILE *pp;
    int print;

    if (g_nomodules) {
        return false;
    }

    if (GetMacroValue(g_contextid,"moduledirectory")) {
        ExpandVarstring("$(moduledirectory)",ebuff,NULL);
    } else {
        snprintf(ebuff, CF_BUFSIZE, "%s/modules", WORKDIR);
    }

    AddSlash(ebuff);
    strcat(ebuff,actiontxt);

    if (stat(ebuff,&statbuf) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"(Plug-in %s not found)",ebuff);
        Banner(g_output);
        return false;
    }

    if ((statbuf.st_uid != 0) && (statbuf.st_uid != getuid())) {
        snprintf(g_output,CF_BUFSIZE*2,
                "Module %s was not owned by uid=%d executing cfagent\n",
                ebuff,getuid());
        CfLog(cferror,g_output,"");
        return false;
    }

    snprintf(g_output,CF_BUFSIZE*2,"Plug-in `%s\'",actiontxt);
    Banner(g_output);

    strcat(ebuff," ");

    if (BufferOverflow(ebuff,args)) {
        snprintf(g_output,CF_BUFSIZE*2,
                "Culprit: class list for module (shouldn't happen)\n" );
        CfLog(cferror,g_output,"");
        return false;
    }

    strcat(ebuff,args);
    ExpandVarstring(ebuff,command,NULL);

    Verbose("Exec module [%s]\n",command);

    if ((pp = cfpopen(command,"r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't open pipe from %s\n", actiontxt);
        CfLog(cferror,g_output,"cfpopen");
        return false;
    }

    while (!feof(pp)) {

        /* abortable */
        if (ferror(pp)) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Shell command pipe %s\n", actiontxt);
            CfLog(cferror,g_output,"ferror");
            break;
        }

        ReadLine(line,CF_BUFSIZE,pp);

        if (strlen(line) > CF_BUFSIZE - 80) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "Line from module %s is too long to be sensible\n",
                    actiontxt);
            CfLog(cferror,g_output,"");
            break;
        }

        /* abortable */
        if (ferror(pp)) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Shell command pipe %s\n", actiontxt);
            CfLog(cferror,g_output,"ferror");
            break;
        }

        print = false;

        for (sp = line; *sp != '\0'; sp++) {
            if (! isspace((int)*sp)) {
                print = true;
                break;
            }
        }

        switch (*line) {
        case '+':
            Verbose("Activated classes: %s\n",line+1);
            CheckClass(line+1,command);
            AddMultipleClasses(line+1);
            break;
        case '-':
            Verbose("Deactivated classes: %s\n",line+1);
            CheckClass(line+1,command);
            NegateCompoundClass(line+1,&g_vnegheap);
            break;
        case '=':
            sscanf(line+1,"%[^=]=%[^\n]",name,content);
            AddMacroValue(g_contextid,name,content);
            break;

        default:
            if (print) {
                snprintf(g_output,CF_BUFSIZE,"%s: %s\n",actiontxt,line);
                CfLog(cferror,g_output,"");
            }
        }
    }

    cfpclose(pp);
    return true;
}



/* ----------------------------------------------------------------- */


void
CheckClass(char *name,char *source)
{
    char *sp;

    for (sp = name; *sp != '\0'; sp++) {
        if (!isalnum((int)*sp) && (*sp != '_')) {

            snprintf(g_output, CF_BUFSIZE, 
                    "Module class contained an illegal character (%c). "
                    "Only alphanumeric or underscores in classes.", *sp);

            CfLog(cferror,g_output,"");
        }
    }
}
