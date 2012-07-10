/*
 * $Id:$
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
 * Header for cfservd - Not generically includable!
 */

extern FILE *yyin;

#define queuesize 50
#define connection 1
#define CFD_INPUT "cfservd.conf"
#define CFD_SOURCE 1
#define RFC931_PORT 113
#define CFSERVD 1

/* ----------------------------------------------------------------- */

struct cfd_connection {
    int  id_verified;
    int  rsa_auth;
    int  synchronized;
    int  maproot;
    int  trust;
    int  sd_reply;
    unsigned char *session_key;
    char hostname[CF_MAXVARSIZE];
    char username[CF_MAXVARSIZE];
    uid_t uid;
    char ipaddr[CF_MAX_IP_LEN];
    char output[CF_BUFSIZE*2];   /* Threadsafe output channel */
};

struct cfd_get_arg {
    struct cfd_connection *connect;
    int encrypt;
    int buf_size;
    char *replybuff;
    char *replyfile;
};


/*
 * --------------------------------------------------------------------
 * Parser
 * --------------------------------------------------------------------
 */

char CFRUNCOMMAND[CF_BUFSIZE];
time_t CFDSTARTTIME;

#ifdef RE_DUP_MAX
# undef RE_DUP_MAX
#endif

/*
 * --------------------------------------------------------------------
 * Sunos4.1.4 need these prototypes
 * --------------------------------------------------------------------
 */

#if defined(SUN4)
extern char *realpath(/* char *path; char resolved_path[MAXPATHLEN] */);
#endif

#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long int) 0xffffffff)
#endif
