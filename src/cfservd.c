/*
 * $Id: cfservd.c 743 2004-05-23 07:27:32Z skaar $
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
 * cfservd: remote server daemon
 */


#include "../pub/getopt.h"
#include "cf.defs.h"
#include "cf.extern.h"
#include "cfservd.h"
#include <db.h>

/*
 * --------------------------------------------------------------------
 * Pthreads
 * --------------------------------------------------------------------
 */

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
pthread_attr_t PTHREADDEFAULTS;
pthread_mutex_t MUTEX_COUNT = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_HOSTNAME = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * --------------------------------------------------------------------
 * Global variables
 * --------------------------------------------------------------------
 */

int CLOCK_DRIFT = 3600;   /* 1hr */
int CFD_MAXPROCESSES = 0;
int ACTIVE_THREADS = 0;
int NO_FORK = false;
int MULTITHREAD = false;
int CHECK_RFC931 = false;
int CFD_INTERVAL = 0;
int DENYBADCLOCKS = true;
int MULTIPLECONNS = false;
int TRIES = 0;
int MAXTRIES = 5;
int LOGCONNS = false;

struct option CFDOPTIONS[] = {
    { "help",no_argument,0,'h' },
    { "debug",optional_argument,0,'d' },
    { "verbose",no_argument,0,'v' },
    { "file",required_argument,0,'f' },
    { "no-fork",no_argument,0,'F' },
    { "parse-only",no_argument,0,'p'},
    { "multithread",no_argument,0,'m'},
    { "ld-library-path",required_argument,0,'L'},
    { NULL,0,0,0 }
};


struct Item *CONNECTIONLIST = NULL;

/*
 * --------------------------------------------------------------------
 * Functions internal to cfservd.c
 * --------------------------------------------------------------------
 */

void CheckOptsAndInit(int argc,char **argv);
void CheckVariables(void);
void SummarizeParsing(void);
void StartServer(int argc, char **argv);
int OpenReceiverChannel(void);
void Syntax(void);
void PurgeOldConnections(struct Item **list,time_t now);
void SpawnConnection(int sd_reply, char *ipaddr);
void CheckFileChanges(int argc, char **argv, int sd);
void *HandleConnection(struct cfd_connection *conn);
int BusyWithConnection(struct cfd_connection *conn);
void *ExitCleanly(int signum);
int MatchClasses(struct cfd_connection *conn);
void DoExec(struct cfd_connection *conn, char *sendbuffer, char *args);
int GetCommand(char *str);
int VerifyConnection(struct cfd_connection *conn, char *buf);
void RefuseAccess(struct cfd_connection *conn, char *sendbuffer, int size, char *errormsg);
int AccessControl(char *filename, struct cfd_connection *conn, int encrypt);
int CheckStoreKey (struct cfd_connection *conn, RSA *key);
int StatFile(struct cfd_connection *conn, char *sendbuffer, char *filename);
void CfGetFile(struct cfd_get_arg *args);
void CompareLocalChecksum(struct cfd_connection *conn, char *sendbuffer, char *recvbuffer);
int CfOpenDirectory(struct cfd_connection *conn, char *sendbuffer, char *dirname);
void Terminate(int sd);
void DeleteAuthList(struct Auth *ap);
int AllowedUser(char *user);
void ReplyNothing(struct cfd_connection *conn);
struct cfd_connection *NewConn(int sd);
void DeleteConn(struct cfd_connection *conn);
time_t SecondsTillAuto(void);
void SetAuto(int seconds);
int cfscanf(char *in, int len1, int len2, char *out1, char *out2, char *out3);
int AuthenticationDialogue(struct cfd_connection *conn,char *buffer);
char *MapAddress(char *addr);
int IsWildKnownHost(RSA *oldkey,RSA *newkey,char *addr,char *user);
void AddToKeyDB(RSA *key,char *addr);
int SafeOpen(char *filename);
void SafeClose(int fd);

/*
 * HvB
*/
unsigned find_inet_addr(char *host);

/*
 * --------------------------------------------------------------------
 * Level 0 : Main
 * --------------------------------------------------------------------
 */

int
main (int argc, char **argv)
{

    CheckOptsAndInit(argc,argv);
    ParseInputFile(g_vinputfile);
    CheckVariables();
    SummarizeParsing();

    if (g_parseonly) {
        exit(0);
    }

    StartServer(argc,argv);

    /* Never exit here */
    return 0;
}

/*
 * --------------------------------------------------------------------
 * Level 1
 * --------------------------------------------------------------------
 */

void
CheckOptsAndInit(int argc, char **argv)
{
    extern char *optarg;
    int optindex = 0;
    char ld_library_path[CF_BUFSIZE], vbuff[CF_BUFSIZE];
    struct stat statbuf;
    int c;

    SetContext("server");
    sprintf(g_vprefix, "cfservd");
    CfOpenLog();
    strcpy(g_vinputfile,CFD_INPUT);
    strcpy(g_cflock,"cfservd");
    g_output[0] = '\0';

    /*
    * HvB: Bas van der Vlies
    */

    g_bindinterface[0] = '\0';

    SetSignals();

    g_iscfengine = false;   /* Switch for the parser */
    g_parseonly  = false;

    ld_library_path[0] = '\0';

    InstallObject("server");

    AddClassToHeap("any");      /* This is a reserved word / wildcard */

    while ((c=getopt_long(argc,argv,"L:d:f:vmhpFV",
                    CFDOPTIONS,&optindex)) != EOF) {

        switch ((char) c) {
        case 'f':
            strncpy(g_vinputfile,optarg,CF_BUFSIZE-1);
            g_minusf = true;
            break;
        case 'd':
            switch ((optarg==NULL)?3:*optarg) {
            case '1':
                g_d1 = true;
                break;
            case '2':
                g_d2 = true;
                break;
            default:
                g_debug = true;
                break;
            }
            NO_FORK = true;
            printf("cfservd: Debug mode: running in foreground\n");
            break;
        case 'v':
            g_verbose = true;
            break;
        case 'V':
            printf("GNU %s-%s daemon\n%s\n",PACKAGE,VERSION,g_copyright);
            printf("This program is covered by the GNU Public License "
                    "and may be\n");
            printf("copied free of charge. No warrenty is implied.\n\n");
            exit(0);
            break;
        case 'p':
            g_parseonly = true;
            break;
        case 'F':
            NO_FORK = true;
            break;
        case 'L':
            Verbose("Setting LD_LIBRARY_PATH=%s\n",optarg);
            snprintf(ld_library_path,CF_BUFSIZE-1,"LD_LIBRARY_PATH=%s",optarg);
            putenv(ld_library_path);
            break;
        case 'm':
            /* No longer needed */
            break;
        default:
            Syntax();
            exit(1);
        }
    }

    g_logging = true;                    /* Do output to syslog */

    IDClasses();
    GetNameInfo();
    GetInterfaceInfo();
    GetV6InterfaceInfo();

    if ((g_cfinitstarttime = time((time_t *)NULL)) == -1) {
        CfLog(cferror,"Couldn't read system clock\n","time");
    }

    if ((g_cfstarttime = time((time_t *)NULL)) == -1) {
        CfLog(cferror,"Couldn't read system clock\n","time");
    }

    snprintf(vbuff,CF_BUFSIZE,"%s/test",WORKDIR);

    MakeDirectoriesFor(vbuff,'y');

    sprintf(g_vbuff,"%s/logs",WORKDIR);
    strncpy(g_vlogdir,g_vbuff,CF_BUFSIZE-1);

    sprintf(g_vbuff,"%s/state",WORKDIR);
    strncpy(g_vlockdir,g_vbuff,CF_BUFSIZE-1);

    g_vifelapsed = CF_EXEC_IFELAPSED;
    g_vexpireafter = CF_EXEC_EXPIREAFTER;

    strcpy(g_vdomain,"undefined.domain");

    g_vcanonicalfile = strdup(CanonifyName(g_vinputfile));
    g_vrepository = strdup("\0");
    g_listseparator = ',';

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    CheckWorkDirectories();

    RandomSeed();
    LoadSecretKeys();
}

/* ----------------------------------------------------------------- */

void
CheckVariables(void)
{
    struct stat statbuf;
    int i, value = -1;
    char ebuff[CF_EXPANDSIZE];

#ifdef HAVE_PTHREAD_H
    CfLog(cfinform,"cfservd Multithreaded version","");
#else
    CfLog(cfinform,"cfservd Single threaded version","");
#endif

    strncpy(g_vfqname,g_vsysname.nodename,CF_MAXVARSIZE-1);

    if ((CFDSTARTTIME = time((time_t *)NULL)) == -1) {
        printf("Couldn't read system clock\n");
    }

    if (OptionIs(g_contextid,"CheckIdent", true)) {
        CHECK_RFC931 = true;
    }

    if (OptionIs(g_contextid,"DenyBadClocks", false)) {
        DENYBADCLOCKS = false;
    }

    if (OptionIs(g_contextid,"LogAllConnections", true)) {
        LOGCONNS = true;
    }

    if (GetMacroValue(g_contextid,"ChecksumDatabase")) {
        ExpandVarstring("$(ChecksumDatabase)",ebuff,NULL);

        g_checksumdb = strdup(ebuff);

        if (*g_checksumdb != '/') {
            FatalError("$(ChecksumDatabase) does not expand to an "
                    "absolute filename\n");
        }
    }


    memset(CFRUNCOMMAND,0,CF_BUFSIZE);

    if (GetMacroValue(g_contextid,"cfrunCommand")) {
        ExpandVarstring("$(cfrunCommand)",ebuff,NULL);

        if (*ebuff != '/') {
            FatalError("$(cfrunCommand) does not expand to an "
                    "absolute filename\n");
        }

        sscanf(ebuff,"%4095s",CFRUNCOMMAND);
        Debug("cfrunCommand is %s\n",CFRUNCOMMAND);

        if (stat(CFRUNCOMMAND,&statbuf) == -1) {
            FatalError("$(cfrunCommand) points to a non-existent file\n");
        }
    }

    if (GetMacroValue(g_contextid,"MaxConnections")) {
        ExpandVarstring("$(MaxConnections)",g_vbuff,NULL);
        Debug("$(MaxConnections) Expanded to %s\n",g_vbuff);

        CFD_MAXPROCESSES = atoi(g_vbuff);

        /* How many attempted connections over max before we commit
         * suicide? */
        MAXTRIES = CFD_MAXPROCESSES / 3;

        if ((CFD_MAXPROCESSES < 1) || (CFD_MAXPROCESSES > 1000)) {
            FatalError("cfservd MaxConnections with silly value");
        }
    } else {
        CFD_MAXPROCESSES = 10;
        MAXTRIES = 5;
    }

    CFD_INTERVAL = 0;

    Debug("MaxConnections = %d\n",CFD_MAXPROCESSES);

    g_checksumupdates = true;

    if (OptionIs(g_contextid,"ChecksumUpdates", false)) {
        g_checksumupdates = false;
    }

    i = 0;

    if (StrStr(g_vsysname.nodename,ToLowerStr(g_vdomain))) {
        strncpy(g_vfqname,g_vsysname.nodename,CF_MAXVARSIZE-1);

        while(g_vsysname.nodename[i++] != '.') { }

        strncpy(g_vuqname,g_vsysname.nodename,i-1);
    } else {
        snprintf(g_vfqname,CF_BUFSIZE,"%s.%s",g_vsysname.nodename,
                ToLowerStr(g_vdomain));
        strncpy(g_vuqname,g_vsysname.nodename,CF_MAXVARSIZE-1);
    }

    /*
    * HvB: Bas van der Vlies
    * bind to only one interface
    */
    if (GetMacroValue(g_contextid,"BindToInterface")) {
        ExpandVarstring("$(BindToInterface)",ebuff,NULL);
        strncpy(g_bindinterface, ebuff, CF_BUFSIZE-1);
        Debug("$(BindToInterface) Expanded to %s\n",g_bindinterface);
    }

}

/* ----------------------------------------------------------------- */

void
SummarizeParsing(void)
{
    struct Auth *ptr;
    struct Item *ip,*ipr;

    if (g_verbose || g_debug || g_d2 || g_d3) {
        ListDefinedClasses();
    }

    if (g_debug || g_d2 || g_d3) {
        printf("ACCESS GRANTED ----------------------:\n\n");

        for (ptr = g_vadmit; ptr != NULL; ptr=ptr->next) {
            printf("Path: %s (encrypt=%d)\n",ptr->path,ptr->encrypt);

            for (ip = ptr->accesslist; ip != NULL; ip=ip->next) {
                printf("   Admit: %s root=",ip->name);
                for (ipr = ptr->maproot; ipr !=NULL; ipr=ipr->next) {
                    printf("%s,",ipr->name);
                }
                printf("\n");
            }
        }

        printf("ACCESS DENIAL ------------------------ :\n\n");

        for (ptr = g_vdeny; ptr != NULL; ptr=ptr->next) {
            printf("Path: %s\n",ptr->path);

            for (ip = ptr->accesslist; ip != NULL; ip=ip->next) {
                printf("   Deny: %s\n",ip->name);
            }
        }

        printf("Host IPs allowed connection access :\n\n");

        for (ip = g_nonattackerlist; ip != NULL; ip=ip->next) {
            printf("IP: %s\n",ip->name);
        }

        printf("Host IPs denied connection access :\n\n");

        for (ip = g_attackerlist; ip != NULL; ip=ip->next) {
            printf("IP: %s\n",ip->name);
        }

        printf("Host IPs allowed multiple connection access :\n\n");

        for (ip = g_multiconnlist; ip != NULL; ip=ip->next) {
            printf("IP: %s\n",ip->name);
        }

        printf("Host IPs from whom we shall accept public keys on trust :\n\n");

        for (ip = g_trustkeylist; ip != NULL; ip=ip->next) {
            printf("IP: %s\n",ip->name);
        }

        printf("Host IPs from NAT which we don't verify :\n\n");

        for (ip = g_skipverify; ip != NULL; ip=ip->next) {
            printf("IP: %s\n",ip->name);
        }

        printf("Dynamical Host IPs (e.g. DHCP) whose bindings could "
                "vary over time :\n\n");

        for (ip = g_dhcplist; ip != NULL; ip=ip->next) {
            printf("IP: %s\n",ip->name);
        }

    }

    if (g_errorcount > 0) {
        FatalError("Execution terminated after parsing due to "
                "errors in program");
    }
}


/* ----------------------------------------------------------------- */

void
StartServer(int argc, char **argv)
{
    char ipaddr[CF_MAXVARSIZE],intime[64];
    int sd,sd_reply,ageing;
    fd_set rset;
    time_t now;

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    int addrlen=sizeof(struct sockaddr_in6);
    struct sockaddr_in6 cin;
#else
    int addrlen=sizeof(struct sockaddr_in);
    struct sockaddr_in cin;
#endif

    if ((sd = OpenReceiverChannel()) == -1) {
        CfLog(cferror,"Unable to start server","");
        exit(1);
    }

    signal(SIGINT,(void*)ExitCleanly);
    signal(SIGTERM,(void*)ExitCleanly);
    signal(SIGHUP,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);

    if (listen(sd,queuesize) == -1) {
        CfLog(cferror,"listen failed","listen");
        exit(1);
    }

    Verbose("Listening for connections ...\n");

    if ((!NO_FORK) && (fork() != 0)) {
        snprintf(g_output,CF_BUFSIZE*2,"cfservd starting %.24s\n",
                ctime(&CFDSTARTTIME));
        CfLog(cfinform,g_output,"");
        exit(0);
    }

    if (!NO_FORK) {
        ActAsDaemon(sd);
    }

    ageing = 0;

    /* Andrew Stribblehill <ads@debian.org> -- close sd on exec */
    fcntl(sd, F_SETFD, FD_CLOEXEC);

    while (true) {
        if (ACTIVE_THREADS == 0) {
            CheckFileChanges(argc,argv,sd);
        }

        FD_ZERO(&rset);
        FD_SET(sd,&rset);
        if (select((sd+1),&rset,NULL,NULL,NULL) == -1) {
            if (errno == EINTR) {
                continue;
            } else {
                CfLog(cferror, "select failed", "select");
                exit(1);
            }
        }

        if ((sd_reply = accept(sd,(struct sockaddr *)&cin,&addrlen)) != -1) {

            /* 
             * Insurance against stale db estimate number of clients
             * arbitrary policy ..
             */
            if (ageing++ > CFD_MAXPROCESSES*50) {
                unlink(g_checksumdb);
                ageing = 0;
            }

            memset(ipaddr,0,CF_MAXVARSIZE);
            snprintf(ipaddr,CF_MAXVARSIZE-1,"%s",
                    sockaddr_ntop((struct sockaddr *)&cin));

            Debug("Obtained IP address of %s on socket %d from accept\n",
                    ipaddr,sd_reply);

            /* Allowed Subnets */
            if ((g_nonattackerlist != NULL) &&
                    !IsFuzzyItemIn(g_nonattackerlist,MapAddress(ipaddr))) {

                snprintf(g_output,CF_BUFSIZE*2,
                        "Denying connection from non-authorized IP %s\n",
                        ipaddr);

                CfLog(cferror,g_output,"");
                close(sd_reply);
                continue;
            }

            /* Denied Subnets */
            if (IsFuzzyItemIn(g_attackerlist,MapAddress(ipaddr))) {

                snprintf(g_output,CF_BUFSIZE*2,
                        "Denying connection from non-authorized IP %s\n",
                        ipaddr);

                CfLog(cferror,g_output,"");
                close(sd_reply);
                continue;
            }

            if ((now = time((time_t *)NULL)) == -1) {
                now = 0;
            }

            PurgeOldConnections(&CONNECTIONLIST,now);

            if (!IsFuzzyItemIn(g_multiconnlist,MapAddress(ipaddr))) {
                if (IsItemIn(CONNECTIONLIST,MapAddress(ipaddr))) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Denying repeated connection from %s\n",ipaddr);
                    CfLog(cferror,g_output,"");
                    close(sd_reply);
                    continue;
                }
            }

            if (LOGCONNS) {
                snprintf(g_output,CF_BUFSIZE*2,
                        "Accepting connection from %s\n",ipaddr);
                CfLog(cfinform,g_output,"");
            }

            snprintf(intime,63,"%d",(int)now);
            PrependItem(&CONNECTIONLIST,MapAddress(ipaddr),intime);
            SpawnConnection(sd_reply,ipaddr);
        }
    }
}


/* ----------------------------------------------------------------- */

/*
 * HvB: Bas van der Vlies
 * find_inet_addr - translate numerical or symbolic host name, from the
 *                  postfix sources and adjusted to cfengine style/syntax
*/

/* Is this redundant? Should probably not have this function */

unsigned
find_inet_addr(char *host)
{

    struct in_addr addr;
    struct hostent *hp;

    addr.s_addr = inet_addr(host);
    if ((addr.s_addr == INADDR_NONE) || (addr.s_addr == 0)) {
        if ((hp = gethostbyname(host)) == 0) {
            snprintf(g_output,CF_BUFSIZE,"\nhost not found: %s\n",host);
            FatalError(g_output);
        }
        if (hp->h_addrtype != AF_INET) {
            snprintf(g_output,CF_BUFSIZE,"unexpected address family: %d\n",
                    hp->h_addrtype);
            FatalError(g_output);
        }
        if (hp->h_length != sizeof(addr)) {
            snprintf(g_output,CF_BUFSIZE,"unexpected address length %d\n",
                    hp->h_length);
            FatalError(g_output);
        }

        memcpy((char *) &addr, hp->h_addr, hp->h_length);
    }

    return (addr.s_addr);
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

int
OpenReceiverChannel(void)
{
    int sd,port;
    int yes=1;

    char *ptr = NULL;

    struct linger cflinger;
#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    struct addrinfo query,*response,*ap;
#else
    struct sockaddr_in sin;
#endif

    cflinger.l_onoff = 1;
    cflinger.l_linger = 60;

    port = CfenginePort();

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)

    memset(&query,0,sizeof(struct addrinfo));

    query.ai_flags = AI_PASSIVE;
    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_STREAM;

/*
 * HvB : Bas van der Vlies
*/
    if (g_bindinterface[0] != '\0' ) {
        ptr = g_bindinterface;
    }

    if (getaddrinfo(ptr,"5308",&query,&response) != 0) {
        CfLog(cferror,"DNS/service lookup failure","getaddrinfo");
        return -1;
    }

    sd = -1;

    for (ap = response ; ap != NULL; ap=ap->ai_next) {
        if ((sd = socket(ap->ai_family, ap->ai_socktype, 
                        ap->ai_protocol)) == -1) {
            continue;
        }

        if (setsockopt(sd, SOL_SOCKET,SO_REUSEADDR,
                    (char *)&yes,sizeof (int)) == -1) {
            CfLog(cferror,"Socket options were not accepted","setsockopt");
            exit(1);
        }

        if (setsockopt(sd, SOL_SOCKET,
                SO_LINGER,(char *)&cflinger,sizeof (struct linger)) == -1) {

            CfLog(cferror,"Socket options were not accepted","setsockopt");
            exit(1);
        }

        if (bind(sd,ap->ai_addr,ap->ai_addrlen) == 0) {
            Debug("Bound to address %s on %s=%d\n",sockaddr_ntop(ap->ai_addr),
                    g_classtext[g_vsystemhardclass],g_vsystemhardclass);

            if (g_vsystemhardclass == openbsd) {
                continue;  /* openbsd doesn't map ipv6 addresses */
            } else {
                break;
            }
        }

        CfLog(cferror,"Could not bind server address","bind");
        close(sd);
        sd = -1;
    }

    if (sd < 0) {
        CfLog(cferror,"Couldn't open bind an open socket\n","");
        exit(1);
    }

    if (response != NULL) {
        freeaddrinfo(response);
    }
#else

    memset(&sin,0,sizeof(sin));

    /*
    * HvB : Bas van der Vlies
    */
    if (g_bindinterface[0] != '\0' ) {
        sin.sin_addr.s_addr = find_inet_addr(g_bindinterface);
    } else {
        sin.sin_addr.s_addr = INADDR_ANY;
    }

    /*  Service returns network byte order */
    sin.sin_port = (unsigned short)(port);
    sin.sin_family = AF_INET;

    if ((sd = socket(AF_INET,SOCK_STREAM,0)) == -1) {
        CfLog(cferror,"Couldn't open socket","socket");
        exit (1);
    }

    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR,
                (char *) &yes, sizeof (int)) == -1) {
        CfLog(cferror,"Couldn't set socket options","sockopt");
        exit (1);
    }

    if (setsockopt(sd, SOL_SOCKET, SO_LINGER, (char *) &cflinger,
                sizeof (struct linger)) == -1) {
        CfLog(cferror,"Couldn't set socket options","sockopt");
        exit (1);
    }

    if (bind(sd,(struct sockaddr *)&sin,sizeof(sin)) == -1) {
        CfLog(cferror,"Couldn't bind to socket","bind");
        exit(1);
    }

#endif

    return sd;
}

/*
 * --------------------------------------------------------------------
 * Level 3
 * --------------------------------------------------------------------
 */

void
Syntax(void)
{
    int i;

    printf("cfng daemon: server module\n%s-%s\n%s\n",
            PACKAGE,VERSION,g_copyright);
    printf("\n");
    printf("Options:\n\n");

    for (i=0; CFDOPTIONS[i].name != NULL; i++) {
        printf("--%-20s    (-%c)\n",CFDOPTIONS[i].name,
                (char)CFDOPTIONS[i].val);
    }

    printf("\nReport issues at http://cfng.tigris.org or to "
                "issues@cfng.tigris.org\n");

}

/* ----------------------------------------------------------------- */

/* 
 * Some connections might not terminate properly. These should be
 * cleaned every couple of hours. That should be enough to prevent
 * spamming.
 */

void
PurgeOldConnections(struct Item **list, time_t now)
{
    struct Item *ip;
    int then=0;

    if (list == NULL) {
        return;
    }

    Debug("Purging Old Connections...\n");

    for (ip = *list; ip != NULL; ip=ip->next) {
        sscanf(ip->classes,"%d",&then);

        if (now > then + 7200) {
            DeleteItem(list,ip);
            snprintf(g_output,CF_BUFSIZE*2,
                    "Purging IP address %s from connection list\n",ip->name);
            CfLog(cfverbose,g_output,"");
        }
    }

    Debug("Done purging\n");
}


/* ----------------------------------------------------------------- */

void
SpawnConnection(int sd_reply, char *ipaddr)
{
    struct cfd_connection *conn;

#ifdef HAVE_PTHREAD_H
    pthread_t tid;
#endif

    conn = NewConn(sd_reply);

    strncpy(conn->ipaddr,ipaddr,CF_MAX_IP_LEN-1);

    Debug("New connection...(from %s/%d)\n",conn->ipaddr,sd_reply);

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD

    Debug("Spawning new thread...\n");

    pthread_attr_init(&PTHREADDEFAULTS);
    pthread_attr_setdetachstate(&PTHREADDEFAULTS,PTHREAD_CREATE_DETACHED);

#ifdef HAVE_PTHREAD_ATTR_SETSTACKSIZE
    pthread_attr_setstacksize(&PTHREADDEFAULTS,(size_t)1024*1024);
#endif

    if (pthread_create(&tid,&PTHREADDEFAULTS,
                (void *)HandleConnection,(void *)conn) != 0) {
        CfLog(cferror,"pthread_create failed","create");
        HandleConnection(conn);
    }

    pthread_attr_destroy(&PTHREADDEFAULTS);

#else

    /* Can't fork here without getting a zombie unless we do some
     * complex waiting? */

    Debug("Single threaded...\n");

    HandleConnection(conn);

#endif
}

/* ----------------------------------------------------------------- */

void
CheckFileChanges(int argc,char **argv,int sd)
{
    struct stat newstat;
    char filename[CF_BUFSIZE],*sp;
    int i;

    memset(&newstat,0,sizeof(struct stat));
    memset(filename,0,CF_BUFSIZE);

    if ((sp=getenv(CF_INPUTSVAR)) != NULL) {

        /* Don't prepend to absolute names */
        if (!IsAbsoluteFileName(g_vinputfile)) {
            strncpy(filename,sp,CF_BUFSIZE-1);
            AddSlash(filename);
        }
    } else {

        /* Don't prepend to absolute names */
        if (!IsAbsoluteFileName(g_vinputfile)) {
            strncpy(filename,WORKDIR,CF_BUFSIZE-1);
            AddSlash(filename);
            strncat(filename,"inputs/",CF_BUFSIZE-1-strlen(filename));
        }
    }

    strncat(filename,g_vinputfile,CF_BUFSIZE-1-strlen(filename));

    if (stat(filename,&newstat) == -1) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Input file %s missing or busy..\n",filename);
        CfLog(cferror,g_output,filename);
        sleep(5);
        return;
    }

    Debug("Checking file updates on %s (%x/%x)\n",filename,
            newstat.st_ctime, CFDSTARTTIME);

    if (CFDSTARTTIME < newstat.st_mtime) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Rereading config files %s..\n",filename);
        CfLog(cfinform,g_output,"");

        /* Free & reload -- lock this to avoid access errors during reload */

        DeleteItemList(g_vheap);
        DeleteItemList(g_vnegheap);
        DeleteItemList(g_trustkeylist);
        DeleteAuthList(g_vadmit);
        DeleteAuthList(g_vdeny);
        strcpy(g_vdomain,"undefined.domain");

        g_vadmit = g_vadmittop = NULL;
        g_vdeny  = g_vdenytop  = NULL;
        g_vheap  = g_vnegheap  = NULL;
        g_trustkeylist = NULL;

        AddClassToHeap("any");
        GetNameInfo();
        GetInterfaceInfo();
        GetV6InterfaceInfo();
        ParseInputFile(g_vinputfile);
        CheckVariables();
        SummarizeParsing();
    }
}

/*
 * --------------------------------------------------------------------
 * Level 4
 * --------------------------------------------------------------------
 */

void *
HandleConnection(struct cfd_connection *conn)
{
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
#ifdef HAVE_PTHREAD_SIGMASK
    sigset_t sigmask;

    sigemptyset(&sigmask);
    pthread_sigmask(SIG_BLOCK,&sigmask,NULL);
#endif

    if (conn == NULL) {
        Debug("Null connection\n");
        return NULL;
    }

    if (pthread_mutex_lock(&MUTEX_COUNT) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","pthread_mutex_lock");
        DeleteConn(conn);
        return NULL;
    }

    ACTIVE_THREADS++;

    if (pthread_mutex_unlock(&MUTEX_COUNT) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","unlock");
    }

    if (ACTIVE_THREADS >= CFD_MAXPROCESSES) {
        if (pthread_mutex_lock(&MUTEX_COUNT) != 0) {
            CfLog(cferror,"pthread_mutex_lock failed","pthread_mutex_lock");
            DeleteConn(conn);
            return NULL;
        }

        ACTIVE_THREADS--;

        /* When to say we're hung / apoptosis threshold */
        if (TRIES++ > MAXTRIES) {
            CfLog(cferror, "Server seems to be paralyzed. "
                    "DOS attack? Committing apoptosis...","");
            ExitCleanly(0);
        }

        if (pthread_mutex_unlock(&MUTEX_COUNT) != 0) {
            CfLog(cferror,"pthread_mutex_unlock failed","unlock");
        }

        snprintf(g_output,CF_BUFSIZE,
                "Too many threads (>=%d) -- increase MaxConnections?",
                CFD_MAXPROCESSES);
        CfLog(cferror,g_output,"");

        snprintf(g_output, CF_BUFSIZE,
                "BAD: Server is currently too busy -- increase "
                "MaxConnections or Splaytime?");

        SendTransaction(conn->sd_reply,g_output,0,CF_DONE);
        DeleteConn(conn);
        return NULL;
    }

    /* As long as there is activity, we're not stuck */
    TRIES = 0;

#endif

    while (BusyWithConnection(conn)) { }

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD

    Debug("Terminating thread...\n");

    if (pthread_mutex_lock(&MUTEX_COUNT) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","pthread_mutex_lock");
        DeleteConn(conn);
        return NULL;
    }

    ACTIVE_THREADS--;

    if (pthread_mutex_unlock(&MUTEX_COUNT) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","unlock");
    }

#endif

    DeleteConn(conn);
    return NULL;
}

/* ----------------------------------------------------------------- */

/* 
 * This is the protocol section. Here we must check that the incoming
 * data are sensible and extract the information from the message
 */

int
BusyWithConnection(struct cfd_connection *conn)
{
    time_t tloc, trem = 0;
    char recvbuffer[CF_BUFSIZE+128], sendbuffer[CF_BUFSIZE],check[CF_BUFSIZE];
    char filename[CF_BUFSIZE],buffer[CF_BUFSIZE];
    char args[CF_BUFSIZE],out[CF_BUFSIZE];
    long time_no_see = 0;
    int len=0, drift, plainlen, received;
    struct cfd_get_arg get_args;

    memset(recvbuffer,0,CF_BUFSIZE);
    memset(&get_args,0,sizeof(get_args));

    if ((received = ReceiveTransaction(conn->sd_reply,recvbuffer,NULL)) == -1) {
        return false;
    }

    if (strlen(recvbuffer) == 0) {
        Debug("cfservd terminating NULL transmission!\n");
        return false;
    }

    Debug("Received: [%s] on socket %d\n",recvbuffer,conn->sd_reply);

    switch (GetCommand(recvbuffer)) {
    case cfd_exec:
        memset(args,0,CF_BUFSIZE);
        sscanf(recvbuffer,"EXEC %[^\n]",args);

        if (!conn->id_verified) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (!AllowedUser(conn->username)) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (!conn->rsa_auth) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (!AccessControl(CFRUNCOMMAND,conn,false)) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (!MatchClasses(conn)) {
            Terminate(conn->sd_reply);
            return false;
        }

        DoExec(conn,sendbuffer,args);
        Terminate(conn->sd_reply);
        return false;
    case cfd_cauth:
        conn->id_verified = VerifyConnection(conn,
                (char *)(recvbuffer+strlen("CAUTH ")));

        if (! conn->id_verified) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
        }
        return conn->id_verified; /* are we finished yet ? */
    case cfd_sauth:   /* This is where key agreement takes place */

        if (! conn->id_verified) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (!AuthenticationDialogue(conn,recvbuffer)) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        return true;
    case cfd_get:
        memset(filename,0,CF_BUFSIZE);
        sscanf(recvbuffer,"GET %d %[^\n]",&(get_args.buf_size),filename);

        if (get_args.buf_size < 0 || get_args.buf_size > CF_BUFSIZE) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (! conn->id_verified) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (!AccessControl(filename,conn,false)) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return true;
        }

        memset(sendbuffer,0,CF_BUFSIZE);

        if (get_args.buf_size >= CF_BUFSIZE) {
            get_args.buf_size = 2048;
        }

        get_args.connect = conn;
        get_args.encrypt = false;
        get_args.replybuff = sendbuffer;
        get_args.replyfile = filename;

        CfGetFile(&get_args);

        return true;
    case cfd_sget:
        memset(buffer,0,CF_BUFSIZE);
        sscanf(recvbuffer,"SGET %d %d",&len,&(get_args.buf_size));

        if (received != len+CF_PROTO_OFFSET) {
            Debug("Protocol error\n");
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        plainlen = DecryptString(recvbuffer+CF_PROTO_OFFSET,
                buffer,conn->session_key,len);

        cfscanf(buffer,strlen("GET"),strlen("dummykey"),check,
                sendbuffer,filename);

        if (strcmp(check,"GET") != 0) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return true;
        }

        if (get_args.buf_size < 0 || get_args.buf_size > 8192) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (get_args.buf_size >= CF_BUFSIZE)
        {
            get_args.buf_size = 2048;
        }

        Debug("Confirm decryption, and thus validity of caller\n");
        Debug("SGET %s with blocksize %d\n",filename,get_args.buf_size);

        if (! conn->id_verified) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (!AccessControl(filename,conn,true)) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        memset(sendbuffer,0,CF_BUFSIZE);

        get_args.connect = conn;
        get_args.encrypt = true;
        get_args.replybuff = sendbuffer;
        get_args.replyfile = filename;

        CfGetFile(&get_args);
        return true;
    case cfd_opendir:
        memset(filename,0,CF_BUFSIZE);
        sscanf(recvbuffer,"OPENDIR %[^\n]",filename);

        if (! conn->id_verified) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        /* opendir don't care about privacy */
        if (!AccessControl(filename,conn,true)) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        CfOpenDirectory(conn,sendbuffer,filename);
        return true;
    case cfd_ssynch:
        memset(buffer,0,CF_BUFSIZE);
        sscanf(recvbuffer,"SSYNCH %d",&len);

        if (received != len+CF_PROTO_OFFSET) {
            Debug("Protocol error: %d\n",len);
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        if (conn->session_key == NULL) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        bcopy(recvbuffer+CF_PROTO_OFFSET,out,len);

        plainlen = DecryptString(out,recvbuffer,conn->session_key,len);

        if (strncmp(recvbuffer,"SYNCH",5) !=0) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return true;
        }
        /* roll through, no break */
    case cfd_synch:
        if (! conn->id_verified) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }

        memset(filename,0,CF_BUFSIZE);
        sscanf(recvbuffer,"SYNCH %ld STAT %[^\n]",&time_no_see,filename);

        trem = (time_t) time_no_see;

        if (time_no_see == 0 || filename[0] == '\0') {
            break;
        }

        if ((tloc = time((time_t *)NULL)) == -1) {
            sprintf(conn->output,"Couldn't read system clock\n");
            CfLog(cfinform,conn->output,"time");
            SendTransaction(conn->sd_reply,
                    "BAD: clocks out of synch",0,CF_DONE);
            return true;
        }

        drift = (int)(tloc-trem);

        if (!AccessControl(filename,conn,true)) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return true;
        }

        if (DENYBADCLOCKS && (drift*drift > CLOCK_DRIFT*CLOCK_DRIFT)) {
            snprintf(conn->output,CF_BUFSIZE*2,
                    "BAD: Clocks are too far unsynchronized %ld/%ld\n",
                    (long)tloc,(long)trem);
            CfLog(cfinform,conn->output,"");
            SendTransaction(conn->sd_reply,conn->output,0,CF_DONE);
            return true;
        } else {
            Debug("Clocks were off by %ld\n",(long)tloc-(long)trem);
            StatFile(conn,sendbuffer,filename);
        }
        return true;
    case cfd_smd5:
        memset(buffer,0,CF_BUFSIZE);
        sscanf(recvbuffer,"SMD5 %d",&len);

        if (received != len+CF_PROTO_OFFSET) {
            Debug("Decryption error: %d\n",len);
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return true;
        }

        bcopy(recvbuffer+CF_PROTO_OFFSET,out,len);
        plainlen = DecryptString(out,recvbuffer,conn->session_key,len);

        if (strncmp(recvbuffer,"MD5",3) !=0) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return false;
        }
        /* roll through, no break */
    case cfd_md5:
        if (! conn->id_verified) {
            RefuseAccess(conn,sendbuffer,0,recvbuffer);
            return true;
        }

        memset(filename,0,CF_BUFSIZE);
        memset(args,0,CF_BUFSIZE);

        CompareLocalChecksum(conn,sendbuffer,recvbuffer);
        return true;

    }
    sprintf (sendbuffer,"BAD: Request denied\n");
    SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
    CfLog(cfinform,"Closing connection\n","");
    return false;
}

/* ----------------------------------------------------------------- */

void *
ExitCleanly(int signum)
{
#ifdef HAVE_PTHREAD_H
    /* Do we care about this?
    for (i = 0; i < MAXTHREAD ; i++)
    {
    if (THREADS[i] != NULL)
        {
        pthread_join(THREADS[i],(void **)NULL);
        }
    }
    */
#endif

    HandleSignal(signum);
    return NULL;
}

/*
 * --------------------------------------------------------------------
 * Level 5
 * --------------------------------------------------------------------
 */

int
MatchClasses(struct cfd_connection *conn)
{
    char recvbuffer[CF_BUFSIZE];
    struct Item *classlist = NULL, *ip;
    int count = 0;

    Debug("Match classes\n");

    /* arbitrary check to avoid infinite loop, DoS attack*/
    while (true && (count < 10)) {
        count++;

        if (ReceiveTransaction(conn->sd_reply,recvbuffer,NULL) == -1) {
            if (errno == EINTR) {
                continue;
            }
        }

        Debug("Got class buffer %s\n",recvbuffer);

        if (strncmp(recvbuffer,CFD_TERMINATOR,strlen(CFD_TERMINATOR)) == 0) {
            if (count == 1) {
                Debug("No classes were sent, assuming no restrictions...\n");
                return true;
            }
            break;
        }

        classlist = SplitStringAsItemList(recvbuffer,' ');

        for (ip = classlist; ip != NULL; ip=ip->next) {
            if (IsDefinedClass(ip->name)) {
                Debug("Class %s matched, accepting...\n",ip->name);
                DeleteItemList(classlist);
                return true;
            }

            if (strncmp(ip->name,CFD_TERMINATOR,strlen(CFD_TERMINATOR)) == 0) {
                Debug("No classes matched, rejecting....\n");
                ReplyNothing(conn);
                DeleteItemList(classlist);
                return false;
            }
        }
    }

    ReplyNothing(conn);
    Debug("No classes matched, rejecting....\n");
    DeleteItemList(classlist);
    return false;
}


/* ----------------------------------------------------------------- */

void
DoExec(struct cfd_connection *conn, char *sendbuffer, char *args)
{
    char ebuff[CF_EXPANDSIZE], line[CF_BUFSIZE], *sp;
    int print = false,i;
    FILE *pp;

    if ((g_cfstarttime = time((time_t *)NULL)) == -1) {
        CfLog(cferror,"Couldn't read system clock\n","time");
    }

    if (GetMacroValue(g_contextid,"cfrunCommand") == NULL) {
        Verbose("cfservd exec request: no cfrunCommand defined\n");
        sprintf(sendbuffer,"Exec request: no cfrunCommand defined\n");
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        return;
    }

    /* Blank out -K -f */
    for (sp = args; *sp != '\0'; sp++) {
        if ((strncmp(sp,"-K",2) == 0) || (strncmp(sp,"-f",2) == 0)) {
            *sp = ' ';
            *(sp+1) = ' ';
        } else if (strncmp(sp,"--no-lock",9) == 0) {
            for (i = 0; i < 9; i++) {
                *(sp+i) = ' ';
            }
        } else if (strncmp(sp,"--file",7) == 0) {
            for (i = 0; i < 7; i++) {
                *(sp+i) = ' ';
            }
        }
    }

    ExpandVarstring("$(cfrunCommand) --no-splay --inform",ebuff,"");

    if (strlen(ebuff)+strlen(args)+6 > CF_BUFSIZE) {
        snprintf(sendbuffer,CF_BUFSIZE,"Command line too long with args: %s\n",
                ebuff);
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        ReleaseCurrentLock();
        return;
    } else {
        if ((args != NULL) & (strlen(args) > 0)) {
            strcat(ebuff," ");
            strcat(ebuff,args);

            snprintf(sendbuffer,CF_BUFSIZE,"cfservd Executing %s\n",ebuff);
            SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        }
    }

    snprintf(conn->output,CF_BUFSIZE,"Executing command %s\n",ebuff);
    CfLog(cfinform,conn->output,"");

    if ((pp = cfpopen(ebuff,"r")) == NULL) {
        snprintf(conn->output,CF_BUFSIZE,"Couldn't open pipe to command %s\n",
                ebuff);
        CfLog(cferror,conn->output,"pipe");
        snprintf(sendbuffer,CF_BUFSIZE,"Unable to run %s\n",ebuff);
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        ReleaseCurrentLock();
        return;
    }

    while (!feof(pp)) {
        if (ferror(pp)) {
            fflush(pp);
            break;
        }

        ReadLine(line,CF_BUFSIZE,pp);

        if (ferror(pp)) {
            fflush(pp);
            break;
        }

        print = false;

        for (sp = line; *sp != '\0'; sp++) {
            if (! isspace((int)*sp)) {
                print = true;
                break;
            }
        }

        if (print) {
            snprintf(sendbuffer,CF_BUFSIZE,"%s\n",line);
            SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        }
    }

    cfpclose(pp);
/* ReleaseCurrentLock(); */
}

/* ----------------------------------------------------------------- */

int
GetCommand (char *str)
{
    int i;
    char op[CF_BUFSIZE];

    sscanf(str,"%4095s",op);

    for (i = 0; g_protocol[i] != NULL; i++) {
        if (strcmp(op,g_protocol[i])==0) {
            return i;
        }
    }

    return -1;
}

/* ----------------------------------------------------------------- */

/* Try reverse DNS lookup
    and RFC931 username lookup to check the authenticity. */

int
VerifyConnection(struct cfd_connection *conn, char buf[CF_BUFSIZE])
{
    char ipstring[CF_MAXVARSIZE], fqname[CF_MAXVARSIZE], username[CF_MAXVARSIZE];
    char dns_assert[CF_MAXVARSIZE],ip_assert[CF_MAXVARSIZE];
    int matched = false;
    struct passwd *pw;
#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    struct addrinfo query, *response=NULL, *ap;
    int err;
#else
    struct sockaddr_in raddr;
    int i,j,len = sizeof(struct sockaddr_in);
    struct hostent *hp = NULL;
    struct Item *ip_aliases = NULL, *ip_addresses = NULL;
#endif

    Debug("Connecting host identifies itself as %s\n",buf);

    memset(ipstring,0,CF_MAXVARSIZE);
    memset(fqname,0,CF_MAXVARSIZE);
    memset(username,0,CF_MAXVARSIZE);

    sscanf(buf,"%255s %255s %255s",ipstring,fqname,username);

    Debug("(ipstring=[%s],fqname=[%s],username=[%s],socket=[%s])\n",
            ipstring,fqname,username,conn->ipaddr);

    strncpy(dns_assert,ToLowerStr(fqname),CF_MAXVARSIZE-1);
    strncpy(ip_assert,ipstring,CF_MAXVARSIZE-1);

    /* 
     * It only makes sense to check DNS by reverse lookup if the key had
     * to be accepted on trust. Once we have a positive key ID, the IP
     * address is irrelevant fr authentication...  We can save a lot of
     * time by not looking this up ...
     */

    if ((conn->trust == false) || IsFuzzyItemIn(g_skipverify,
                MapAddress(conn->ipaddr))) {
        snprintf(conn->output,CF_BUFSIZE*2,
                "Allowing %s to connect without (re)checking ID\n",ip_assert);
        CfLog(cfverbose,conn->output,"");
        Verbose("Non-verified Host ID is %s (Using skipverify)\n",dns_assert);
        strncpy(conn->hostname,dns_assert,CF_MAXVARSIZE);
        Verbose("Non-verified User ID seems to be %s (Using skipverify)\n",
                username);
        strncpy(conn->username,username,CF_MAXVARSIZE);

        /* Keep this inside mutex */
        if ((pw=getpwnam(username)) == NULL) {
            printf("username was");
            conn->uid = -2;
        } else {
            conn->uid = pw->pw_uid;
        }

        LastSeen(dns_assert,cf_accept);
        return true;
    }

    if (strcmp(ip_assert,MapAddress(conn->ipaddr)) != 0) {
        Verbose("IP address mismatch between client's assertion (%s) "
                "and socket (%s) - untrustworthy connection\n",
                ip_assert,conn->ipaddr);
        return false;
    }

    Verbose("Socket caller address appears honest (%s matches %s)\n",
            ip_assert,MapAddress(conn->ipaddr));

    snprintf(conn->output,CF_BUFSIZE,"Socket originates from %s=%s\n",
            ip_assert,dns_assert);
    CfLog(cfverbose,conn->output,"");

    Debug("Attempting to verify honesty by looking up hostname (%s)\n",
            dns_assert);

    /* 
     * Do a reverse DNS lookup, like tcp wrappers to see if hostname
     * matches IP
     */

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)

    Debug("Using v6 compatible lookup...\n");

    memset(&query,0,sizeof(struct addrinfo));
    query.ai_family = AF_UNSPEC;
    query.ai_socktype = SOCK_STREAM;
    query.ai_flags = AI_PASSIVE;

    if ((err=getaddrinfo(dns_assert,NULL,&query,&response)) != 0) {
        snprintf(conn->output,CF_BUFSIZE,"Unable to lookup %s (%s)",
                dns_assert,gai_strerror(err));
        CfLog(cferror,conn->output,"");
    }

    for (ap = response; ap != NULL; ap = ap->ai_next) {
        Debug("CMP: %s %s\n",MapAddress(conn->ipaddr),
                sockaddr_ntop(ap->ai_addr));

        if (strcmp(MapAddress(conn->ipaddr),
                    sockaddr_ntop(ap->ai_addr)) == 0) {
            Debug("Found match\n");
            matched = true;
        }
    }

    if (response != NULL) {
        freeaddrinfo(response);
    }

#else

    Debug("IPV4 hostnname lookup on %s\n",dns_assert);

# ifdef HAVE_PTHREAD_H
    if (pthread_mutex_lock(&MUTEX_HOSTNAME) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","unlock");
        exit(1);
    }
# endif

    if ((hp = gethostbyname(dns_assert)) == NULL) {
        Verbose("cfservd Couldn't look up name %s\n",fqname);
        Verbose("     Make sure that fully qualified names can be looked "
                "up at your site!\n");
        Verbose("     i.e. www.gnu.org, not just www. If you use NIS or "
                "/etc/hosts\n");
        Verbose("     make sure that the full form is registered too "
                "as an alias!\n");

        snprintf(g_output,CF_BUFSIZE,"DNS lookup of %s failed",dns_assert);
        CfLog(cflogonly,g_output,"gethostbyname");
        matched = false;
    } else {
        matched = true;

        Debug("Looking for the peername of our socket...\n");

        if (getpeername(conn->sd_reply,(struct sockaddr *)&raddr,&len) == -1) {
            CfLog(cferror,"Couldn't get socket address\n","getpeername");
            matched = false;
        }

        Verbose("Looking through hostnames on socket with IPv4 %s\n",
                sockaddr_ntop((struct sockaddr *)&raddr));

        for (i = 0; hp->h_addr_list[i]; i++) {
            Verbose("Reverse lookup address found: %d\n",i);
            if (memcmp(hp->h_addr_list[i],(char *)&(raddr.sin_addr),
                        sizeof(raddr.sin_addr)) == 0) {
                Verbose("Canonical name matched host's assertion "
                        "- id confirmed as %s\n",dns_assert);
                break;
            }
        }

        if (hp->h_addr_list[0] != NULL) {
            Verbose("Checking address number %d for non-canonical "
                    "names (aliases)\n",i);
            for (j = 0; hp->h_aliases[j] != NULL; j++) {
                Verbose("Comparing [%s][%s]\n",hp->h_aliases[j],ip_assert);
                if (strcmp(hp->h_aliases[j],ip_assert) == 0) {
                    Verbose("Non-canonical name (alias) matched "
                            "host's assertion - id confirmed as %s\n",
                            dns_assert);
                    break;
                }
            }

            if ((hp->h_addr_list[i] != NULL) && (hp->h_aliases[j] != NULL)) {
                snprintf(conn->output, CF_BUFSIZE, 
                        "Reverse hostname lookup failed, "
                        "host claiming to be %s was %s\n",
                        buf, sockaddr_ntop((struct sockaddr *)&raddr));
                CfLog(cflogonly,conn->output,"");
                matched = false;
            } else {
                Verbose("Reverse lookup succeeded\n");
            }
        } else {
            snprintf(conn->output, CF_BUFSIZE, 
                    "No name was registered in DNS for %s - "
                    "reverse lookup failed\n", dns_assert);
            CfLog(cflogonly,conn->output,"");
            matched = false;
        }
    }


    /* Keep this inside mutex */
    if ((pw=getpwnam(username)) == NULL) {

        printf("username was");
        conn->uid = -2;
    } else {
        conn->uid = pw->pw_uid;
    }


# ifdef HAVE_PTHREAD_H
    if (pthread_mutex_unlock(&MUTEX_HOSTNAME) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","unlock");
        exit(1);
    }
# endif

#endif

    if (!matched) {
        snprintf(conn->output,CF_BUFSIZE,
                "Failed on DNS reverse lookup of %s\n",dns_assert);
        CfLog(cflogonly,conn->output,"gethostbyname");
        snprintf(conn->output,CF_BUFSIZE,"Client sent: %s",buf);
        CfLog(cflogonly,conn->output,"");
        return false;
    }

    Verbose("Host ID is %s\n",dns_assert);
    strncpy(conn->hostname,dns_assert,CF_MAXVARSIZE-1);
    LastSeen(dns_assert,cf_accept);

    Verbose("User ID seems to be %s\n",username);
    strncpy(conn->username,username,CF_MAXVARSIZE-1);

    return true;
}


/* ----------------------------------------------------------------- */

int
AllowedUser(char *user)
{
    if (IsItemIn(g_allowuserlist,user)) {
        Debug("User %s granted connection privileges\n",user);
        return true;
    }

    Debug("User %s is not allowed on this server\n",user);
    return false;
}

/* ----------------------------------------------------------------- */

int
AccessControl(char *filename, struct cfd_connection *conn, int encrypt)
{
    struct Auth *ap;
    int access = false;
    char realname[CF_BUFSIZE];
    struct stat statbuf;

    Debug("AccessControl(%s)\n",filename);
    memset(realname,0,CF_BUFSIZE);

    if (lstat(filename,&statbuf) == -1) {
        snprintf(conn->output,CF_BUFSIZE*2,
                "Couldn't stat filename %s from host %s\n",
                filename,conn->hostname);
        CfLog(cfverbose,conn->output,"lstat");
        CfLog(cflogonly,conn->output,"lstat");
        return false;
    }

    if (S_ISLNK(statbuf.st_mode)) {
        strncpy(realname,filename,CF_BUFSIZE);
    } else {
#ifdef HAVE_REALPATH
        if (realpath(filename,realname) == NULL) {
            snprintf(conn->output,CF_BUFSIZE*2,
                    "Couldn't resolve filename %s from host %s\n",
                    filename,conn->hostname);
            CfLog(cfverbose,conn->output,"lstat");
            CfLog(cflogonly,conn->output,"lstat");
            return false;
        }
#else
        CompressPath(realname,filename); /* in links.c */
#endif
    }

    Debug("AccessControl(%s,%s) encrypt request=%d\n",
            realname,conn->hostname,encrypt);

    if (g_vadmit == NULL) {
        Verbose("cfservd access list is empty, no files are visible\n");
        return false;
    }

    conn->maproot = false;

    for (ap = g_vadmit; ap != NULL; ap=ap->next) {
        int res = false;
        Debug("Examining rule in access list (%s,%s)?\n",realname,ap->path);

        if ((strlen(realname) > strlen(ap->path))  &&
                strncmp(ap->path,realname,strlen(ap->path)) == 0 &&
                realname[strlen(ap->path)] == '/') {

            /* Substring means must be a / to link, else just a
             * substring og filename */
            res = true;
        }

        if (strcmp(ap->path,realname) == 0) {

            /* Exact match means single file to admit */
            res = true;
        }

        if (res) {
            Debug("Found a matching rule in access list (%s,%s)\n",
                    realname,ap->path);

            if (stat(ap->path,&statbuf) == -1) {
                snprintf(g_output, CF_BUFSIZE, 
                        "Warning cannot stat file object %s in admit/"
                        "grant, or access list refers to dangling link\n",
                        ap->path);
                CfLog(cflogonly,g_output,"");
                continue;
            }

            if (!encrypt && (ap->encrypt == true)) {
                snprintf(conn->output,CF_BUFSIZE,
                        "File %s requires encrypt connection...will "
                        "not serve\n",ap->path);
                CfLog(cferror,conn->output,"");
                access = false;
            } else {
                    Debug("Checking whether to map root privileges..\n");

                if (IsWildItemIn(ap->maproot,conn->hostname) ||
                        IsWildItemIn(ap->maproot,
                            MapAddress(conn->ipaddr)) ||
                        IsFuzzyItemIn(ap->maproot,
                            MapAddress(conn->ipaddr))) {

                    conn->maproot = true;
                    Debug("Mapping root privileges\n");
                } else {
                    Debug("No root privileges granted\n");
                }

                if (IsWildItemIn(ap->accesslist,conn->hostname) ||
                        IsWildItemIn(ap->accesslist,
                            MapAddress(conn->ipaddr)) ||
                        IsFuzzyItemIn(ap->accesslist,
                            MapAddress(conn->ipaddr))) {

                    access = true;
                    Debug("Access privileges - match found\n");
                }
            }
            break;
        }
    }

    for (ap = g_vdeny; ap != NULL; ap=ap->next) {
        if (strncmp(ap->path,realname,strlen(ap->path)) == 0) {
            if (IsWildItemIn(ap->accesslist,conn->hostname) ||
                    IsWildItemIn(ap->accesslist,
                        MapAddress(conn->ipaddr)) ||
                    IsFuzzyItemIn(ap->accesslist,
                        MapAddress(conn->ipaddr))) {

                access = false;
                snprintf(conn->output,CF_BUFSIZE*2,
                        "Host %s explicitly denied access to %s\n",
                        conn->hostname,realname);
                CfLog(cfverbose,conn->output,"");
                break;
            }
        }
    }

    if (access) {
        snprintf(conn->output,CF_BUFSIZE*2,
                "Host %s granted access to %s\n",conn->hostname,realname);
        CfLog(cfverbose,conn->output,"");
    } else {
        snprintf(conn->output,CF_BUFSIZE*2,
                "Host %s denied access to %s\n",conn->hostname,realname);
        CfLog(cfverbose,conn->output,"");
        CfLog(cflogonly,conn->output,"");
    }

    if (!conn->rsa_auth) {
        CfLog(cfverbose,"Cannot map root access without RSA authentication","");
        conn->maproot = false; /* only public files accessible */
        /* return false; */
    }

    return access;
}

/* ----------------------------------------------------------------- */

int
AuthenticationDialogue(struct cfd_connection *conn, char *recvbuffer)
{
    char in[CF_BUFSIZE],*out, *decrypted_nonce;
    BIGNUM *counter_challenge = NULL;
    unsigned char digest[EVP_MAX_MD_SIZE+1];
    unsigned int crypt_len, nonce_len = 0,len = 0, encrypted_len, keylen;
    char sauth[10], iscrypt ='n';
    unsigned long err;
    RSA *newkey;

    if (PRIVKEY == NULL || PUBKEY == NULL) {
        CfLog(cferror,"No public/private key pair exists,"
                " create one with cfkey\n","");
        return false;
    }

    /* proposition C1 */
    /* Opening string is a challenge from the client (some agent) */

    sscanf(recvbuffer,"%s %c %d %d",sauth,&iscrypt,&crypt_len,&nonce_len);

    if ((strcmp(sauth,"SAUTH") != 0) || (nonce_len == 0) || (crypt_len == 0)) {
        CfLog(cfinform,"Protocol error in RSA authentation from IP %s\n",
                conn->hostname);
        return false;
    }

    Debug("Challenge encryption = %c, nonce = %d, buf = %d\n",
            iscrypt,nonce_len,crypt_len);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","lock");
    }
#endif

    if ((decrypted_nonce = malloc(crypt_len)) == NULL) {
        FatalError("memory failure");
    }

    if (iscrypt == 'y') {
        if (RSA_private_decrypt(crypt_len,recvbuffer+CF_RSA_PROTO_OFFSET,
                    decrypted_nonce,PRIVKEY,RSA_PKCS1_PADDING) <= 0) {
            err = ERR_get_error();
            snprintf(conn->output,CF_BUFSIZE,"Private decrypt failed = %s\n",
                    ERR_reason_error_string(err));
            CfLog(cferror,conn->output,"");
            free(decrypted_nonce);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
            if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
                CfLog(cferror,"pthread_mutex_unlock failed","lock");
            }
#endif
            return false;
        }
    } else {
        bcopy(recvbuffer+CF_RSA_PROTO_OFFSET,decrypted_nonce,nonce_len);
    }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","lock");
    }
#endif

    /* Client's ID is now established by key or trusted, reply with md5 */

    ChecksumString(decrypted_nonce,nonce_len,digest,'m');
    free(decrypted_nonce);

    /* Get the public key from the client */


#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","lock");
    }
#endif

    newkey = RSA_new();

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","lock");
    }
#endif



    /* proposition C2 */
    if ((len = ReceiveTransaction(conn->sd_reply,recvbuffer,NULL)) == -1) {
        CfLog(cfinform,"Protocol error 1 in RSA authentation from IP %s\n",
                conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if (len == 0) {
        CfLog(cfinform,"Protocol error 2 in RSA authentation from IP %s\n",
                conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if ((newkey->n = BN_mpi2bn(recvbuffer,len,NULL)) == NULL) {
        err = ERR_get_error();
        snprintf(conn->output,CF_BUFSIZE,"Private decrypt failed = %s\n",
                ERR_reason_error_string(err));
        CfLog(cferror,conn->output,"");
        RSA_free(newkey);
        return false;
    }

    /* proposition C3 */

    if ((len=ReceiveTransaction(conn->sd_reply,recvbuffer,NULL)) == -1) {
        CfLog(cfinform,"Protocol error 3 in RSA authentation from IP %s\n",
                conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if (len == 0) {
        CfLog(cfinform,"Protocol error 4 in RSA authentation from IP %s\n",
                conn->hostname);
        RSA_free(newkey);
        return false;
    }

    if ((newkey->e = BN_mpi2bn(recvbuffer,len,NULL)) == NULL) {
        err = ERR_get_error();
        snprintf(conn->output,CF_BUFSIZE,"Private decrypt failed = %s\n",
                ERR_reason_error_string(err));
        CfLog(cferror,conn->output,"");
        RSA_free(newkey);
        return false;
    }

    if (g_debug||g_d2) {
        RSA_print_fp(stdout,newkey,0);
    }

    /* conceals proposition S1 */
    if (!CheckStoreKey(conn,newkey)) {
        if (!conn->trust) {
            RSA_free(newkey);
            return false;
        }
    }

    /* Reply with md5 of original challenge */

    /* proposition S2 */
    SendTransaction(conn->sd_reply,digest,16,CF_DONE);

    /* Send counter challenge to be sure this is a live session */

    counter_challenge = BN_new();
    BN_rand(counter_challenge,256,0,0);
    nonce_len = BN_bn2mpi(counter_challenge,in);
    ChecksumString(in,nonce_len,digest,'m');

    /* encrypted_len = BN_num_bytes(newkey->n);  encryption buffer is
     * always the same size as n */

    /* encryption buffer is always the same size as n */
    encrypted_len = RSA_size(newkey);


#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","lock");
    }
#endif

    if ((out = malloc(encrypted_len+1)) == NULL) {
        FatalError("memory failure");
    }

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","lock");
    }
#endif


    if (RSA_public_encrypt(nonce_len,in,out,newkey,RSA_PKCS1_PADDING) <= 0) {
        err = ERR_get_error();
        snprintf(conn->output,CF_BUFSIZE,"Public encryption failed = %s\n",
                ERR_reason_error_string(err));
        CfLog(cferror,conn->output,"");
        RSA_free(newkey);
        free(out);
        return false;
    }

    /* proposition S3 */
    SendTransaction(conn->sd_reply,out,encrypted_len,CF_DONE);

    /* if the client doesn't have our public key, send it */

    if (iscrypt != 'y') {
        /* proposition S4  - conditional */
        memset(in,0,CF_BUFSIZE);
        len = BN_bn2mpi(PUBKEY->n,in);
        SendTransaction(conn->sd_reply,in,len,CF_DONE);

        /* proposition S5  - conditional */
        memset(in,0,CF_BUFSIZE);
        len = BN_bn2mpi(PUBKEY->e,in);
        SendTransaction(conn->sd_reply,in,len,CF_DONE);
    }

    /* Receive reply to counter_challenge */

    /* proposition C4 */
    memset(in,0,CF_BUFSIZE);
    ReceiveTransaction(conn->sd_reply,in,NULL);

    /* replay / piggy in the middle attack ? */
    if (!ChecksumsMatch(digest,in,'m')) {
        BN_free(counter_challenge);
        free(out);
        RSA_free(newkey);
        snprintf(conn->output,CF_BUFSIZE,
                "Challenge response from client %s was incorrect - ID false?",
                conn->ipaddr);
        CfLog(cfinform,conn->output,"");
        return false;
    } else {
        if (!conn->trust) {
            snprintf(conn->output,CF_BUFSIZE,
                    "Strong authentication of client %s/%s achieved",
                    conn->hostname,conn->ipaddr);
        } else {
            snprintf(conn->output,CF_BUFSIZE,
                    "Weak authentication of trusted client %s/%s "
                    "(key accepted on trust).\n",conn->hostname,conn->ipaddr);
        }
        CfLog(cfverbose,conn->output,"");
    }

    /* Receive random session key, blowfish style ... */

    /* proposition C5 */
    memset(in,0,CF_BUFSIZE);
    keylen = ReceiveTransaction(conn->sd_reply,in,NULL);


#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","lock");
    }
#endif

    conn->session_key = malloc(keylen);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","lock");
    }
#endif

    bcopy(in,conn->session_key,keylen);
    Debug("Got a session key...\n");

    BN_free(counter_challenge);
    free(out);
    RSA_free(newkey);
    conn->rsa_auth = true;
    return true;
}

/* ----------------------------------------------------------------- */

/* 
 * Because we do not know the size or structure of remote datatypes, the
 * simplest way to transfer the data is to convert them into plain text
 * and interpret them on the other side. 
 */

int
StatFile(struct cfd_connection *conn, char *sendbuffer, char *filename)
{
    struct cfstat cfst;
    struct stat statbuf;
    char linkbuf[CF_BUFSIZE];

    Debug("StatFile(%s)\n",filename);

    memset(&cfst,0,sizeof(struct cfstat));

    if (strlen(ReadLastNode(filename)) > CF_MAXLINKSIZE) {
        snprintf(sendbuffer,CF_BUFSIZE*2,
                "BAD: Filename suspiciously long [%s]\n",
                filename);
        CfLog(cferror,sendbuffer,"");
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        return -1;
    }

    if (lstat(filename,&statbuf) == -1) {
        snprintf(sendbuffer,CF_BUFSIZE,"BAD: unable to stat file %s",filename);
        CfLog(cfverbose,sendbuffer,"lstat");
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        return -1;
    }

    cfst.cf_readlink = NULL;
    cfst.cf_lmode = 0;
    cfst.cf_nlink = CF_NOSIZE;

    memset(linkbuf,0,CF_BUFSIZE);

    if (S_ISLNK(statbuf.st_mode)) {

        /* pointless - overwritten */
        cfst.cf_type = cf_link;
        cfst.cf_lmode = statbuf.st_mode & 07777;
        cfst.cf_nlink = statbuf.st_nlink;

        if (readlink(filename,linkbuf,CF_BUFSIZE-1) == -1) {
            sprintf(sendbuffer,"BAD: unable to read link\n");
            CfLog(cferror,sendbuffer,"readlink");
            SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
            return -1;
        }

        Debug("readlink: %s\n",linkbuf);

        cfst.cf_readlink = linkbuf;
    }

    if (stat(filename,&statbuf) == -1)
    {
    snprintf(sendbuffer,CF_BUFSIZE,"BAD: unable to stat file %s\n",filename);
    CfLog(cfverbose,conn->output,"stat");
    SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
    return -1;
    }

    if (S_ISDIR(statbuf.st_mode)) {
        cfst.cf_type = cf_dir;
    }

    if (S_ISREG(statbuf.st_mode)) {
        cfst.cf_type = cf_reg;
    }

    if (S_ISSOCK(statbuf.st_mode)) {
        cfst.cf_type = cf_sock;
    }

    if (S_ISCHR(statbuf.st_mode)) {
        cfst.cf_type = cf_char;
    }

    if (S_ISBLK(statbuf.st_mode)) {
        cfst.cf_type = cf_block;
    }

    if (S_ISFIFO(statbuf.st_mode)) {
        cfst.cf_type = cf_fifo;
    }

    cfst.cf_mode     = statbuf.st_mode  & 07777;
    cfst.cf_uid      = statbuf.st_uid   & 0xFFFFFFFF;
    cfst.cf_gid      = statbuf.st_gid   & 0xFFFFFFFF;
    cfst.cf_size     = statbuf.st_size;
    cfst.cf_atime    = statbuf.st_atime;
    cfst.cf_mtime    = statbuf.st_mtime;
    cfst.cf_ctime    = statbuf.st_ctime;
    cfst.cf_ino      = statbuf.st_ino;
    cfst.cf_dev      = statbuf.st_dev;
    cfst.cf_readlink = linkbuf;

    if (cfst.cf_nlink == CF_NOSIZE) {
        cfst.cf_nlink = statbuf.st_nlink;
    }

#ifndef IRIX
    if (statbuf.st_size > statbuf.st_blocks * DEV_BSIZE)
#else
# ifdef HAVE_ST_BLOCKS
    if (statbuf.st_size > statbuf.st_blocks * DEV_BSIZE)
# else
    if (statbuf.st_size > ST_NBLOCKS(statbuf) * DEV_BSIZE)
# endif
#endif
    {
        cfst.cf_makeholes = 1;   /* must have a hole to get checksum right */
    } else {
        cfst.cf_makeholes = 0;
    }

    memset(sendbuffer,0,CF_BUFSIZE);

    /* send as plain text */

    Debug("OK: type=%d\n mode=%o\n lmode=%o\n uid=%d\n gid=%d\n "
            "size=%ld\n atime=%d\n mtime=%d\n",
            cfst.cf_type, cfst.cf_mode, cfst.cf_lmode, cfst.cf_uid,
            cfst.cf_gid, (long)cfst.cf_size, cfst.cf_atime, cfst.cf_mtime);

    snprintf(sendbuffer, CF_BUFSIZE, 
            "OK: %d %d %d %d %d %ld %d %d %d %d %d %d %d",
            cfst.cf_type, cfst.cf_mode, cfst.cf_lmode, cfst.cf_uid,
            cfst.cf_gid, (long)cfst.cf_size, cfst.cf_atime, cfst.cf_mtime,
            cfst.cf_ctime, cfst.cf_makeholes, cfst.cf_ino, cfst.cf_nlink,
            cfst.cf_dev);

    SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
    memset(sendbuffer,0,CF_BUFSIZE);

    if (cfst.cf_readlink != NULL) {
        strcpy(sendbuffer,"OK:");
        strcat(sendbuffer,cfst.cf_readlink);
    } else {
        sprintf(sendbuffer,"OK:");
    }

    SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
    return 0;
}

/* ----------------------------------------------------------------- */

void
CfGetFile(struct cfd_get_arg *args)
{
    int sd,fd,n_read,total=0,cipherlen,sendlen=0,count = 0;
    char sendbuffer[CF_BUFSIZE+1],out[CF_BUFSIZE],*filename;
    struct stat statbuf;
    uid_t uid;
    unsigned char iv[] = {1,2,3,4,5,6,7,8}, *key;
    EVP_CIPHER_CTX ctx;

    sd         = (args->connect)->sd_reply;
    filename   = args->replyfile;
    key        = (args->connect)->session_key;
    uid        = (args->connect)->uid;

    stat(filename,&statbuf);
    Debug("CfGetFile(%s on sd=%d), size=%d\n",filename,sd,statbuf.st_size);

    /* Now check to see if we have remote permission */

    /* should remote root be local root */
    if (uid != 0 && !args->connect->maproot) {
        if (statbuf.st_uid == uid) {
            Debug("Caller %s is the owner of the file\n",
                    (args->connect)->username);
        } else {
            /* We are not the owner of the file and we don't care about
             * groups */
            if (statbuf.st_mode & S_IROTH) {
                Debug("Caller %s not owner of the file "
                        "but permission granted\n",(args->connect)->username);
            } else {
                Debug("Caller %s is not the owner of the file\n",
                        (args->connect)->username);
                RefuseAccess(args->connect,sendbuffer,args->buf_size,"");
                return;
            }
        }
    }

    if (args->buf_size < 512) {
        snprintf(args->connect->output,CF_BUFSIZE,
                "blocksize for %s was only %d\n",filename,args->buf_size);
        CfLog(cferror,args->connect->output,"");
    }

    if (args->encrypt) {
        EVP_CIPHER_CTX_init(&ctx);
        EVP_EncryptInit(&ctx,EVP_bf_cbc(),key,iv);
    }

    if ((fd = SafeOpen(filename)) == -1) {
        snprintf(sendbuffer,CF_BUFSIZE,"Open error of file [%s]\n",filename);
        CfLog(cferror,sendbuffer,"open");
        snprintf(sendbuffer,CF_BUFSIZE,"%s",CF_FAILEDSTR);
        SendSocketStream(sd,sendbuffer,args->buf_size,0);
    } else {
        while(true) {
            memset(sendbuffer,0,CF_BUFSIZE);

            Debug("Now reading from disk...\n");

            if ((n_read = read(fd,sendbuffer,args->buf_size)) == -1) {
                CfLog(cferror,"read failed in GetFile","read");
                break;
            }

            Debug("Read completed..\n");

            if (strncmp(sendbuffer,CF_FAILEDSTR,strlen(CF_FAILEDSTR)) == 0) {
                Debug("SENT FAILSTRING BY MISTAKE!\n");
            }

            if (n_read == 0) {
                break;
            } else {
                int savedlen = statbuf.st_size;

                /* This can happen with log files /databases etc */

                /* Don't do this too often */
                if (count++ % 3 == 0) {
                    Debug("Restatting %s\n",filename);
                    stat(filename,&statbuf);
                }

                if (statbuf.st_size != savedlen) {
                    snprintf(sendbuffer,CF_BUFSIZE,"%s%s: %s",CF_CHANGEDSTR1,
                            CF_CHANGEDSTR2,filename);

                    if (SendSocketStream(sd,sendbuffer,args->buf_size,0) == -1) {
                        CfLog(cfverbose,"Send failed in GetFile","send");
                    }

                    Debug("Aborting transfer after %d: file is "
                            "changing rapidly at source.\n",total);
                    break;
                }

                if ((savedlen - total)/args->buf_size > 0) {
                    sendlen = args->buf_size;
                } else if (savedlen != 0) {
                    sendlen = (savedlen - total);
                }
            }

            total += n_read;

            if (args->encrypt) {
                if (!EVP_EncryptUpdate(&ctx,out,&cipherlen,sendbuffer,n_read)) {
                    close(fd);
                    return;
                }

                if (cipherlen) {
                    if (SendTransaction(sd,out,cipherlen,CF_MORE) == -1) {
                        CfLog(cfverbose,"Send failed in GetFile","send");
                        break;
                    }
                }
            } else {
                Debug("Sending data on socket (%d)\n",sendlen);

                if (SendSocketStream(sd,sendbuffer,sendlen,0) == -1) {
                    CfLog(cfverbose,"Send failed in GetFile","send");
                    break;
                }

                Debug("Sending complete...\n");
            }
        }

        if (args->encrypt) {
            if (!EVP_EncryptFinal(&ctx,out,&cipherlen)) {
                close(fd);
                return;
            }

            Debug("Cipher len of extra data is %d\n",cipherlen);
            SendTransaction(sd,out,cipherlen,CF_DONE);
            EVP_CIPHER_CTX_cleanup(&ctx);
        }

        close(fd);
    }
    Debug("Done with GetFile()\n");
}

/* ----------------------------------------------------------------- */

void
CompareLocalChecksum(struct cfd_connection *conn, char *sendbuffer, char *recvbuffer)
{
    unsigned char digest[EVP_MAX_MD_SIZE+1];
    char filename[CF_BUFSIZE];
    char *sp;
    int i;

    sscanf(recvbuffer,"MD5 %[^\n]",filename);

    sp = recvbuffer + strlen(recvbuffer) + CF_SMALL_OFFSET;

    for (i = 0; i < CF_MD5_LEN; i++) {
        digest[i] = *sp++;
    }

    Debug("CompareLocalChecksums(%s,%s)\n",filename,ChecksumPrint('m',digest));
    memset(sendbuffer,0,CF_BUFSIZE);

    if (ChecksumChanged(filename,digest,cfverbose,true,'m')) {
        sprintf(sendbuffer,"%s",CFD_TRUE);
        Debug("Checksums didn't match\n");
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
    } else {
        sprintf(sendbuffer,"%s",CFD_FALSE);
        Debug("Checksums matched ok\n");
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
    }
}

/* ----------------------------------------------------------------- */

int
CfOpenDirectory(struct cfd_connection *conn, char *sendbuffer, char *dirname)
{
    DIR *dirh;
    struct dirent *dirp;
    int offset;

    Debug("CfOpenDirectory(%s)\n",dirname);

    if (*dirname != '/') {
        sprintf(sendbuffer,"BAD: request to access a non-absolute filename\n");
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        return -1;
    }

    if ((dirh = opendir(dirname)) == NULL) {
        Debug("cfng, couldn't open dir %s\n",dirname);
        snprintf(sendbuffer,CF_BUFSIZE,"BAD: cfng, couldn't open dir %s\n",
                dirname);
        SendTransaction(conn->sd_reply,sendbuffer,0,CF_DONE);
        return -1;
    }

    /* Pack names for transmission */

    memset(sendbuffer,0,CF_BUFSIZE);

    offset = 0;

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (strlen(dirp->d_name)+1+offset >= CF_BUFSIZE - CF_MAXLINKSIZE) {
            SendTransaction(conn->sd_reply,sendbuffer,offset+1,CF_MORE);
            offset = 0;
            memset(sendbuffer,0,CF_BUFSIZE);
        }

        strncpy(sendbuffer+offset,dirp->d_name,CF_MAXLINKSIZE);
        offset += strlen(dirp->d_name) + 1;     /* + zero byte separator */
    }

    strcpy(sendbuffer+offset,CFD_TERMINATOR);
    SendTransaction(conn->sd_reply,sendbuffer,
            offset+2+strlen(CFD_TERMINATOR),CF_DONE);
    Debug("END CfOpenDirectory(%s)\n",dirname);
    closedir(dirh);
    return 0;
}

/* ----------------------------------------------------------------- */

void
Terminate(int sd)
{
    char buffer[CF_BUFSIZE];

    memset(buffer,0,CF_BUFSIZE);

    strcpy(buffer,CFD_TERMINATOR);

    if (SendTransaction(sd,buffer,strlen(buffer)+1,CF_DONE) == -1) {
        CfLog(cfverbose,"","send");
        Verbose("Unable to reply with terminator...\n");
    }
}

/* ----------------------------------------------------------------- */

void
DeleteAuthList(struct Auth *ap)
{
    if (ap != NULL) {
        DeleteAuthList(ap->next);
        ap->next = NULL;

        DeleteItemList(ap->accesslist);

        free((char *)ap);
    }
}

/*
 * --------------------------------------------------------------------
 * Level 6
 * --------------------------------------------------------------------
 */

void
RefuseAccess(struct cfd_connection *conn, char *sendbuffer, 
        int size, char *errmesg)
{
    char *hostname, *username, *ipaddr;
    static char *def = "?";

    if (strlen(conn->hostname) == 0) {
        hostname = def;
    } else {
        hostname = conn->hostname;
    }

    if (strlen(conn->username) == 0) {
        username = def;
    } else {
        username = conn->username;
    }

    if (strlen(conn->ipaddr) == 0) {
        ipaddr = def;
    } else {
        ipaddr = conn->ipaddr;
    }

    snprintf(sendbuffer,CF_BUFSIZE,"%s",CF_FAILEDSTR);
    CfLog(cfinform,"Host authorization/authentication failed "
            "or access denied\n","");
    SendTransaction(conn->sd_reply,sendbuffer,size,CF_DONE);
    snprintf(sendbuffer,CF_BUFSIZE,"From (host=%s,user=%s,ip=%s)",
            hostname,username,ipaddr);
    CfLog(cfinform,sendbuffer,"");

    if (strlen(errmesg) > 0) {
        snprintf(g_output,CF_BUFSIZE,"ID from connecting host: (%s)",errmesg);
        CfLog(cflogonly,g_output,"");
    }
}

/* ----------------------------------------------------------------- */

void
ReplyNothing(struct cfd_connection *conn)
{
    char buffer[CF_BUFSIZE];

    snprintf(buffer, CF_BUFSIZE, 
            "Hello %s (%s), nothing relevant to do here...\n\n",
            conn->hostname,conn->ipaddr);

    if (SendTransaction(conn->sd_reply,buffer,0,CF_DONE) == -1) {
        CfLog(cferror,"","send");
    }
}

/* ----------------------------------------------------------------- */

char *
MapAddress(char *unspec_address)
{
    /* Is the address a mapped ipv4 over ipv6 address */

    if (strncmp(unspec_address,"::ffff:",7) == 0) {
        return (char *)(unspec_address+7);
    } else {
        return unspec_address;
    }
}

/* ----------------------------------------------------------------- */

int
CheckStoreKey(struct cfd_connection *conn, RSA *key)
{
    RSA *savedkey;
    char keyname[CF_MAXVARSIZE];

    if (OptionIs(g_contextid,"HostnameKeys",true)) {
        snprintf(keyname,CF_MAXVARSIZE,"%s-%s",conn->username,conn->hostname);
    } else {
        snprintf(keyname,CF_MAXVARSIZE,"%s-%s",conn->username,
                MapAddress(conn->ipaddr));
    }

    if (savedkey = HavePublicKey(keyname)) {
        Verbose("A public key was already known from %s/%s - "
                "no trust required\n",conn->hostname,conn->ipaddr);

        Verbose("Adding IP %s to SkipVerify - no need to check this "
                "if we have a key\n",conn->ipaddr);
        PrependItem(&g_skipverify,MapAddress(conn->ipaddr),NULL);

        if ((BN_cmp(savedkey->e,key->e) == 0) &&
                (BN_cmp(savedkey->n,key->n) == 0)) {
            Verbose("The public key identity was confirmed as %s@%s\n",
                    conn->username,conn->hostname);
            SendTransaction(conn->sd_reply,"OK: key accepted",0,CF_DONE);
            RSA_free(savedkey);
            return true;
        } else {

            /* 
             * If we find a key, but it doesn't match, see if we permit
             * dynamical IP addressing.
             */

            if ((g_dhcplist != NULL) && IsFuzzyItemIn(g_dhcplist,
                        MapAddress(conn->ipaddr))) {
                int result;
                result = IsWildKnownHost(savedkey,key,
                        MapAddress(conn->ipaddr),conn->username);
                RSA_free(savedkey);
                if (result) {
                    SendTransaction(conn->sd_reply,
                            "OK: key accepted",0,CF_DONE);
                } else {
                    SendTransaction(conn->sd_reply,
                            "BAD: keys did not match",0,CF_DONE);
                }
                return result;
            } else {
                /* if not, reject it */
                Verbose("The new public key does not match the old one! "
                        "Spoofing attempt!\n");
                SendTransaction(conn->sd_reply,
                        "BAD: keys did not match",0,CF_DONE);
                RSA_free(savedkey);
                return false;
            }
        }

        return true;
    } else if ((g_trustkeylist != NULL) &&
            IsFuzzyItemIn(g_trustkeylist,MapAddress(conn->ipaddr))) {

        Verbose("Host %s/%s was found in the list of hosts to trust\n",
                conn->hostname,conn->ipaddr);
        conn->trust = true;
        /* conn->maproot = false; ?? */
        SendTransaction(conn->sd_reply,
                "OK: key was accepted on trust",0,CF_DONE);
        SavePublicKey(keyname,key);
        AddToKeyDB(key,MapAddress(conn->ipaddr));
        return true;
    } else {
        Verbose("No previous key found, and unable to accept "
                "this one on trust\n");
        SendTransaction(conn->sd_reply,
                "BAD: key could not be accepted on trust",0,CF_DONE);
        return false;
    }
}

/* ----------------------------------------------------------------- */

/* 
 * This builds security from trust only gradually with DHCP - takes
 * time!  But what else are we going to do? ssh doesn't have this
 * problem - it just asks the user interactively. We can't do that ... 
 */

int
IsWildKnownHost(RSA *oldkey, RSA *newkey, char *mipaddr, char *username)
{
    DBT key,value;
    DB *dbp;
    int trust = false;
    char keyname[CF_MAXVARSIZE];
    char keydb[CF_MAXVARSIZE];

    snprintf(keyname,CF_MAXVARSIZE,"%s-%s",username,mipaddr);
    snprintf(keydb,CF_MAXVARSIZE,"%s/keys/dynamic",WORKDIR);

    Debug("The key does not match the known, key but the host "
            "has dynamic IP...\n");

    if ((g_trustkeylist != NULL) && IsFuzzyItemIn(g_trustkeylist,mipaddr)) {
        Debug("We will accept a new key for this IP on trust\n");
        trust = true;
    } else {
        Debug("Will not accept the new key, unless we have seen it before\n");
    }

    /* 
     * If the host is allowed to have a variable IP range, we can accept
     * the new key on trust for the given IP address provided we have seen
     * the key before.  Check for it in a database.
     */

    Debug("Checking to see if we have seen the new key before..\n");

    if ((errno = db_create(&dbp,NULL,0)) != 0) {
        sprintf(g_output,"Couldn't open average database %s\n",keydb);
        CfLog(cferror,g_output,"db_open");
        return false;
    }

#ifdef CF_OLD_DB
    if ((errno = dbp->open(dbp,keydb,NULL,DB_BTREE,
                    DB_CREATE,0644)) != 0)
#else
    if ((errno = dbp->open(dbp,NULL,keydb,NULL,DB_BTREE,
                    DB_CREATE,0644)) != 0)
#endif
    {
        sprintf(g_output,"Couldn't open average database %s\n",keydb);
        CfLog(cferror,g_output,"db_open");
        return false;
    }

    memset(&key,0,sizeof(newkey));
    memset(&value,0,sizeof(value));

    key.data = newkey;
    key.size = sizeof(RSA);

    if ((errno = dbp->get(dbp,NULL,&key,&value,0)) != 0) {
        Debug("The new key is not previously known, so we need to "
                "use policy for trusting the host %s\n",mipaddr);

        if (trust) {
            Debug("Policy says to trust the changed key from %s and "
                    "note that it could vary in future\n",mipaddr);
            memset(&key,0,sizeof(key));
            memset(&value,0,sizeof(value));

            key.data = newkey;
            key.size = sizeof(RSA);

            value.data = mipaddr;
            value.size = strlen(mipaddr)+1;

            if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0) {
                dbp->err(dbp,errno,NULL);
            }
        } else {
            Debug("Found no grounds for trusting this new key from %s\n",
                    mipaddr);
        }
    } else {
        snprintf(g_output,CF_BUFSIZE,
                "Public key was previously owned by %s now by %s - updating\n",
                value.data,mipaddr);
        CfLog(cfverbose,g_output,"");
        Debug("Now trusting this new key, because we have seen it before\n");
        DeletePublicKey(keyname);
        trust = true;
    }

    /* 
     * Save this new key in the database, for future reference,
     * regardless of whether we accept, but only change IP if trusted 
     */

    SavePublicKey(keyname,newkey);

    dbp->close(dbp,0);
    chmod(keydb,0644);

    return trust;
}

/* ----------------------------------------------------------------- */

void
AddToKeyDB(RSA *newkey, char *mipaddr)
{
    DBT key,value;
    DB *dbp;
    char keydb[CF_MAXVARSIZE];

    snprintf(keydb,CF_MAXVARSIZE,"%s/keys/dynamic",WORKDIR);

    if ((g_dhcplist != NULL) && IsFuzzyItemIn(g_dhcplist,mipaddr)) {
        /* 
         * Cache keys in the db as we see them is there are dynamical
         * addresses
         */

        if ((errno = db_create(&dbp,NULL,0)) != 0) {
            sprintf(g_output,"Couldn't open average database %s\n",keydb);
            CfLog(cferror,g_output,"db_open");
            return;
        }

#ifdef CF_OLD_DB
        if ((errno = dbp->open(dbp,keydb,NULL,DB_BTREE,
                        DB_CREATE,0644)) != 0)
#else
        if ((errno = dbp->open(dbp,NULL,keydb,NULL,DB_BTREE,
                        DB_CREATE,0644)) != 0)
#endif
        {
            sprintf(g_output,"Couldn't open average database %s\n",keydb);
            CfLog(cferror,g_output,"db_open");
            return;
        }

        memset(&key,0,sizeof(key));
        memset(&value,0,sizeof(value));

        key.data = newkey;
        key.size = sizeof(RSA);

        value.data = mipaddr;
        value.size = strlen(mipaddr)+1;

        if ((errno = dbp->put(dbp,NULL,&key,&value,0)) != 0) {
            dbp->err(dbp,errno,NULL);
        }

        dbp->close(dbp,0);
        chmod(keydb,0644);
    }
}


/*
 * --------------------------------------------------------------------
 * Toolkit/Class: conn
 * --------------------------------------------------------------------
 */

/* construct */

struct cfd_connection *
NewConn(int sd)
{
    struct cfd_connection *conn;

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","lock");
    }
#endif

    conn = (struct cfd_connection *) malloc(sizeof(struct cfd_connection));

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","lock");
    }
#endif

    if (conn == NULL) {
        CfLog(cferror,"Unable to allocate conn","malloc");
        ExitCleanly(0);
    }

    conn->sd_reply = sd;
    conn->id_verified = false;
    conn->rsa_auth = false;
    conn->trust = false;
    conn->hostname[0] = '\0';
    conn->ipaddr[0] = '\0';
    conn->username[0] = '\0';
    conn->session_key = NULL;

    Debug("*** New socket [%d]\n",sd);

    return conn;
}

/* ----------------------------------------------------------------- */

/* destruct */

void
DeleteConn(struct cfd_connection *conn)
{
    Debug("***Closing socket %d from %s\n",conn->sd_reply,conn->ipaddr);

    close(conn->sd_reply);

    if (conn->ipaddr != NULL) {
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
        if (pthread_mutex_lock(&MUTEX_COUNT) != 0) {
            CfLog(cferror,"pthread_mutex_lock failed","pthread_mutex_lock");
            DeleteConn(conn);
            return;
        }
#endif

        DeleteItemMatching(&CONNECTIONLIST,MapAddress(conn->ipaddr));

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
        if (pthread_mutex_unlock(&MUTEX_COUNT) != 0) {
            CfLog(cferror,"pthread_mutex_unlock failed","pthread_mutex_unlock");
            DeleteConn(conn);
            return;
        }
#endif
    }

    free ((char *)conn);
}

/*
 * --------------------------------------------------------------------
 * ERS
 * --------------------------------------------------------------------
 */

int
SafeOpen(char *filename)
{
    int fd;

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","pthread_mutex_lock");
    }
#endif

    fd = open(filename,O_RDONLY);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","pthread_mutex_unlock");
    }
#endif

    return fd;
}


/* ----------------------------------------------------------------- */

void
SafeClose(int fd)
{
#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_lock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_lock failed","pthread_mutex_lock");
    }
#endif

    close(fd);

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
    if (pthread_mutex_unlock(&MUTEX_SYSCALL) != 0) {
        CfLog(cferror,"pthread_mutex_unlock failed","pthread_mutex_unlock");
    }
#endif
}

/* ----------------------------------------------------------------- */

int
cfscanf(char *in, int len1, int len2, char *out1, char *out2, char *out3)
{
    int len3=0;
    char *sp;

    sp = in;
    bcopy(sp,out1,len1);
    out1[len1]='\0';

    sp += len1 + 1;
    bcopy(sp,out2,len2);

    sp += len2 + 1;
    len3=strlen(sp);
    bcopy(sp,out3,len3);
    out3[len3]='\0';

    return (len1 + len2 + len3 + 2);
}


/*
 * --------------------------------------------------------------------
 * Linking simplification
 * --------------------------------------------------------------------
 */

void
GetRemoteMethods(void)
{
}

/* ----------------------------------------------------------------- */

int
RecursiveTidySpecialArea(char *name, struct Tidy *tp, int maxrecurse,
        struct stat *sb)
{
    return true;
}

int
CompareMD5Net(char *file1, char *file2, struct Image *ip)
{
    return 0;
}

struct Method *
IsDefinedMethod(char *name, char *s)
{
    return NULL;
}
