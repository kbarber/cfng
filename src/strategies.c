/*
 * $Id: strategies.c 741 2004-05-23 06:55:46Z skaar $
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
InstallStrategy(char *alias,char *classes)
{
    struct Strategy *ptr;
    char ebuff[CF_EXPANDSIZE];

    Debug1("InstallStrategy(%s,%s)\n",alias,classes);

    if (! IsInstallable(classes)) {
        Debug1("Not installing Strategy no match\n");
        return;
    }

    ExpandVarstring(alias,ebuff,"");

    if ((ptr = (struct Strategy *)malloc(sizeof(struct Strategy))) == NULL) {
        FatalError("Memory Allocation failed for InstallStrategy() #1");
    }

    if ((ptr->name = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed in InstallStrategy");
    }

    ExpandVarstring(classes,ebuff,"");

    if ((ptr->classes = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed in InstallStrategy");
    }

        /* First element in the list */
    if (g_vstrategylisttop == NULL) {
        g_vstrategylist = ptr;
    } else {
        g_vstrategylisttop->next = ptr;
    }

    ptr->next = NULL;
    ptr->type = 'r';
    ptr->done = 'n';
    ptr->strategies = NULL;

    g_vstrategylisttop = ptr;
}

/* ----------------------------------------------------------------- */

void
AddClassToStrategy(char *alias,char *class,char *value)
{
    struct Strategy *sp;
    char buf[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];
    int val = -1;

    if (class[strlen(class)-1] != ':') {
        yyerror("Strategic class definition doesn't end in colon");
        return;
    }

    memset(buf,0,CF_MAXVARSIZE);
    sscanf(class,"%[^:]",&buf);

    ExpandVarstring(value,ebuff,"");
    Debug("AddClassToStrategy(%s,%s,%s)\n",alias,class,ebuff);

    sscanf(ebuff,"%d",&val);

    if (val <= 0) {
        yyerror("strategy distribution weight must be an integer");
        return;
    }

    for (sp = g_vstrategylist; sp != NULL; sp=sp->next) {
        if (strcmp(alias,sp->name) == 0) {
            AppendItem(&(sp->strategies),buf,ebuff);
            AddInstallable(buf);
        }
    }
}

/* ----------------------------------------------------------------- */

void
SetStrategies(void)
{
    struct Strategy *ptr;
    struct Item *ip;
    int total,count;
    double *array,*cumulative,cum,fluct;


    for (ptr = g_vstrategylist; ptr != NULL; ptr=ptr->next) {
        if (ptr->done == 'y') {
            continue;
        } else {
            ptr->done = 'y';
        }

        Verbose("\n  Evaluating strategy %s (type=%c)\n",ptr->name,ptr->type);
        if (ptr->strategies) {
            total = count = 0;
            for (ip = ptr->strategies; ip !=NULL; ip=ip->next) {
                count++;
                total += atoi(ip->classes);
            }

            count++;
            array = (double *)malloc(count*sizeof(double));
            cumulative = (double *)malloc(count*sizeof(double));
            count = 1;
            cum = 0.0;
            cumulative[0] = 0;

            for (ip = ptr->strategies; ip !=NULL; ip=ip->next) {
                array[count] = ((double)atoi(ip->classes))/((double)total);
                cum += array[count];
                cumulative[count] = cum;
                Debug("%s : %f cum %f\n",
                        ip->name,array[count],cumulative[count]);
                count++;
            }

            /* Get random number 0-1 */

            fluct = drand48();

            count = 1;
            cum = 0.0;

            for (ip = ptr->strategies; ip !=NULL; ip=ip->next) {
                Verbose("    Class %d: %f-%f\n",
                        count,cumulative[count-1],cumulative[count]);
                if ((cumulative[count-1] < fluct) &&
                        (fluct < cumulative[count])) {
                    Verbose("   - Choosing %s (%f)\n",ip->name,fluct);
                    AddClassToHeap(ip->name);
                    break;
                }
                count++;
            }
            free(cumulative);
            free(array);
        }
    }
}
