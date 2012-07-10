/*
 * $Id: ip.c 748 2004-05-25 14:47:10Z skaar $
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

/* Handle ipv4 or ipv6 connection */
int
RemoteConnect(char *host,int forceipv4)
{
    int err;

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)

    if (forceipv4 == 'n') {
        struct addrinfo query, *response, *ap;
        struct addrinfo query2, *response2, *ap2;
        int err,connected = false;

        memset(&query,0,sizeof(struct addrinfo));

        query.ai_family = AF_UNSPEC;
        query.ai_socktype = SOCK_STREAM;

        if ((err = getaddrinfo(host,"5308",&query,&response)) != 0) {
            snprintf(g_output,CF_BUFSIZE,
                    "Unable to lookup hostname or cfengine service: %s",
                    gai_strerror(err));
            CfLog(cfinform,g_output,"");
            return false;
        }

        for (ap = response; ap != NULL; ap = ap->ai_next) {
            Verbose("Connect to %s = %s on port cfengine\n",
                    host,sockaddr_ntop(ap->ai_addr));

            if ((g_conn->sd = socket(ap->ai_family,ap->ai_socktype,ap->ai_protocol)) == -1) {

                CfLog(cfinform,"Couldn't open a socket","socket");
                continue;
            }


            if (g_bindinterface[0] != '\0') {
                memset(&query2,0,sizeof(struct addrinfo));

                query.ai_family = AF_UNSPEC;
                query.ai_socktype = SOCK_STREAM;

                if ((err = getaddrinfo(g_bindinterface,NULL,&query2,
                                &response2)) != 0) {
                    snprintf(g_output,CF_BUFSIZE,
                            "Unable to lookup hostname or "
                            "cfengine service: %s",
                            gai_strerror(err));
                    CfLog(cferror,g_output,"");
                    return false;
                }

                for (ap2 = response2; ap2 != NULL; ap2 = ap2->ai_next) {
                    if (bind(g_conn->sd, ap2->ai_addr, ap2->ai_addrlen) == 0) {
                        freeaddrinfo(response2);
                        break;
                    }
                }

                if (response2) {
                    free(response2);
                }
            }

            signal(SIGALRM,(void *)TimeOut);
            alarm(g_cf_timeout);

            if (connect(g_conn->sd,ap->ai_addr,ap->ai_addrlen) >= 0) {
                connected = true;
                alarm(0);
                signal(SIGALRM,SIG_DFL);
                break;
            }

            alarm(0);
            signal(SIGALRM,SIG_DFL);
        }

        if (connected) {
            g_conn->family = ap->ai_family;
            snprintf(g_conn->remoteip,CF_MAX_IP_LEN-1,"%s",
                    sockaddr_ntop(ap->ai_addr));
        } else {
            close(g_conn->sd);
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Couldn't connect to host %s\n", host);
            g_conn->sd = CF_NOT_CONNECTED;
        }

        if (response != NULL) {
            freeaddrinfo(response);
        }

        if (!connected) {
            return false;
        }
    } else
#endif
/* ---------------------- only have ipv4 ---------------------------------*/
    {
        struct hostent *hp;
        struct sockaddr_in cin;
        memset(&cin,0,sizeof(cin));

        if ((hp = gethostbyname(host)) == NULL) {
            snprintf(g_output, CF_BUFSIZE, "Unable to look up "
                    "IP address of %s", host);
            CfLog(cferror,g_output,"gethostbyname");
            return false;
        }

        cin.sin_port = CfenginePort();
        cin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
        cin.sin_family = AF_INET;

        Verbose("Connect to %s = %s, port h=%d\n",
                host,inet_ntoa(cin.sin_addr),htons(g_portnumber));

        if ((g_conn->sd = socket(AF_INET,SOCK_STREAM,0)) == -1) {
            CfLog(cferror,"Couldn't open a socket","socket");
            return false;
        }

        if (g_bindinterface[0] != '\0') {
            Verbose("Cannot bind interface with this OS.\n");
            /* Could fix this - any point? */
        }


        g_conn->family = AF_INET;
        snprintf(g_conn->remoteip, CF_MAX_IP_LEN-1,
                "%s", inet_ntoa(cin.sin_addr));

        signal(SIGALRM,(void *)TimeOut);
        alarm(g_cf_timeout);

        if (err=connect(g_conn->sd,(void *)&cin,sizeof(cin)) == -1) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Couldn't connect to host %s\n", host);
            CfLog(cfinform,g_output,"connect");
            return false;
        }

        alarm(0);
        signal(SIGALRM,SIG_DFL);
    }
    LastSeen(host,cf_connect);
    return true;
}

/* ----------------------------------------------------------------- */

short
CfenginePort(void)
{
    struct servent *server;

    if ((server = getservbyname(CFENGINE_SERVICE,"tcp")) == NULL) {
        CfLog(cflogonly,
                "Couldn't get cfengine service, "
                "using default","getservbyname");
        return htons(5308);
    }

    return server->s_port;
}


/* ----------------------------------------------------------------- */

char *
Hostname2IPString(char *hostname)
{
    static char ipbuffer[65];
    int err;

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)

    struct addrinfo query, *response, *ap;

    memset(&query,0,sizeof(struct addrinfo));
    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_STREAM;

    memset(ipbuffer,0,63);

    if ((err = getaddrinfo(hostname,NULL,&query,&response)) != 0) {
        snprintf(g_output,CF_BUFSIZE,
                "Unable to lookup hostname (%s) or cfengine service: %s",
                hostname,gai_strerror(err));
        CfLog(cferror,g_output,"");
        return hostname;
    }

    for (ap = response; ap != NULL; ap = ap->ai_next) {
        strncpy(ipbuffer,sockaddr_ntop(ap->ai_addr),64);
        Debug("Found address (%s) for host %s\n",ipbuffer,hostname);

        freeaddrinfo(response);
        return ipbuffer;
    }
#else
    struct hostent *hp;
    struct sockaddr_in cin;
    memset(&cin,0,sizeof(cin));

    memset(ipbuffer,0,63);

    if ((hp = gethostbyname(hostname)) != NULL) {
        cin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
        strncpy(ipbuffer,inet_ntoa(cin.sin_addr),63);
        Verbose("Found address (%s) for host %s\n",ipbuffer,hostname);
        return ipbuffer;
    }
#endif

    snprintf(ipbuffer,63,"Unknown IP %s",hostname);
    return ipbuffer;
}


/* ----------------------------------------------------------------- */

/* String form (char *ipaddress) */
char *
IPString2Hostname(char *ipaddress)
{
    static char hostbuffer[128];
    int err;

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)

    struct addrinfo query, *response, *ap;

    memset(&query,0,sizeof(query));
    memset(&response,0,sizeof(response));

    query.ai_flags = AI_CANONNAME;

    memset(hostbuffer,0,128);

    if ((err = getaddrinfo(ipaddress,NULL,&query,&response)) != 0) {
        snprintf(g_output,CF_BUFSIZE,"Unable to lookup IP address (%s): %s",
                ipaddress,gai_strerror(err));
        CfLog(cferror,g_output,"");
        snprintf(hostbuffer,127,"(Non registered IP)");
        return hostbuffer;
    }

    for (ap = response; ap != NULL; ap = ap->ai_next) {

        if ((err = getnameinfo(ap->ai_addr,ap->ai_addrlen,
                        hostbuffer,127,0,0,0)) != 0) {

            snprintf(hostbuffer,127,"(Non registered IP)");
            freeaddrinfo(response);
            return hostbuffer;
        }

        Debug("Found address (%s) for host %s\n",hostbuffer,ipaddress);
        freeaddrinfo(response);
        return hostbuffer;
    }

    snprintf(hostbuffer,127,"(Non registered IP)");
#else
    snprintf(hostbuffer,127,"(Not for old BIND)");
#endif
    return hostbuffer;
}

/* ----------------------------------------------------------------- */

void
LastSeen(char *hostname, enum roles role)
{
    DBT key,value;
    DB *dbp;
    DB_ENV *dbenv = NULL;
    char name[CF_BUFSIZE],databuf[CF_BUFSIZE];
    time_t lastseen,now = time(NULL);
    static struct LastSeen entry;
    double average;

    snprintf(name,CF_BUFSIZE-1,"%s/%s",g_vlockdir,CF_LASTDB_FILE);

    if ((errno = db_create(&dbp,dbenv,0)) != 0) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't open last-seen database %s\n", name);
        CfLog(cferror,g_output,"db_open");
        return;
    }

#ifdef CF_OLD_DB
    if ((errno = dbp->open(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
    if ((errno = dbp->open(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
    snprintf(g_output, CF_BUFSIZE*2, 
            "Couldn't open last-seen database %s\n",name);
    CfLog(cferror,g_output,"db_open");
    dbp->close(dbp,0);
    return;
   }

    memset(&value,0,sizeof(value));
    memset(&key,0,sizeof(key));

    switch (role) {
    case cf_accept:
        snprintf(databuf,CF_BUFSIZE-1,"%s (hailed us)",hostname);
        break;
    case cf_connect:
        snprintf(databuf,CF_BUFSIZE-1,"%s (answered us)",hostname);
        break;
    }

    key.data = databuf;
    key.size = strlen(databuf)+1;

    value.data = (void *)&entry;
    value.size = sizeof(entry);

    if ((errno = dbp->get(dbp,NULL,&key,&value,0)) == 0) {
        average = entry.expect_lastseen;
        lastseen = entry.lastseen;

        /* Update the geometrical memory of the expectation value for
         * this arrival-process */

        entry.lastseen = now;
        entry.expect_lastseen = (0.7 * average + 0.3 *
                (double)(now - lastseen));

        key.data = databuf;
        key.size = strlen(databuf)+1;

        Verbose("Updating last-seen time for %s\n",hostname);

        if ((errno = dbp->del(dbp,NULL,&key,0)) != 0) {
            CfLog(cferror,"","db->del");
        }

        key.data = databuf;
        key.size = strlen(databuf)+1;
        value.data = (void *) &entry;
        value.size = sizeof(entry);

        if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0) {
            CfLog(cferror,"put failed","db->put");
        }

        dbp->close(dbp,0);
    } else {
        key.data = databuf;
        key.size = strlen(databuf)+1;

        entry.lastseen = now;
        entry.expect_lastseen = 0;

        value.data = (void *) &entry;
        value.size = sizeof(entry);

        if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0) {
            CfLog(cferror,"put failed","db->put");
        }

        dbp->close(dbp,0);
    }
}

/* ----------------------------------------------------------------- */

/* 
 * Go through the database of recent connections and check for Long Time
 * No See ...
 */

void CheckFriendConnections(int hours)
{
    DBT key,value;
    DB *dbp;
    DBC *dbcp;
    DB_ENV *dbenv = NULL;
    int ret, secs = 3600*hours, criterion;
    struct stat statbuf;
    time_t now = time(NULL),splaytime = 0;
    char name[CF_BUFSIZE];
    static struct LastSeen entry;
    double average = 0;

    if (GetMacroValue(g_contextid,"SplayTime")) {
        splaytime = atoi(GetMacroValue(g_contextid,"SplayTime"));
        if (splaytime < 0) {
            splaytime = 0;
        }
    }

    Verbose("CheckFriendConnections(%d)\n",hours);
    snprintf(name,CF_BUFSIZE-1,"%s/%s",g_vlockdir,CF_LASTDB_FILE);

    if ((errno = db_create(&dbp,dbenv,0)) != 0) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't open last-seen database %s\n", name);
        CfLog(cferror,g_output,"db_open");
        return;
    }

#ifdef CF_OLD_DB
    if ((errno = dbp->open(dbp,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#else
    if ((errno = dbp->open(dbp,NULL,name,NULL,DB_BTREE,DB_CREATE,0644)) != 0)
#endif
   {
    snprintf(g_output, CF_BUFSIZE*2, 
            "Couldn't open last-seen database %s\n", name);
    CfLog(cferror,g_output,"db_open");
    dbp->close(dbp,0);
    return;
   }

    /* Acquire a cursor for the database. */

    if ((ret = dbp->cursor(dbp, NULL, &dbcp, 0)) != 0) {
        CfLog(cferror,"Error reading from last-seen database","");
        dbp->err(dbp, ret, "DB->cursor");
        return;
    }

    /* Initialize the key/data return pair. */

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    memset(&entry, 0, sizeof(entry));

    /* Walk through the database and print out the key/data pairs. */

    while (dbcp->c_get(dbcp, &key, &value, DB_NEXT) == 0) {
        time_t then;

        bcopy(value.data,&then,sizeof(then));

        if (value.data != NULL) {
            bcopy(value.data,&entry,sizeof(entry));
            then = entry.lastseen;
            average = entry.expect_lastseen;
        } else {
            continue;
        }

        if (secs == 0) {
            criterion = now > then + splaytime + (int)(average+0.5);
        } else {
            criterion = now > then + splaytime + secs;
        }

        if ((int)average > CF_WEEK) {
            criterion = false;
        }

        /* anomalous couplings do not count*/
        if (average < 1800) {
            criterion = false;
        }

        snprintf(g_output, CF_BUFSIZE, 
                "Host %s last at %s\ti.e. not seen for %.2fhours\n\t"
                "(Expected <delta_t> = %.2f secs (= %.2f hours))",
                (char *)key.data,ctime(&then),
                (now-then) / 3600.0,average,
                average / 3600.0);

        if (criterion) {
            CfLog(cferror,g_output,"");

            if (now > CF_WEEK + then + splaytime + 2*3600) {

                snprintf(g_output, CF_BUFSIZE*2,
                        "INFO: Giving up on %s, last seen more than "
                        "a week ago at %s.", key.data, ctime(&then));

                CfLog(cferror,g_output,"");

            if ((errno = dbp->del(dbp,NULL,&key,0)) != 0) {
                CfLog(cferror,"","db_store");
                }
            }
        } else {
            CfLog(cfinform,g_output,"");
        }
    }

    dbcp->c_close(dbcp);
    dbp->close(dbp,0);
}
