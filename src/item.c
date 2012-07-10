/*
 * $Id: item.c 757 2004-05-25 20:12:59Z skaar $
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
 * --------------------------------------------------------------------
 * Toolkit: the "item" object library for cfengine.
 * --------------------------------------------------------------------
 */

#include "cf.defs.h"
#include "cf.extern.h"

/*
 * --------------------------------------------------------------------
 * Item list.
 * --------------------------------------------------------------------
 */

int
ListLen(struct Item *list)
{
    int count = 0;
    struct Item *ip;

    for (ip = list; ip != NULL; ip = ip->next) {
        count++;
    }

    return count;
}

/* ----------------------------------------------------------------- */

int
ByteSizeList(struct Item *list)
{
    int count = 0;
    struct Item *ip;

    for (ip = list; ip != NULL; ip = ip->next) {
        count+=strlen(ip->name);
    }

    return count;
}

/* ----------------------------------------------------------------- */

int
IsItemIn(struct Item *list,char *item)
{
    struct Item *ptr;

    if ((item == NULL) || (strlen(item) == 0)) {
        return true;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->name, item) == 0) {
            return(true);
        }
    }

    return(false);
}

/* ----------------------------------------------------------------- */

int
GetItemListCounter(struct Item *list, char *item)
{
    struct Item *ptr;

    if ((item == NULL) || (strlen(item) == 0)) {
        return -1;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->name, item) == 0) {
            return ptr->counter;
        }
    }

    return -1;
}

/* ----------------------------------------------------------------- */

void
IncrementItemListCounter(struct Item *list, char *item)
{
    struct Item *ptr;

    if ((item == NULL) || (strlen(item) == 0)) {
        return;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->name, item) == 0) {
            ptr->counter++;
            return;
        }
    }
}

/* ----------------------------------------------------------------- */

void
SetItemListCounter(struct Item *list, char *item, int value)
{
    struct Item *ptr;

    if ((item == NULL) || (strlen(item) == 0)) {
        return;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->name, item) == 0) {
            ptr->counter = value;
            return;
        }
    }
}

/* ----------------------------------------------------------------- */

/* 
 * This is for matching ranges of IP addresses, like CIDR e.g.
 *   Range1 = ( 128.39.89.250/24 )
 *   Range2 = ( 128.39.89.100-101 )
 */

int
IsFuzzyItemIn(struct Item *list, char *item)
{
    struct Item *ptr;

    Debug("\nFuzzyItemIn(LIST,%s)\n", item);

    if ((item == NULL) || (strlen(item) == 0)) {
        return true;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next) {
        Debug(" Try FuzzySetMatch(%s,%s)\n", ptr->name, item);

        if (FuzzySetMatch(ptr->name, item) == 0) {
            return(true);
        }
    }

    return(false);
}

/* ----------------------------------------------------------------- */

/* 
 * Notes:  Refrain from freeing list2 after using ConcatLists list1 must
 *         have at least one element in it 
 */

struct Item *
ConcatLists (struct Item *list1, struct Item *list2)
{
    struct Item *endOfList1;

    if (list1 == NULL) {
        FatalError("ConcatLists: first argument must have "
                "at least one element");
    }

    for (endOfList1=list1; 
         endOfList1->next != NULL; 
         endOfList1=endOfList1->next) { }

    endOfList1->next = list2;
    return list1;
}

/* ----------------------------------------------------------------- */

void
PrependItem(struct Item **liststart, char *itemstring, char *classes)
{
    struct Item *ip;
    char *sp, *spe = NULL;

    if (!g_parsing && (g_action == editfiles)) {
        snprintf(g_output, CF_BUFSIZE, "Prepending [%s]\n", itemstring);
        CfLog(cfinform, g_output,"");
    } else {
        EditVerbose("Prepending [%s]\n", itemstring);
    }

    if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL) {
        CfLog(cferror, "", "malloc");
        FatalError("");
    }

    if ((sp = malloc(strlen(itemstring)+2)) == NULL) {
        CfLog(cferror, "", "malloc");
        FatalError("");
    }

    if ((classes != NULL) && (spe = malloc(strlen(classes)+2)) == NULL) {
        CfLog(cferror,"","malloc");
        FatalError("");
    }

    strcpy(sp, itemstring);
    ip->name = sp;
    ip->next = *liststart;
    ip->counter = 0;
    *liststart = ip;

    if (classes != NULL) {
        strcpy(spe, classes);
        ip->classes = spe;
    } else {
        ip->classes = NULL;
    }
    g_numberofedits++;
}

/* ----------------------------------------------------------------- */

void
AppendItems(struct Item **liststart, char *itemstring, char *classes)
{
    struct Item *ip, *lp;
    char *sp, *spe = NULL;
    char currentitem[CF_MAXVARSIZE], local[CF_MAXVARSIZE];

    if ((itemstring == NULL) || strlen(itemstring) == 0) {
        return;
    }

    memset(local, 0, CF_MAXVARSIZE);
    strncpy(local, itemstring, CF_MAXVARSIZE-1);

    /* split and iteratate across the list with space and comma sep */

    for (sp = local ; *sp != '\0'; sp++) {
        /* find our separating character */
        memset(currentitem, 0, CF_MAXVARSIZE);
        sscanf(sp, "%250[^ ,\n\t]", currentitem);

        if (strlen(currentitem) == 0) {
            continue;
        }

        /* add it to whatever list we're operating on */
        Debug("Appending %s\n", currentitem);
        AppendItem(liststart, currentitem, classes);

        /* and move up to the next list */
        sp += strlen(currentitem);
    }
}

/* ----------------------------------------------------------------- */

void
AppendItem(struct Item **liststart, char *itemstring, char *classes)
{
    struct Item *ip, *lp;
    char *sp, *spe = NULL;

    if (!g_parsing && (g_action == editfiles)) {
        snprintf(g_output, CF_BUFSIZE, "Appending [%s]\n", itemstring);
        CfLog(cfinform, g_output, "");
    } else {
        EditVerbose("Appending [%s]\n", itemstring);
    }

    if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL) {
        CfLog(cferror, "", "malloc");
        FatalError("");
    }

    if ((sp = malloc(strlen(itemstring) + CF_EXTRA_SPACE)) == NULL) {
        CfLog(cferror, "", "malloc");
        FatalError("");
    }

    if (*liststart == NULL) {
        *liststart = ip;
    } else {
        for (lp = *liststart; lp->next != NULL; lp = lp->next) { }
        lp->next = ip;
    }

    if ((classes != NULL) && (spe = malloc(strlen(classes)+2)) == NULL) {
        CfLog(cferror, "", "malloc");
        FatalError("");
    }

    strcpy(sp, itemstring);
    ip->name = sp;
    ip->next = NULL;
    ip->counter = 0;

    if (classes != NULL) {
        strcpy(spe, classes);
        ip->classes = spe;
    } else {
        ip->classes = NULL;
    }

    g_numberofedits++;
}


/* ----------------------------------------------------------------- */

void
InstallItem (struct Item **liststart, char *itemstring, char *classes,
    int ifelapsed, int expireafter)
{
    struct Item *ip, *lp;
    char *sp,*spe = NULL;

    if (!g_parsing && (g_action == editfiles)) {
        snprintf(g_output, CF_BUFSIZE, "Appending [%s]\n", itemstring);
        CfLog(cfinform, g_output, "");
    } else {
        EditVerbose("Appending [%s]\n", itemstring);
    }


    if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL) {
        CfLog(cferror, "", "malloc");
        FatalError("");
    }

    if ((sp = malloc(strlen(itemstring) + CF_EXTRA_SPACE)) == NULL) {
        CfLog(cferror, "", "malloc");
        FatalError("");
    }

    if (*liststart == NULL) {
        *liststart = ip;
    } else {
        for (lp = *liststart; lp->next != NULL; lp = lp->next) { }
        lp->next = ip;
    }

    if ((classes!= NULL) && (spe = malloc(strlen(classes) + 2)) == NULL) {
        CfLog(cferror, "", "malloc");
        FatalError("");
    }

    strcpy(sp, itemstring);

    if (g_pifelapsed != -1) {
        ip->ifelapsed = g_pifelapsed;
    } else {
        ip->ifelapsed = ifelapsed;
    }

    if (g_pexpireafter != -1) {
        ip->expireafter = g_pexpireafter;
    } else {
        ip->expireafter = expireafter;
    }

    ip->name = sp;
    ip->next = NULL;

    if (classes != NULL) {
        strcpy(spe, classes);
        ip->classes = spe;
    } else {
        ip->classes = NULL;
    }

    g_numberofedits++;
}

/* ----------------------------------------------------------------- */

/* delete starting from item */
void
DeleteItemList(struct Item *item)
{
    if (item != NULL) {
        DeleteItemList(item->next);
        item->next = NULL;

        if (item->name != NULL) {
            free(item->name);
        }

        if (item->classes != NULL) {
            free(item->classes);
        }

        free((char *)item);
    }
}

/* ----------------------------------------------------------------- */

void
DeleteItem(struct Item **liststart, struct Item *item)
{
    struct Item *ip, *sp;

    if (item != NULL) {
        EditVerbose("Delete Item: %s\n", item->name);

        if (item->name != NULL) {
            free(item->name);
        }

        if (item->classes != NULL) {
            free(item->classes);
        }

        sp = item->next;

        if (item == *liststart) {
            *liststart = sp;
        } else {
            for (ip = *liststart; ip->next != item; ip = ip->next) { }
            ip->next = sp;
        }

        free((char *)item);

        g_numberofedits++;
    }
}

/* ----------------------------------------------------------------- */


void
DebugListItemList(struct Item *liststart)
{
    struct Item *ptr;

    for (ptr = liststart; ptr != NULL; ptr = ptr->next) {
        printf("CFDEBUG: [%s]\n", ptr->name);
    }
}

/* ----------------------------------------------------------------- */

int
ItemListsEqual(struct Item *list1, struct Item *list2)
{
    struct Item *ip1, *ip2;

    ip1 = list1;
    ip2 = list2;

    while (true) {
        if ((ip1 == NULL) && (ip2 == NULL)) {
            return true;
        }

        if ((ip1 == NULL) || (ip2 == NULL)) {
            return false;
        }

        if (strcmp(ip1->name, ip2->name) != 0) {
            return false;
        }

        ip1 = ip1->next;
        ip2 = ip2->next;
    }
}

/*
 * --------------------------------------------------------------------
 * Fuzzy Match
 * --------------------------------------------------------------------
 */

int
FuzzyMatchParse(char *s)
{
    char *sp;
    short isCIDR = false, isrange = false, isv6 = false, isv4 = false;
    short isADDR = false;
    char address[128];
    int mask,count = 0;

    Debug("Check ParsingIPRange(%s)\n", s);

    /* Is this an address or hostname */
    for (sp = s; *sp != '\0'; sp++) {
        if (!isxdigit((int)*sp)) {
            isADDR = false;
            break;
        }

        /* Catches any ipv6 address */
        if (*sp == ':') {
            isADDR = true;
            break;
        }

        /* catch non-ipv4 address - no more than 3 digits */
        if (isdigit((int)*sp)) {
            count++;
            if (count > 3) {
                isADDR = false;
                break;
            }
        } else {
            count = 0;
        }
    }

    if (! isADDR) {
        return true;
    }

    if (strstr(s,"/") != 0) {
        isCIDR = true;
    }

    if (strstr(s,"-") != 0) {
        isrange = true;
    }

    if (strstr(s,".") != 0) {
        isv4 = true;
    }

    if (strstr(s,":") != 0) {
        isv6 = true;
    }

    if (isv4 && isv6) {
        yyerror("Mixture of IPv6 and IPv4 addresses");
        return false;
    }

    if (isCIDR && isrange) {
        yyerror("Cannot mix CIDR notation with xx-yy range notation");
        return false;
    }

    if (isv4 && isCIDR) {
        /* xxx.yyy.zzz.mmm/cc */
        if (strlen(s) > 4+3*4+1+2) {
            yyerror("IPv4 address looks too long");
            return false;
        }

        address[0] = '\0';
        mask = 0;
        sscanf(s, "%16[^/]/%d", address,&mask);

        if (mask < 8) {
            snprintf(g_output, CF_BUFSIZE,
                    "Mask value %d in %s is less than 8", mask,s);
            yyerror(g_output);
            return false;
        }

        if (mask > 30) {
            snprintf(g_output, CF_BUFSIZE,
                    "Mask value %d in %s is silly (> 30)", mask,s);
            yyerror(g_output);
            return false;
        }
    }


    if (isv4 && isrange) {
        long i, from = -1, to = -1, cmp = -1;
        char *sp1,buffer1[8];
        char buffer2[8];

        sp1 = s;

        for (i = 0; i < 4; i++) {
            buffer1[0] = '\0';
            sscanf(sp1, "%[^.]", buffer1);
            sp1 += strlen(buffer1)+1;

            if (strstr(buffer1,"-")) {
                sscanf(buffer1, "%ld-%ld", &from, &to);

                if (from < 0 || to < 0) {
                    yyerror("Error in IP range - looks like address, "
                            "or bad hostname");
                    return false;
                }

                if (to < from) {
                    yyerror("Bad IP range");
                    return false;
                }

            }
        }
    }

    if (isv6 && isCIDR) {
        char address[128];
        int mask,blocks;

        if (strlen(s) < 20) {
            yyerror("IPv6 address looks too short");
            return false;
        }

        if (strlen(s) > 42) {
            yyerror("IPv6 address looks too long");
            return false;
        }

        address[0] = '\0';
        mask = 0;
        sscanf(s, "%40[^/]/%d", address, &mask);
        blocks = mask/8;

        if (mask % 8 != 0) {
            CfLog(cferror,"Cannot handle ipv6 masks which "
                    "are not 8 bit multiples (fix me)","");
            return false;
        }

        if (mask > 15) {
            yyerror("IPv6 CIDR mask is too large");
            return false;
        }
    }

    return true;
}

/* ----------------------------------------------------------------- */

/* 
 * Match two IP strings - with : or . in hex or decimal s1 is the test
 * string, and s2 is the reference e.g.:
 *      FuzzySetMatch("128.39.74.10/23","128.39.75.56") == 0 
 */
int
FuzzySetMatch(char *s1, char *s2)
{
    char *sp;
    short isCIDR = false, isrange = false, isv6 = false, isv4 = false;
    char address[128];
    int mask, significant;
    unsigned long a1, a2;

    if (strstr(s1, "/") != 0) {
        isCIDR = true;
    }

    if (strstr(s1, "-") != 0) {
        isrange = true;
    }

    if (strstr(s1, ".") != 0) {
        isv4 = true;
    }

    if (strstr(s1, ":") != 0) {
        isv6 = true;
    }

    if (isv4 && isv6) {
        snprintf(g_output, CF_BUFSIZE,
                "Mixture of IPv6 and IPv4 addresses: %s", s1);
        CfLog(cferror, g_output, "");
        return -1;
    }

    if (isCIDR && isrange) {
        snprintf(g_output, CF_BUFSIZE,
                "Cannot mix CIDR notation with xxx-yyy range notation: %s", s1);
        CfLog(cferror, g_output, "");
        return -1;
    }

    if (!(isv6 || isv4)) {
        snprintf(g_output, CF_BUFSIZE,
                "Not a valid address range - or not "
                "a fully qualified name: %s", s1);
        CfLog(cferror, g_output, "");
        return -1;
    }

    if (!(isrange||isCIDR)) {
        /* do partial string match */
        return strncmp(s1, s2, strlen(s1)); 
    }


    if (isv4) {
        struct sockaddr_in addr1, addr2;
        int shift;

        memset(&addr1, 0, sizeof(struct sockaddr_in));
        memset(&addr2, 0, sizeof(struct sockaddr_in));

    if (isCIDR) {
        address[0] = '\0';
        mask = 0;
        sscanf(s1, "%16[^/]/%d", address,&mask);
        shift = 32 - mask;

        bcopy((struct sockaddr_in *) sockaddr_pton(AF_INET, address),
                &addr1, sizeof(struct sockaddr_in));
        bcopy((struct sockaddr_in *) sockaddr_pton(AF_INET, s2),
                &addr2, sizeof(struct sockaddr_in));

        a1 = htonl(addr1.sin_addr.s_addr);
        a2 = htonl(addr2.sin_addr.s_addr);

        a1 = a1 >> shift;
        a2 = a2 >> shift;

        if (a1 == a2) {
            return 0;
        } else {
            return -1;
        }
    } else {
        long i, from = -1, to = -1, cmp = -1;
        char *sp1, *sp2, buffer1[8], buffer2[8];

        sp1 = s1;
        sp2 = s2;

        for (i = 0; i < 4; i++) {
            if (sscanf(sp1, "%[^.]", buffer1) <= 0) {
                break;
            }
            sp1 += strlen(buffer1) + 1;
            sscanf(sp2, "%[^.]", buffer2);
            sp2 += strlen(buffer2) + 1;

            if (strstr(buffer1, "-")) {
                sscanf(buffer1, "%ld-%ld", &from, &to);
                sscanf(buffer2, "%ld", &cmp);

                if (from < 0 || to < 0) {
                    Debug("Couldn't read range\n");
                    return -1;
                }

                if ((from > cmp) || (cmp > to)) {
                    Debug("Out of range %d > %d > %d (range %s)\n",
                            from, cmp, to, buffer2);
                    return -1;
                }
            } else {
                sscanf(buffer1, "%ld", &from);
                sscanf(buffer2, "%ld", &cmp);

                if (from != cmp) {
                    Debug("Unequal\n");
                    return -1;
                }
            }

            Debug("Matched octet %s with %s\n", buffer1, buffer2);
        }

        Debug("Matched IP range\n");
        return 0;
        }
    }

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    if (isv6) {
        struct sockaddr_in6 addr1,addr2;
        int blocks, i;

        memset(&addr1, 0, sizeof(struct sockaddr_in6));
        memset(&addr2, 0, sizeof(struct sockaddr_in6));

    if (isCIDR) {
        address[0] = '\0';
        mask = 0;
        sscanf(s1, "%40[^/]/%d", address,&mask);
        blocks = mask/8;

        if (mask % 8 != 0) {
            CfLog(cferror,"Cannot handle ipv6 masks which are "
                    "not 8 bit multiples (fix me)", "");
            return -1;
        }

        bcopy((struct sockaddr_in6 *) sockaddr_pton(AF_INET6,address),
                &addr1, sizeof(struct sockaddr_in6));
        bcopy((struct sockaddr_in6 *) sockaddr_pton(AF_INET6,s2),
                &addr2, sizeof(struct sockaddr_in6));

        /* blocks < 16 */
        for (i = 0; i < blocks; i++) {
            if (addr1.sin6_addr.s6_addr[i] != addr2.sin6_addr.s6_addr[i]) {
                return -1;
            }
        }
        return 0;
    } else {
        long i, from = -1, to = -1, cmp = -1;
        char *sp1, *sp2;
        char buffer1[16], buffer2[16];

        sp1 = s1;
        sp2 = s2;

        for (i = 0; i < 8; i++) {
            sscanf(sp1, "%[^:]", buffer1);
            sp1 += strlen(buffer1)+1;
            sscanf(sp2, "%[^:]", buffer2);
            sp2 += strlen(buffer2)+1;

            if (strstr(buffer1, "-")) {
                sscanf(buffer1, "%lx-%lx", &from, &to);
                sscanf(buffer2, "%lx", &cmp);

                if (from < 0 || to < 0) {
                    return -1;
                }

                if ((from >= cmp) || (cmp > to)) {
                    printf("%x < %x < %x\n", from, cmp, to);
                    return -1;
                }
            } else {
                sscanf(buffer1, "%ld", &from);
                sscanf(buffer2, "%ld", &cmp);

                if (from != cmp) {
                    return -1;
                }
            }
        }
        return 0;
        }
    }
#endif

    return -1;
}


/*
 * --------------------------------------------------------------------
 * Host Match
 * written by steve rader <rader@hep.wisc.edu>
 * --------------------------------------------------------------------
 */

/*
 * do something like...
 *   if ( $s !~ /^.*?\,\d+\-\d+$/ ) { return false; }
 *   return true;
 */
int FuzzyHostParse(char *s)
{
    struct Item *args;
    char *sp;
    long start = -1, end = -1, where = -1;
    int n;

    Debug("SRDEBUG in FuzzyHostParse(): %s\n", s);
    args = SplitStringAsItemList(s, ',');

    if ( args->next == NULL ) {
        yyerror("HostRange() syntax error: not enough args (expecting two)");
        return false;
    }
    if ( args->next->next != NULL ) {
        yyerror("HostRange() syntax error: too many args (expecting two)");
        return false;
    }

    sp = args->next->name;
    n = sscanf(sp, "%ld-%ld%n", &start, &end, &where);
    Debug("SRDEBUG start=%d end=%d num_matched=%d\n", start, end, n);

    if ( n >= 2 && sp[where] != '\0' ) {

        /* X-Ycrud syntax error */
        yyerror("HostRange() syntax error: second arg "
                "should have X-Y format where X and Y are decimal numbers");

        return false;
    }

    if ( n != 2 ) {

        /* all other syntax errors */
        yyerror("HostRange() syntax error: second arg "
                "should have X-Y format where X and Y are decimal numbers");

        return false;
    }

    Debug("SRDEBUG syntax is okay\n");
    return true;
}

/* ----------------------------------------------------------------- */

/*
 * do something like...
 *   @args = split(/,/,$s1);
 *   if ( $s2 !~ /(\d+)$/ ) { 
 *     return 1; # failure: no num in hostname
 *   }
 *   if ( $1 < $args[1] || $1 > $args[2] ) { 
 *     return 1; # failure: hostname num not in range
 *   }
 *   $s2 =~ s/^(.*?)\d.*$/$1/;
 *   if ( $s2 ne $args[0] ) {
 *     return 1; # failure: hostname doesn't match basename
 *   }
 *   return 0; # success
 */

int FuzzyHostMatch(char *s1, char *s2)
{
    struct Item *args;
    char *sp;
    long cmp = -1, start = -1, end = -1;
    Debug("SRDEBUG in FuzzyHostMatch(): %s vs %s\n",s2,s1);
    args = SplitStringAsItemList(s1, ',');
    sp = s2;

    for (sp = s2+strlen(s2)-1; sp > s2; sp--) {
        if ( ! isdigit((int)*sp) ) {
            sp++;
            if ( sp != s2+strlen(s2) ) {
                Debug("SRDEBUG extracted string %s\n", sp);
            }
            break;
        }
    }

    if ( sp == s2+strlen(s2) ) { 
        Debug("SRDEBUG FuzzyHostMatch() failed: did not extract "
                "int from the end of %s\n", s2);
        return 1;
    }

    sscanf(sp, "%ld", &cmp);
    Debug("SRDEBUG extracted int %d\n", cmp, sp);

    if ( cmp < 0 ) {
        Debug("SRDEBUG FuzzyHostMatch() failed: %s doesn't have "
                "an int in it's domain name\n", s2);
        return -1;
    }
    sscanf(args->next->name, "%ld-%ld", &start, &end);

    if ( cmp < start || cmp > end ) { 
        Debug("SRDEBUG FuzzyHostMatch() failed: %ld is not in "
                "(%ld..%ld)\n", cmp, start, end);
         return 1;
     }

    Debug("SRDEBUG FuzzyHostMatch() %s is in (%ld..%ld)\n", s2, start, end);
  
    for (sp = s2; sp < s2+strlen(s2); sp++ ) {
        if ( isdigit((int)*sp) ) {
            *sp = '\0';
            break;
        }
    }

    Debug("SRDEBUG extracted basename %s\n", s2);
    Debug("SRDEBUG basename check: %s vs %s...\n", s2, args->name);
  
    if ( strcmp(s2,args->name) != 0 ) {
         Debug("SRDEBUG FuzzyHostMatch() failed: basename %s "
                 "does not match %s\n", s2, args->name);
         return 1;
    }

    Debug("SRDEBUG basename matches\n");
    Debug("SRDEBUG FuzzyHostMatch() succeeded\n");
    return 0;
}


/* 
 * --------------------------------------------------------------------
 * String handling
 * --------------------------------------------------------------------
 */

/* 
 * Splits a string containing a separator like : into a linked list of
 * separate items
 */
struct Item *
SplitStringAsItemList(char *string, char sep)
{
    struct Item *liststart = NULL;
    char *sp;
    char format[9]; 
    char node[CF_MAXVARSIZE];

    Debug("SplitStringAsItemList(%s,%c)\n", string, sep);

    /* set format string to search */
    sprintf(format, "%%255[^%c]", sep);

    for (sp = string; *sp != '\0'; sp++) {
        memset(node, 0, CF_MAXVARSIZE);
        sscanf(sp, format, node);

        if (strlen(node) == 0) {
            continue;
        }

        sp += strlen(node)-1;

        AppendItem(&liststart, node, NULL);

        if (*sp == '\0') {
            break;
        }
    }

    return liststart;
}

/* ----------------------------------------------------------------- */


/* 
 * Borrowed this algorithm from merge-sort implementation 
 *
 * Alphabetical:
 */

struct Item *
SortItemListNames(struct Item *list) 
{ 
    struct Item *p, *q, *e, *tail, *oldhead;
    int insize, nmerges, psize, qsize, i;

    if (list == NULL) { 
        return NULL;
    }
 
    insize = 1;
 
    while (true) {

        p = list;

        /* only used for circular linkage */
        oldhead = list;
        list = NULL;
        tail = NULL;
        
        /* count number of merges we do in this pass */
        nmerges = 0;
    
        while (p) {

            /* there exists a merge to be done */
            nmerges++;

            /* step `insize' places along from p */
            q = p;
            psize = 0;
       
            for (i = 0; i < insize; i++) {
                psize++;
                q = q->next;

                if (!q) {
                    break;
                }
            }
       
            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;
       
            /* now we have two lists; merge them */
            while (psize > 0 || (qsize > 0 && q)) {          

                /* decide whether next element of merge comes from p or q */
                if (psize == 0) {

                    /* p is empty; e must come from q. */
                    e = q; q = q->next; qsize--;
                } else if (qsize == 0 || !q) {

                    /* q is empty; e must come from p. */
                    e = p; p = p->next; psize--;

                } else if (strcmp(p->name, q->name) <= 0) {
                    /* First element of p is lower (or same);
                    * e must come from p. */
                    e = p; p = p->next; psize--;
                } else {
                    /* First element of q is lower; e must come from q. */
                    e = q; q = q->next; qsize--;
                }
          
                /* add the next element to the merged list */
                if (tail) {
                    tail->next = e;
                } else {
                    list = e;
                }
                tail = e;
            }
       
            /* now p has stepped `insize' places along, and q has too */
            p = q;
        }
        tail->next = NULL;
    
        /* If we have done only one merge, we're finished. */

        /* allow for nmerges==0, the empty list case */
        if (nmerges <= 1) {
            return list;
        }

        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }
}

/* ----------------------------------------------------------------- */

/* 
 * Borrowed this algorithm from merge-sort implementation 
 *
 * Biggest first:
 */
struct Item *
SortItemListCounters(struct Item *list) 
{ 
    struct Item *p, *q, *e, *tail, *oldhead;
    int insize, nmerges, psize, qsize, i;

    if (list == NULL) { 
        return NULL;
    }
    
    insize = 1;
 
    while (true) {
        p = list;
        oldhead = list;     /* only used for circular linkage */
        list = NULL;
        tail = NULL;
        
        nmerges = 0;        /* count number of merges we do in this pass */
    
        while (p) {
            nmerges++;  /* there exists a merge to be done */
            /* step `insize' places along from p */
            q = p;
            psize = 0;
       
            for (i = 0; i < insize; i++) {
                psize++;
                q = q->next;

                if (!q) {
                    break;
                }
            }
       
            /* if q hasn't fallen off end, we have two lists to merge */
            qsize = insize;
       
            /* now we have two lists; merge them */
            while (psize > 0 || (qsize > 0 && q)) {          
                /* decide whether next element of merge comes from p or q */
                if (psize == 0) {
                    /* p is empty; e must come from q. */
                    e = q; q = q->next; qsize--;
                } else if (qsize == 0 || !q) {
                    /* q is empty; e must come from p. */
                    e = p; p = p->next; psize--;
                } else if (p->counter > q->counter) {
                    /* First element of p is higher (or same);
                    * e must come from p. */
                    e = p; p = p->next; psize--;
                } else {
                    /* First element of q is lower; e must come from q. */
                    e = q; q = q->next; qsize--;
                }
                
                /* add the next element to the merged list */
                if (tail) {
                    tail->next = e;
                } else {
                    list = e;
                }
                tail = e;
            }
       
            /* now p has stepped `insize' places along, and q has too */
            p = q;
        }
        tail->next = NULL;
    
        /* If we have done only one merge, we're finished. */

        /* allow for nmerges==0, the empty list case */
        if (nmerges <= 1) {
            return list;
        }

        /* Otherwise repeat, merging lists twice the size */
        insize *= 2;
    }
}
