/*
 * $Id: eval.c 738 2004-05-23 06:23:30Z skaar $
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
 * Toolkit: Class string evaluation
 * Dependency: item.c toolkit 
 * --------------------------------------------------------------------
 */

#include "cf.defs.h"
#include "cf.extern.h"


/*
 * --------------------------------------------------------------------
 * Object variables
 * --------------------------------------------------------------------
 */

char *DAYTEXT[] = {
    "Monday",
    "Tuesday",
    "Wednesday",
    "Thursday",
    "Friday",
    "Saturday",
    "Sunday"
};

char *MONTHTEXT[] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
};

/*
 * --------------------------------------------------------------------
 * Level 1
 * --------------------------------------------------------------------
 */

int
Day2Number(char *datestring)
{
    int i = 0;

    for (i = 0; i < 7; i++) {
        if (strncmp(datestring,DAYTEXT[i],3) == 0) {
            return i;
        }
    }

    return -1;
}

/* ----------------------------------------------------------------- */

void
AddInstallable(char *classlist)
{
    char *sp;
    char currentitem[CF_MAXVARSIZE];

    if (classlist == NULL) {
        return;
    }

    Debug("AddInstallable(%s)\n", classlist);

    for (sp = classlist; *sp != '\0'; sp++) {
        currentitem[0] = '\0';

        sscanf(sp, "%[^,:.]", currentitem);

        sp += strlen(currentitem);

        if (! IsItemIn(g_valladdclasses, currentitem)) {
            AppendItem(&g_valladdclasses, currentitem, NULL);
        }

        if (*sp == '\0') {
            break;
        }
    }
}

/* ----------------------------------------------------------------- */

void
AddPrefixedMultipleClasses(char *name, char *classlist)
{
    char *sp; 
    char currentitem[CF_MAXVARSIZE]; 
    char local[CF_MAXVARSIZE]; 
    char pref[CF_BUFSIZE];

    if ((classlist == NULL) || strlen(classlist) == 0) {
        return;
    }

    memset(local, 0, CF_MAXVARSIZE);
    strncpy(local, classlist, CF_MAXVARSIZE-1);

    Debug("AddPrefixedMultipleClasses(%s,%s)\n", name, local);

    for (sp = local; *sp != '\0'; sp++) {
        memset(currentitem, 0, CF_MAXVARSIZE);

        sscanf(sp, "%250[^.:,]", currentitem);

        sp += strlen(currentitem);

        pref[0] = '\0';
        snprintf(pref, CF_BUFSIZE, "%s_%s", name, currentitem);


        if (IsHardClass(pref)) {
            FatalError("cfagent: You cannot use -D to define a "
                    "reserved class!");
        }

        AddClassToHeap(CanonifyName(pref));
    }
}

/* ----------------------------------------------------------------- */

void
AddMultipleClasses(char *classlist)
{
    char *sp;
    char currentitem[CF_MAXVARSIZE], local[CF_MAXVARSIZE];

    if ((classlist == NULL) || strlen(classlist) == 0) {
        return;
    }

    memset(local, 0, CF_MAXVARSIZE);
    strncpy(local, classlist, CF_MAXVARSIZE-1);

    Debug("AddMultipleClasses(%s)\n", local);

    for (sp = local; *sp != '\0'; sp++) {
        memset(currentitem, 0, CF_MAXVARSIZE);

        sscanf(sp, "%250[^.:,]", currentitem);

        sp += strlen(currentitem);

        if (IsHardClass(currentitem)) {
            FatalError("cfagent: You cannot use -D to define a "
                    "reserved class!");
        }

        AddClassToHeap(CanonifyName(currentitem));
    }
}

/* ----------------------------------------------------------------- */

void
AddTimeClass(char *str)
{
    int i;
    char buf2[10], buf3[10], buf4[10], buf5[10], buf[10], out[10];

    for (i = 0; i < 7; i++) {
        if (strncmp(DAYTEXT[i], str, 3) == 0) {
            AddClassToHeap(DAYTEXT[i]);
            break;
        }
    }

    sscanf(str, "%*s %s %s %s %s", buf2, buf3, buf4, buf5);

    /* Hours */
    sscanf(buf4, "%[^:]", buf);
    sprintf(out, "Hr%s", buf);
    AddClassToHeap(out);
    memset(g_vhr, 0, 3);
    strncpy(g_vhr, buf, 2);

    /* Minutes */

    sscanf(buf4, "%*[^:]:%[^:]", buf);
    sprintf(out, "Min%s", buf);
    AddClassToHeap(out);
    memset(g_vminute, 0, 3);
    strncpy(g_vminute, buf, 2);

    sscanf(buf, "%d", &i);

    switch ((i / 5)) {
    case 0:
        AddClassToHeap("Min00_05");
        break;
    case 1:
        AddClassToHeap("Min05_10");
        break;
    case 2:
        AddClassToHeap("Min10_15");
        break;
    case 3:
        AddClassToHeap("Min15_20");
        break;
    case 4:
        AddClassToHeap("Min20_25");
        break;
    case 5:
        AddClassToHeap("Min25_30");
        break;
    case 6:
        AddClassToHeap("Min30_35");
        break;
    case 7:
        AddClassToHeap("Min35_40");
        break;
    case 8:
        AddClassToHeap("Min40_45");
        break;
    case 9:
        AddClassToHeap("Min45_50");
        break;
    case 10:
        AddClassToHeap("Min50_55");
        break;
    case 11:
        AddClassToHeap("Min55_00");
        break;
    }

    /* Add quarters */
    switch ((i / 15)) {
    case 0:
        AddClassToHeap("Q1");
        sprintf(out, "Hr%s_Q1", g_vhr);
        AddClassToHeap(out);
        break;
    case 1:
        AddClassToHeap("Q2");
        sprintf(out, "Hr%s_Q2", g_vhr);
        AddClassToHeap(out);
        break;
    case 2:
        AddClassToHeap("Q3");
        sprintf(out, "Hr%s_Q3", g_vhr);
        AddClassToHeap(out);
        break;
    case 3:
        AddClassToHeap("Q4");
        sprintf(out, "Hr%s_Q4", g_vhr);
        AddClassToHeap(out);
        break;
    }


    /* Day */
    sprintf(out, "Day%s", buf3);
    AddClassToHeap(out);
    memset(g_vday, 0, 3);
    strncpy(g_vday, buf3, 2);

    /* Month */
    for (i = 0; i < 12; i++) {
        if (strncmp(MONTHTEXT[i], buf2, 3) == 0) {
            AddClassToHeap(MONTHTEXT[i]);
            memset(g_vmonth, 0, 4);
            strncpy(g_vmonth, MONTHTEXT[i], 3);
            break;
        }
    }

    /* Year */
    strcpy(g_vyear, buf5);

    sprintf(out, "Yr%s", buf5);
    AddClassToHeap(out);
}

/* ----------------------------------------------------------------- */

int
Month2Number(char *string)
{
    int i;

    if (string == NULL) {
        return -1;
    }

    for (i = 0; i < 12; i++) {
        if (strncmp(MONTHTEXT[i], string, strlen(string)) == 0) {
            return i+1;
            break;
        }
    }

    return -1;
}

/* ----------------------------------------------------------------- */

void
AddClassToHeap(char *class)
{
    Debug("AddClassToHeap(%s)\n", class);

    /*if (! IsItemIn(g_valladdclasses,class))
    {
    snprintf(g_output,CF_BUFSIZE,"Defining class %s -- but it isn't declared installable",class);
    yyerror(g_output);
    }*/

    Chop(class);

    if (IsItemIn(g_vheap, class)) {
        return;
    }

    AppendItem(&g_vheap, class, g_contextid);
}

/* ----------------------------------------------------------------- */ 

void
DeleteClassFromHeap(char *class)
{
    DeleteItemLiteral(&g_vheap, class);
}

/* ----------------------------------------------------------------- */ 

void
DeleteClassesFromContext(char *context)
{
    struct Item *ip;

    Verbose("Purging private classes from context %s\n", context);

    for (ip = g_vheap; ip != NULL; ip=ip->next) {
        if (strcmp(ip->classes, context) == 0) {
            Debug("Deleting context private class %s from heap\n", ip->name);
            DeleteItem(&g_vheap, ip);
        }
    }
}

/* ----------------------------------------------------------------- */ 

/* 
 * true if string matches a hardwired class e.g. hpux 
 */
int
IsHardClass(char *sp)
{
    int i;

    for (i = 2; g_classtext[i] != '\0'; i++) {
        if (strcmp(g_classtext[i], sp) == 0) {
            return(true);
        }
    }

    for (i = 0; i < 7; i++) {
        if (strcmp(DAYTEXT[i], sp)==0) {
            return(false);
        }
    }

    return(false);
}

/* ----------------------------------------------------------------- */

int
IsSpecialClass(char *class)
{
    int value = -1;

    if (strncmp(class, "IfElapsed", strlen("IfElapsed")) == 0) {
        sscanf(class, "IfElapsed%d", &value);

        if (value < 0) {
            Silent("%s: silly IfElapsed parameter in action sequence, "
                    "using default...\n", g_vprefix);
            return true;
        }

        if (!g_parsing) {
            g_vifelapsed = value;
            Verbose("                  IfElapsed time: %d minutes\n",
                    g_vifelapsed);
            return true;
        }
    }

    if (strncmp(class, "ExpireAfter", strlen("ExpireAfter")) == 0) {
        sscanf(class, "ExpireAfter%d", &value);

        if (value <= 0) {
            Silent("%s: silly ExpireAter parameter in action sequence, "
                    "using default...\n", g_vprefix);
            return true;
        }

        if (!g_parsing) {
            g_vexpireafter = value;
            Verbose("\n                  ExpireAfter time: %d minutes\n",
                    g_vexpireafter);
            return true;
        }
    }

    return false;
}

/* ----------------------------------------------------------------- */ 

int
IsExcluded(char *exception)
{
    if (! IsDefinedClass(exception)) {
        Debug2("%s is excluded!\n", exception);
        return true;
    }

    return false;
}

/* ----------------------------------------------------------------- */


/* 
 * Evaluates a.b.c|d.e.f etc and returns true if the class is currently
 * true, given the defined heap and negations
 */
int
IsDefinedClass(char *class)
{
    if (class == NULL) {
        return true;
    }

    return EvaluateORString(class, g_vaddclasses);
}


/* ----------------------------------------------------------------- */ 

/* 
 * Evaluates to true if the class string COULD become true in the course
 * of the execution - but might not be true now. 
 */
int
IsInstallable(char *class)
{
    char buffer[CF_BUFSIZE]; 
    char *sp;
    int i = 0;

    for (sp = class; *sp != '\0'; sp++) {
        if (*sp == '!') {
            /* some actions might be presented as !class */
            continue;
        }
        buffer[i++] = *sp;
    }

    buffer[i] = '\0';

    /* return (EvaluateORString(buffer,g_valladdclasses)||EvaluateORString(class,g_vaddclasses));*/

    return (EvaluateORString(buffer, g_valladdclasses) 
            || EvaluateORString(class, g_valladdclasses)
            || EvaluateORString(class, g_vaddclasses));
}


/* ----------------------------------------------------------------- */

void
NegateCompoundClass(char *class, struct Item **heap)
{
    char *sp = class;
    char cbuff[CF_MAXVARSIZE];
    char err[CF_BUFSIZE];

    Debug1("NegateCompoundClass(%s)", class);

    while(*sp != '\0') {
        sscanf(sp, "%[^.]", cbuff);

        while ((*sp != '\0') && ((*sp !='.')||(*sp == '&'))) {
            sp++;
        }

        if (*sp == '.' || *sp == '&') {
            sp++;
        }

        if (IsHardClass(cbuff)) {
            yyerror("Illegal exception");
            sprintf (err, "Cannot negate the reserved class [%s]\n", cbuff);
            FatalError(err);
        }

        AppendItem(heap,cbuff,NULL);
    }
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

int
EvaluateORString(char *class, struct Item *list)
{
    char *sp; 
    char cbuff[CF_BUFSIZE];
    int result = false;

    if (class == NULL) {
        return false;
    }

    Debug4("EvaluateORString(%s)\n", class);

    for (sp = class; *sp != '\0'; sp++) {
        while (*sp == '|') {
            sp++;
        }

        memset(cbuff, 0, CF_BUFSIZE);

        sp += GetORAtom(sp, cbuff);

        if (strlen(cbuff) == 0) {
            break;
        }

        /* Strip brackets */
        if (IsBracketed(cbuff)) {
            cbuff[strlen(cbuff)-1] = '\0';
            result |= EvaluateORString(cbuff+1, list);
        } else {
            result |= EvaluateANDString(cbuff, list);
        }

        if (*sp == '\0') {
            break;
        }
    }

    Debug4("EvaluateORString(%s) returns %d\n", class, result);
    return result;
}

/*
 * --------------------------------------------------------------------
 * Level 3
 * --------------------------------------------------------------------
 */

int
EvaluateANDString(char *class, struct Item *list)
{
    char *sp, *atom;
    char cbuff[CF_BUFSIZE];
    int count = 1;
    int negation = false;

    Debug4("EvaluateANDString(%s)\n", class);

    count = CountEvalAtoms(class);
    sp = class;

    while(*sp != '\0') {
        negation = false;

        while (*sp == '!') {
            negation = !negation;
            sp++;
        }

        memset(cbuff, 0, CF_BUFSIZE);

        sp += GetANDAtom(sp, cbuff) + 1;

        atom = cbuff;

        /* Test for parentheses */

        if (IsBracketed(cbuff)) {
            atom = cbuff+1;

            cbuff[strlen(cbuff)-1] = '\0';

            if (EvaluateORString(atom, list)) {
                if (negation) {
                return false;
            } else {
                count--;
            }
        } else {
            if (negation) {
                count--;
            } else {
                return false;
            }
        }
        continue;

        } else {
            atom = cbuff;
        }

        /* End of parenthesis check */

        if (*sp == '.' || *sp == '&') {
            sp++;
        }

        if (IsItemIn(g_vnegheap, atom)) {
            if (negation) {
                count--;
            } else {
                return false;
            }
        } else if (IsItemIn(g_vheap, atom)) {
            if (negation) {
                return false;
            } else {
                count--;
            }
        } else if (IsItemIn(list, atom)) {
            if (negation) {
                return false;
            } else {
                count--;
            }

        /* ! (an undefined class) == true */

        } else if (negation) {
            count--;
        } else {
            return false;
        }
    }

    if (count == 0) {
        Debug4("EvaluateANDString(%s) returns true\n", class);
        return(true);
    } else {
        Debug4("EvaluateANDString(%s) returns false\n", class);
        return(false);
    }
}

/* ----------------------------------------------------------------- */ 

int
GetORAtom(char *start, char *buffer)
{
    char *sp = start;
    char *spc = buffer;
    int bracklevel = 0, len = 0;

    while ((*sp != '\0') && !((*sp == '|') && (bracklevel == 0))) {
        if (*sp == '(') {
            Debug4("+(\n");
            bracklevel++;
        }

        if (*sp == ')') {
            Debug4("-)\n");
            bracklevel--;
        }

        Debug4("(%c)", *sp);
        *spc++ = *sp++;
        len++;
    }

    *spc = '\0';

    Debug4("GetORATom(%s)->%s\n", start, buffer);
    return len;
}

/*
 * --------------------------------------------------------------------
 * Level 4
 * --------------------------------------------------------------------
 */

int
GetANDAtom(char *start, char *buffer)
{
    char *sp = start;
    char *spc = buffer;
    int bracklevel = 0, len = 0;

    while ((*sp != '\0') && !(((*sp == '.')||(*sp == '&')) &&
                (bracklevel == 0))) {
        if (*sp == '(') {
            Debug4("+(\n");
            bracklevel++;
        }

        if (*sp == ')') {
            Debug("-)\n");
            bracklevel--;
        }

        *spc++ = *sp++;

        len++;
    }

    *spc = '\0';
    Debug4("GetANDATom(%s)->%s\n", start, buffer);

    return len;
}

/* ----------------------------------------------------------------- */ 

int
CountEvalAtoms(char *class)
{
    char *sp;
    int count = 0, bracklevel = 0;

    for (sp = class; *sp != '\0'; sp++) {
        if (*sp == '(') {
            Debug4("+(\n");
            bracklevel++;
            continue;
        }

        if (*sp == ')') {
            Debug4("-)\n");
            bracklevel--;
            continue;
        }

        if ((bracklevel == 0) && ((*sp == '.')||(*sp == '&'))) {
            count++;
        }
    }

    if (bracklevel != 0) {
        sprintf(g_output,"Bracket mismatch, in [class=%s], level = %d\n",
                class, bracklevel);
        yyerror(g_output);;
        FatalError("Aborted");
    }

    return count+1;
}


/*
 * --------------------------------------------------------------------
 * Toolkit: actions
 * --------------------------------------------------------------------
 */

enum actions
ActionStringToCode(char *str)
{
    char *sp;
    int i;
    enum actions action;

    action = none;

    for (sp = str; *sp != '\0'; sp++) {
        *sp = ToLower(*sp);
        if (*sp == ':') {
            *sp = '\0';
        }
    }

    for (i = 1; g_actionid[i] != '\0'; i++) {
        if (strcmp(g_actionid[i],str) == 0) {
            action = (enum actions) i;
            break;
        }
    }

    if (action == none) {
        yyerror("Indexed macro specified no action");
        FatalError("Could not compile action");
    }

    return (enum actions) i;
}

/* ----------------------------------------------------------------- */

/* 
 * IsBracketed: return true if the entire string is bracketed, not just
 * if if contains brackets 
 */ 
int
IsBracketed(char *s)
{
    int i, level= 0;

    if (*s != '(') {
        return false;
    }

    for (i = 0; i < strlen(s)-1; i++) {
        if (s[i] == '(') {
            level++;
        }

        if (s[i] == ')') {
            level--;
        }

        if (level == 0) {
            /* premature ) */
            return false;  
        }
    }
    return true;
}
