/*
 * $Id: state.c 741 2004-05-23 06:55:46Z skaar $
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
#include <db.h>

struct CfState {
   unsigned int expires;
   enum statepolicy policy;
};

/* ----------------------------------------------------------------- */

void
AddPersistentClass(char *name, unsigned int ttl_minutes,
        enum statepolicy policy)
{
    int errno;
    DBT key,value;
    DB *dbp;
    struct CfState state;
    time_t now = time(NULL),expires;
    char filename[CF_BUFSIZE];

    snprintf(filename,CF_BUFSIZE,"%s/%s",g_vlockdir,CF_STATEDB_FILE);

    if ((errno = db_create(&dbp,NULL,0)) != 0) {
        snprintf(g_output,CF_BUFSIZE,
                "Couldn't open average database %s\n",filename);
        CfLog(cferror,g_output,"db_open");
        return;
    }

#ifdef CF_OLD_DB
    if ((errno = dbp->open(dbp,filename,NULL,DB_BTREE,
                    DB_CREATE,0644)) != 0)
#else
    if ((errno = dbp->open(dbp,NULL,filename,NULL,DB_BTREE,
                    DB_CREATE,0644)) != 0)
#endif
    {
        snprintf(g_output, CF_BUFSIZE, 
                "Couldn't open average database %s\n", filename);
        CfLog(cferror,g_output,"db_open");
        return;
    }

    chmod(filename,0644);

    memset(&key,0,sizeof(key));
    memset(&value,0,sizeof(value));

    key.data = name;
    key.size = strlen(name)+1;

    if ((errno = dbp->get(dbp,NULL,&key,&value,0)) != 0) {
        if (errno != DB_NOTFOUND) {
            dbp->err(dbp,errno,NULL);
            dbp->close(dbp,0);
            return;
        }
    }

    if (value.data != NULL) {
        bcopy(value.data,(void *)&state,sizeof(state));

        if (state.policy == cfpreserve) {
            if (now < state.expires) {

                Verbose("Persisent state %s is already in "
                        "a preserved state --  %d minutes to go\n", 
                        name, (state.expires-now) / 60);

                dbp->close(dbp,0);
                return;
            }
        }
    } else {
        Verbose("New state %s with expiry %d, policy %d\n",
                key.data,state.expires,state.policy);
    }


    memset(&key,0,sizeof(key));
    memset(&value,0,sizeof(value));

    key.data = name;
    key.size = strlen(name)+1;

    state.expires = now + ttl_minutes * 60;
    state.policy = policy;

    value.data = &state;
    value.size = sizeof(state);

    if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0) {
        CfLog(cferror,"put failed","db->put");
    }

    Verbose("(Re)Set persistent state %s for %d minutes\n",name,ttl_minutes);
    dbp->close(dbp,0);
}

/* ----------------------------------------------------------------- */

void
DeletePersistentClass(char *name)
{
    int errno;
    DBT key,value;
    DB *dbp;
    struct CfState state;
    time_t now = time(NULL),expires;
    char filename[CF_BUFSIZE];

    snprintf(filename,CF_BUFSIZE,"%s/%s",g_vlockdir,CF_STATEDB_FILE);

    if ((errno = db_create(&dbp,NULL,0)) != 0) {
        snprintf(g_output, CF_BUFSIZE,
                "Couldn't open average database %s\n", filename);
        CfLog(cferror,g_output,"db_open");
        return;
    }

#ifdef CF_OLD_DB
    if ((errno = dbp->open(dbp,filename,NULL,DB_BTREE,
                    DB_CREATE,0644)) != 0)
#else
    if ((errno = dbp->open(dbp,NULL,filename,NULL,DB_BTREE,
                    DB_CREATE,0644)) != 0)
#endif
    {
    snprintf(g_output,CF_BUFSIZE,"Couldn't open average database %s\n",filename);
    CfLog(cferror,g_output,"db_open");
    return;
    }

    chmod(filename,0644);

    memset(&key,0,sizeof(key));
    memset(&value,0,sizeof(value));

    key.data = name;
    key.size = strlen(name)+1;

    if ((errno = dbp->del(dbp,NULL,&key,0)) != 0) {
        CfLog(cferror,"","db_store");
    }

    Debug("Deleted persistent state %s if found\n",name);
    dbp->close(dbp,0);
}

/* ----------------------------------------------------------------- */

void
PersistentClassesToHeap(void)
{
    DBT key,value;
    DB *dbp;
    DBC *dbcp;
    DB_ENV *dbenv = NULL;
    int ret;
    time_t now = time(NULL);
    struct stat statbuf;
    struct CfState q;
    char filename[CF_BUFSIZE];

    Banner("Loading persistent classes");

    snprintf(filename,CF_BUFSIZE,"%s/%s",g_vlockdir,CF_STATEDB_FILE);

    if ((errno = db_create(&dbp,dbenv,0)) != 0) {
        snprintf(g_output,CF_BUFSIZE*2,
                "Couldn't open checksum database %s\n",filename);
        CfLog(cferror,g_output,"db_open");
        return;
    }

#ifdef CF_OLD_DB
    if ((errno = dbp->open(dbp,filename,NULL,DB_BTREE,
                    DB_CREATE,0644)) != 0)
#else
    if ((errno = dbp->open(dbp,NULL,filename,NULL,DB_BTREE,
                    DB_CREATE,0644)) != 0)
#endif
    {
        snprintf(g_output,CF_BUFSIZE*2,
                "Couldn't open persistent state database %s\n",filename);
        CfLog(cferror,g_output,"db_open");
        dbp->close(dbp,0);
        return;
    }

    /* Acquire a cursor for the database. */
    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        CfLog(cferror,"Error reading from persistent state database","");
        dbp->err(dbp, ret, "DB->cursor");
        return;
    }

    /* Initialize the key/data return pair. */
    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));

    /* Walk through the database and print out the key/data pairs. */
    while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0) {
        bcopy(value.data,(void *)&q,sizeof(struct CfState));

        Debug(" - Found key %s...\n",key.data);

        if (now > q.expires) {
            Verbose(" Persistent class %s expired\n",key.data);
            if ((errno = dbp->del(dbp,NULL,&key,0)) != 0) {
                CfLog(cferror,"","db_store");
            }
        } else {

            Verbose(" Persistent class %s for %d more minutes\n",
                    key.data,(q.expires-now)/60);

            Verbose(" Adding persistent class %s to heap\n",key.data);
            AddMultipleClasses(key.data);
        }
    }

    dbcp->c_close(dbcp);
    dbp->close(dbp,0);

    Banner("Loaded persistent memory");
}

/* ----------------------------------------------------------------- */

void
DePort(char *address)
{
    char *sp;
    int dot = 0, colon = 0;

    for (sp = address; *sp != '\0'; sp++) {
        if (*sp == ':')
            colon++;
        if (*sp == '.')
            dot++;
    }

    if (colon && dot)
        Debug("No port number found\n");
    else if (dot > 3)
        Debug("No port number found\n");
    else
        return;

    for (sp = address+strlen(address); *sp != '.'; sp--)
        if (sp < address+strlen(address)-12)
        return;

    *sp = '\0';
}
