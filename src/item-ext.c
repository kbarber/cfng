/*
 * $Id: item-ext.c 713 2004-05-21 20:22:06Z skaar $
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
 * Toolkit: the "item extension" object library for cfengine
 *
 * The functions in this file are mostly used by "editfiles" commands;
 * DoEditFile() may need to reset more globals before returning for
 *  them to always work as expected. 
 */

#include "cf.defs.h"
#include "cf.extern.h"


/* ----------------------------------------------------------------- */

/* Splits a string with quoted components etc */
struct Item *
ListFromArgs(char *string)
{
    struct Item *ip = NULL;
    int dquote_level = 0,i = 0;
    int paren_level = 0;
    char *sp;
    char lastch = '\0';
    char item[CF_BUFSIZE];

    memset(item, 0, CF_BUFSIZE);

    for (sp = string; *sp != '\0'; sp++) {
        switch (*sp) {
        case '\"':
            /* Escaped quote */
            if (lastch == '\\') {
                sp--;
                *sp = ')';
                continue;
            } else {
                if (dquote_level == 0) {
                    dquote_level = 1;
                    continue;
                } else {
                    dquote_level = 0;
                    continue;
                }
            }
            break;

        case '(':
            paren_level++;
            break;

        case ')':
            paren_level--;
            break;

        case ',':
            if (dquote_level == 0 && paren_level == 0) {
                item[i] = '\0';
                i = 0;
                AppendItem(&ip, item, "");
                memset(item, 0, CF_BUFSIZE);
                continue;
            }
        }

        item[i++] = *sp;
        lastch = *sp;
    }

    AppendItem(&ip, item, "");
    return ip;
}

/* ----------------------------------------------------------------- */

int
OrderedListsMatch(struct Item *list1, struct Item *list2)
{
    struct Item *ip1, *ip2;

    for (ip1 = list1, ip2 = list2; (ip1!=NULL) && (ip2!=NULL);
            ip1 = ip1->next, ip2 = ip2->next) {

        if (strcmp(ip1->name, ip2->name) != 0) {
            Debug("OrderedListMatch failed on (%s,%s)\n",
                    ip1->name, ip2->name);
            return false;
        }
    }

    if (ip1 != ip2) {
        return false;
    }

    return true;
}


/* ----------------------------------------------------------------- */

int
IsClassedItemIn(struct Item *list, char *item)
{
    struct Item *ptr;

    if ((item == NULL) || (strlen(item) == 0)) {
        return true;
    }

    for (ptr = list; ptr != NULL; ptr = ptr->next) {
        if (strcmp(ptr->name, item) == 0) {
            if (IsExcluded(ptr->classes)) {
                continue;
            }
            return(true);
        }
    }

    return(false);
}

/* ----------------------------------------------------------------- */

/* Checks whether item matches a list of wildcards */
int
IsWildItemIn(struct Item *list, char *item)
{
    struct Item *ptr;

    for (ptr = list; ptr != NULL; ptr = ptr->next) {
        if (IsExcluded(ptr->classes)) {
            continue;
        }

        if (WildMatch(ptr->name, item)) {
            Debug("IsWildItem(%s,%s)\n", item, ptr->name);
            return(true);
        }
    }
    return(false);
}

/* ----------------------------------------------------------------- */

void
InsertItemAfter(struct Item **filestart, struct Item *ptr, char *string)
{
    struct Item *ip;
    char *sp;

    EditVerbose("Inserting %s \n", string);

    if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL) {
        CfLog(cferror, "", "Can't allocate memory in InsertItemAfter()");
        FatalError("");
    }

    if ((sp = malloc(strlen(string)+1)) == NULL) {
        CfLog(cferror, "", "Can't allocate memory in InsertItemAfter()");
        FatalError("");
    }

    /* File is empty */
    if (g_currentlineptr == NULL) {
        if (filestart == NULL) {
            *filestart = ip;
            ip->next = NULL;
        } else {
            ip->next = (*filestart)->next;
            (*filestart)->next = ip;
        }

        strcpy(sp, string);
        ip->name = sp;
        ip->classes = NULL;
        g_currentlineptr = ip;
        g_currentlinenumber = 1;
    } else {
        ip->next = g_currentlineptr->next;
        g_currentlinenumber++;
        g_currentlineptr->next = ip;
        g_currentlineptr = ip;
        strcpy(sp, string);
        ip->name = sp;
        ip->classes = NULL;
    }

    g_numberofedits++;

    return;
}



/* ----------------------------------------------------------------- */

struct Item *
LocateNextItemContaining(struct Item *list, char *string)
{
    struct Item *ip;

    for (ip = list; ip != NULL; ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name,g_veditabort)) {
            EditVerbose("Aborting search, regex %s matches line\n",
                    g_veditabort);
            return NULL;
        }

        if (strstr(ip->name, string)) {
            return ip;
        }
    }

    return NULL;
}

/* ----------------------------------------------------------------- */

int
RegexOK(char *string)
{
    regex_t rx;

    if (CfRegcomp(&rx, string, REG_EXTENDED) != 0) {
        return false;
    }

    regfree(&rx);
    return true;
}

/* ----------------------------------------------------------------- */

struct Item *
LocateNextItemMatching(struct Item *list, char *string)
{
    struct Item *ip;
    regex_t rx,rxcache;
    regmatch_t pmatch;

    if (CfRegcomp(&rxcache, string, REG_EXTENDED) != 0) {
        return NULL;
    }

    for (ip = list; ip != NULL; ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n",
                    g_veditabort);
            regfree(&rx);
            return NULL;
        }

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache, &rx, sizeof(rx));

        if (regexec(&rx, ip->name,1, &pmatch,0) == 0) {
            if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(ip->name))) {
                regfree(&rx);
                return ip;
            }
        }
    }
    /* regfree(&rx); */
    return NULL;
}

/* ----------------------------------------------------------------- */

struct Item *
LocateNextItemStarting(struct Item *list, char *string)
{
    struct Item *ip;

    for (ip = list; (ip != NULL); ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            return NULL;
        }

        if (strncmp(ip->name, string, strlen(string)) == 0) {
            return ip;
        }
    }

    return NULL;
}

/* ----------------------------------------------------------------- */

struct Item *
LocateItemMatchingRegExp(struct Item *list, char *string)
{
    struct Item *ip;
    regex_t rx,rxcache;
    regmatch_t pmatch;
    int line = g_currentlinenumber;

    if (list != NULL) {
        Debug("LocateItemMatchingRexExp(%s,%s)\n", list->name, string);
    }

    if (CfRegcomp(&rxcache, string, REG_EXTENDED) != 0) {
        return NULL;
    }

    for (ip = list; (ip != NULL); ip = ip->next, line++) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            regfree(&rx);
            return NULL;
        }

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache, &rx, sizeof(rx));

        if (regexec(&rx, ip->name, 1, &pmatch, 0) == 0) {
            if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(ip->name))) {
                EditVerbose("Edit: Search ended at line %d\n", line);
                EditVerbose("Edit: (Found %s)\n", ip->name);
                g_currentlinenumber = line;
                g_currentlineptr = ip;
                regfree(&rx);
                return ip;
            }
        }
    }

    EditVerbose("Edit: Search for %s failed. Current line still %d\n",
            string, g_currentlinenumber);
    /* regfree(&rx); */
    return NULL;
}

/* ----------------------------------------------------------------- */

struct Item *
LocateItemContainingRegExp(struct Item *list, char *string)
{
    struct Item *ip;
    regex_t rx,rxcache;
    regmatch_t pmatch;
    int line = 1;

    if (CfRegcomp(&rxcache, string, REG_EXTENDED) != 0) {
        return NULL;
    }

    for (ip = list; (ip != NULL); ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            regfree(&rx);
            return NULL;
        }

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache, &rx, sizeof(rx));

        if (regexec(&rx, ip->name, 1, &pmatch, 0) == 0) {
            EditVerbose("Search ended at line %d\n", line);
            g_currentlinenumber = line;
            g_currentlineptr = ip;
            regfree(&rx);
            return ip;
        }
    }

    EditVerbose("Search for %s failed. Current line still %d\n",
            string, g_currentlinenumber);

    /* regfree(&rx);*/
    return NULL;
}

/* ----------------------------------------------------------------- */

/* Delete up to but not including a line matching the regex */
int
DeleteToRegExp(struct Item **filestart, char *string)
{
    struct Item *ip, *ip_prev, *ip_end = NULL;
    int linefound = false;
    regex_t rx,rxcache;
    regmatch_t pmatch;

    Debug2("DeleteToRegExp(list,%s)\n", string);

    /* Shouldn't happen */
    if (g_currentlineptr == NULL) {
        CfLog(cferror, "File line-pointer undefined during "
                "editfile action\n","");
        return true;
    }

    if (CfRegcomp(&rxcache, string, REG_EXTENDED) != 0) {
        return false;
    }

    for (ip = g_currentlineptr; (ip != NULL); ip=ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            regfree(&rx);
            return false;
        }

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache, &rx, sizeof(rx));

        if (regexec(&rx, ip->name, 1, &pmatch, 0) == 0) {
            if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(ip->name))) {
                linefound = true;
                ip_end = ip;
                break;
            }
        }
    }

    if (! linefound)
    {
    return false;
    }


    for (ip_prev = *filestart; ip_prev != g_currentlineptr &&
            ip_prev->next != g_currentlineptr; ip_prev = ip_prev->next) { }

    for (ip = g_currentlineptr; ip != NULL; ip = g_currentlineptr) {
        if (ip->name == NULL) {
            continue;
        }

        if (ip == ip_end) {
            EditVerbose("Edit: terminating line: %s (Done)\n", ip->name);
            return true;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            regfree(&rx);
            return false;
        }

        EditVerbose("Edit: delete line %s\n", ip->name);
        g_numberofedits++;
        g_currentlineptr = ip->next;

        if (ip == *filestart) {
            *filestart = ip->next;
        } else {
            ip_prev->next = ip->next;
        }

        free (ip->name);
        free ((char *)ip);
    }

    /* regfree(&rx); */
    return true;
}

/* ----------------------------------------------------------------- */

/* 
 * DeleteItem* function notes:
 * - They all take an item list and an item specification ("string"
 *   argument.)
 * - Some of them treat the item spec as a literal string, while others
 *   treat it as a regular expression.
 * - They all delete the first item meeting their criteria, as below.
 *   function deletes item
 * --------------------------------------------------------------------
 * DeleteItemStarting      start is literally equal to string item spec
 * DeleteItemLiteral       literally equal to string item spec
 * DeleteItemMatching      fully matched by regex item spec
 * DeleteItemContaining    containing string item spec
 */

/* ----------------------------------------------------------------- */

int
DeleteItemGeneral(struct Item **list, char *string, enum matchtypes type)
{
    struct Item *ip,*last = NULL;
    int match = 0, matchlen = 0;
    regex_t rx, rxcache;
    regmatch_t pmatch;

    if (list == NULL) {
        return false;
    }

    switch (type) {
    case literalStart:
        matchlen = strlen(string);
        break;
    case regexComplete:
    case NOTregexComplete:
        if (CfRegcomp(&rxcache, string, REG_EXTENDED) != 0) {
            return false;
        }
        break;
    }

    for (ip = *list; ip != NULL; ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            return false;
        }

        switch(type) {
        case NOTliteralStart:
            match = (strncmp(ip->name, string, matchlen) != 0);
            break;
        case literalStart:
            match = (strncmp(ip->name, string, matchlen) == 0);
            break;
        case NOTliteralComplete:
            match = (strcmp(ip->name, string) != 0);
            break;
        case literalComplete:
            match = (strcmp(ip->name, string) == 0);
            break;
        case NOTliteralSomewhere:
            match = (strstr(ip->name, string) == NULL);
            break;
        case literalSomewhere:
            match = (strstr(ip->name, string) != NULL);
            break;
        case NOTregexComplete:
        case regexComplete:
            /* To fix a bug on some implementations where rx gets emptied */
            bcopy(&rxcache, &rx, sizeof(rx));
            match = (regexec(&rx, ip->name, 1, &pmatch, 0) == 0) &&
                (pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(ip->name));

            if (type == NOTregexComplete) {
                match = !match;
            }
            break;
        }

        if (match) {
            if (type == regexComplete || type == NOTregexComplete) {
                regfree(&rx);
            }

            EditVerbose("Deleted item %s\n",ip->name);

            if (ip == *list) {
                free((*list)->name);
                if (ip->classes != NULL) {
                    free(ip->classes);
                }
                *list = ip->next;
                free((char *)ip);

                g_numberofedits++;
                return true;
            } else {
                if (ip != NULL) {
                    if (last != NULL) {
                        last->next = ip->next;
                    }
                    free(ip->name);
                    if (ip->classes != NULL) {
                        free(ip->classes);
                    }
                    free((char *)ip);
                }
                g_numberofedits++;
                return true;
            }
        }
        last = ip;
    }
    return false;
}

/* ----------------------------------------------------------------- */

/* delete 1st item starting with string */
int
DeleteItemStarting(struct Item **list, char *string)
{
    return DeleteItemGeneral(list, string, literalStart);
}

/* ----------------------------------------------------------------- */

/* delete 1st item starting with string */
int
DeleteItemNotStarting(struct Item **list, char *string)
{
    return DeleteItemGeneral(list, string, NOTliteralStart);
}

/* ----------------------------------------------------------------- */

/* delete 1st item which is string */
int
DeleteItemLiteral(struct Item **list, char *string)
{
    return DeleteItemGeneral(list, string, literalComplete);
}

/* ----------------------------------------------------------------- */

/* delete 1st item fully matching regex */
int
DeleteItemMatching(struct Item **list, char *string)
{
    return DeleteItemGeneral(list, string, regexComplete);
}

/* ----------------------------------------------------------------- */

/* delete 1st item fully matching regex */
int
DeleteItemNotMatching(struct Item **list, char *string)
{
    return DeleteItemGeneral(list, string, NOTregexComplete);
}

/* ----------------------------------------------------------------- */

/* delete first item containing string */
int
DeleteItemContaining(struct Item **list, char *string)
{
    return DeleteItemGeneral(list, string, literalSomewhere);
}

/* ----------------------------------------------------------------- */

/* delete first item containing string */
int
DeleteItemNotContaining(struct Item **list, char *string)
{
    return DeleteItemGeneral(list, string, NOTliteralSomewhere);
}


/* ----------------------------------------------------------------- */

int
CommentItemStarting(struct Item **list, char *string, char *comm, char *end)
{
    struct Item *ip;
    char buff[CF_BUFSIZE];

    for (ip = *list; ip != NULL; ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            return false;
        }

        if (strncmp(ip->name, string, strlen(string)) == 0) {
            if (strlen(ip->name) + strlen(comm) + strlen(end) 
                    + 2 > CF_BUFSIZE) {
                CfLog(cferror,
                        "Bufsize overflow while commenting line - abort\n","");
                return false;
            }

            if (strncmp(ip->name, comm, strlen(comm))== 0) {
                continue;
            }

            EditVerbose("Commenting %s%s%s\n", comm, ip->name, end);

            snprintf(buff, CF_BUFSIZE, "%s%s%s", comm, ip->name,end);
            free(ip->name);

            if ((ip->name = malloc(strlen(buff)+1)) == NULL) {
                CfLog(cferror, "malloc in CommentItemStarting\n", "");
                FatalError("");;
            }

            strcpy(ip->name, buff);
            g_numberofedits++;

            return true;
        }
    }
    return false;
}

/* ----------------------------------------------------------------- */

int
CommentItemContaining(struct Item **list, char *string, char *comm, char *end)
{
    struct Item *ip;
    char buff[CF_BUFSIZE];

    for (ip = *list; ip != NULL; ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            return false;
        }

        if (strstr(ip->name,string)) {
            if (strlen(ip->name) + strlen(comm) 
                    + strlen(end) + 2 > CF_BUFSIZE) {
                CfLog(cferror,
                        "Bufsize overflow while commenting line - abort\n","");
                return false;
            }

            if (strncmp(ip->name, comm, strlen(comm))== 0) {
                continue;
            }

            EditVerbose("Commenting %s%s%s\n", comm, ip->name, end);

            snprintf(buff, CF_BUFSIZE, "%s%s%s", comm, ip->name, end);
            free(ip->name);

            if ((ip->name = malloc(strlen(buff)+1)) == NULL) {
                CfLog(cferror, "malloc in CommentItemContaining\n", "");
                FatalError("");;
            }

            strcpy(ip->name,buff);
            g_numberofedits++;

            return true;
        }
    }
    return false;
}

/* ----------------------------------------------------------------- */

int
CommentItemMatching(struct Item **list, char *string, char *comm, char *end)
{
    struct Item *ip;
    char buff[CF_BUFSIZE];
    regex_t rx,rxcache;
    regmatch_t pmatch;

    if (CfRegcomp(&rxcache, string, REG_EXTENDED) != 0) {
        return false;
    }

    for (ip = *list; ip != NULL; ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            regfree(&rx);
            return false;
        }

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache, &rx, sizeof(rx));

        if (regexec(&rx, ip->name, 1, &pmatch, 0) == 0) {
            if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(ip->name))) {

                if (strlen(ip->name) + strlen(comm) 
                        + strlen(end)+2 > CF_BUFSIZE) {

                    CfLog(cferror, "Bufsize overflow while commenting "
                            "line - abort\n","");

                    regfree(&rx);
                    return false;
                }

                /* Already commented */
                if (strncmp(ip->name, comm, strlen(comm)) == 0) {
                    continue;
                }

                EditVerbose("Commenting %s%s%s\n", comm, ip->name, end);

                snprintf(buff,CF_BUFSIZE, "%s%s%s", comm, ip->name, end);
                free(ip->name);

                if ((ip->name = malloc(strlen(buff)+1)) == NULL) {
                    CfLog(cferror, "malloc in CommentItemContaining\n ", "");
                    FatalError("");;
                }

                strcpy(ip->name,buff);
                g_numberofedits++;

                regfree(&rx);
                return true;
            }
        }
    }
    /* regfree(&rx); */
    return false;
}

/* ----------------------------------------------------------------- */

int
UnCommentItemMatching(struct Item **list, char *string, char *comm, char *end)
{
    struct Item *ip;
    char *sp, *sp1, *sp2, *spc;
    regex_t rx, rxcache;
    regmatch_t pmatch;

    if (CfRegcomp(&rxcache, string, REG_EXTENDED) != 0) {
        snprintf(g_output, CF_BUFSIZE, "Failed to compile expression %s",
                string);
        CfLog(cferror, g_output, "");
        return false;
    }

    for (ip = *list; ip != NULL; ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache, &rx, sizeof(rx));

        if (regexec(&rx, ip->name, 1, &pmatch, 0) == 0) {
            if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(ip->name))) {
                if (strlen(ip->name) + strlen(comm) 
                        + strlen(end) + 2 > CF_BUFSIZE) {

                    CfLog(cferror, "Bufsize overflow while commenting "
                            "line - abort\n","");

                    regfree(&rx);
                    return false;
                }

                if (strstr(ip->name, comm) == NULL) {
                    g_currentlineptr = ip->next;
                    continue;
                }

                EditVerbose("Uncomment line %s\n", ip->name);
                g_currentlineptr = ip->next;

                if ((sp = malloc(strlen(ip->name) + 2)) == NULL) {
                    CfLog(cferror, "No Memory in UnCommentNLines\n", "malloc");
                    regfree(&rx);
                    return false;
                }

                spc = sp;

                for (sp1 = ip->name; isspace((int)*sp1); sp1++) {
                    *spc++ = *sp1;
                }

                *spc = '\0';

                sp2 = ip->name + strlen(ip->name);

                if ((strlen(end) != 0) && (strstr(ip->name,end) != NULL)) {

                    for (sp2 = ip->name + strlen(ip->name);
                            strncmp(sp2, end, strlen(end)) != 0; sp2--) { }

                    *sp2 = '\0';
                }

                strcat(sp, sp1 + strlen(comm));

                if (sp2 != ip->name + strlen(ip->name)) {
                    strcat(sp, sp2 + strlen(end));
                }

                if (strcmp(sp, ip->name) != 0) {
                    g_numberofedits++;
                }

                free(ip->name);
                ip->name = sp;
                regfree(&rx);
                return true;
            }
        }
    }
    /* regfree(&rx); */
    return false;
}

/* ----------------------------------------------------------------- */

int
UnCommentItemContaining(struct Item **list, char *string, char *comm, 
        char *end)
{
    struct Item *ip;
    char *sp, *sp1, *sp2, *spc;

    for (ip = *list; ip != NULL; ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (strstr(ip->name, string)) {
            if (strstr(ip->name, comm) == NULL) {
                g_currentlineptr = ip->next;
                continue;
            }

            EditVerbose("Uncomment line %s\n", ip->name);
            g_currentlineptr = ip->next;

            if ((sp = malloc(strlen(ip->name) + 2)) == NULL) {
                CfLog(cferror, "No memory in UnCommentNLines\n", "malloc");
                return false;
            }

            spc = sp;

            for (sp1 = ip->name; isspace((int)*sp1); sp1++) {
                *spc++ = *sp1;
            }

            *spc = '\0';

            sp2 = ip->name+strlen(ip->name);

            if ((strlen(end) != 0) && (strstr(ip->name, end) != NULL)) {
                for (sp2 = ip->name + strlen(ip->name);
                        strncmp(sp2, end, strlen(end)) != 0; sp2--) { }

                *sp2 = '\0';
            }

            strcat(sp, sp1 + strlen(comm));

            if (sp2 != ip->name + strlen(ip->name)) {
                strcat(sp, sp2 + strlen(end));
            }

            if (strcmp(sp, ip->name) != 0) {
                g_numberofedits++;
            }

            free(ip->name);
            ip->name = sp;
            return true;
        }
    }

    return false;
}


/* ----------------------------------------------------------------- */

/* Delete up to and including a line matching the regex */
int
CommentToRegExp(struct Item **filestart, char *string, char *comm, char *end)
{
    struct Item *ip, *ip_end = NULL;
    int linefound = false, done;
    char *sp;
    regex_t rx,rxcache;
    regmatch_t pmatch;

    Debug2("CommentToRegExp(list,%s %s)\n", comm, string);

    /* Shouldn't happen */
    if (g_currentlineptr == NULL) {
        CfLog(cferror, "File line-pointer undefined during editfile "
                "action\n","");
        return true;
    }

    if (CfRegcomp(&rxcache, string, REG_EXTENDED) != 0) {
        return false;
    }

    for (ip = g_currentlineptr; (ip != NULL); ip = ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            regfree(&rx);
            return false;
        }

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache, &rx, sizeof(rx));
        if (regexec(&rx, ip->name, 1, &pmatch, 0) == 0) {
            if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(ip->name))) {
                linefound = true;
                ip_end = ip;
                break;
            }
        }
    }

    if (! linefound) {
        return false;
    }

    done = false;

    for (ip = g_currentlineptr; ip != NULL; ip = g_currentlineptr) {
        if (ip == ip_end) {
            EditVerbose("Terminating line: %s (Done)\n", ip->name);
            done = true;
        }

        EditVerbose("Comment line %s%s%s\n", comm, ip->name, end);
        g_numberofedits++;
        g_currentlineptr = ip->next;

        if ((sp = malloc(strlen(ip->name) + strlen(comm) 
                        + strlen(end) + 2)) == NULL) {

            CfLog(cferror, "No memory in CommentToRegExp\n", "malloc");
            regfree(&rx);
            return false;
        }

        strcpy (sp, comm);
        strcat (sp, ip->name);
        strcat (sp, end);

        free (ip->name);
        ip->name = sp;

        if (done) {
            break;
        }
    }

    /* regfree(&rx); */
    return true;
}

/* ----------------------------------------------------------------- */

/* Deletes up to N lines from current position */

int
DeleteSeveralLines(struct Item **filestart, char *string)
{
    struct Item *ip, *ip_prev;
    int ctr, N = -99, done = false;

    Debug2("DeleteNLines(list,%s)\n", string);

    sscanf(string, "%d", &N);

    if (N < 1) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Illegal number value in DeleteNLines: %s\n", string);
        CfLog(cferror, g_output, "");
        return false;
    }

    /* Shouldn't happen */
    if (g_currentlineptr == NULL) {
        CfLog(cferror,
                "File line-pointer undefined during editfile action\n","");
        return true;
    }

    for (ip_prev = *filestart;
            ip_prev && ip_prev->next != g_currentlineptr;
            ip_prev = ip_prev->next) { }

    ctr = 1;

    for (ip = g_currentlineptr; ip != NULL; ip = g_currentlineptr) {
        if (ip->name == NULL) {
            continue;
        }

        if (g_edabortmode && ItemMatchesRegEx(ip->name, g_veditabort)) {
            Verbose("Aborting search, regex %s matches line\n", g_veditabort);
            return false;
        }

        if (ctr == N) {
            EditVerbose("Terminating line: %s (Done)\n", ip->name);
            done = true;
        }

        EditVerbose("Delete line %s\n", ip->name);
        g_numberofedits++;
        g_currentlineptr = ip->next;

        if (ip_prev == NULL) {
            *filestart = ip->next;
        } else {
            ip_prev->next = ip->next;
        }

        free (ip->name);
        free ((char *)ip);
        ctr++;

        if (done) {
            break;
        }
    }

    if (ctr-1 < N) {
        snprintf(g_output, CF_BUFSIZE*2,
                "DeleteNLines deleted only %d lines (not %d)\n", ctr-1, N);
        CfLog(cfsilent, g_output, "");
    }

    return true;
}

/* ----------------------------------------------------------------- */

struct Item *
GotoLastItem(struct Item *list)
{
    struct Item *ip;

    g_currentlinenumber = 1;
    g_currentlineptr = list;

    for (ip = list; ip != NULL && ip->next != NULL; ip = ip->next) {
        g_currentlinenumber++;
    }

    g_currentlineptr = ip;

    return ip;
}

/* ----------------------------------------------------------------- */

int
LineMatches (char *line, char *regexp)
{
    regex_t rx, rxcache;
    regmatch_t pmatch;

    if (CfRegcomp(&rxcache, regexp, REG_EXTENDED) != 0) {
        return false;
    }

    /* To fix a bug on some implementations where rx gets emptied */
    bcopy(&rxcache, &rx, sizeof(rx));

    if (regexec(&rx, line, 1, &pmatch, 0) == 0) {
        /* Exact match of whole line */

        if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(line))) {
            regfree(&rx);
            return true;
        }
    }

    regfree(&rx);
    return false;
}

/* ----------------------------------------------------------------- */

int
GlobalReplace(struct Item **liststart, char *search, char *replace)
{
    int i;
    char *sp, *start = NULL;
    struct Item *ip;
    struct Item *oldCurrentLinePtr;
    regex_t rx, rxcache;
    regmatch_t match, matchcheck;

    EditVerbose("Checking for replace/%s/%s\n", search, replace);

    if (CfRegcomp(&rxcache, search, REG_EXTENDED) != 0) {
        return false;
    }

    for (ip = *liststart; ip != NULL; ip=ip->next) {
        if (ip->name == NULL) {
            continue;
        }

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache, &rx, sizeof(rx));

        if (regexec(&rx, ip->name, 1, &match, 0) == 0) {
            start = ip->name + match.rm_so;
        } else {
            continue;
        }

        memset(g_vbuff, 0, CF_BUFSIZE);

        i = 0;

        for (sp = ip->name; *sp != '\0'; sp++) {
            if (sp != start) {
                g_vbuff[i] = *sp;
            } else {
                sp += match.rm_eo - match.rm_so - 1;
                g_vbuff[i] = '\0';
                strcat(g_vbuff, replace);
                i += strlen(replace)-1;

                /* 
                 * To fix a bug on some implementations where rx gets
                 * emptied.
                 */
                bcopy(&rxcache, &rx, sizeof(rx));

                if (regexec(&rx, sp, 1, &match, 0) == 0) {
                    start = sp + match.rm_so;
                } else {
                    start = 0;
                }
            }

        i++;
        }

        Debug("Replace:\n  (%s)\nwith      (%s)\n", ip->name, g_vbuff);

        if (regexec(&rx, g_vbuff, 1, &matchcheck, 0) == 0) {

            snprintf(g_output ,CF_BUFSIZE*2, 
                    "WARNING: Non-convergent edit operation "
                    "ReplaceAll [%s] With [%s]", replace, search);

            CfLog(cferror, g_output, "");
            snprintf(g_output, CF_BUFSIZE*2, "Line begins [%.40s]", ip->name);
            CfLog(cferror, g_output, "");

            CfLog(cferror, "Replacement matches search string "
                    "and will thus replace every time - edit was not done", "");

            return false;
        }

        g_currentlineptr = ip;
        InsertItemAfter(liststart, ip, g_vbuff);

        /* set by Insert */
        oldCurrentLinePtr = g_currentlineptr;

        DeleteItem(liststart, ip);
        ip = oldCurrentLinePtr;
    }

    regfree(&rxcache);
    return true;
}


/* ----------------------------------------------------------------- */

/* Comments up to N lines from current position */
int
CommentSeveralLines(struct Item **filestart, char *string, 
        char *comm, char *end)
{
    struct Item *ip;
    int ctr, N = -99, done = false;
    char *sp;

    Debug2("CommentNLines(list,%s)\n", string);

    sscanf(string, "%d", &N);

    if (N < 1) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Illegal number value in CommentNLines: %s\n", string);
        CfLog(cferror, g_output, "");
        return false;
    }

    /* Shouldn't happen */
    if (g_currentlineptr == NULL) {
        snprintf(g_output, CF_BUFSIZE*2,
                "File line-pointer undefined during editfile action\n");
        CfLog(cferror, g_output, "");
        return true;
    }

    ctr = 1;

    for (ip = g_currentlineptr; ip != NULL; ip = g_currentlineptr) {
        if (ip->name == NULL) {
            continue;
        }

        if (ctr > N) {
            break;
        }

        if (ctr == N) {
            EditVerbose("Terminating line: %s (Done)\n", ip->name);
            done = true;
        }

        for (sp = ip->name; isspace((int)*sp); sp++) { }

        if (strncmp(sp, comm, strlen(comm)) == 0) {
            g_currentlineptr = ip->next;
            ctr++;
            continue;
        }

        EditVerbose("Comment line %s\n", ip->name);
        g_numberofedits++;
        g_currentlineptr = ip->next;

        if ((sp = malloc(strlen(ip->name) + strlen(comm) 
                        + strlen(end) + 2)) == NULL) {

            CfLog(cferror, "No memory in CommentNLines\n", "malloc");
            return false;
        }

        strcpy (sp, comm);
        strcat (sp, ip->name);
        strcat (sp, end);

        free (ip->name);
        ip->name = sp;
        ctr++;

        if (done) {
            break;
        }
    }

    if (ctr-1 < N) {
        snprintf(g_output, CF_BUFSIZE*2,
                "CommentNLines commented only %d lines (not %d)\n", ctr-1, N);
        CfLog(cfinform, g_output, "");
    }

    return true;
}

/* ----------------------------------------------------------------- */

/* Comments up to N lines from current position */
int
UnCommentSeveralLines(struct Item **filestart, char *string,
        char *comm, char *end)
{
    struct Item *ip;
    int ctr, N = -99, done = false;
    char *sp, *sp1, *sp2, *spc;

    Debug2("UnCommentNLines(list,%s)\n", string);

    sscanf(string, "%d", &N);

    if (N < 1) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Illegal number value in CommentNLines: %s\n", string);
        CfLog(cferror, g_output, "");
        return false;
    }


    /* Shouldn't happen */
    if (g_currentlineptr == NULL)  {
        snprintf(g_output, CF_BUFSIZE*2,
                "File line-pointer undefined during editfile action\n");
        CfLog(cferror, g_output, "");
        return true;
   }

    ctr = 1;

    for (ip = g_currentlineptr; ip != NULL; ip = g_currentlineptr) {
        if (ip->name == NULL) {
            continue;
        }

        if (ctr > N) {
            break;
        }

        if (ctr == N) {
            EditVerbose("Terminating line: %s (Done)\n", ip->name);
            done = true;
        }

        if (strstr(ip->name,comm) == NULL) {
            g_currentlineptr = ip->next;
            ctr++;
            continue;
        }

        EditVerbose("Uncomment line %s\n", ip->name);
        g_currentlineptr = ip->next;

        if ((sp = malloc(strlen(ip->name) + 2)) == NULL) {
            CfLog(cferror, "No memory in UnCommentNLines\n", "malloc");
            return false;
        }

        spc = sp;

        for (sp1 = ip->name; isspace((int)*sp1); sp1++) {
            *spc++ = *sp1;
        }

        *spc = '\0';

        sp2 = ip->name+strlen(ip->name);

        if ((strlen(end) != 0) && (strstr(ip->name, end) != NULL)) {

            for (sp2 = ip->name+strlen(ip->name);
                    strncmp(sp2, end, strlen(end)) != 0; sp2--) { }

            *sp2 = '\0';
        }

        strcat(sp, sp1 + strlen(comm));

        if (sp2 != ip->name + strlen(ip->name)) {
            strcat(sp, sp2 + strlen(end));
        }

        ctr++;

        if (strcmp(sp, ip->name) != 0) {
            g_numberofedits++;
        }

        free(ip->name);
        ip->name = sp;

        if (done) {
            break;
        }
    }

    if (ctr-1 < N) {
        snprintf(g_output, CF_BUFSIZE*2,
                "CommentNLines commented only %d lines (not %d)\n", ctr-1, N);
        CfLog(cfinform, g_output, "");
    }

    return true;
}

/* ----------------------------------------------------------------- */

int
ItemMatchesRegEx(char *item, char *regex)
{
    regex_t rx,rxcache;
    regmatch_t pmatch;

    Debug("ItemMatchesRegEx(%s %s)\n", item, regex);

    if (CfRegcomp(&rxcache, regex, REG_EXTENDED) != 0) {
        return true;
    }

    /* To fix a bug on some implementations where rx gets emptied */
    bcopy(&rxcache, &rx, sizeof(rx));

    if (regexec(&rx, item, 1, &pmatch, 0) == 0) {
        if ((pmatch.rm_so == 0) && (pmatch.rm_eo == strlen(item))) {
            regfree(&rx);
            return true;
        }
    }

    regfree(&rx);
    return false;
}

/* ----------------------------------------------------------------- */

void
ReplaceWithFieldMatch(struct Item **filestart,
        char *field, char *replace, char split, char *filename)
{
    struct Item *ip;
    char match[CF_BUFSIZE], linefield[CF_BUFSIZE];
    char *sp, *sps, *spe;
    int matching_field = 0, fcount, i, linenum, count = 0;

    Debug("ReplaceWithFieldMatch(%s,%s,%c)\n", field, replace, split);

    if ((replace == NULL) || (strlen(replace) == 0)) {

        EditVerbose("Ignoring empty line which doing "
                "ReplaceLinesMatchingField\n");

        return;
    }

    matching_field = atoi(field);
    memset(match,0,CF_BUFSIZE);

    fcount = 1;
    sps = spe = NULL;

    for (sp = replace; *sp != '\0'; sp++) {
        if (*sp == split) {
            if (fcount == matching_field) {
                spe = sp;
                break;
            }
            fcount++;
        }

        if ((fcount == matching_field) && (sps == NULL)) {
            sps = sp;
        }
    }

    if (fcount < matching_field) {

        CfLog(cfsilent, "File formats did not completely match "
                "in ReplaceLinesMatchingField\n","");

        snprintf(g_output, CF_BUFSIZE*2, "while editing %s\n", filename);
        CfLog(cfsilent, g_output, "");
        return;
    }

    if (spe == NULL) {
        spe = sp;
    }

    for (i = 0, sp = sps; sp != spe; i++, sp++) {
        match[i] = *sp;
    }

    Debug2("Edit: Replacing lines matching field %d == \"%s\"\n",
            matching_field, match);

    linenum = 1;

    for (ip = *filestart; ip != NULL; ip=ip->next, linenum++) {
        memset(linefield, 0, CF_BUFSIZE);
        fcount = 1;
        sps = spe = NULL;

        if (ip->name == NULL) {
            continue;
        }

        for (sp = ip->name; *sp != '\0'; sp++) {
            if (*sp == split) {
                if (fcount == matching_field) {
                    spe = sp;
                    break;
                }
                fcount++;
            }
            if ((fcount == matching_field) && (sps == NULL)) {
                    sps = sp;
            }
        }

        if (spe == NULL) {
            spe = sp;
        }

        if (sps == NULL) {
            sps = sp;
        }

        for (i = 0, sp = sps; sp != spe; i++, sp++) {
            linefield[i] = *sp;
        }

        if (strcmp(linefield, match) == 0) {
            EditVerbose("Replacing line %d (key %s)\n", linenum, match);

            count++;

            if (strcmp(replace, ip->name) == 0) {
                continue;
            }

            if (count > 1) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Several lines in %s matched key %s\n", 
                        filename, match);
                CfLog(cfsilent, g_output, "");
            }

            g_numberofedits++;

            free(ip->name);

            ip->name = (char *) malloc(strlen(replace)+1);
            strcpy(ip->name, replace);
            EditVerbose("Edit:   With (%s)\n", replace);
        }
    }
}

/* ----------------------------------------------------------------- */

void
AppendToLine(struct Item *current, char *text, char *filename)
{
    char *new;

    if (strstr(current->name,text)) {
        return;
    }

    EditVerbose("Appending %s to line %-60s...\n", text, current->name);

    new = malloc(strlen(current->name) + strlen(text) + 1);
    strcpy(new, current->name);
    strcat(new, text);
    g_numberofedits++;

    free(current->name);
    current->name = new;
}

/* ----------------------------------------------------------------- */

int
CfRegcomp(regex_t *preg, const char *regex, int cflags)
{
    int code;
    char buf[CF_BUFSIZE];

    code = regcomp(preg, regex, cflags);

    if (code != 0) {
        snprintf(buf, CF_BUFSIZE, "Regular expression error %d for %s\n",
                code, regex);
        CfLog(cferror, buf, "");

        regerror(code, preg, buf, CF_BUFSIZE);
        CfLog(cferror, buf, "");
        return -1;
   }
    return 0;
}
