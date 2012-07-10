/*
 * $Id: parse.c 744 2004-05-23 07:53:59Z skaar $
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
 * Parse zone for cfng
 * The routines here are called by the lexer and the yacc parser
 */

#define INET

#include <stdio.h>
#include "cf.defs.h"
#include "cf.extern.h"

extern FILE *yyin;


/* ----------------------------------------------------------------- */

int
ParseInputFile(char *file)
{
    char filename[CF_BUFSIZE], *sp;
    struct Item *ptr;
    struct stat s;

    NewParser();
    g_parsing = true;

    if (strcmp(file,"-") == 0) {
        Debug("(BEGIN PARSING STDIN)\n");
        ParseStdin();
    } else {
        sp = FindInputFile(filename,file);

        Debug("(BEGIN PARSING %s)\n",filename);

        Verbose("Looking for an input file %s\n",filename);

        if (stat(filename,&s) == -1) {
            Verbose("(No file %s)\n",filename);
            Debug("(END OF g_parsing %s)\n",filename);
            Verbose("Finished with %s\n",filename);
            g_parsing = false;
            DeleteParser();
            return false;
        }

    ParseFile(filename,sp);
    }


    for (ptr = g_vimport; ptr != NULL; ptr=ptr->next) {
        if (IsExcluded(ptr->classes)) {
            continue;
        }

        Debug("(BEGIN PARSING %s)\n",ptr->name);
        Verbose("Looking for an input file %s\n",ptr->name);

        sp = FindInputFile(filename,ptr->name);
        ParseFile(filename,sp);
    }

    g_parsing = false;
    DeleteParser();

    Debug("(END OF PARSING %s)\n",file);
    Verbose("Finished with %s\n\n",file);

    if (strcmp(file,"update.conf") == 0) {
        GetRemoteMethods();
    }

    return true;
}

/* ----------------------------------------------------------------- */

void
NewParser(void)
{
    Debug("New Parser Object::");
    g_findertype = (char *) malloc(CF_BUFSIZE);
    strncpy(g_findertype, "*", CF_BUFSIZE); /* "*" = no findertype set */
    g_vuidname = (char *) malloc(CF_BUFSIZE);
    g_vgidname = (char *) malloc(CF_BUFSIZE);
    g_filtername = (char *) malloc(CF_BUFSIZE);
    g_strategyname = (char *) malloc(CF_BUFSIZE);
    g_currentitem = (char *) malloc(CF_BUFSIZE);
    g_groupbuff = (char *) malloc(CF_BUFSIZE);
    g_actionbuff = (char *) malloc(CF_BUFSIZE);
    g_currentobject = (char *) malloc(CF_BUFSIZE);
    g_currentauthpath = (char *) malloc(CF_BUFSIZE);
    g_classbuff = (char *) malloc(CF_BUFSIZE);
    g_linkfrom = (char *) malloc(CF_BUFSIZE);
    g_linkto = (char *) malloc(CF_BUFSIZE);
    g_error = (char *) malloc(CF_BUFSIZE);
    g_expr = (char *) malloc(CF_BUFSIZE);
    g_mountfrom = (char *) malloc(CF_BUFSIZE);
    g_mountonto = (char *) malloc(CF_BUFSIZE);
    *g_mountfrom = '\0';
    *g_mountonto = '\0';
    g_mountopts = (char *) malloc(CF_BUFSIZE);

    g_pkgver = (char *) malloc(CF_BUFSIZE);
    g_pkgver[0] = '\0';

    g_destination = (char *) malloc(CF_BUFSIZE);

    g_imageaction = (char *) malloc(CF_BUFSIZE);

    g_chdir = (char *) malloc(CF_BUFSIZE);
    g_restart = (char *) malloc(CF_BUFSIZE);
    g_localrepos = (char *) malloc(CF_BUFSIZE);
    g_filterdata = (char *) malloc(CF_BUFSIZE);
    g_strategydata = (char *) malloc(CF_BUFSIZE);
}

/* ----------------------------------------------------------------- */

void
DeleteParser(void)
{
    Debug("Delete Parser Object::");

    free(g_findertype);
    free(g_vuidname);
    free(g_vgidname);
    free(g_filtername);
    free(g_strategyname);
    free(g_currentitem);
    free(g_groupbuff);
    free(g_actionbuff);
    free(g_currentobject);
    free(g_currentauthpath);
    free(g_classbuff);
    free(g_linkfrom);
    free(g_linkto);
    free(g_error);
    free(g_expr);
    free(g_restart);
    free(g_mountfrom);
    free(g_mountonto);
    free(g_mountopts);
    free(g_destination);
    free(g_imageaction);
    free(g_chdir);
    free(g_localrepos);
    free(g_filterdata);
    free(g_strategydata);
}

/* ----------------------------------------------------------------- */

int RemoveEscapeSequences(char *from,char *to)
{
    char *sp,*cp;
    char start = *from;
    int len = strlen(from);


    if (len == 0) {
        return 0;
    }

    for (sp=from+1,cp=to; (sp-from) < len; sp++,cp++) {
        if ((*sp == start)) {
            *(cp) = '\0';
            if (*(sp+1) != '\0') {
                return (2+(sp - from));
            }
            return 0;
        }
        if (*sp == '\n') {
            g_linenumber++;
        }
        if (*sp == '\\') {
            switch (*(sp+1)) {
            case '\n':
                g_linenumber++;
                sp+=2;
                break;
            case '\\':
            case '\"':
            case '\'':
                sp++;
                break;
            }
        }
        *cp = *sp;
    }

    yyerror("Runaway string");
    *(cp) = '\0';
    return 0;
}

/* ----------------------------------------------------------------- */


/* Change state to major action */
void SetAction (enum actions action)
{
  
    /* Flush any existing actions */
    InstallPending(g_action);   

    SetStrategies();

    Debug1("\n\n==============================BEGIN NEW ACTION %s=============\n\n",g_actiontext[action]);

    g_action = action;
    strcpy(g_actionbuff,g_actiontext[action]);

    switch (g_action) {
    case interfaces:
    case files:
    case makepath:
    case tidy:
    case disable:
    case rename_disable:
    case filters:
    case strategies:
    case image:
    case links:
    case required:
    case disks:
    case shellcommands:
    case alerts:
    case unmounta:
    case admit:
    case deny:
    case methods:
    case processes:
        InitializeAction();
    }

    Debug1("\nResetting CLASS to ANY\n\n");
    strcpy(g_classbuff,CF_ANYCLASS);    /* default class */
}

/* ----------------------------------------------------------------- */

/* Record name of a variable assignment */
void
HandleLValue(char *id)
{
    Debug1("HandleLVALUE(%s) in action %s\n",id,g_actiontext[g_action]);

    /* Check for IP names in cfd */
    switch(g_action) {
    case control:

        if ((g_controlvar = ScanVariable(id)) != nonexistentvar) {
            strcpy(g_currentitem,id);
            return;
        } else {
            if (IsDefinedClass(g_classbuff)) {
                RecordMacroId(id);
            }
            return;
        }
        break;

    case groups:
        {
            int count = 1;
            char *cid = id;

            while (*cid != '\0') {
                if (*cid++ == '.') {
                    count++;
                }
            }

            if (strcmp(id,CF_ANYCLASS) == 0) {
                yyerror("Reserved class <any>");
            }

            /* compound class */
            if (count > 1) {
                yyerror("Group with compound identifier");
                FatalError("Dots [.] not allowed in group identifiers");
            }

            if (IsHardClass(id)) {
                yyerror("Reserved class name (choose a different name)");
            }

            strcpy(g_groupbuff,id);
        }
        break;

    }
}


/* ----------------------------------------------------------------- */

/* Record identifier for braced object */
void
HandleBraceObjectID(char *id)
{
    Debug1("HandleBraceObjectID(%s) in action %s\n",id,g_actiontext[g_action]);

    switch (g_action) {
    case acls:
        strcpy(g_currentobject,id);
        InstallACL(id,g_classbuff);
        break;

    case strategies:
        if (strlen(g_strategyname) == 0) {
            strcpy(g_strategyname,id);
            InstallStrategy(id,g_classbuff);
        } else {
            yyerror("Multiple identifiers or forgotten quotes in strategy");
        }
        break;

    case editfiles:
        if (strlen(g_currentobject) == 0) {
            strcpy(g_currentobject,id);
            g_editgrouplevel = 0;
            g_foreachlevel = 0;
            g_searchreplacelevel = 0;
        } else {
            yyerror("Multiple filenames in editfiles");
        }
        break;

    case filters:
        if (strlen(g_filtername)==0) {
            strcpy(g_filtername,id);
            InstallFilter(id);
        } else {
            yyerror("Multiple identifiers in filter");
        }
        break;
    }
}

/* ----------------------------------------------------------------- */

/* Record LHS item in braced object */
void
HandleBraceObjectClassifier(char *id)
{
    Debug1("HandleClassifier(%s) in action %s\n",id,g_actiontext[g_action]);

    switch (g_action) {
    case acls:
        AddACE(g_currentobject,id,g_classbuff);
        break;

    case filters:
    case strategies:
        strcpy(g_currentitem,id);
        break;

    case editfiles:
        if (strcmp(id,"EndGroup") == 0 ||
            strcmp(id,"GotoLastLine") == 0 ||
            strcmp(id,"AutoCreate") == 0  ||
            strcmp(id,"EndLoop") == 0  ||
            strcmp(id,"CatchAbort") == 0  ||
            strcmp(id,"EmptyEntireFilePlease") == 0) {
                    HandleEdit(g_currentobject,id,NULL);
        }

        strcpy(g_currentitem,id);
        break;
    }
}

/* ----------------------------------------------------------------- */

void
HandleClass (char *id)
{
    int members;
    char *sp;

    Debug("HandleClass(%s)\n",id);

    for (sp = id; *sp != '\0'; sp++) {
        switch (*sp) {
        case '-':
        case '*':
            snprintf(g_output,CF_BUFSIZE,
                "Illegal character (%c) in class %s",*sp,id);
            yyerror(g_output);
        }
    }

    InstallPending(g_action);

    /* Parse compound id */
    if ((members = CompoundId(id)) > 1) {
        Debug1("Compound class = (%s) with %d members\n",id,members);
    } else {
        Debug1("Simple class = (%s)\n",id);
    }

    strcpy(g_classbuff,id);
}

/* ----------------------------------------------------------------- */

void
HandleQuotedString(char *qstring)
{
    Debug1("HandleQuotedString %s\n",qstring);

    switch (g_action) {
    case editfiles:
        HandleEdit(g_currentobject,g_currentitem,qstring);
        break;

    case filters:
        strcpy(g_filterdata,qstring);
        g_actionpending = true;
        InstallPending(g_action);
        break;

    case strategies:
        strcpy(g_strategydata,qstring);
        g_actionpending = true;
        InstallPending(g_action);
        break;

    /* Handle anomalous syntax of restart/setoptonstring */
    case processes:


        if (strcmp(g_currentobject,"SetOptionString") == 0) {
            if (g_have_restart) {
                yyerror("Processes syntax error");
            }
            strcpy(g_restart,qstring);
            g_actionpending = true;
            InstallPending(g_action);

            return;
        }

        /* Any string must be new rule */
        if (strlen(g_restart) > 0 || (!g_have_restart && strlen(g_expr) > 0)) {
            if (!g_actionpending) {
                yyerror("Insufficient or incomplete processes statement");
            }
            InstallPending(g_action);
            InitializeAction();
        }

        if (g_expr[0] == '\0') {
            if (g_have_restart) {
                yyerror("Missing process search expression");
            }
            Debug1("Installing expression %s\n",qstring);
            strcpy(g_expr,qstring);
            g_have_restart = false;
        } else if (g_have_restart) {
            Debug1("Installing restart expression\n");
            strncpy(g_restart,qstring,CF_BUFSIZE-1);
            g_actionpending = true;
        }
        break;

    case interfaces:
        strncpy(g_vifname,qstring,15);
        break;

    default:
        InstallPending(g_action);
        strncpy(g_currentobject,qstring,CF_BUFSIZE-1);
        g_actionpending = true;
        return;
    }
}

/* ----------------------------------------------------------------- */

/* Assignment in groups/classes */
void
HandleGroupRValue(char *rval)
{
    Debug1("\nHandleGroupRvalue(%s)\n",rval);

    if (strcmp(rval,"+") == 0 || strcmp(rval,"-") == 0) {
        yyerror("+/- not bound to identifier");
    }

    /* Lookup in NIS */
    if (rval[0] == '+') {
        rval++;

        /* Irrelevant */
        if (rval[0] == '@') {
            rval++;
        }

        Debug1("Netgroup rval, lookup NIS group (%s)\n",rval);
        InstallGroupRValue(rval,netgroup);

    } else if ((rval[0] == '-') && (g_action != processes)) {
    rval++;

        /* Irrelevant */
        if (rval[0] == '@') {
            rval++;
            InstallGroupRValue(rval,groupdeletion);
        } else {
            InstallGroupRValue(rval,deletion);
        }

    } else if (rval[0] == '\"' || rval[0] == '\'' || rval[0] == '`') {
        *(rval+strlen(rval)-1) = '\0';

        InstallGroupRValue(rval,classscript);

    } else if (rval[0] == '/') {
        InstallGroupRValue(rval,classscript);
    } else {
        InstallGroupRValue(rval,simple);
    }
}

/* ----------------------------------------------------------------- */

/* Function in main body */
void
HandleFunctionObject(char *fn)
{
    char local[CF_BUFSIZE];

    Debug("HandleFunctionObject(%s)\n",fn);

    if (g_action == methods) {
        strncpy(g_currentobject,fn,CF_BUFSIZE-1);
        g_actionpending = true;
        return;
    }

    if (IsBuiltinFunction(fn)) {
        local[0] = '\0';
        strcpy(local,EvaluateFunction(fn,local));

        switch (g_action) {
        case groups:
            InstallGroupRValue(local,simple);
            break;
        case control:

            if (strncmp("CF_ASSOCIATIVE_ARRAY",local,
                        strlen("CF_ASSOCIATIVE_ARRAY")) == 0) {
                break;
            }

            InstallControlRValue(g_currentitem,local);
            break;

        case alerts:

            if (strcmp(local,"noinstall") != 0) {
                Debug("Recognized function %s\n",fn);
                strncpy(g_currentobject,fn,CF_BUFSIZE-1);
                g_actionpending = true;
            } else {
                Debug("Not recognized %s\n",fn);
            }
            break;

        default:
            snprintf(g_output,CF_BUFSIZE,"Function call %s out of place",fn);
            yyerror(g_output);
        }
    }
}

/* ----------------------------------------------------------------- */

void
HandleVarObject(char *object)
{

    Debug1("Handling Object = (%s)\n",object);

    /* Yes this must be here */
    strncpy(g_currentobject,object,CF_BUFSIZE-1);

    /* we're parsing an action */
    g_actionpending = true;

    /* to-link (after ->) */
    if (g_action_is_link || g_action_is_linkchildren) {
        strncpy(g_linkto,g_currentobject,CF_BUFSIZE-1);
        return;
    }

    switch (g_action) {
    case control:
        switch (g_controlvar) {
        case cfmountpat:
            SetMountPath(object);
            break;
        case cfrepos:
            SetRepository(object);
            break;
        case cfrepchar:
            if (strlen(object) > 1) {
                yyerror("reposchar can only be a single letter");
            }
            if (object[0] == '/') {
                yyerror("illegal value for reposchar");
            }
            g_reposchar = object[0];
            break;
        case cflistsep:
            if (strlen(object) > 1) {
                yyerror("listseparator can only be a single letter");
            }
            if (object[0] == '/') {
                yyerror("illegal value for listseparator");
            }
            g_listseparator = object[0];
            break;
        case cfhomepat:
            yyerror("Path relative to mountpath required");
            FatalError("Absolute path was specified\n");
            break;
        case nonexistentvar:
            if (IsDefinedClass(g_classbuff)) {
                AddMacroValue(g_contextid,g_currentitem,object);
            }
            break;
        }
        break;
    case import:
        AppendImport(object); /* Recursion */
        break;
    case links:      /* from link (see cf.l) */
        break;
    case filters:
        if (strlen(g_filtername) == 0) {
            yyerror("empty or broken filter");
        } else {
            strncpy(g_currentitem,object,CF_BUFSIZE-1);
            g_actionpending = true;
        }
        break;
    case disks:
    case required:
        strncpy(g_currentobject,object,CF_BUFSIZE-1);
        break;
    case shellcommands:
        break;
    case mountables:
        break;
    case defaultroute:
        InstallDefaultRouteItem(object);
        break;
    case resolve:
        AppendNameServer(object);
        break;
    case broadcast:
        InstallBroadcastItem(object);
        break;
    case mailserver:
        InstallMailserverPath(object);
        break;
    case tidy:
        strncpy(g_currentitem,object,CF_BUFSIZE-1);
        break;
    case rename_disable:
    case disable:
        strncpy(g_currentobject,object,CF_BUFSIZE-1);
        g_actionpending = true;
        break;
    case makepath:
        strncpy(g_currentobject,object,CF_BUFSIZE-1);
        break;
    case ignore:
        strncpy(g_currentobject,object,CF_BUFSIZE-1);
        break;
    case misc_mounts:
        if (! g_mount_from) {
            g_mount_from = true;
            strncpy(g_mountfrom,g_currentobject,CF_BUFSIZE-1);
        } else {
            if (g_mount_onto) {
                yyerror ("Path not expected");
                FatalError("miscmounts: syntax error");
            }
            g_mount_onto = true;
            strncpy(g_mountonto,g_currentobject,CF_BUFSIZE-1);
        }
        break;
    case unmounta:
        strncpy(g_currentobject,object,CF_BUFSIZE-1);
        break;
    case image:
    case files:
        break;
    case editfiles:  /* file recorded in g_currentobject */
        strncpy(g_currentobject,object,CF_BUFSIZE-1);
        break;
    case processes:
        if (strcmp(ToLowerStr(object),"restart") == 0) {
            if (g_have_restart) {
                yyerror("Repeated Restart option in processes");
            }
            g_have_restart = true;
            Debug1("Found restart directive\n");
        } else if (strcmp(ToLowerStr(object),"setoptionstring") == 0) {
            InstallPending(g_action);
            InitializeAction();
            Debug1("\nFound SetOptionString\n");
            strcpy(g_currentobject,"SetOptionString");
            strcpy(g_expr,"SetOptionString");
        } else if (g_have_restart) {
            Debug1("Installing restart expression\n");
            strncpy(g_restart,object,CF_BUFSIZE-1);
            g_actionpending = true;
        } else {
            Debug1("Dropped %s :(\n",object);
        }
        break;
    case binservers:
        InstallBinserverItem(object);
        break;
    case homeservers:
        InstallHomeserverItem(object);
        break;
    case packages:
        break;
    case methods:
        if (strlen(object) > CF_BUFSIZE-1) {
            yyerror("Method argument string is too long");
        }
        break;
    default:
        yyerror("Unknown command or name out of context");
   }
}

/* ----------------------------------------------------------------- */

void
HandleServerRule(char *object)
{
    char buffer[CF_EXPANDSIZE];

    ExpandVarstring(object,buffer,"");
    Debug("HandleServerRule(%s=%s)\n",object,buffer);

    if (*buffer == '/') {
        Debug("\n\nNew admit/deny object=%s\n",buffer);
        strcpy(g_currentauthpath,object);
    } else {
        /* Check for IP names in cfservd */
        switch(g_action) {
        case admit:
            FuzzyMatchParse(buffer);
            InstallAuthItem(g_currentauthpath,object,&g_vadmit,
                    &g_vadmittop,g_classbuff);
            break;
        case deny:
            FuzzyMatchParse(buffer);
            InstallAuthItem(g_currentauthpath,object,&g_vdeny,
                    &g_vdenytop,g_classbuff);
            break;
        }
    }
}

/* ----------------------------------------------------------------- */

void
HandleOption(char *option)
{
    Debug("HandleOption(%s)\n",option);

    switch (g_action) {
    case files:
        HandleOptionalFileAttribute(option);
        break;
    case image:
        HandleOptionalImageAttribute(option);
        break;
    case tidy:
        HandleOptionalTidyAttribute(option);
        break;
    case makepath:
        HandleOptionalDirAttribute(option);
        break;
    case disable:
    case rename_disable:
        HandleOptionalDisableAttribute(option);
        break;
    case links:
        HandleOptionalLinkAttribute(option);
        break;
    case processes:
        HandleOptionalProcessAttribute(option);
        break;
    case misc_mounts:
        HandleOptionalMiscMountsAttribute(option);
        break;
    case mountables:
        HandleOptionalMountablesAttribute(option);
        break;
    case unmounta:
        HandleOptionalUnMountAttribute(option);
        break;
    case shellcommands:
        HandleOptionalScriptAttribute(option);
        break;
    case alerts:  HandleOptionalAlertsAttribute(option);
        break;
    case disks:
    case required:
        HandleOptionalRequired(option);
        break;
    case interfaces:
        HandleOptionalInterface(option);
        if (strlen(g_vifname) && strlen(g_currentobject) &&
                strlen(g_destination)) {
            g_actionpending = true;
        }
        break;
    case admit:
        InstallAuthItem(g_currentauthpath,option,&g_vadmit,
                &g_vadmittop,g_classbuff);
        break;
    case deny:
        InstallAuthItem(g_currentauthpath, option, &g_vdeny,
                &g_vdenytop, g_classbuff);
        break;
    case import:  /* Need option for private modules... */
        break;
    case packages:
        HandleOptionalPackagesAttribute(option);
        break;
    case methods: HandleOptionalMethodsAttribute(option);
        break;
    default:
        yyerror("Options cannot be used in this context:");
    }
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

void
ParseFile(char *filename,char *env)
{
    FILE *save_yyin = yyin;

    signal(SIGALRM,(void *)TimeOut);
    alarm(g_rpctimeout);

    if (!FileSecure(filename)) {
        FatalError("Security exception");
    }

    /* Open root file */
    if ((yyin = fopen(filename,"r")) == NULL) {
        printf("%s: Can't open file %s\n",g_vprefix,filename);

        if (env == NULL) {
            printf("%s: (%s is set to <nothing>)\n",g_vprefix,CF_INPUTSVAR);
        } else {
            printf("%s: (%s is set to %s)\n",g_vprefix,CF_INPUTSVAR,env);
        }

        exit (1);
    }

    strcpy(g_vcurrentfile,filename);

    Debug("\n##########################################################################\n");
    Debug("# BEGIN PARSING %s\n",g_vcurrentfile);
    Debug("##########################################################################\n\n");

    g_linenumber=1;

    while (!feof(yyin)) {
        yyparse();

        /* abortable */
        if (ferror(yyin)) {
            printf("%s: Error reading %s\n",g_vprefix,g_vcurrentfile);
            perror("cfagent");
            exit(1);
        }
    }

    fclose (yyin);
    yyin = save_yyin;

    alarm(0);
    signal(SIGALRM,SIG_DFL);
    InstallPending(g_action);
}

/* ----------------------------------------------------------------- */

void
ParseStdin(void)
{
    yyin = stdin;

    Debug("\n##########################################################################\n");
    Debug("# BEGIN PARSING stdin\n");
    Debug("##########################################################################\n\n");

    g_linenumber=1;

    while (!feof(yyin)) {
        yyparse();

        /* abortable */
        if (ferror(yyin)) {
            printf("%s: Error reading %s\n",g_vprefix,g_vcurrentfile);
            perror("cfagent");
            exit(1);
        }
    }

    InstallPending(g_action);
}

/* ----------------------------------------------------------------- */

/* check for dots in the name */
int
CompoundId(char *id)
{
    int count = 1;
    char *cid = id;

    for (cid = id; *cid != '\0'; cid++) {
        if (*cid == '.' || *cid == '|') {
            count++;
        }
    }

    return(count);
}


/* ----------------------------------------------------------------- */

void
RecordMacroId(char *name)
{
    Debug("RecordMacroId(%s)\n",name);
    strcpy(g_currentitem,name);

    if (strcmp(name,"this") == 0) {
        yyerror("$(this) is a reserved variable");
    }
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

char *
FindInputFile(char *result,char *filename)
{
    char *sp;

    result[0] = '\0';

    if (g_minusf && (*filename == '.' || *filename == '/')) {
        Verbose("Manually overidden relative path (%s)\n",filename);
        strncpy(result,filename,CF_BUFSIZE-1);
        return result;
    }

    if ((sp=getenv(CF_INPUTSVAR)) != NULL) {
        /* Don't prepend to absolute names */
        if (!IsAbsoluteFileName(filename)) {
            strcpy(result,sp);

            if (! IsAbsoluteFileName(result)) {

                Verbose("CFINPUTS was not an absolute path, "
                        "overriding with %s\n",WORKDIR);

                snprintf(result,CF_BUFSIZE,"%s/inputs",WORKDIR);
            }

        AddSlash(result);
        }
    } else {
        /* Don't prepend to absolute names */
        if (!IsAbsoluteFileName(filename)) {
            strcpy(result,WORKDIR);
            AddSlash(result);
            strcat(result,"inputs/");
        }
    }

    strcat(result,filename);
    return result;
}


/*
 * --------------------------------------------------------------------
 * Toolkits misc
 * --------------------------------------------------------------------
 */

/* Set defaults */
void
InitializeAction(void)
{
    Debug1("InitializeAction()\n");

    g_actionpending = false;

    g_plusmask = (mode_t)0;
    g_minusmask = (mode_t)0;
    g_plusflag = (u_long)0;
    g_minusflag = (u_long)0;
    g_vrecurse = 0;
    g_have_restart = false;
    g_vage = 99999;
    strncpy(g_findertype,"*",CF_BUFSIZE);
    strcpy(g_vuidname,"*");
    strcpy(g_vgidname,"*");
    g_have_restart = 0;
    g_fileaction=warnall;
    g_pifelapsed=-1;
    g_pexpireafter=-1;

    *g_currentauthpath = '\0';
    *g_currentobject = '\0';
    *g_currentitem = '\0';
    *g_destination = '\0';
    *g_imageaction = '\0';
    *g_localrepos = '\0';
    *g_expr = '\0';
    *g_restart = '\0';
    *g_filterdata = '\0';
    *g_strategydata = '\0';
    *g_chdir ='\0';
    g_methodfilename[0] = '\0';
    g_methodreturnclasses[0] = '\0';
    g_methodreplyto[0] = '\0';
    g_methodforce[0] = '\0';
    g_chroot[0] = '\0';
    strcpy(g_vifname,"");
    g_ptravlinks = (short) '?';
    g_imagebackup = 'y';
    g_encrypt = 'n';
    g_verify = 'n';
    g_rotate=0;
    g_tidysize=0;
    g_promatches=-1;
    g_procomp = '!';
    g_proaction = 'd';
    g_prosignal=0;
    g_compress='n';
    g_agetype='a';
    g_copytype = g_defaultcopytype; /* 't' */
    g_linkdirs = 'k';
    g_useshell = 'y';
    g_logp = 'd';
    g_informp = 'd';
    g_purge = 'n';
    g_trustkey = 'n';
    g_noabspath = false;
    g_scan = 'n';
    g_checksum = 'n';
    g_tidydirs = false;
    g_vexcludeparse = NULL;
    g_vincludeparse = NULL;
    g_vignoreparse = NULL;
    g_vaclbuild = NULL;
    g_vfilterbuild = NULL;
    g_vstrategybuild = NULL;
    g_vcplnparse = NULL;
    g_vtimeout=0;

    g_pkgmgr = g_defaultpkgmgr; /* pkgmgr_none */
    g_cmpsense = cmpsense_eq;
    g_pkgver[0] = '\0';

    g_strategyname[0] = '\0';
    g_filtername[0] = '\0';
    memset(g_allclassbuffer,0,CF_BUFSIZE);
    memset(g_elseclassbuffer,0,CF_BUFSIZE);

    strcpy(g_cfserver,"localhost");

    g_imgcomp = g_discomp='>';
    g_imgsize = g_disablesize = CF_NOSIZE;
    g_deletedir = 'y';   /* t=true */
    g_deletefstab = 'y';
    g_force = 'n';
    g_forcedirs = 'n';
    g_stealth = 'n';
    g_xdev = 'n';
    g_preservetimes = 'n';
    g_typecheck = 'y';
    g_umask = 077;     /* Default umask for scripts/files */
    g_fork = 'n';
    g_preview = 'n';
    g_compatibility = 'n';

    if (g_mount_from && g_mount_onto) {
        Debug("Resetting miscmount data\n");
        g_mount_from = false;
        g_mount_onto = false;
        g_mountmode='w';
        *g_mountfrom = '\0';
        *g_mountonto = '\0';
    }

    /*
    * HvB: Bas van der Vlies
    */
    g_mount_ro=false;
    g_mountopts[0]='\0';

    /* Make sure we don't clean the buffer in the middle of a link! */

    if ( ! g_action_is_link && ! g_action_is_linkchildren) {
        memset(g_linkfrom,0,CF_BUFSIZE);
        memset(g_linkto,0,CF_BUFSIZE);  /* ALSO g_restart */
        g_linksilent = false;
        g_linktype = 's';
        g_forcelink = 'n';
        g_deadlinks = false;
    }
}

/* ----------------------------------------------------------------- */

void
SetMountPath (char *value)
{
    char buff[CF_EXPANDSIZE];

    ExpandVarstring(value,buff,"");

    Debug("Appending [%s] to mountlist\n",buff);

    AppendItem(&g_vmountlist,buff,g_classbuff);
}

/* ----------------------------------------------------------------- */

void
SetRepository (char *value)
{
    char ebuff[CF_EXPANDSIZE];

    if (*value != '/') {
        yyerror("File repository must be an absolute directory name");
    }

    if (strlen(g_vrepository) != 0) {
        yyerror("Redefinition of system variable repository");
    }

    ExpandVarstring(value,ebuff,"");
    g_vrepository = strdup(ebuff);
}
