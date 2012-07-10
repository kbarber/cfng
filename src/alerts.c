/*
 * $Id: alerts.c 741 2004-05-23 06:55:46Z skaar $
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
DoAlerts(void)
{ 
    struct Item *ip;
    char ebuffer[CF_EXPANDSIZE]; 

    Banner("Alerts");
 
    for (ip = g_valerts; ip != NULL; ip=ip->next) {

        if (IsDefinedClass(ip->classes)) {

            if (!GetLock(ASUniqueName("alert"),
                    CanonifyName(ip->name),ip->ifelapsed,
                    ip->expireafter,g_vuqname,g_cfstarttime)) {
            continue;
        }
            if (IsBuiltinFunction(ip->name)) {
            memset(ebuffer,0,CF_EXPANDSIZE);
            strncpy(ebuffer,EvaluateFunction(ip->name,ebuffer),CF_EXPANDSIZE-1);

            if (strlen(ebuffer) > 0) {
                printf("%s: %s\n",g_vprefix,ebuffer);
            }

        } else {
            ExpandVarstring(ip->name,ebuffer,NULL);
            CfLog(cferror,ebuffer,"");
        }

            ReleaseCurrentLock();

        } else {
            Debug("Not evaluating %s::%s\n",ip->name,ip->classes);
        }
    }
}
