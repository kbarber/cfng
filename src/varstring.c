/*
 * $Id: varstring.c 748 2004-05-25 14:47:10Z skaar $
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

/*
 * Varstring: variable names
 */

char *VVNAMES[] = {
    "version",
    "faculty",
    "site",
    "host",
    "fqhost",
    "ipaddress",
    "binserver",
    "sysadm",
    "domain",
    "timezone",
    "netmask",
    "nfstype",
    "sensiblesize",
    "sensiblecount",
    "editfilesize",
    "editbinfilesize",
    "actionsequence",
    "mountpattern",
    "homepattern",
    "addclasses",
    "addinstallable",
    "schedule",
    "access",
    "class",
    "arch",
    "ostype",
    "date",
    "year",
    "month",
    "day",
    "hr",
    "min",
    "allclasses",
    "excludecopy",
    "singlecopy",
    "autodefine",
    "excludelink",
    "copylinks",
    "linkcopies",
    "repository",
    "spc",
    "tab",
    "lf",
    "cr",
    "n",
    "dblquote",
    "quote",
    "dollar",
    "repchar",
    "split",
    "underscoreclasses",
    "interfacename",
    "expireafter",
    "ifelapsed",
    "fileextensions",
    "suspiciousnames",
    "spooldirectories",
    "allowconnectionsfrom", /* nonattackers */
    "denyconnectionsfrom",
    "allowmultipleconnectionsfrom",
    "methodparameters",
    "methodname",
    "methodpeers",
    "trustkeysfrom",
    "dynamicaddresses",
    "allowusers",
    "skipverify",
    "defaultcopytype",
    "allowredefinitionof",
    "defaultpkgmgr",     /* For packages */
    NULL
};

/*
 * Toolkit: Varstring expansion
 */

int
TrueVar(char *var)
{
    char buff[CF_EXPANDSIZE];
    char varbuf[CF_MAXVARSIZE];

    if (GetMacroValue(g_contextid,var)) {
        snprintf(varbuf,CF_MAXVARSIZE,"$(%s)",var);
        ExpandVarstring(varbuf,buff,NULL);

        if (strcmp(ToLowerStr(buff),"on") == 0) {
            return true;
        }

        if (strcmp(ToLowerStr(buff),"true") == 0) {
            return true;
        }
    }
    return false;
}

/* ----------------------------------------------------------------- */

int
CheckVarID(char *var)
{
    char *sp;

    printf("Checking %s\n",var);

    for (sp = var; *sp != '\0'; sp++) {
        if (isalnum((int)*sp)) {
        } else if ((*sp == '_') || (*sp == '[') || (*sp == ']')) {
        } else {
            snprintf(g_output,CF_BUFSIZE,
                "Non identifier character (%c) in variable identifier (%s)",
                *sp,var);
            yyerror(g_output);
            return false;
        }
    }
    return true;
}

/* ----------------------------------------------------------------- */

int
IsVarString(char *str)
{
    char *sp;
    char left = 'x', right = 'x';
    int dollar = false;
    int bracks = 0, vars = 0;

    Debug1("IsVarString(%s) - syntax verify\n",str);

    for (sp = str; *sp != '\0' ; sp++) {       /* check for varitems */
        switch (*sp) {
        case '$':
            dollar = true;
            break;
        case '(':
        case '{':
            if (dollar) {
                left = *sp;
                bracks++;
            }
            break;
        case ')':
        case '}':
            if (dollar) {
                bracks--;
                right = *sp;
            }
            break;
        }

        if (left == '(' && right == ')' && dollar && (bracks == 0)) {
            vars++;
            dollar=false;
        }

        if (left == '{' && right == '}' && dollar && (bracks == 0)) {
            vars++;
            dollar = false;
        }
    }


    if (bracks != 0) {
        yyerror("Incomplete variable syntax or bracket mismatch");
        return false;
    }

    Debug("Found %d variables in (%s)\n",vars,str);
    return vars;
}


/* ----------------------------------------------------------------- */

char *
ExtractInnerVarString(char *str,char *substr)
{
    char *sp;
    char left = 'x', right = 'x';
    int dollar = false;
    int bracks = 1, vars = 0;

    Debug1("ExtractInnerVarString(%s) - syntax verify\n",str);

    memset(substr,0,CF_BUFSIZE);

    for (sp = str; *sp != '\0' ; sp++) {       /* check for varitems */
        switch (*sp) {
        case '(':
        case '{':
            bracks++;
            break;
        case ')':
        case '}':
            bracks--;
            break;
        default:
            if (isalnum((int)*sp) || (*sp != '_') ||
                    (*sp != '[')|| (*sp != ']')) {
            } else {
                yyerror("Illegal character somewhere in variable "
                        "or nested expansion");
            }
        }

        if (bracks == 0) {
            strncpy(substr,str,sp-str);
            Debug("Returning substring value %s\n",substr);
            return substr;
        }
    }

    if (bracks != 0) {
        yyerror("Incomplete variable syntax or bracket mismatch");
        return false;
    }

    return sp-1;
}


/* ----------------------------------------------------------------- */

char *
ExtractOuterVarString(char *str, char *substr)
{
    char *sp;
    char left = 'x', right = 'x';
    int dollar = false;
    int bracks = 0, vars = 0, onebrack = false;

    Debug("ExtractOuterVarString(%s) - syntax verify\n",str);

    memset(substr,0,CF_BUFSIZE);

    /* check for varitems */
    for (sp = str; *sp != '\0' ; sp++) {
        switch (*sp) {
        case '$':
            dollar = true;
        break;
        case '(':
        case '{':
            bracks++;
            onebrack = true;
            break;
        case ')':
        case '}':
            bracks--;
            break;
        }

        if (dollar && (bracks == 0) && onebrack) {
            strncpy(substr,str,sp-str+1);
            Debug("Extracted outer variable %s\n",substr);
            return substr;
        }
    }

    if (dollar == false) {
        return str; /* This is not a variable*/
    }

    if (bracks != 0) {
        yyerror("Incomplete variable syntax or bracket mismatch");
        return false;
    }

    return sp-1;
}

/* ----------------------------------------------------------------- */

int
ExpandVarstring(char *string,char buffer[CF_EXPANDSIZE],char *bserver)
{
    char *sp,*env;
    char varstring = false;
    char currentitem[CF_EXPANDSIZE],temp[CF_BUFSIZE];
    char name[CF_MAXVARSIZE],scanstr[6];
    int len;
    time_t tloc;

    memset(buffer,0,CF_EXPANDSIZE);

    if (string == 0 || strlen(string) == 0) {
        return false;
    }

    Debug("ExpandVarstring(%s)\n",string);

    /* check for varitems */
    for (sp = string; /* No exit */ ; sp++) {
        currentitem[0] = '\0';

        sscanf(sp,"%[^$]",currentitem);

        if (ExpandOverflow(buffer,currentitem)) {
            FatalError("Can't expand varstring");
        }

        strcat(buffer,currentitem);
        sp += strlen(currentitem);

        if (*sp == '$') {
            switch (*(sp+1)) {
            case '(':
                varstring = ')';
                break;
            case '{':
                varstring = '}';
                break;
            default:
                strcat(buffer,"$");
                continue;
            }
            sp++;
        }

        currentitem[0] = '\0';

        if (*sp == '\0') {
            break;
        } else {
            temp[0] = '\0';
            ExtractInnerVarString(++sp,temp);

            if (strstr(temp,"$")) {
                Debug("Nested variables");
                ExpandVarstring(temp,currentitem,"");
                CheckVarID(currentitem);

                /* $() */
                sp += 3;
            } else {
                strncpy(currentitem,temp,CF_BUFSIZE-1);
            }

            Debug("Scanning variable %s\n",currentitem);

            switch (ScanVariable(currentitem)) {
            case cfversionvar:
                if (BufferOverflow(buffer,VERSION)) {
                    FatalError("Can't expand varstring");
                }
                strcat(buffer,VERSION);
                break;
            case cffaculty:
            case cfsite:
                if (g_vfaculty[0] == '\0') {
                    yyerror("faculty/site undefined variable");
                }

                if (ExpandOverflow(buffer,g_vfaculty)) {
                    FatalError("Can't expand varstring");
                }
                strcat(buffer,g_vfaculty);
                break;

            case cfhost:
                if (strlen(g_vuqname) == 0) {
                    if (BufferOverflow(buffer,g_vdefaultbinserver.name)) {
                        FatalError("Can't expand varstring");
                    }
                    strcat(buffer,g_vdefaultbinserver.name);
                } else {
                    if (ExpandOverflow(buffer,g_vuqname)) {
                        FatalError("Can't expand varstring");
                    }
                    strcat(buffer,g_vuqname);
                }
                break;

            case cffqhost:
                if (ExpandOverflow(buffer,g_vfqname)) {
                    FatalError("Can't expand varstring");
                }
                strcat(buffer,g_vfqname);
                break;

            case cfnetmask:
                if (ExpandOverflow(buffer,g_vnetmask)) {
                    FatalError("Can't expand varstring");
                }
                strcat(buffer,g_vnetmask);
                break;

            case cfipaddr:
                if (ExpandOverflow(buffer,g_vipaddress)) {
                    FatalError("Can't expand varstring");
                }
                strcat(buffer,g_vipaddress);
                break;

            case cfbinserver:
                if (g_action != links && g_action != required) {
                    yyerror("Inappropriate use of variable binserver");
                    FatalError("Bad variable");
                }

                if (ExpandOverflow(buffer,"$(binserver)")) {
                    FatalError("Can't expand varstring");
                }
                strcat(buffer,"$(binserver)");
                break;

            case cfsysadm:
                if (g_vsysadm[0] == '\0') {
                    yyerror("sysadm undefined variable");
                }

                if (ExpandOverflow(buffer,g_vsysadm)) {
                    FatalError("Can't expand varstring");
                }
                strcat(buffer,g_vsysadm);
                break;

            case cfdomain:
                if (g_vdomain[0] == '\0') {
                    yyerror("domain undefined variable");
                }

                if (ExpandOverflow(buffer,g_vdomain)) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,ToLowerStr(g_vdomain));
                break;

            case cfnfstype:
                if (ExpandOverflow(buffer,g_vnfstype)) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,g_vnfstype);
                break;

            case cftimezone:
                if (g_vtimezone == NULL) {
                    yyerror("timezone undefined variable");
                }

                if (ExpandOverflow(buffer,g_vtimezone->name)) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,g_vtimezone->name);
                break;

            case cfclass:
                if (ExpandOverflow(buffer,g_classtext[g_vsystemhardclass])) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,g_classtext[g_vsystemhardclass]);
                break;

            case cfarch:
                if (ExpandOverflow(buffer,g_varch)) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,g_varch);
                break;

            case cfarch2:
                if (ExpandOverflow(buffer,g_varch2)) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,g_varch2);
                break;


            case cfdate:
                if ((tloc = time((time_t *)NULL)) == -1) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Couldn't read system clock\n");
                    CfLog(cferror,"Couldn't read clock","time");
                }

                if (ExpandOverflow(buffer,ctime(&tloc))) {
                    FatalError("Can't expandvarstring");
                } else {
                    strcat(buffer,Space2Score(ctime(&tloc)));
                    Chop(buffer);
                }
                break;

            case cfyear:
                if (ExpandOverflow(buffer,g_vyear)) {
                    FatalError("Can't expandvarstring");
                } else {
                    strcat(buffer,g_vyear);
                }
                break;

            case cfmonth:
                if (ExpandOverflow(buffer,g_vmonth)) {
                    FatalError("Can't expandvarstring");
                } else {
                    strcat(buffer,g_vmonth);
                }
                break;

            case cfday:
                if (ExpandOverflow(buffer,g_vday)) {
                    FatalError("Can't expandvarstring");
                } else {
                    strcat(buffer,g_vday);
                }
                break;
            case cfhr:
                if (ExpandOverflow(buffer,g_vhr)) {
                    FatalError("Can't expandvarstring");
                } else {
                    strcat(buffer,g_vhr);
                }
                break;

            case cfmin:
                if (ExpandOverflow(buffer,g_vminute)) {
                    FatalError("Can't expandvarstring");
                } else {
                    strcat(buffer,g_vminute);
                }
                break;

            case cfallclass:
                if (strlen(g_allclassbuffer) == 0) {
                    snprintf(name,CF_MAXVARSIZE,"$(%s)",currentitem);
                    strcat(buffer,name);
                }

                if (ExpandOverflow(buffer,g_allclassbuffer)) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,g_allclassbuffer);
                break;

            case cfspc:
                if (ExpandOverflow(buffer," ")) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer," ");
                break;

            case cftab:
                if (ExpandOverflow(buffer," ")) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,"\t");
                break;

            case cflf:
                if (ExpandOverflow(buffer," ")) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,"\012");
                break;

            case cfcr:
                if (ExpandOverflow(buffer," ")) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,"\015");
                break;

            case cfn:
                if (ExpandOverflow(buffer," ")) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,"\n");
                break;

            case cfdblquote:
                if (ExpandOverflow(buffer," ")) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,"\"");
                break;
            case cfquote:
                if (ExpandOverflow(buffer," ")) {
                    FatalError("Can't expandvarstring");
                }
                strcat(buffer,"\'");
                break;

            case cfdollar:
                if (!g_parsing) {
                    if (ExpandOverflow(buffer," ")) {
                        FatalError("Can't expandvarstring");
                    }
                    strcat(buffer,"$");
                } else {
                    if (ExpandOverflow(buffer,"$(dollar)")) {
                        FatalError("Can't expandvarstring");
                    }
                    strcat(buffer,"$(dollar)");
                }
                break;

            case cfrepchar:
                if (ExpandOverflow(buffer," ")) {
                    FatalError("Can't expandvarstring");
                }
                len = strlen(buffer);
                buffer[len] = g_reposchar;
                buffer[len+1] = '\0';
                break;

            case cflistsep:
                if (ExpandOverflow(buffer,"")) {
                    FatalError("Can't expandvarstring");
                }
                len = strlen(buffer);
                buffer[len] = g_listseparator;
                buffer[len+1] = '\0';
                break;

            default:

                if ((env = GetMacroValue(g_contextid,currentitem)) != NULL) {
                    if (ExpandOverflow(buffer,env)) {
                        FatalError("Can't expandvarstring");
                    }
                    strcat(buffer,env);
                    Debug("Expansion gave (%s)\n",buffer);
                    break;
                }

                Debug("Currently non existent variable $(%s)\n",
                        currentitem);

                if (varstring == '}') {
                    snprintf(name,CF_MAXVARSIZE,"${%s}",currentitem);
                } else {
                    snprintf(name,CF_MAXVARSIZE,"$(%s)",currentitem);
                }
                strcat(buffer,name);
            }

            sp += strlen(currentitem);
            currentitem[0] = '\0';
        }
    }
    return varstring;
}

/* ----------------------------------------------------------------- */

int
ExpandVarbinserv(char *string,char *buffer,char *bserver)
{
    char *sp;
    char varstring = false;
    char currentitem[CF_EXPANDSIZE], scanstr[6];

    Debug("ExpandVarbinserv %s, ",string);

    if (bserver != NULL) {
        Debug("(Binserver is %s)\n",bserver);
    }

    buffer[0] = '\0';

    /* check for varitems */
    for (sp = string; /* No exit */ ; sp++) {
        currentitem[0] = '\0';

        sscanf(sp,"%[^$]",currentitem);

        strcat(buffer,currentitem);
        sp += strlen(currentitem);

        if (*sp == '$') {
            switch (*(sp+1)) {
            case '(':
                varstring = ')';
                break;
            case '{':
                varstring = '}';
                break;
            default:
                strcat(buffer,"$");
                continue;
            }
        sp++;
        }

        currentitem[0] = '\0';

        if (*sp == '\0') {
            break;
        } else {
            /* select the correct terminator */
            sprintf(scanstr,"%%[^%c]",varstring);

            /* reduce item */
            sscanf(++sp,scanstr,currentitem);

            switch (ScanVariable(currentitem)) {
            case cfbinserver:
                if (ExpandOverflow(buffer,bserver)) {
                    FatalError("Can't expand varstring");
                }
                strcat(buffer,bserver);
                break;
            }

            sp += strlen(currentitem);
            currentitem[0] = '\0';
        }
    }
    return varstring;
}

/* ----------------------------------------------------------------- */

enum vnames
ScanVariable(char *name)
{
    int i = nonexistentvar;

    for (i = 0; VVNAMES[i] != '\0'; i++) {
        if (strcmp(VVNAMES[i],ToLowerStr(name)) == 0) {
            return (enum vnames) i;
        }
    }

    return (enum vnames) i;
}

/* 
 * Splits a string containing a separator like : into a linked list of
 * separate items. 
 */ 
struct Item *
SplitVarstring(char *varstring,char sep)
{
    struct Item *liststart = NULL;
    char format[6], *sp;
    char node[CF_BUFSIZE];
    char buffer[CF_EXPANDSIZE],variable[CF_BUFSIZE];
    char before[CF_BUFSIZE],after[CF_BUFSIZE],result[CF_BUFSIZE];
    int i,bracks = 1;

    Debug("SplitVarstring(%s,%c=%d)\n",varstring,sep,sep);

    memset(before,0,CF_BUFSIZE);
    memset(after,0,CF_BUFSIZE);

    /* Handle path = / as special case */
    if (strcmp(varstring,"") == 0) {
        AppendItem(&liststart,"/",NULL);
        return liststart;
    }

    if (!IsVarString(varstring)) {
        AppendItem(&liststart,varstring,NULL);
        return liststart;
    }

    /* set format string to search */
    sprintf(format,"%%[^%c]",sep);

    i = 0;

    /* extract variable */
    for(sp = varstring; *sp != '$' && *sp != '\0' ; sp++) {
        before[i++] = *sp;
    }
    before[i] = '\0';

    ExtractOuterVarString(varstring,variable);

    /* Exception! $(date) contains : but is not a list*/
    if (strcmp(variable,"$(date)") == 0) {
        ExpandVarstring(varstring,buffer,"");
        AppendItem(&liststart,buffer,NULL);
        return liststart;
    }

    ExpandVarstring(variable,buffer,"");

    Debug("EXPAND %s -> %s\n",variable,buffer);

    sp += strlen(variable);

    while(*sp != '\0') {
        after[i++] = *sp++;
    }

    for (sp = buffer; *sp != '\0'; sp++) {
        memset(node,0,CF_MAXLINKSIZE);
        sscanf(sp,format,node);

        if (strlen(node) == 0) {
            continue;
        }

        sp += strlen(node)-1;

        if (strlen(before)+strlen(node)+strlen(after) >= CF_BUFSIZE) {
            FatalError("Buffer overflow expanding variable string");
            printf("Concerns: %s%s%s in %s",before,node,after,varstring);
        }

        snprintf(result,CF_BUFSIZE,"%s%s%s",before,node,after);

        AppendItem(&liststart,result,NULL);

        if (*sp == '\0') {
            break;
        }
    }
    return liststart;
}
