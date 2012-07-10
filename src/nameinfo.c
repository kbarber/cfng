/*
 * $Id: nameinfo.c 711 2004-05-21 19:05:36Z skaar $
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
 * Toolkits: "object" library
 */

#include "cf.defs.h"
#include "cf.extern.h"
#include "../pub/global.h"
#ifdef IRIX
#include <sys/syssgi.h>
#endif

/* FreeBSD has size macro defined as ifreq is variable size */
#ifdef _SIZEOF_ADDR_IFREQ
#define SIZEOF_IFREQ(x) _SIZEOF_ADDR_IFREQ(x)
#else
#define SIZEOF_IFREQ(x) sizeof(struct ifreq)
#endif

/* ----------------------------------------------------------------- */

void
GetNameInfo(void)
{
    int i, found = false;
    char *sp, *sp2;
    time_t tloc;
    struct hostent *hp;
    struct sockaddr_in cin;
#ifdef AIX
    char real_version[_SYS_NMLN];
#endif
#ifdef IRIX
    char real_version[256]; /* see <sys/syssgi.h> */
#endif
#ifdef HAVE_SYSINFO
#ifdef SI_ARCHITECTURE
    long sz;
#endif
#endif

    Debug("GetNameInfo()\n");

    g_vfqname[0] = g_vuqname[0] = '\0';

    if (uname(&g_vsysname) == -1) {
        perror("uname ");
        FatalError("Uname couldn't get kernel name info!!\n");
    }

#ifdef AIX
    snprintf(real_version, _SYS_NMLN, "%.80s.%.80s",
            g_vsysname.version, g_vsysname.release);
    strncpy(g_vsysname.release, real_version, _SYS_NMLN);
#elif defined IRIX
/* This gets us something like `6.5.19m' rather than just `6.5'.  */
    syssgi (SGI_RELEASE_NAME, 256, real_version);
#endif

    for (sp = g_vsysname.sysname; *sp != '\0'; sp++) {
        *sp = ToLower(*sp);
    }

    for (sp = g_vsysname.machine; *sp != '\0'; sp++) {
        *sp = ToLower(*sp);
    }

    for (i = 0; g_classattributes[i][0] != '\0'; i++) {

        if (WildMatch(g_classattributes[i][0],
                    ToLowerStr(g_vsysname.sysname))) {

            if (WildMatch(g_classattributes[i][1], 
                        g_vsysname.machine)) {

                if (WildMatch(g_classattributes[i][2], 
                            g_vsysname.release)) {

                    if (g_underscore_classes) {
                        snprintf(g_vbuff, CF_BUFSIZE, "_%s", g_classtext[i]);
                        AddClassToHeap(g_vbuff);
                    } else {
                        AddClassToHeap(g_classtext[i]);
                    }
                    found = true;
                    g_vsystemhardclass = (enum classes) i;
                    break;
                }
            } else {
                Debug2("Cfengine: I recognize %s but not %s\n",
                        g_vsysname.sysname, g_vsysname.machine);
                    continue;
            }
        }
   }

    if ((sp = malloc(strlen(g_vsysname.nodename)+1)) == NULL) {
        FatalError("malloc failure in initialize()");
    }

    strcpy(sp, g_vsysname.nodename);
    SetDomainName(sp);

    /* Truncate fully qualified name */
    for (sp2=sp; *sp2 != '\0'; sp2++) {
        if (*sp2 == '.') {
            *sp2 = '\0';
            Debug("Truncating fully qualified hostname %s to %s\n",
                    g_vsysname.nodename,sp);
            break;
        }
    }

    g_vdefaultbinserver.name = sp;

    AddClassToHeap(CanonifyName(sp));


    if ((tloc = time((time_t *)NULL)) == -1) {
        printf("Couldn't read system clock\n");
    }

    if (g_verbose || g_debug || g_d2 || g_d3) {
        if (g_underscore_classes) {
            snprintf(g_vbuff, CF_BUFSIZE, "_%s", g_classtext[i]);
        } else {
            snprintf(g_vbuff, CF_BUFSIZE, "%s", g_classtext[i]);
        }

        if (g_iscfengine) {
            printf ("cfng: configuration agent (cfagent) - \n%s\n%s\n\n",
                    VERSION, g_copyright);
        } else {
            printf ("cfng: configuration server (cfservd) - \n%s\n%s\n\n",
                    VERSION, g_copyright);
        }

        printf ("------------------------------------------------------------------------\n\n");
        printf ("Host name is: %s\n", g_vsysname.nodename);
        printf ("Operating System Type is %s\n", g_vsysname.sysname);
        printf ("Operating System Release is %s\n", g_vsysname.release);
        printf ("Architecture = %s\n\n\n", g_vsysname.machine);
        printf ("Using internal soft-class %s for host %s\n\n",
                g_vbuff, g_classtext[g_vsystemhardclass]);
        printf ("The time is now %s\n\n", ctime(&tloc));
        printf ("------------------------------------------------------------------------\n\n");

    }

    sprintf(g_vbuff, "%d_bit", sizeof(long)*8);
    AddClassToHeap(g_vbuff);
    Verbose("Additional hard class defined as: %s\n", CanonifyName(g_vbuff));

    snprintf(g_vbuff, CF_BUFSIZE, "%s_%s", g_vsysname.sysname,
            g_vsysname.release);
    AddClassToHeap(CanonifyName(g_vbuff));

#ifdef IRIX
    /* 
     * Get something like `irix64_6_5_19m' defined as well as
     * `irix64_6_5'.  Just copying the latter into g_vsysname.release
     * wouldn't be backwards-compatible.
     */
    snprintf(g_vbuff, CF_BUFSIZE, "%s_%s", g_vsysname.sysname, real_version);
    AddClassToHeap(CanonifyName(g_vbuff));
#endif

    AddClassToHeap(CanonifyName(g_vsysname.machine));

    Verbose("Additional hard class defined as: %s\n",CanonifyName(g_vbuff));

    snprintf(g_vbuff, CF_BUFSIZE,"%s_%s", g_vsysname.sysname,
            g_vsysname.machine);
    AddClassToHeap(CanonifyName(g_vbuff));

    Verbose("Additional hard class defined as: %s\n",CanonifyName(g_vbuff));

    snprintf(g_vbuff, CF_BUFSIZE, "%s_%s_%s",
            g_vsysname.sysname, g_vsysname.machine, g_vsysname.release);

    AddClassToHeap(CanonifyName(g_vbuff));

    Verbose("Additional hard class defined as: %s\n", CanonifyName(g_vbuff));

#ifdef HAVE_SYSINFO
#ifdef SI_ARCHITECTURE
    sz = sysinfo(SI_ARCHITECTURE, g_vbuff, CF_BUFSIZE);
    if (sz == -1) {
        Verbose("cfagent internal: sysinfo returned -1\n");
    } else {
        AddClassToHeap(CanonifyName(g_vbuff));
        Verbose("Additional hard class defined as: %s\n", g_vbuff);
    }
#endif
#endif

    snprintf(g_vbuff, CF_BUFSIZE, "%s_%s_%s_%s",
            g_vsysname.sysname, g_vsysname.machine, g_vsysname.release,
            g_vsysname.version);

    if (strlen(g_vbuff) < CF_MAXVARSIZE-2) {
        g_varch = strdup(CanonifyName(g_vbuff));
    } else {

        Verbose("cfagent internal: $(arch) overflows CF_MAXVARSIZE! Truncating\n");
        g_varch = strdup(CanonifyName(g_vsysname.sysname));
    }

    snprintf(g_vbuff, CF_BUFSIZE, "%s_%s",
            g_vsysname.sysname, g_vsysname.machine);

    g_varch2 = strdup(CanonifyName(g_vbuff));

    AddClassToHeap(g_varch);

    Verbose("Additional hard class defined as: %s\n", g_varch);

    if (! found) {

        CfLog(cferror,"Cfengine: I don't understand "
                "what architecture this is!","");

    }

    strcpy(g_vbuff, "compiled_on_");
    strcat(g_vbuff, CanonifyName(AUTOCONF_SYSNAME));

    AddClassToHeap(CanonifyName(g_vbuff));

    Verbose("\nGNU autoconf class from compile time: %s\n\n", g_vbuff);

    /* Get IP address from nameserver */
    if ((hp = gethostbyname(g_vsysname.nodename)) == NULL) {
        return;
    } else {
        memset(&cin, 0, sizeof(cin));
        cin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr))->s_addr;
        Verbose("Address given by nameserver: %s\n", inet_ntoa(cin.sin_addr));
        strcpy(g_vipaddress, inet_ntoa(cin.sin_addr));

        for (i=0; hp->h_aliases[i] != NULL; i++) {
            Debug("Adding alias %s..\n", hp->h_aliases[i]);
            AddClassToHeap(CanonifyName(hp->h_aliases[i]));
        }
    }
}

/* ----------------------------------------------------------------- */

void
GetInterfaceInfo(void)
{
    int fd, len, i, j;
    struct ifreq ifbuf[512], ifr, *ifp;
    struct ifconf list;
    struct sockaddr_in *sin;
    struct hostent *hp;
    char *sp;
    char ip[CF_MAXVARSIZE];
    char name[CF_MAXVARSIZE];

    Debug("GetInterfaceInfo()\n");

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        CfLog(cferror, "Couldn't open socket", "socket");
        exit(1);
    }

    list.ifc_len = sizeof(ifbuf);
    list.ifc_req = ifbuf;

#ifdef SIOCGIFCONF
    if (ioctl(fd, SIOCGIFCONF, &list) == -1 ||
            (list.ifc_len < (sizeof(struct ifreq))))
#else
    if (ioctl(fd, OSIOCGIFCONF, &list) == -1 ||
            (list.ifc_len < (sizeof(struct ifreq))))
#endif
    {
        CfLog(cferror, "Couldn't get interfaces", "ioctl");
        exit(1);
    }

    for (j = 0, len = 0, ifp = list.ifc_req; len < list.ifc_len;
            len+=SIZEOF_IFREQ(*ifp), j++, ifp=&ifbuf[j]) {
        if (ifp->ifr_addr.sa_family == 0) {
            continue;
        }

        Verbose("Interface %d: %s\n", j+1, ifp->ifr_name);
        if(g_underscore_classes) {
            snprintf(g_vbuff, CF_BUFSIZE, "_net_iface_%s",
                    CanonifyName(ifp->ifr_name));
        } else {
            snprintf(g_vbuff, CF_BUFSIZE, "net_iface_%s",
                    CanonifyName(ifp->ifr_name));
        }
        AddClassToHeap(g_vbuff);

        if (ifp->ifr_addr.sa_family == AF_INET) {
            strncpy(ifr.ifr_name, ifp->ifr_name, sizeof(ifp->ifr_name));

            if (ioctl(fd,SIOCGIFFLAGS,&ifr) == -1) {
                CfLog(cferror, "No such network device", "ioctl");
                close(fd);
                return;
            }

            /* 
             * Used to check if interface was "up" if ((ifr.ifr_flags &
             * IFF_UP) && !(ifr.ifr_flags & IFF_LOOPBACK)) Now check
             * whether it is configured ...
             */
            if ((ifr.ifr_flags & IFF_BROADCAST) &&
                    !(ifr.ifr_flags & IFF_LOOPBACK)) {
                sin=(struct sockaddr_in *)&ifp->ifr_addr;

                snprintf(name, CF_MAXVARSIZE-1, "ipv4[%s]",
                        CanonifyName(ifp->ifr_name));
                AddMacroValue(g_contextid, name, 
                        inet_ntoa(sin->sin_addr));

                if ((hp = gethostbyaddr((char *)&(sin->sin_addr.s_addr),
                            sizeof(sin->sin_addr.s_addr), AF_INET)) == NULL) {
                    Debug("Host information for %s not found\n",
                            inet_ntoa(sin->sin_addr));
                } else {

                    if (hp->h_name != NULL) {
                        Debug("Adding hostip %s..\n",
                                inet_ntoa(sin->sin_addr));
                        AddClassToHeap(CanonifyName(inet_ntoa(sin->sin_addr)));
                        Debug("Adding hostname %s..\n", hp->h_name);
                        AddClassToHeap(CanonifyName(hp->h_name));

                        for (i=0; hp->h_aliases[i] != NULL; i++) {
                            Debug("Adding alias %s..\n", hp->h_aliases[i]);
                            AddClassToHeap(CanonifyName(hp->h_aliases[i]));
                        }

                        /* Old style compat */
                        strcpy(ip,inet_ntoa(sin->sin_addr));
                        AppendItem(&g_ipaddresses, ip, "");

                        for (sp = ip+strlen(ip)-1; *sp != '.'; sp--) { }
                        *sp = '\0';
                        AddClassToHeap(CanonifyName(ip));

                        /* New style */
                        strcpy(ip, "ipv4_");
                        strcat(ip, inet_ntoa(sin->sin_addr));
                        AddClassToHeap(CanonifyName(ip));

                        for (sp = ip+strlen(ip)-1; (sp > ip); sp--) {
                            if (*sp == '.') {
                                *sp = '\0';
                                AddClassToHeap(CanonifyName(ip));
                            }
                        }
                    }
                }
            }
        }

        ifp = (struct ifreq *)((char *)ifp + SIZEOF_IFREQ(*ifp));
    }

    close(fd);
}

/* ----------------------------------------------------------------- */

/* 
 * Whatever the manuals might say, you cannot get IPV6 interface
 * configuration from the ioctls. This seems to be implemented in a non
 * standard way across OSes BSDi has done getifaddrs(), solaris 8 has a
 * new ioctl, Stevens book shows the suggestion which has not been
 * implemented...
 */
void
GetV6InterfaceInfo(void)
{
    FILE *pp;
    char buffer[CF_BUFSIZE];

    Verbose("Trying to locate my IPv6 address\n");

    switch (g_vsystemhardclass) {
    case cfnt:
        /* NT cannot do this */
        break;
    default:
        if ((pp = cfpopen("/sbin/ifconfig -a", "r")) == NULL) {
            Verbose("Could not find interface info\n");
            return;
        }

        while (!feof(pp)) {
            fgets(buffer, CF_BUFSIZE, pp);

            if (StrStr(buffer, "inet6")) {
                struct Item *ip,*list = NULL;
                char *sp;

                list = SplitStringAsItemList(buffer, ' ');

                for (ip = list; ip != NULL; ip=ip->next) {
                    for (sp = ip->name; *sp != '\0'; sp++) {

                        /* Remove CIDR mask */
                        if (*sp == '/')  {
                            *sp = '\0';
                        }
                    }

                    if (IsIPV6Address(ip->name) &&
                            (strcmp(ip->name, "::1") != 0)) {

                        Verbose("Found IPv6 address %s\n", ip->name);
                        AppendItem(&g_ipaddresses, ip->name, "");
                        AddClassToHeap(CanonifyName(ip->name));

                    }
                }
                DeleteItemList(list);
            }
        }
        fclose(pp);
        break;
    }
}

/* ----------------------------------------------------------------- */

/* Function contrib David Brownlee <abs@mono.org> */

void
AddNetworkClass(char *netmask)
{
    struct in_addr ip,nm;
    char *sp;
    char nmbuf[CF_MAXVARSIZE], ipbuf[CF_MAXVARSIZE];

    /*
     * Has to differentiate between cases such as:
     *      192.168.101.1/24 -> 192.168.101   and
     *      192.168.101.1/26 -> 192.168.101.0
     * We still have the, um... 'interesting' Class C default Network Class
     * set by GetNameInfo()
     */

    /* This is also a convenient method to ensure valid dotted quad */
    if ((nm.s_addr = inet_addr(netmask)) != -1 &&
            (ip.s_addr = inet_addr(g_vipaddress)) != -1) {

        /* Will not work with IPv6 */
        ip.s_addr &= nm.s_addr;
        strcpy(ipbuf,inet_ntoa(ip));

        strcpy(nmbuf, inet_ntoa(nm));

        while( (sp = strrchr(nmbuf, '.')) && strcmp(sp, ".0") == 0 ) {
            *sp = 0;
            *strrchr(ipbuf, '.') = 0;
        }
        AddClassToHeap(CanonifyName(ipbuf));
    }
}



/* ----------------------------------------------------------------- */

void
SetDomainName(char *sp)           /* Bas van der Vlies */
{
    char fqn[CF_MAXVARSIZE];
    char *ptr;
    char buffer[CF_BUFSIZE];

    if (gethostname(fqn, sizeof(fqn)) != -1) {
        strcpy(g_vfqname, fqn);
        strcpy(buffer, g_vfqname);
        AddClassToHeap(CanonifyName(buffer));
        AddClassToHeap(CanonifyName(ToLowerStr(buffer)));

        if (strstr(fqn, ".")) {
            ptr = strchr(fqn, '.');
            strcpy(g_vdomain, ++ptr);
        }
    }

    if (strstr(g_vfqname, ".") == 0 
            && (strcmp(g_vdomain, CF_START_DOMAIN) != 0)) {

        strcat(g_vfqname, ".");
        strcat(g_vfqname, g_vdomain);
    }

    AddClassToHeap(CanonifyName(g_vdomain));
    DeleteClassFromHeap("undefined_domain");
}
