/*
 * $Id: sockaddr.c 744 2004-05-23 07:53:59Z skaar $
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

/*
 * Toolkit: INET independent address/struct conversion routines
 */

char *
sockaddr_ntop(struct sockaddr *sa)
{
#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    static char addrbuf[INET6_ADDRSTRLEN];
    void *addr;
#else
    static char addrbuf[20];
    struct in_addr addr;
#endif

    switch (sa->sa_family) {
    case AF_INET:
        Debug("IPV4 address\n");
        snprintf(addrbuf,20,"%.19s",
                inet_ntoa(((struct sockaddr_in *)sa)->sin_addr));
        break;

#ifdef AF_LOCAL
    case AF_LOCAL:
        Debug("Local socket\n") ;
        strcpy(addrbuf, "127.0.0.1") ;
        break;
#endif

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    case AF_INET6:
        Debug("IPV6 address\n");
        addr = &((struct sockaddr_in6 *)sa)->sin6_addr;
        inet_ntop(sa->sa_family,addr,addrbuf,sizeof(addrbuf));
        break;
#endif
    default:
        Debug("Address family was %d\n",sa->sa_family);
        FatalError("Software failure in sockaddr_ntop\n");
    }

    Debug("sockaddr_ntop(%s)\n",addrbuf);
    return addrbuf;
}

/* ----------------------------------------------------------------- */

/* 
 * Example:
 *
 * struct sockaddr_in *p;
 * struct sockaddr_in6 *p6;
 * 
 * p = (struct sockaddr_in *) sockaddr_pton(AF_INET,"128.39.89.10");
 * p6 = (struct sockaddr_in6 *) sockaddr_pton(AF_INET6,
 *                                  "2001:700:700:3:290:27ff:fea2:477b");
 *
 * printf("Coded %s\n",sockaddr_ntop((struct sockaddr *)p));
 */

/* ----------------------------------------------------------------- */

void *
sockaddr_pton(int af,void *src)
{
    int err;
#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    static struct sockaddr_in6 adr6;
#endif
    static struct sockaddr_in adr;

    switch (af) {
    case AF_INET:
        memset(&adr,0,sizeof(adr));
        adr.sin_family = AF_INET;
        adr.sin_addr.s_addr = inet_addr(src);
        Debug("Coded ipv4 %s\n",sockaddr_ntop((struct sockaddr *)&adr));
        return (void *)&adr;

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    case AF_INET6:
        memset(&adr6,0,sizeof(adr6));
        adr6.sin6_family = AF_INET6;
        err = inet_pton(AF_INET6,src,&(adr6.sin6_addr));

        if (err > 0) {
            Debug("Coded ipv6 %s\n",
                    sockaddr_ntop((struct sockaddr *)&adr6));
            return (void *)&adr6;
        } else {
            return NULL;
        }
        break;
#endif
    default:
        Debug("Address family was %d\n",af);
        FatalError("Software failure in sockaddr_pton\n");
    }
    return NULL;
}
