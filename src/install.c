/*
 * $Id: install.c 760 2004-05-25 21:39:05Z skaar $
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
 * Routines which install actions parsed by the parser
 *
 * Derived from parse.c (Parse object)
 */


#include "cf.defs.h"
#include "cf.extern.h"


void
InstallControlRValue(char *lvalue, char *varvalue)
{
    int number = -1;
    char *sp;
    char buffer[CF_MAXVARSIZE], command[CF_MAXVARSIZE]; 
    char value[CF_EXPANDSIZE];

    if (!IsInstallable(g_classbuff)) {
        Debug1("Not installing %s=%s, no match (%s)\n", 
                lvalue, varvalue, g_classbuff);
        return;
    }

    if (strcmp(varvalue,CF_NOCLASS) == 0) {
        Debug1("Not installing %s, evaluated to false\n", varvalue);
        return;
    }

    ExpandVarstring(varvalue, value, NULL);

    /* begin version compat */
    if (strncmp(value, "exec ", 5) == 0) {
        for (sp = value+4; *sp == ' '; sp++) { }

        if (*sp == '/') {
            strncpy(command, sp, CF_MAXVARSIZE);
            GetExecOutput(command, value);
            Chop(value);
            /* Truncate to CF_MAXVARSIZE */
            value[CF_MAXVARSIZE-1] = '\0';  
        } else {
            Warning("Exec string in control did not specify an absolute path");
        }
    }

    /* end version 1 compat*/

    /* 
     * Actionsequence needs to be dynamical here, so make exception -
     * should be IsInstallable??
     */

    if ((ScanVariable(lvalue) != cfactseq) && !IsDefinedClass(g_classbuff)) {
        Debug("Class %s not defined, not defining\n", g_classbuff);
        return;
    } else {
        Debug1("Assign variable [%s=%s] when %s)\n",
                lvalue, value, g_classbuff);
    }

    switch (ScanVariable(lvalue)) {
    case cfsite:
    case cffaculty:
        if (!IsDefinedClass(g_classbuff)) {
            break;
        }

        if (g_vfaculty[0] != '\0') {
            yyerror("Multiple declaration of variable faculty / site");
            FatalError("Redefinition of basic system variable");
        }

        strcpy(g_vfaculty, value);
        break;
    case cfdomain:
        if (!IsDefinedClass(g_classbuff)) {
            break;
        }

        if (strlen(value) > 0) {
            strcpy(g_vdomain, value);
        } else {
            yyerror("domain is empty");
        }

        if (!StrStr(g_vsysname.nodename, g_vdomain)) {
            snprintf(g_vfqname, CF_BUFSIZE, "%s.%s",
                    g_vsysname.nodename, ToLowerStr(g_vdomain));
            strcpy(g_vuqname, g_vsysname.nodename);
        } else {
            int n = 0;
            strcpy(g_vfqname, g_vsysname.nodename);

            while(g_vsysname.nodename[n++] != '.') { }

            strncpy(g_vuqname, g_vsysname.nodename,n-1);
        }

        if (! g_nohardclasses) {
            if (strlen(g_vfqname) > CF_MAXVARSIZE-1) {
                FatalError("The fully qualified name is longer "
                        "than CF_MAXVARSIZE!!");
            }

            strcpy(buffer, g_vfqname);

            AddClassToHeap(CanonifyName(buffer));
        }

        break;
    case cfsysadm:  /* Can be redefined */
        if (!IsDefinedClass(g_classbuff)) {
            break;
        }

        strcpy(g_vsysadm, value);
        break;
    case cfnetmask:
        if (!IsDefinedClass(g_classbuff)) {
            break;
        }

        if (g_vnetmask[0] != '\0') {
            yyerror("Multiple declaration of variable netmask");
            FatalError("Redefinition of basic system variable");
        }
        strcpy(g_vnetmask,value);
        AddNetworkClass(g_vnetmask);
        break;
    case cfmountpat:
        SetMountPath(value);
        break;
    case cfrepos:
        SetRepository(value);
        break;
    case cfhomepat:
        Debug1("Installing %s as home pattern\n", value);
        AppendItem(&g_vhomepatlist, value, g_classbuff);
        break;
    case cfextension:
        AppendItems(&g_extensionlist, value, g_classbuff);
        break;
    case cfsuspicious:
        AppendItems(&g_suspiciouslist, value, g_classbuff);
        break;
    case cfschedule:
        AppendItems(&g_schedule, value, g_classbuff);
        break;
    case cfspooldirs:
        AppendItem(&g_spooldirlist, value, g_classbuff);
        break;
    case cfmethodpeers:
        AppendItems(&g_vrpcpeerlist, value, g_classbuff);
        break;
    case cfnonattackers:
        AppendItems(&g_nonattackerlist, value, g_classbuff);
        break;
    case cfmulticonn:
        AppendItems(&g_multiconnlist, value, g_classbuff);
        break;
    case cftrustkeys:
        AppendItems(&g_trustkeylist, value, g_classbuff);
        break;
    case cfdynamic:
        AppendItems(&g_dhcplist, value, g_classbuff);
        break;
    case cfallowusers:
        AppendItems(&g_allowuserlist, value, g_classbuff);
        break;
    case cfskipverify:
        AppendItems(&g_skipverify, value, g_classbuff);
        break;
    case cfredef:
        AppendItems(&g_vredefines, value, g_classbuff);
        break;
    case cfattackers:
        AppendItems(&g_attackerlist, value, g_classbuff);
        break;
    case cftimezone:
        if (!IsDefinedClass(g_classbuff)) {
            break;
        }

        AppendItem(&g_vtimezone, value, NULL);
        break;
    case cfssize:
        sscanf(value, "%d", &number);
        if (number >= 0) {
            g_sensiblefssize = number;
        } else {
            yyerror("Silly value for sensiblesize (must be positive integer)");
        }
        break;
    case cfscount:
        sscanf(value, "%d", &number);
        if (number > 0) {
            g_sensiblefilecount = number;
        } else {
            yyerror("Silly value for sensiblecount (must be positive integer)");
        }
        break;
    case cfeditsize:
        sscanf(value, "%d", &number);

        if (number >= 0) {
            g_editfilesize = number;
        } else {
            yyerror("Silly value for editfilesize (must be positive integer)");
        }
        break;
    case cfbineditsize:
        sscanf(value, "%d", &number);

        if (number >= 0) {
            g_editbinfilesize = number;
        } else {
            yyerror("Silly value for editbinaryfilesize "
                    "(must be positive integer)");
        }

        break;
    case cfifelapsed:
        sscanf(value, "%d", &number);

        if (number >= 0) {
            g_vdefaultifelapsed = g_vifelapsed = number;
        } else {
            yyerror("Silly value for IfElapsed");
        }

        break;
    case cfexpireafter:
        sscanf(value, "%d", &number);

        if (number > 0) {
            g_vdefaultexpireafter = g_vexpireafter = number;
        } else {
            yyerror("Silly value for ExpireAfter");
        }
        break;
    case cfactseq:
        AppendToActionSequence(value);
        break;
    case cfaccess:
        AppendToAccessList(value);
        break;
    case cfnfstype:
        strcpy(g_vnfstype,value);
        break;
    case cfmethodname:
        if (strcmp(g_methodname, "cf-nomethod") != 0) {
            yyerror("Redefinition of method name");
        }
        strncpy(g_methodname, value, CF_BUFSIZE-1);
        SetContext("private-method");
        break;
    case cfarglist:
        AppendItem(&g_methodargs, value, g_classbuff);
        break;
    case cfaddclass:
        AddMultipleClasses(value);
        break;
    case cfinstallclass:
        AddInstallable(value);
        break;
    case cfexcludecp:
        PrependItem(&g_vexcludecopy, value, g_classbuff);
        break;
    case cfsinglecp:
        PrependItem(&g_vsinglecopy, value, g_classbuff);
        break;
    case cfexcludeln:
        PrependItem(&g_vexcludelink, value, g_classbuff);
        break;
    case cfcplinks:
        PrependItem(&g_vcopylinks, value, g_classbuff);
        break;
    case cflncopies:
        PrependItem(&g_vlinkcopies, value, g_classbuff);
        break;
    case cfrepchar:
        if (strlen(value) > 1) {
            yyerror("reposchar can only be a single letter");
            break;
        }
        if (value[0] == '/') {
            yyerror("illegal value for reposchar");
            break;
        }
        g_reposchar = value[0];
        break;
    case cflistsep:
        if (strlen(value) > 1) {
            yyerror("listseparator can only be a single letter");
            break;
        }
        if (value[0] == '/') {
            yyerror("illegal value for listseparator");
            break;
        }
        g_listseparator = value[0];
        break;
    case cfunderscore:
        if (strcmp(value,"on") == 0) {
            char rename[CF_MAXVARSIZE];
            g_underscore_classes=true;
            Verbose("Resetting classes using underscores...\n");

            while(DeleteItemContaining(&g_vheap,
                        g_classtext[g_vsystemhardclass])) { }

            sprintf(rename, "_%s", g_classtext[g_vsystemhardclass]);

            AddClassToHeap(rename);
            break;
        }

        if (strcmp(value,"off") == 0) {
            g_underscore_classes=false;
            break;
        }

        yyerror("illegal value for underscoreclasses");
        break;
    case cfifname:
        if (strlen(value)>15) {
            yyerror("Silly interface name, (should be something link eth0)");
        }

        strcpy(g_vifnameoverride, value);
        g_vifdev[g_vsystemhardclass] = g_vifnameoverride; /* override */
        Debug("Overriding interface with %s\n", g_vifdev[g_vsystemhardclass]);
        break;
    case cfdefcopy:
        if (strcmp(value,"ctime") == 0) {
            g_defaultcopytype = 't';
            return;
        } else if (strcmp(value,"mtime") == 0) {
            g_defaultcopytype = 'm';
            return;
        } else if (strcmp(value,"checksum")==0 || strcmp(value,"sum") == 0) {
            g_defaultcopytype = 'c';
            return;
        } else if (strcmp(value,"byte")==0 || strcmp(value,"binary") == 0) {
            g_defaultcopytype = 'b';
            return;
        }
        yyerror("Illegal default copy type");
        break;
    case cfdefpkgmgr:
        g_defaultpkgmgr = GetPkgMgr(value);
        break;
    default:
        AddMacroValue(g_contextid, lvalue, value);
        break;
    }
}

/* ----------------------------------------------------------------- */

/* child routines in edittools.c */
void
HandleEdit(char *file, char *edit, char *string)
{
    if (! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing Edit no match\n");
        return;
    }

    if (string == NULL) {
        Debug1("Handling Edit of %s, action [%s] with no data if (%s)\n",
                file, edit, g_classbuff);
    } else {
        Debug1("Handling Edit of %s, action [%s] with data <%s> if (%s)\n",
                file, edit, string, g_classbuff);
    }

    if (EditFileExists(file)) {
        AddEditAction(file, edit, string);
    } else {
        InstallEditFile(file, edit, string);
    }
}

/* ----------------------------------------------------------------- */

void
HandleOptionalFileAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item,ebuff,NULL);

    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("File attribute with no value");
    }

    Debug1("HandleOptionalFileAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfrecurse:
        HandleRecurse(value);
        break;
    case cfmode:
        ParseModeString(value, &g_plusmask, &g_minusmask);
        break;
    case cfflags:
        ParseFlagString(value, &g_plusflag, &g_minusflag);
        break;
    case cfowner:
        if (strlen(value) < CF_BUFSIZE) {
            strcpy(g_vuidname, value);
        } else {
            yyerror("Too many owners");
        }
        break;
    case cfgroup:
        if (strlen(value) < CF_BUFSIZE) {
            strcpy(g_vgidname, value);
        } else {
            yyerror("Too many groups");
        }
        break;
    case cfaction:
        g_fileaction = (enum fileactions) GetFileAction(value);
        break;
    case cflinks:
        HandleTravLinks(value);
        break;
    case cfexclude:
        DeleteSlash(value);
        PrependItem(&g_vexcludeparse, value, CF_ANYCLASS);
        break;
    case cfinclude:
        DeleteSlash(value);
        PrependItem(&g_vincludeparse, value, CF_ANYCLASS);
        break;
    case cfignore:
        PrependItem(&g_vignoreparse, value, CF_ANYCLASS);
        break;
    case cfacl:
        PrependItem(&g_vaclbuild, value, CF_ANYCLASS);
        break;
    case cffilter:
        PrependItem(&g_vfilterbuild, value, CF_ANYCLASS);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfchksum:
        HandleChecksum(value);
        break;
    case cfxdev:
        HandleCharSwitch("xdev", value, &g_xdev);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:
        yyerror("Illegal file attribute");
    }
}


/* ----------------------------------------------------------------- */

void
HandleOptionalImageAttribute(char *item)
{
    char value[CF_BUFSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%[^\n]", value);

    if (value[0] == '\0') {
        yyerror("Copy attribute with no value");
    }

    Debug1("HandleOptionalImageAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
#ifdef DARWIN
    case cffindertype:
        if (strlen(value) == 4) {
            strncpy(g_findertype, value, CF_BUFSIZE);
        } else {
            yyerror("Attribute findertype must be exactly 4 characters");
        }
        break;
#endif
    case cfmode:
        ParseModeString(value, &g_plusmask, &g_minusmask);
        break;
    case cfflags:
        ParseFlagString(value, &g_plusflag, &g_minusflag);
        break;
    case cfowner:
        strcpy(g_vuidname, value);
        break;
    case cfgroup:
        strcpy(g_vgidname, value);
        break;
    case cfdest:
        strcpy(g_destination, value);
        break;
    case cfaction:
        strcpy(g_imageaction, value);
        break;
    case cfcompat:
        HandleCharSwitch("oldserver", value, &g_compatibility);
        break;
    case cfforce:
        HandleCharSwitch("force", value, &g_force);
        break;
    case cfforcedirs:
        HandleCharSwitch("forcedirs", value, &g_forcedirs);
        break;
    case cfforceipv4:
        HandleCharSwitch("forceipv4", value, &g_forceipv4);
        break;
    case cfbackup:
        HandleCharSwitch("backup", value, &g_imagebackup);
        break;
    case cfrecurse:
        HandleRecurse(value);
        break;
    case cftype:
        HandleCopyType(value);
        break;
    case cfexclude:
        PrependItem(&g_vexcludeparse, value, CF_ANYCLASS);
        break;
    case cfsymlink:
        PrependItem(&g_vcplnparse, value, CF_ANYCLASS);
        break;
    case cfinclude:
        PrependItem(&g_vincludeparse, value, CF_ANYCLASS);
        break;
    case cfignore:
        PrependItem(&g_vignoreparse, value, CF_ANYCLASS);
        break;
    case cflntype:
        HandleLinkType(value);
        break;
    case cfserver:
        HandleServer(value);
        break;
    case cfencryp:
        HandleCharSwitch("encrypt", value, &g_encrypt);
        break;
    case cfverify:
        HandleCharSwitch("verify", value, &g_verify);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cffailover:
        HandleFailover(value);
        break;
    case cfsize:
        HandleCopySize(value);
        break;
    case cfacl:
        PrependItem(&g_vaclbuild, value, CF_ANYCLASS);
        break;
    case cfpurge:
        HandleCharSwitch("purge", value, &g_purge);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfstealth:
        HandleCharSwitch("stealth", value, &g_stealth);
        break;
    case cftypecheck:
        HandleCharSwitch("typecheck", value, &g_typecheck);
        break;
    case cfrepository:
        strncpy(g_localrepos, value, CF_BUFSIZE - BUFFER_MARGIN);
        break;
    case cffilter:
        PrependItem(&g_vfilterbuild, value, CF_ANYCLASS);
        break;

    case cftrustkey:
        HandleCharSwitch("trustkey", value, &g_trustkey);
        break;
    case cftimestamps:
        HandleTimeStamps(value);
        break;
    case cfxdev:
        HandleCharSwitch("xdev", value, &g_xdev);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:
        yyerror("Illegal copy attribute");
    }
}

/* ----------------------------------------------------------------- */

void
HandleOptionalRequired(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Required/disk attribute with no value");
    }

    Debug1("HandleOptionalRequiredAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cffree:
        HandleRequiredSize(value);
        break;
    case cfscan:
        HandleCharSwitch("scanarrivals", value, &g_scan);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    case cfforce:
        HandleCharSwitch("force", value, &g_force);
        break;
    default:
        yyerror("Illegal disk/required attribute");
    }
}

/* ----------------------------------------------------------------- */

void
HandleOptionalInterface(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("intefaces attribute with no value");
    }

    Debug1("HandleOptionalInterfaceAttribute(value=%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfsetnetmask:
        HandleNetmask(value);
        break;
    case cfsetbroadcast:
        HandleBroadcast(value);
        break;
    case cfsetipaddress:
        HandleIPAddress(value);
        break;
    default:
        yyerror("Illegal interfaces attribute");
   }

}

/* ----------------------------------------------------------------- */

void
HandleOptionalMountablesAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("mount attribute with no value");
    }

    Debug1("HandleOptionalMountItem(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfmountoptions:
        strcpy(g_mountopts, value);
        break;
    case cfreadonly:
        if ((strcmp(value, "on") == 0) || (strcmp(value, "true") == 0)) {
            g_mount_ro = true;
        }
        break;
    default:
        yyerror("Illegal mount option"
                                "(mountoptions/readonly)");
    }
}


/* ----------------------------------------------------------------- */

void
HandleOptionalUnMountAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Unmount attribute with no value");
    }

    Debug1("HandleOptionalUnMountsItem(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfdeldir:
        if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0) {
            g_deletedir = 'y';
            break;
        }

        if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0) {
            g_deletedir = 'n';
            break;
        }
    case cfdelfstab:
        if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0) {
            g_deletefstab = 'y';
            break;
        }

        if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0) {
            g_deletefstab = 'n';
            break;
        }
    case cfforce:
        if (strcmp(value, "true") == 0 || strcmp(value, "yes") == 0) {
            g_force = 'y';
            break;
        }
        if (strcmp(value, "false") == 0 || strcmp(value, "no") == 0) {
            g_force = 'n';
            break;
        }
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:
        yyerror("Illegal unmount option"
                " (deletedir/deletefstab/force)");
   }

}

/* ----------------------------------------------------------------- */

void
HandleOptionalMiscMountsAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Miscmounts attribute with no value");
    }

    Debug1("HandleOptionalMiscMOuntsItem(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfmode:
        {
            struct Item *op;
            struct Item *next;

            if (g_mountoptlist)                     /* just in case */
                DeleteItemList(g_mountoptlist);

            g_mountoptlist = SplitStringAsItemList(value, ',');

            for (op = g_mountoptlist; op != NULL; op = next) {
                /* in case op is deleted */
                next = op->next;
                Debug1("miscmounts option: %s\n", op->name);

                if (strcmp(op->name,"rw") == 0) {
                    g_mountmode = 'w';
                    DeleteItem(&g_mountoptlist, op);
                } else if (strcmp(op->name, "ro") == 0) {
                    g_mountmode = 'o';
                    DeleteItem(&g_mountoptlist, op);
                    }
                else if (strcmp(op->name, "bg") == 0) {
                    /* backgrounded mounts can hang MountFileSystems */
                    DeleteItem(&g_mountoptlist, op);
                }
                /* validate other mount options here */
            }
        }
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:
        yyerror("Illegal miscmounts attribute (rw/ro)");
    }

}

/* ----------------------------------------------------------------- */

void
HandleOptionalTidyAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Tidy attribute with no value");
    }

    Debug1("HandleOptionalTidyAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfrecurse:
        HandleRecurse(value);
        break;
    case cfexclude:
        PrependItem(&g_vexcludeparse, value, CF_ANYCLASS);
        break;
    case cfignore:
        PrependItem(&g_vignoreparse, value, CF_ANYCLASS);
        break;
    case cfinclude:
    case cfpattern:
        strcpy(g_currentitem, value);
        if (*value == '/') {
            yyerror("search pattern begins with / must be a relative name");
        }
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    case cfage:
        HandleAge(value);
        break;
    case cflinks:
        HandleTravLinks(value);
        break;
    case cfsize:
        HandleTidySize(value);
        break;
    case cftype:
        HandleTidyType(value);
        break;
    case cfdirlinks:
        HandleTidyLinkDirs(value);
        break;
    case cfrmdirs:
        HandleTidyRmdirs(value);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfcompress:
        HandleCharSwitch("compress", value, &g_compress);
        break;
    case cffilter:
        PrependItem(&g_vfilterbuild, value, CF_ANYCLASS);
        break;
    case cfxdev:
        HandleCharSwitch("xdev", value, &g_xdev);
        break;
    default:
        yyerror("Illegal tidy attribute");
   }
}

/* ----------------------------------------------------------------- */

void
HandleOptionalDirAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(item, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Directory attribute with no value");
    }

    Debug1("HandleOptionalDirAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfmode:
        ParseModeString(value, &g_plusmask, &g_minusmask);
        break;
    case cfflags:
        ParseFlagString(value, &g_plusflag, &g_minusflag);
        break;
    case cfowner:
        strcpy(g_vuidname, value);
        break;
    case cfgroup:
        strcpy(g_vgidname, value);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:        
        yyerror("Illegal directory attribute");
   }
}


/* ----------------------------------------------------------------- */

void
HandleOptionalDisableAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Disable attribute with no value");
    }

    Debug1("HandleOptionalDisableAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfaction:
        if (strcmp(value, "warn") == 0) {
            g_proaction = 'w';
        } else if (strcmp(value, "delete") == 0) {
            g_proaction = 'd';
        } else {
            yyerror("Unknown action for disable");
        }
        break;
    case cftype:
        HandleDisableFileType(value);
        break;
    case cfrotate:
        HandleDisableRotate(value);
        break;
    case cfsize:
        HandleDisableSize(value);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfrepository:
        strncpy(g_localrepos, value, CF_BUFSIZE - BUFFER_MARGIN);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0,999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    case cfdest:
        strncpy(g_destination, value, CF_BUFSIZE-1);
        break;
    default:
        yyerror("Illegal disable attribute");
    }
}


void
HandleOptionalLinkAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Link attribute with no value");
    }

    Debug1("HandleOptionalLinkAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfaction:
        HandleLinkAction(value);
        break;
    case cftype:
        HandleLinkType(value);
        break;
    case cfexclude:
        PrependItem(&g_vexcludeparse, value, CF_ANYCLASS);
        break;
    case cfinclude:
        PrependItem(&g_vincludeparse, value, CF_ANYCLASS);
        break;
    case cffilter:
        PrependItem(&g_vfilterbuild, value, CF_ANYCLASS);
        break;
    case cfignore:
        PrependItem(&g_vignoreparse, value, CF_ANYCLASS);
        break;
    case cfcopy:
        PrependItem(&g_vcplnparse, value, CF_ANYCLASS);
        break;
    case cfrecurse:
        HandleRecurse(value);
        break;
    case cfcptype:
        HandleCopyType(value);
        break;
    case cfnofile:
        HandleDeadLinks(value);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:        
        yyerror("Illegal link attribute");
    }
}

/* ----------------------------------------------------------------- */

void
HandleOptionalProcessAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Process attribute with no value");
    }

    Debug1("HandleOptionalProcessAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfaction:
        if (strcmp(value, "signal") == 0 || strcmp(value, "do") == 0) {
            g_proaction = 's';
        } else if (strcmp(value, "bymatch") == 0) {
            g_proaction = 'm';
        } else if (strcmp(value, "warn") == 0) {
            g_proaction = 'w';
        } else {
            yyerror("Unknown action for processes");
        }
        break;
    case cfmatches:
        HandleProcessMatches(value);
        g_actionpending = true;
        break;
    case cfsignal:
        HandleProcessSignal(value);
        g_actionpending = true;
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfuseshell:
        HandleUseShell(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfexclude:
        PrependItem(&g_vexcludeparse, value, CF_ANYCLASS);
        break;
    case cfinclude:
        PrependItem(&g_vincludeparse, value, CF_ANYCLASS);
        break;
    case cffilter:
        PrependItem(&g_vfilterbuild, value, CF_ANYCLASS);
        break;
    case cfowner:
        strcpy(g_vuidname, value);
        break;
    case cfgroup:
        strcpy(g_vgidname, value);
        break;
    case cfchdir:
        HandleChDir(value);
        break;
    case cfchroot:
        HandleChRoot(value);
        break;
    case cfumask:
        HandleUmask(value);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:
        yyerror("Illegal process attribute");
    }
}

/* ----------------------------------------------------------------- */

void
HandleOptionalPackagesAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Packages attribute with no value");
    }

    Debug1("HandleOptionalPackagesAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfversion:
        strcpy(g_pkgver,value);
        break;
    case cfcmp:
        g_cmpsense = (enum cmpsense) GetCmpSense(value);
        break;
    case cfpkgmgr:
        g_pkgmgr = (enum pkgmgrs) GetPkgMgr(value);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:
        yyerror("Illegal packages attribute");
    }
}

/* ----------------------------------------------------------------- */

void
HandleOptionalMethodsAttribute(char *item)
{
    char value[CF_BUFSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Methods attribute with no value");
    }

    switch(GetCommAttribute(item)) {
    case cfserver:
        HandleServer(value);
        break;
    case cfaction:
        strncpy(g_actionbuff, value, CF_BUFSIZE-1);
        break;
    case cfretvars:
        strncpy(g_methodfilename, value, CF_BUFSIZE-1);
        break;
    case cfretclasses:
        strncpy(g_methodreturnclasses, value, CF_BUFSIZE-1);
        break;
    case cfforcereplyto:
        strncpy(g_methodforce, value, CF_BUFSIZE-1);
        break;
    case cfsendclasses:
        strncpy(g_methodreplyto, value, CF_MAXVARSIZE-1);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    case cfowner:
        strncpy(g_vuidname, value, CF_BUFSIZE-1);
        break;
    case cfgroup:
        strncpy(g_vgidname, value, CF_BUFSIZE-1);
        break;
    case cfchdir:
        HandleChDir(value);
        break;
    case cfchroot:
        HandleChRoot(value);
        break;
    default:
        yyerror("Illegal methods attribute");
    }
    g_actionpending = true;
}

/* ----------------------------------------------------------------- */

void
HandleOptionalScriptAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Shellcommand attribute with no value");
    }

    Debug1("HandleOptionalScriptAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cftimeout:
        HandleTimeOut(value);
        break;
    case cfuseshell:
        HandleUseShell(value);
        break;
    case cfsetlog:
        HandleCharSwitch("log", value, &g_logp);
        break;
    case cfsetinform:
        HandleCharSwitch("inform", value, &g_informp);
        break;
    case cfowner:
        strcpy(g_vuidname, value);
        break;
    case cfgroup:
        strcpy(g_vgidname, value);
        break;
    case cfdefine:
        HandleDefine(value);
        break;
    case cfelsedef:
        HandleElseDefine(value);
        break;
    case cfumask:
        HandleUmask(value);
        break;
    case cffork:
        HandleCharSwitch("background", value, &g_fork);
        break;
    case cfchdir:
        HandleChDir(value);
        break;
    case cfchroot:
        HandleChRoot(value);
        break;
    case cfpreview:
        HandleCharSwitch("preview", value, &g_preview);
        break;
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:
        yyerror("Illegal shellcommand attribute");
   }
}


void
HandleOptionalAlertsAttribute(char *item)
{
    char value[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(item, ebuff, NULL);
    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        yyerror("Alerts attribute with no value");
    }

    Debug1("HandleOptionalAlertsAttribute(%s)\n", value);

    switch(GetCommAttribute(item)) {
    case cfifelap:
        HandleIntSwitch("ifelapsed", value, &g_pifelapsed, 0, 999999);
        break;
    case cfexpaft:
        HandleIntSwitch("expireafter", value, &g_pexpireafter, 0, 999999);
        break;
    default:
        yyerror("Illegal alerts attribute");
    }
}

/* ----------------------------------------------------------------- */

void
HandleChDir(char *value)
{
    if (!IsAbsoluteFileName(value)) {
        yyerror("chdir is not an absolute directory name");
    }

    strcpy(g_chdir, value);
}

/* ----------------------------------------------------------------- */

void
HandleChRoot(char *value)
{
    if (!IsAbsoluteFileName(value)) {
        yyerror("chdir is not an absolute directory name");
    }

    strcpy(g_chroot, value);
}

/* ----------------------------------------------------------------- */

void HandleFileItem(char *item)
{
    if (strcmp(item,"home") == 0) {
        g_actionpending = true;
        strcpy(g_currentobject, "home");
        return;
    }

    snprintf(g_output, 100, "Unknown attribute %s", item);
    yyerror(g_output);
}


/* ----------------------------------------------------------------- */

void InstallBroadcastItem(char *item)
{
    Debug1("Install broadcast mode (%s)\n", item);

    if ( ! IsInstallable(g_classbuff)) {
        Debug1("Not installing %s, no match\n", item);
        return;
    }

    if (g_vbroadcast[0] != '\0') {
        yyerror("Multiple declaration of variable broadcast");
        FatalError("Redefinition of basic system variable");
    }

    if (strcmp("ones", item) == 0) {
        strcpy(g_vbroadcast, "one");
        return;
    }

    if (strcmp("zeroes", item) == 0) {
        strcpy(g_vbroadcast, "zero");
        return;
    }

    if (strcmp("zeros", item) == 0) {
        strcpy(g_vbroadcast, "zero");
        return;
    }

    yyerror ("Unknown broadcast mode (should be ones, zeros or zeroes)");
    FatalError("Unknown broadcast mode");
}

/* ----------------------------------------------------------------- */

void
InstallDefaultRouteItem(char *item)
{
    struct hostent *hp;
    struct in_addr inaddr;
    char ebuff[CF_EXPANDSIZE];

    Debug1("Install defaultroute mode (%s)\n", item);

    if (!IsDefinedClass(g_classbuff)) {
        Debug1("Not installing %s, no match\n", item);
        return;
    }

    if (g_vdefaultroute[0] != '\0') {
        yyerror("Multiple declaration of variable defaultroute");
        FatalError("Redefinition of basic system variable");
    }

    ExpandVarstring(item, ebuff, NULL);

    if (inet_addr(ebuff) == -1) {
        if ((hp = gethostbyname(ebuff)) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Bad address/host (%s) in default route\n", ebuff);
            CfLog(cferror, g_output, "gethostbyname");
            yyerror ("Bad specification of default packet route: "
                    "hostname or decimal IP address");
        } else {
            bcopy(hp->h_addr, &inaddr, hp->h_length);
            strcpy(g_vdefaultroute, inet_ntoa(inaddr));
        }
    } else {
        strcpy(g_vdefaultroute, ebuff);
    }
}

/* ----------------------------------------------------------------- */

void
InstallGroupRValue(char *item, enum itemtypes type)
{
    char *machine, *user, *domain;
    char ebuff[CF_EXPANDSIZE];

    if (!IsDefinedClass(g_classbuff)) {
        Debug("Not defining class (%s) - no match of container class (%s)\n",
                item, g_classbuff);
        return;
    }

    if (*item == '\'' || *item == '"' || *item == '`') {
        ExpandVarstring(item+1, ebuff, NULL);
    } else {
        ExpandVarstring(item, ebuff, NULL);
    }

    Debug1("HandleGroupRVal(%s) group (%s), type=%d\n", 
            ebuff, g_groupbuff, type);

    switch (type) {
    case simple:
        if (strcmp(ebuff, g_vdefaultbinserver.name) == 0) {
            AddClassToHeap(g_groupbuff);
            break;
        }

        if (strcmp(ebuff,g_vfqname) == 0) {
            AddClassToHeap(g_groupbuff);
            break;
        }

        /* group reference */
        if (IsItemIn(g_vheap,ebuff)) {
            AddClassToHeap(g_groupbuff);
            break;
        }

        if (IsDefinedClass(ebuff)) {
            AddClassToHeap(g_groupbuff);
            break;
        }

        Debug("[No match of class %s]\n\n",ebuff);
        break;
    case netgroup:
        setnetgrent(ebuff);
        while (getnetgrent(&machine, &user,&domain)) {
            if (strcmp(machine, g_vdefaultbinserver.name) == 0) {
                Debug1("Matched %s in netgroup %s\n", machine, ebuff);
                AddClassToHeap(g_groupbuff);
                break;
            }

            if (strcmp(machine, g_vfqname) == 0) {
                Debug1("Matched %s in netgroup %s\n", machine, ebuff);
                AddClassToHeap(g_groupbuff);
                break;
            }
        }
        endnetgrent();
        break;
    case groupdeletion:
        setnetgrent(ebuff);
        while (getnetgrent(&machine, &user, &domain)) {
            if (strcmp(machine, g_vdefaultbinserver.name) == 0) {
                Debug1("Matched delete item %s in netgroup %s\n",
                        machine, ebuff);
                DeleteItemStarting(&g_vheap, g_groupbuff);
                break;
            }

            if (strcmp(machine, g_vfqname) == 0) {
                Debug1("Matched delete item %s in netgroup %s\n",
                        machine, ebuff);
                DeleteItemStarting(&g_vheap, g_groupbuff);
                break;
            }

        }

        endnetgrent();
        break;
    case classscript:
        if (ebuff[0] != '/') {
            yyerror("Quoted scripts must begin with / for absolute path");
            break;
        }
        SetClassesOnScript(ebuff, g_groupbuff, NULL, false);
        break;
    case deletion:
        if (IsDefinedClass(ebuff)) {
            DeleteItemStarting(&g_vheap, g_groupbuff);
        }
        break;
    default:
        yyerror("Software error");
        FatalError("Unknown item type");
    }
}

/* ----------------------------------------------------------------- */

void
HandleHomePattern(char *pattern)
{
    char ebuff[CF_EXPANDSIZE];

    ExpandVarstring(pattern, ebuff, "");
    AppendItem(&g_vhomepatlist, ebuff, g_classbuff);
}

/* ----------------------------------------------------------------- */

void
AppendNameServer(char *item)
{
    char ebuff[CF_EXPANDSIZE];

    Debug1("Installing item (%s) in the nameserver list\n", item);

    if ( ! IsInstallable(g_classbuff)) {
        Debug1("Not installing %s, no match\n", item);
        return;
    }

    ExpandVarstring(item, ebuff, "");
    AppendItem(&g_vresolve, ebuff, g_classbuff);
}

/* ----------------------------------------------------------------- */

void
AppendImport(char *item)
{
    char ebuff[CF_EXPANDSIZE];

    if (!IsInstallable(g_classbuff)) {
        Debug1("Not installing %s, no match\n", item);
        return;
    }

    if (strcmp(item, g_vcurrentfile) == 0) {
        yyerror("A file cannot import itself");
        FatalError("Infinite self-reference in class inheritance");
    }

    Debug1("\n\n [Installing item (%s) in the import list]\n\n", item);

    ExpandVarstring(item, ebuff, "");
    AppendItem(&g_vimport, ebuff, g_classbuff);
}

/* ----------------------------------------------------------------- */

void
InstallObject(char *name)
{
    struct cfObject *ptr;

    Debug1("Adding object %s", name);

    /*
    if ( ! IsInstallable(g_classbuff))
    {
    return;
    }
    */

    for (ptr = g_vobj; ptr != NULL; ptr=ptr->next) {
        if (strcmp(ptr->scope, name) == 0) {
            Debug("Object %s already exists\n", name);
            return;
        }
    }

    if ((ptr = (struct cfObject *)malloc(sizeof(struct cfObject))) == NULL) {
        FatalError("Memory Allocation failed for cfObject");
    }

    if (g_vobjtop == NULL) {
        g_vobj = ptr;
    } else {
        g_vobjtop->next = ptr;
    }

    InitHashTable(ptr->hashtable);

    ptr->next = NULL;
    ptr->scope = strdup(name);
    g_vobjtop = ptr;
}


/* ----------------------------------------------------------------- */

void
InstallHomeserverItem(char *item)
{
    char ebuff[CF_EXPANDSIZE];

    if (! IsInstallable(g_classbuff)) {
        Debug1("Not installing %s, no match\n", item);
        return;
    }

    ExpandVarstring(item, ebuff, "");
    AppendItem(&g_vhomeservers, ebuff, g_classbuff);
}

/* ----------------------------------------------------------------- */

/* Install if matches classes */
void
InstallBinserverItem(char *item)
{
    char ebuff[CF_EXPANDSIZE];

    if (! IsInstallable(g_classbuff)) {
        Debug1("Not installing %s, no match\n", item);
        return;
    }

    ExpandVarstring(item, ebuff,"");
    AppendItem(&g_vbinservers, ebuff, g_classbuff);
}

/* ----------------------------------------------------------------- */

void
InstallMailserverPath(char *path)
{
    if ( ! IsInstallable(g_classbuff)) {
        Debug1("Not installing %s, no match\n", path);
        return;
    }

    if (g_vmailserver[0] != '\0') {
        FatalError("Redefinition of mailserver");
    }

    strcpy(g_vmailserver, path);

    Debug1("Installing mailserver (%s) for group (%s)", path, g_groupbuff);
}

/* ----------------------------------------------------------------- */

void
InstallLinkItem (char *from, char *to)
{
    struct Link *ptr;
    char buffer[CF_EXPANDSIZE], buffer2[CF_EXPANDSIZE];
    char ebuff[CF_EXPANDSIZE];

    Debug1("Storing Link: (From)%s->(To)%s\n", from, to);

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing link no match\n");
        return;
    }

    ExpandVarstring(from,ebuff,"");

    if (strlen(ebuff) > 1) {
        DeleteSlash(ebuff);
    }

    ExpandVarstring(to,buffer,"");

    if (strlen(buffer) > 1) {
        DeleteSlash(buffer);
    }

    ExpandVarstring(g_allclassbuffer, buffer2, "");

    if ((ptr = (struct Link *)malloc(sizeof(struct Link))) == NULL) {
        FatalError("Memory Allocation failed for InstallListItem() #1");
    }

    if ((ptr->from = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallListItem() #2");
    }

    if ((ptr->to = strdup(buffer)) == NULL) {
        FatalError("Memory Allocation failed for InstallListItem() #3");
    }

    if ((ptr->classes = strdup(g_classbuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallListItem() #4");
    }

    if ((ptr->defines = strdup(buffer2)) == NULL) {
        FatalError("Memory Allocation failed for InstallListItem() #4a");
    }

    ExpandVarstring(g_elseclassbuffer,buffer,"");

    if ((ptr->elsedef = strdup(buffer)) == NULL) {
        FatalError("Memory Allocation failed xxx");
    }

    AddInstallable(ptr->defines);
    AddInstallable(ptr->elsedef);

    /* First element in the list */
    if (g_vlinktop == NULL) {
        g_vlink = ptr;
    } else {
        g_vlinktop->next = ptr;
    }

    if (strlen(ptr->from) > 1) {
        DeleteSlash(ptr->from);
    }

    if (strlen(ptr->to) > 1) {
        DeleteSlash(ptr->to);
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ptr->force = g_forcelink;
    ptr->silent = g_linksilent;
    ptr->type = g_linktype;
    ptr->copytype = g_copytype;
    ptr->next = NULL;
    ptr->copy = g_vcplnparse;
    ptr->exclusions = g_vexcludeparse;
    ptr->inclusions = g_vincludeparse;
    ptr->ignores = g_vignoreparse;
    ptr->filters=g_vfilterbuild;
    ptr->recurse = g_vrecurse;
    ptr->nofile = g_deadlinks;
    ptr->log = g_logp;
    ptr->inform = g_informp;
    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);

    g_vlinktop = ptr;

    if (ptr->recurse != 0) {
        yyerror("Recursion can only be used with +> multiple links");
    }

    InitializeAction();
}

/* ----------------------------------------------------------------- */

void
InstallLinkChildrenItem(char *from, char *to)
{
    struct Link *ptr;
    char *sp; 
    char buffer[CF_EXPANDSIZE];
    char ebuff[CF_EXPANDSIZE];
    struct TwoDimList *tp = NULL;

    Debug1("Storing Linkchildren item: %s -> %s\n", from, to);

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing linkchildren no match\n");
        return;
    }

    ExpandVarstring(from, ebuff, "");
    ExpandVarstring(g_allclassbuffer, buffer, "");

    Build2DListFromVarstring(&tp, to, '/');

    Set2DList(tp);

    for (sp = Get2DListEnt(tp); sp != NULL; sp = Get2DListEnt(tp)) {
        if ((ptr = (struct Link *)malloc(sizeof(struct Link))) == NULL) {
            FatalError("Memory Allocation failed for "
                    "InstallListChildrenItem() #1");
        }

        if ((ptr->from = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for "
                    "InstallLinkchildrenItem() #2");
        }

        if ((ptr->to = strdup(sp)) == NULL) {
            FatalError("Memory Allocation failed for "
                    "InstallLinkChildrenItem() #3");
        }

        if ((ptr->classes = strdup(g_classbuff)) == NULL) {
            FatalError("Memory Allocation failed for "
                    "InstallLinkChildrenItem() #3");
        }

        if ((ptr->defines = strdup(buffer)) == NULL) {
            FatalError("Memory Allocation failed for "
                    "InstallListItem() #4a");
        }

        ExpandVarstring(g_elseclassbuffer,buffer,"");

        if ((ptr->elsedef = strdup(buffer)) == NULL) {
            FatalError("Memory Allocation failed for "
                    "Installrequied() #2");
        }

        AddInstallable(ptr->defines);
        AddInstallable(ptr->elsedef);

        /* First element in the list */
        if (g_vchlinktop == NULL) {
            g_vchlink = ptr;
        } else {
            g_vchlinktop->next = ptr;
        }

        if (g_pifelapsed != -1) {
            ptr->ifelapsed = g_pifelapsed;
        } else {
            ptr->ifelapsed = g_vifelapsed;
        }

        if (g_pexpireafter != -1) {
            ptr->expireafter = g_pexpireafter;
        } else {
            ptr->expireafter = g_vexpireafter;
        }

        ptr->force = g_forcelink;
        ptr->silent = g_linksilent;
        ptr->type = g_linktype;
        ptr->next = NULL;
        ptr->copy = g_vcplnparse;
        ptr->nofile = g_deadlinks;
        ptr->exclusions = g_vexcludeparse;
        ptr->inclusions = g_vincludeparse;
        ptr->ignores = g_vignoreparse;
        ptr->recurse = g_vrecurse;
        ptr->log = g_logp;
        ptr->inform = g_informp;
        ptr->done = 'n';
        ptr->scope = strdup(g_contextid);

        g_vchlinktop = ptr;

        if (ptr->recurse != 0 && strcmp(ptr->to,"linkchildren") == 0) {
            yyerror("Sorry don't know how to recurse with "
                    "linkchildren keyword");
        }
    }
    Delete2DList(tp);
    InitializeAction();
}


/* ----------------------------------------------------------------- */

void
InstallRequiredPath(char *path, int freespace)
{
    struct Disk *ptr;
    char *sp;
    char buffer[CF_EXPANDSIZE];
    char ebuff[CF_EXPANDSIZE];
    struct TwoDimList *tp = NULL;

    Build2DListFromVarstring(&tp,path,'/');

    Set2DList(tp);

    for (sp = Get2DListEnt(tp); sp != NULL; sp = Get2DListEnt(tp)) {
        Debug1("Installing item (%s) in the required list\n",sp);

        if ( ! IsInstallable(g_classbuff)) {
            InitializeAction();
            Debug1("Not installing %s, no match\n", sp);
            return;
        }

        ebuff[0] = '\0';
        ExpandVarstring(sp, ebuff, "");

        if ((ptr = (struct Disk *)malloc(sizeof(struct Disk))) == NULL) {
            FatalError("Memory Allocation failed for InstallRequired() #1");
        }

        if ((ptr->name = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for InstallRequired() #2");
        }

        if ((ptr->classes = strdup(g_classbuff)) == NULL) {
            FatalError("Memory Allocation failed for InstallRequired() #2");
        }

        ExpandVarstring(g_allclassbuffer, buffer, "");

        if ((ptr->define = strdup(buffer)) == NULL) {
            FatalError("Memory Allocation failed for Installrequied() #2");
        }

        ExpandVarstring(g_elseclassbuffer, buffer, "");

        if ((ptr->elsedef = strdup(buffer)) == NULL) {
            FatalError("Memory Allocation failed for Installrequied() #2");
        }

        /* First element in the list */
        if (g_vrequired == NULL) {
            g_vrequired = ptr;
        } else {
            g_vrequiredtop->next = ptr;
        }

        AddInstallable(ptr->define);
        AddInstallable(ptr->elsedef);

        if (g_pifelapsed != -1) {
            ptr->ifelapsed = g_pifelapsed;
        } else {
            ptr->ifelapsed = g_vifelapsed;
        }

        if (g_pexpireafter != -1) {
            ptr->expireafter = g_pexpireafter;
        } else {
            ptr->expireafter = g_vexpireafter;
        }

        ptr->freespace = freespace;
        ptr->next = NULL;
        ptr->log = g_logp;
        ptr->inform = g_informp;
        ptr->done = 'n';
        ptr->scanarrivals = g_scan;
        ptr->scope = strdup(g_contextid);

        /* HvB : Bas van der Vlies */
        ptr->force = g_force;

        g_vrequiredtop = ptr;
    }
    Delete2DList(tp);
    InitializeAction();
}

/* ----------------------------------------------------------------- */

void
InstallMountableItem(char *path, char *mnt_opts, flag readonly)
{
    struct Mountables *ptr;
    char ebuff[CF_EXPANDSIZE];

    Debug1("Adding mountable %s to list\n", path);

    if (!IsDefinedClass(g_classbuff)) {
        return;
    }

    ExpandVarstring(path, ebuff, "");

    /*
    * Check if mount entry already exists
    */
    if ( g_vmountables != NULL ) {
        for (ptr = g_vmountables; ptr != NULL; ptr = ptr->next) {
            if ( strcmp(ptr->filesystem, ebuff) == 0 ) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "Only one definition per mount allowed: %s\n",
                        ptr->filesystem);
                yyerror(g_output);
                return;
            }
        }
    }

    if ((ptr = (struct Mountables *)malloc(sizeof(struct Mountables))) == NULL) {
        FatalError("Memory Allocation failed for InstallMountableItem() #1");
    }

    if ((ptr->filesystem = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallMountableItem() #2");
    }

    if ( mnt_opts[0] != '\0' ) {
        if ( (ptr->mountopts = strdup(mnt_opts)) == NULL ) {
            FatalError("Memory Allocation failed for "
                    "InstallMountableItem() #3");
        }
    } else {
        ptr->mountopts = NULL;
    }

    ptr->readonly = readonly;
    ptr->next = NULL;
    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);

    /* First element in the list */
    if (g_vmountablestop == NULL) {
        g_vmountables = ptr;
    } else {
        g_vmountablestop->next = ptr;
    }
    g_vmountablestop = ptr;
}

/* ----------------------------------------------------------------- */

void
AppendUmount(char *path, char deldir, char delfstab, char force)
{
    struct UnMount *ptr;
    char ebuff[CF_EXPANDSIZE];

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing %s, no match\n", path);
        return;
    }

    ExpandVarstring(path, ebuff, "");

    if ((ptr = (struct UnMount *)malloc(sizeof(struct UnMount))) == NULL) {
        FatalError("Memory Allocation failed for AppendUmount() #1");
    }

    if ((ptr->name = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for AppendUmount() #2");
    }

    if ((ptr->classes = strdup(g_classbuff)) == NULL) {
        FatalError("Memory Allocation failed for AppendUmount() #5");
    }

    /* First element in the list */
    if (g_vunmounttop == NULL) {
        g_vunmount = ptr;
    } else {
        g_vunmounttop->next = ptr;
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ptr->next = NULL;
    ptr->deletedir = deldir;  /* t/f - true false */
    ptr->deletefstab = delfstab;
    ptr->force = force;
    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);

    g_vunmounttop = ptr;
}

/* ----------------------------------------------------------------- */

void
AppendMiscMount(char *from, char *onto, char *opts)
{
    struct MiscMount *ptr;
    char ebuff[CF_EXPANDSIZE];

    Debug1("Adding misc mountable %s %s (%s) to list\n", from, onto, opts);

    if ( ! IsInstallable(g_classbuff)) {
        Debug1("Not installing %s, no match\n", from);
        return;
    }

    if ((ptr = (struct MiscMount *)malloc(sizeof(struct MiscMount))) == NULL) {
        FatalError("Memory Allocation failed for AppendMiscMount #1");
    }

    ExpandVarstring(from, ebuff, "");

    if ((ptr->from = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for AppendMiscMount() #2");
    }

    ExpandVarstring(onto, ebuff, "");

    if ((ptr->onto = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for AppendMiscMount() #3");
    }

    if ((ptr->options = strdup(opts)) == NULL) {
        FatalError("Memory Allocation failed for AppendMiscMount() #4");
    }

    if ((ptr->classes = strdup(g_classbuff)) == NULL) {
        FatalError("Memory Allocation failed for AppendMiscMount() #5");
    }

    /* First element in the list */
    if (g_vmiscmounttop == NULL) {
        g_vmiscmount = ptr;
    } else {
        g_vmiscmounttop->next = ptr;
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ptr->next = NULL;
    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);
    g_vmiscmounttop = ptr;
}


/* ----------------------------------------------------------------- */

void
AppendIgnore(char *path)
{
    struct TwoDimList *tp = NULL;
    char *sp;

    Debug1("Installing item (%s) in the ignore list\n", path);

    if (!IsInstallable(g_classbuff)) {
        Debug1("Not installing %s, no match\n", path);
        return;
    }

    Build2DListFromVarstring(&tp, path, '/');

    Set2DList(tp);

    for (sp = Get2DListEnt(tp); sp != NULL; sp = Get2DListEnt(tp)) {
        AppendItem(&g_vignore, sp, g_classbuff);
    }

    Delete2DList(tp);
}

/* ----------------------------------------------------------------- */

void
InstallPending(enum actions action)
{
    if (g_actionpending) {
        Debug1("\n[BEGIN InstallPending %s\n", g_actiontext[action]);
    } else {
        Debug1("(No actions pending in %s)\n", g_actiontext[action]);
        return;
    }

    switch (action) {
    case filters:
        InstallFilterTest(g_filtername, g_currentitem, g_filterdata);
        g_currentitem[0] = '\0';
        g_filterdata[0] = '\0';
        break;
    case strategies:
        AddClassToStrategy(g_strategyname, g_currentitem, g_strategydata);
        break;
    case resolve:
        AppendNameServer(g_currentobject);
        break;
    case files:
        InstallFileListItem(g_currentobject, g_plusmask, g_minusmask,
                g_fileaction, g_vuidname, g_vgidname, g_vrecurse,
                (char)g_ptravlinks, g_checksum);
        break;
    case processes:
        InstallProcessItem(g_expr, g_restart, g_promatches, g_procomp,
                g_prosignal, g_proaction, g_classbuff, g_useshell, 
                g_vuidname, g_vgidname);
        break;
    case image:
        InstallImageItem(g_findertype, g_currentobject, g_plusmask,
                g_minusmask, g_destination, g_imageaction, g_vuidname,
                g_vgidname, g_imgsize, g_imgcomp, g_vrecurse, g_copytype,
                g_linktype, g_cfserver);
        break;
    case ignore:
        AppendIgnore(g_currentobject);
        break;
    case tidy:
        if (g_vage >= 99999) {
            yyerror("Must specify an age for tidy actions");
            return;
        }
        InstallTidyItem(g_currentobject, g_currentitem, g_vrecurse, g_vage,
                (char)g_ptravlinks, g_tidysize, g_agetype, g_linkdirs,
                g_tidydirs, g_classbuff);
        break;
    case makepath:
        InstallMakePath(g_currentobject, g_plusmask, g_minusmask, 
                g_vuidname, g_vgidname);
        break;
    case methods:
        InstallMethod(g_currentobject, g_actionbuff);
        break;
    case rename_disable:
    case disable:
        AppendDisable(g_currentobject, g_currentitem, g_rotate, g_discomp,
                g_disablesize);
        break;
    case shellcommands:
        AppendScript(g_currentobject, g_vtimeout, g_useshell, g_vuidname,
                g_vgidname);
        InitializeAction();
        break;
    case alerts:
        if (strcmp(g_classbuff,"any") == 0) {
            yyerror("Alerts cannot be in class any - probably a mistake");
        } else {
            Debug("Install %s if %s\n", g_currentobject, g_classbuff);
            InstallItem(&g_valerts, g_currentobject, g_classbuff, 0, 0);
        }
        InitializeAction();
        break;
    case interfaces:
        AppendInterface(g_vifname, g_linkto, g_destination, g_currentobject);
        break;
    case disks:
    case required:
        InstallRequiredPath(g_currentobject, g_imgsize);
        break;
    case mountables:
        InstallMountableItem(g_currentobject, g_mountopts, g_mount_ro);
        break;
    case misc_mounts:
        if ((strlen(g_mountfrom) != 0) && (strlen(g_mountonto) != 0)) {
            switch (g_mountmode) {
            case 'o':
                strcpy(g_mountopts, "ro");
                break;
            case 'w':
                strcpy(g_mountopts, "rw");
                break;
            default:
                printf("Install pending, miscmount, shouldn't happen\n");
                g_mountopts[0] = '\0'; /* no mount mode set! */
            }

            /* valid mount mode set */
            if (strlen(g_mountopts) != 0) {
                struct Item *op;

                for (op = g_mountoptlist; op != NULL; op = op->next) {
                    if (BufferOverflow(g_mountopts, op->name)) {
                        printf(" culprit: InstallPending, skipping "
                                "miscmount %s %s\n",
                                g_mountfrom, g_mountonto);
                        return;
                    }
                    strcat(g_mountopts, ",");
                    strcat(g_mountopts, op->name);
                }
                AppendMiscMount(g_mountfrom, g_mountonto, g_mountopts);
            }
        }
        InitializeAction();
        break;
    case unmounta:
            AppendUmount(g_currentobject, g_deletedir, g_deletefstab, 
                    g_force);
            break;
    case links:
        if (g_linkto[0] == '\0') {
            return;
        }

        if (g_action_is_linkchildren) {
            InstallLinkChildrenItem(g_linkfrom, g_linkto);
            g_action_is_linkchildren = false;
        } else if (g_action_is_link) {
            InstallLinkItem(g_linkfrom, g_linkto);
            g_action_is_link = false;
        } else {
            /* Don't have whole command */
            return;
        }
        break;
    case packages:
        InstallPackagesItem(g_currentobject, g_pkgver, g_cmpsense, g_pkgmgr);
        break;
    }
    Debug1("END InstallPending]\n\n");
}

/* ----------------------------------------------------------------- */
/* Level 3                                                         */
/* ----------------------------------------------------------------- */

void
HandleCharSwitch(char *name, char *value, char *pflag)
{
    Debug1("HandleCharSwitch(%s=%s)\n", name, value);

    if ((strcmp(value, "true") == 0) || (strcmp(value, "on") == 0)) {
        *pflag = 'y';
        return;
    }

    if ((strcmp(value, "false") == 0) || (strcmp(value, "off") == 0)) {
        *pflag = 'n';
        return;
    }

    if ((strcmp(value, "timestamp") == 0)) {
        *pflag = 's';
        return;
    }

    if (g_action == image) {
        printf("Switch %s=(true/false/timestamp)|(on/off)", name);
        yyerror("Illegal switch value");
    } else {
        printf("Switch %s=(true/false)|(on/off)", name);
        yyerror("Illegal switch value");
    }
}

/* ----------------------------------------------------------------- */

void
HandleIntSwitch(char *name, char *value, int *pflag, int min, int max)
{
    int numvalue = -17267592; /* silly number, never happens */

    Debug1("HandleIntSwitch(%s=%s,%d,%d)\n", name, value, min, max);

    sscanf(value, "%d", &numvalue);

    if (numvalue == -17267592) {
        snprintf(g_output, CF_BUFSIZE,
                "Integer expected as argument to %s", name);
        yyerror(g_output);
        return;
    }

    if ((numvalue <= max) && (numvalue >= min)) {
        *pflag = numvalue;
        return;
    } else {
        snprintf(g_output, CF_BUFSIZE,
                "Integer %s out of range (%d <= %s <= %d)",
                name, min,name, max);
        yyerror(g_output);
    }
}

/* ----------------------------------------------------------------- */

int
EditFileExists(char *file)
{
    struct Edit *ptr;
    char ebuff[CF_EXPANDSIZE];

    ExpandVarstring(file, ebuff, "");

    for (ptr=g_veditlist; ptr != NULL; ptr=ptr->next) {
        if (strcmp(ptr->fname, ebuff) == 0) {
            return true;
        }
    }
    return false;
}

/* ----------------------------------------------------------------- */

/* Buffer initially contains whole exec string */
void
GetExecOutput(char *command, char *buffer)
{
    int offset = 0;
    char *sp;
    char line[CF_BUFSIZE];
    FILE *pp;

    Debug1("GetExecOutput(%s,%s)\n", command, buffer);

    if (g_dontdo) {
        return;
    }

    if ((pp = cfpopen(command,"r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Couldn't open pipe to command %s\n", command);
        CfLog(cfinform, g_output, "pipe");
        return;
    }

    memset(buffer, 0, CF_BUFSIZE);

    while (!feof(pp)) {

        /* abortable */
        if (ferror(pp)) {
            fflush(pp);
            break;
        }

        ReadLine(line, CF_BUFSIZE, pp);

        /* abortable */
        if (ferror(pp)) {
            fflush(pp);
            break;
        }

        for (sp = line; *sp != '\0'; sp++) {
            if (*sp == '\n') {
                *sp = ' ';
            }
        }

        if (strlen(line)+offset > CF_BUFSIZE-10) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "Buffer exceeded %d bytes in exec %s\n",
                    CF_MAXVARSIZE, command);
            CfLog(cferror, g_output,"");
            break;
        }

        snprintf(buffer+offset, CF_BUFSIZE, "%s ", line);
        offset += strlen(line)+1;
    }

    if (offset > 0) {
        Chop(buffer);
    }

    Debug("GetExecOutput got: [%s]\n", buffer);

    cfpclose(pp);
}

/* ----------------------------------------------------------------- */

void
InstallEditFile(char *file, char *edit, char *data)
{
    struct Edit *ptr;
    char ebuff[CF_EXPANDSIZE];

    if (data == NULL) {
        Debug1("InstallEditFile(%s,%s,-) with classes %s\n",
                file, edit, g_classbuff);
    } else {
        Debug1("InstallEditFile(%s,%s,%s) with classes %s\n",
                file, edit, data, g_classbuff);
    }

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing Edit no match\n");
        return;
    }

    ExpandVarstring(file, ebuff, "");

    if ((ptr = (struct Edit *)malloc(sizeof(struct Edit))) == NULL) {
        FatalError("Memory Allocation failed for InstallEditFile() #1");
    }

    if ((ptr->fname = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallEditFile() #2");
    }

    /* First element in the list */
    if (g_veditlisttop == NULL) {
        g_veditlist = ptr;
    } else {
        g_veditlisttop->next = ptr;
    }

    if (strncmp(ebuff, "home", 4) == 0 && strlen(ebuff) < 6) {
        yyerror("Can't edit home directories: missing a filename after home");
    }

    if (strlen(g_localrepos) > 0) {
        ExpandVarstring(g_localrepos, ebuff, "");
        ptr->repository = strdup(ebuff);
    } else {
        ptr->repository = NULL;
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);
    ptr->recurse = 0;
    ptr->useshell = 'y';
    ptr->binary = 'n';
    ptr->next = NULL;
    ptr->actions = NULL;
    ptr->filters = NULL;
    ptr->ignores = NULL;
    ptr->umask = g_umask;
    ptr->exclusions = NULL;
    ptr->inclusions = NULL;
    g_veditlisttop = ptr;
    AddEditAction(file, edit, data);
}

/* ----------------------------------------------------------------- */

void
AddEditAction(char *file, char *edit, char *data)
{
    struct Edit *ptr;
    struct Edlist *top,*new;
    char varbuff[CF_EXPANDSIZE];
    char ebuff[CF_EXPANDSIZE];

    if (data == NULL) {
        Debug2("AddEditAction(%s,%s,-)\n", file, edit);
    } else {
        Debug2("AddEditAction(%s,%s,%s)\n", file, edit, data);
    }

    if ( ! IsInstallable(g_classbuff)) {
        Debug1("Not installing Edit no match\n");
        return;
    }

    for (ptr = g_veditlist; ptr != NULL; ptr=ptr->next) {
        ExpandVarstring(file, varbuff, "");

        if (strcmp(ptr->fname,varbuff) == 0) {
            if ((new = (struct Edlist *)malloc(sizeof(struct Edlist))) == NULL) {
                FatalError("Memory Allocation failed for AddEditAction() #1");
            }

            if (ptr->actions == NULL) {
                ptr->actions = new;
            } else {
                for (top = ptr->actions; top->next != NULL; top=top->next) { }
                    top->next = new;
            }

            if (data == NULL) {
                new->data = NULL;
            } else {
                /* Expand any variables */
                ExpandVarstring(data, ebuff, "");

                if ((new->data = strdup(ebuff)) == NULL) {
                    FatalError("Memory Allocation failed for "
                            "AddEditAction() #1");
                }
            }

            new->next = NULL;

            if ((new->classes = strdup(g_classbuff)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallEditFile() #3");
            }

            if ((new->code = EditActionsToCode(edit)) == NoEdit) {
                snprintf(g_output, CF_BUFSIZE, 
                        "Unknown edit action \"%s\"", edit);
                yyerror(g_output);
            }

            switch(new->code) {
            case EditUmask:
                HandleUmask(data);
                ptr->umask = g_umask;
            case EditIgnore:
                PrependItem(&(ptr->ignores), data, CF_ANYCLASS);
                break;
            case EditExclude:
                PrependItem(&(ptr->exclusions), data, CF_ANYCLASS);
                break;
            case EditInclude:
                PrependItem(&(ptr->inclusions), data, CF_ANYCLASS);
                break;
            case EditRecurse:
                if (strcmp(data,"inf") == 0) {
                    ptr->recurse = CF_INF_RECURSE;
                } else {
                    ptr->recurse = atoi(data);
                    if (ptr->recurse < 0) {
                        yyerror("Illegal recursion value");
                    }
                }
                break;

            case Append:
                if (g_editgrouplevel == 0) {
                    yyerror("Append used outside of Group - non-convergent");
                }

            case EditMode:
                if (strcmp(data, "Binary") == 0) {
                    ptr->binary = 'y';
                }
                break;

            case BeginGroupIfNoMatch:
            case BeginGroupIfNoLineMatching:
            case BeginGroupIfNoLineContaining:
            case BeginGroupIfNoSuchLine:
            case BeginGroupIfDefined:
            case BeginGroupIfNotDefined:
            case BeginGroupIfFileIsNewer:
            case BeginGroupIfFileExists:
                g_editgrouplevel++;
                break;

            case EndGroup:
                g_editgrouplevel--;
                if (g_editgrouplevel < 0) {
                    yyerror("EndGroup without Begin");
                }
                break;

            case ReplaceAll:
                if (g_searchreplacelevel > 0) {
                    yyerror("ReplaceAll without With before or at line");
                }

                g_searchreplacelevel++;
                break;

            case With:
                g_searchreplacelevel--;
                break;

            case ForEachLineIn:
                if (g_foreachlevel > 0) {
                    yyerror("Nested ForEach loops not allowed");
                }

                g_foreachlevel++;
                break;

            case EndLoop:
                g_foreachlevel--;
                if (g_foreachlevel < 0) {
                    yyerror("EndLoop without ForEachLineIn");
                }
                break;
            case DefineInGroup:
                if (g_editgrouplevel < 0) {
                    yyerror("DefineInGroup outside a group");
                }
                AddInstallable(new->data);
                break;
            case SetLine:
                if (g_foreachlevel > 0) {
                    yyerror("SetLine inside ForEachLineIn loop");
                }
                break;
            case FixEndOfLine:
                if (strlen(data) > CF_EXTRA_SPACE - 1) {
                    yyerror("End of line type is too long!");
                    printf("          (max %d characters allowed)\n",
                            CF_EXTRA_SPACE);
                }
                break;
            case ReplaceLinesMatchingField:
                if (atoi(data) == 0) {
                    yyerror("Argument must be an integer, greater than zero");
                }
                break;
            case ElseDefineClasses:
            case EditFilter:
            case DefineClasses:
                if (g_editgrouplevel > 0 || g_foreachlevel > 0) {
                    yyerror("Class definitions inside conditionals or "
                            "loops are not allowed. Did you mean "
                            "DefineInGroup?");
                }
                AddInstallable(new->data);
                break;
            case EditRepos:
                ptr->repository = strdup(data);
                break;
            }
            return;
        }
    }
    printf("cfng: software error - no file matched installing %s edit\n",file);
}

/* ----------------------------------------------------------------- */

enum editnames
EditActionsToCode(char *edit)
{
    int i;

    Debug2("EditActionsToCode(%s)\n",edit);

    for (i = 0; g_veditnames[i] != '\0'; i++) {
        if (strcmp(g_veditnames[i], edit) == 0) {
            return (enum editnames) i;
        }
    }

    return (NoEdit);
}


/* ----------------------------------------------------------------- */

void
AppendInterface(char *ifname, char *address, char *netmask, char *broadcast)
{
    struct Interface *ifp;

    Debug1("Installing item (%s:%s:%s) in the interfaces list\n",
            ifname, netmask, broadcast);

    if (!IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing %s, no match\n", ifname);
        return;
    }

    if (strlen(netmask) < 7) {
        yyerror("illegal or missing netmask");
        InitializeAction();
        return;
    }

    if (strlen(broadcast) < 3) {
        yyerror("illegal or missing broadcast address");
        InitializeAction();
        return;
    }

    if ((ifp = (struct Interface *)malloc(sizeof(struct Interface))) == NULL) {
        FatalError("Memory Allocation failed for AppendInterface() #1");
    }

    if ((ifp->classes = strdup(g_classbuff)) == NULL) {
        FatalError("Memory Allocation failed for Appendinterface() #2");
    }

    if ((ifp->ifdev = strdup(ifname)) == NULL) {
        FatalError("Memory Allocation failed for AppendInterface() #3");
    }

    if ((ifp->netmask = strdup(netmask)) == NULL) {
        FatalError("Memory Allocation failed for AppendInterface() #3");
    }

    if ((ifp->broadcast = strdup(broadcast)) == NULL) {
        FatalError("Memory Allocation failed for AppendInterface() #3");
    }

    if ((ifp->ipaddress = strdup(address)) == NULL) {
        FatalError("Memory Allocation failed for AppendInterface() #3");
    }

    /* First element in the list */
    if (g_viflisttop == NULL) {
        g_viflist = ifp;
    } else {
        g_viflisttop->next = ifp;
    }

    ifp->next = NULL;
    ifp->done = 'n';
    ifp->scope = strdup(g_contextid);

    g_viflisttop = ifp;

    InitializeAction();
}

/* ----------------------------------------------------------------- */

void
AppendScript(char *item, int timeout, char useshell,
        char *uidname, char *gidname)
{
    struct TwoDimList *tp = NULL;
    struct ShellComm *ptr;
    struct passwd *pw;
    struct group *gw;
    char *sp;
    char ebuff[CF_EXPANDSIZE];
    int uid = CF_NOUSER;
    int gid = CF_NOUSER;

    Debug1("Installing shellcommand (%s) in the script list\n", item);

    if (!IsInstallable(g_classbuff)) {
        Debug1("Not installing (%s), no class match (%s)\n", 
                item, g_classbuff);
        InitializeAction();
        return;
    }

    /* Must be at least one space between each var */
    Build2DListFromVarstring(&tp, item, ' ');

    Set2DList(tp);

    for (sp = Get2DListEnt(tp); sp != NULL; sp = Get2DListEnt(tp)) {

        if ((ptr = (struct ShellComm *)malloc(sizeof(struct ShellComm))) == NULL) {

            FatalError("Memory Allocation failed for AppendScript() #1");
        }

        if ((ptr->classes = strdup(g_classbuff)) == NULL) {
            FatalError("Memory Allocation failed for Appendscript() #2");
        }

        if (*sp != '/' && !g_noabspath) {
            yyerror("scripts or commands must have absolute path names");
            printf ("cfagent: concerns: %s\n",sp);
            return;
        }

        if ((ptr->name = strdup(sp)) == NULL) {
            FatalError("Memory Allocation failed for Appendscript() #3");
        }

        ExpandVarstring(g_chroot, ebuff, "");

        if ((ptr->chroot = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for Appendscipt() #4b");
        }

        ExpandVarstring(g_chdir, ebuff, "");

        if ((ptr->chdir = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for Appendscript() #4c");
        }

        /* First element in the list */
        if (g_vscripttop == NULL) {
            g_vscript = ptr;
        } else {
            g_vscripttop->next = ptr;
        }

        if (*uidname == '*') {
            ptr->uid = CF_SAME_OWNER;
        } else if (isdigit((int)*uidname)) {
            sscanf(uidname, "%d", &uid);
            if (uid == CF_NOUSER) {
                yyerror("Unknown or silly user id");
                return;
            } else {
                ptr->uid = uid;
            }
        } else if ((pw = getpwnam(uidname)) == NULL) {
            yyerror("Unknown or silly user id");
            return;
        } else {
            ptr->uid = pw->pw_uid;
        }

        if (*gidname == '*') {
            ptr->gid = CF_SAME_GROUP;
        } else if (isdigit((int)*gidname)) {
            sscanf(gidname, "%d", &gid);
            if (gid == CF_NOUSER) {
                yyerror("Unknown or silly group id");
                continue;
            } else {
                ptr->gid = gid;
            }
        } else if ((gw = getgrnam(gidname)) == NULL) {
            yyerror("Unknown or silly group id");
            continue;
        } else {
            ptr->gid = gw->gr_gid;
        }

        if (g_pifelapsed != -1) {
            ptr->ifelapsed = g_pifelapsed;
        } else {
            ptr->ifelapsed = g_vifelapsed;
        }

        if (g_pexpireafter != -1) {
            ptr->expireafter = g_pexpireafter;
        }
        else {
            ptr->expireafter = g_vexpireafter;
        }

        ptr->log = g_logp;
        ptr->inform = g_informp;
        ptr->timeout = timeout;
        ptr->useshell = useshell;
        ptr->umask = g_umask;
        ptr->fork = g_fork;
        ptr->preview = g_preview;
        ptr->next = NULL;
        ptr->done = 'n';
        ptr->scope = strdup(g_contextid);

        ExpandVarstring(g_allclassbuffer, ebuff, "");

        if ((ptr->defines = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #3");
        }

        ExpandVarstring(g_elseclassbuffer, ebuff, "");

        if ((ptr->elsedef = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #3");
        }

        AddInstallable(ptr->defines);
        AddInstallable(ptr->elsedef);
        g_vscripttop = ptr;
    }
    Delete2DList(tp);
}

/* ----------------------------------------------------------------- */

void
AppendDisable(char *path, char *type, short rotate, char comp, int size)
{
    char *sp;
    struct Disable *ptr;
    struct TwoDimList *tp = NULL;
    char ebuff[CF_EXPANDSIZE];

    Debug1("Installing item (%s) in the disable list\n", path);

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing %s, no match\n", path);
        return;
    }

    /* Must be at least one space between each var */
    Build2DListFromVarstring(&tp, path, ' ');

    Set2DList(tp);

    for (sp = Get2DListEnt(tp); sp != NULL; sp = Get2DListEnt(tp)) {

        if (strlen(type) > 0 && strcmp(type,"plain") != 0 &&
                strcmp(type, "file") !=0 && strcmp(type, "link") !=0 &&
                strcmp(type, "links") !=0 ) {

            yyerror("Invalid file type in Disable");
        }

        if ((ptr = (struct Disable *)malloc(sizeof(struct Disable))) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #1");
        }

        if ((ptr->name = strdup(sp)) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #2");
        }

        ExpandVarstring(g_allclassbuffer, ebuff, "");

        if ((ptr->defines = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #3");
        }

        ExpandVarstring(g_elseclassbuffer, ebuff, "");

        if ((ptr->elsedef = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #3");
        }

        if ((ptr->classes = strdup(g_classbuff)) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #4");
        }

        if (strlen(type) == 0) {
            sprintf(ebuff, "all");
        } else {
            sprintf(ebuff, "%s", type);
        }

        if ((ptr->type = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #4");
        }

        ExpandVarstring(g_destination, ebuff, "");

        if ((ptr->destination = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for AppendDisable() #3");
        }

        /* First element in the list */
        if (g_vdisabletop == NULL) {
            g_vdisablelist = ptr;
        } else {
            g_vdisabletop->next = ptr;
        }

        if (strlen(g_localrepos) > 0) {
            ExpandVarstring(g_localrepos, ebuff, "");
            ptr->repository = strdup(ebuff);
        } else {
            ptr->repository = NULL;
        }

        if (g_pifelapsed != -1) {
            ptr->ifelapsed = g_pifelapsed;
        } else {
            ptr->ifelapsed = g_vifelapsed;
        }

        if (g_pexpireafter != -1) {
            ptr->expireafter = g_pexpireafter;
        } else {
            ptr->expireafter = g_vexpireafter;
        }

        ptr->rotate = rotate;
        ptr->comp = comp;
        ptr->size = size;
        ptr->next = NULL;
        ptr->log = g_logp;
        ptr->inform = g_informp;
        ptr->done = 'n';
        ptr->scope = strdup(g_contextid);
        ptr->action = g_proaction;

        g_vdisabletop = ptr;
        InitializeAction();
        AddInstallable(ptr->defines);
        AddInstallable(ptr->elsedef);
    }
    Delete2DList(tp);
}

/* ----------------------------------------------------------------- */

void
InstallMethod(char *function, char *file)
{
    char *sp; 
    char work[CF_EXPANDSIZE], name[CF_BUFSIZE];
    struct Method *ptr;
    struct Item *bare_send_args = NULL;
    uid_t uid = CF_NOUSER;
    gid_t gid = CF_NOUSER;
    struct passwd *pw;
    struct group *gw;

    Debug1("Installing item (%s=%s) in the methods list\n", function, file);

    if (strlen(file) == 0) {
        snprintf(g_output, CF_BUFSIZE,
                "Missing action file from declaration of method %s", 
                function);
        yyerror(g_output);
        return;
    }

    memset(name, 0, CF_BUFSIZE-1);

    if (! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing %s, no match\n", function);
        return;
    }

    if ((ptr = (struct Method *)malloc(sizeof(struct Method))) == NULL) {
        FatalError("Memory Allocation failed for InstallMethod() #1");
    }

    if (g_vmethodstop == NULL) {
        g_vmethods = ptr;
    } else {
        g_vmethodstop->next = ptr;
    }

    if (!strstr(function, "(")) {
        yyerror("Missing parenthesis or extra space");
        InitializeAction();
        return;
    }

    /* First look at bare args to cache an arg fingerprint */
    strcpy(work, function);

    if (work[strlen(work)-1] != ')') {
        yyerror("Illegal use of space or nested parentheses");
    }

    work [strlen(work)-1] = '\0';   /*chop last ) */

    sscanf(function, "%[^(]", name);

    if (strlen(name) == 0) {
        yyerror("Empty method");
        return;
    }

    /* Pick out the args*/
    for (sp = work; sp != NULL; sp++) {
        if (*sp == '(') {
            break;
        }
    }

    sp++;

    if (strlen(sp) == 0) {
        yyerror("Missing argument (void?) to method");
    }

    bare_send_args = ListFromArgs(sp);
    ChecksumList(bare_send_args, ptr->digest, 'm');
    DeleteItemList(bare_send_args);

    /* Now expand variables */
    ExpandVarstring(function, work, "");

    if (work[strlen(work)-1] != ')') {
        yyerror("Illegal use of space or nested parentheses");
    }

    work [strlen(work)-1] = '\0';   /*chop last ) */

    sscanf(function, "%[^(]", name);

    if (strlen(name) == 0) {
        yyerror("Empty method");
        return;
    }

    /* Pick out the args*/
    for (sp = work; sp != NULL; sp++) {
        if (*sp == '(') {
            break;
        }
    }

    sp++;

    if (strlen(sp) == 0) {
        yyerror("Missing argument (void?) to method");
    }

    ptr->send_args = ListFromArgs(sp);
    ptr->send_classes = SplitStringAsItemList(g_methodreplyto, ',');

    if ((ptr->name = strdup(name)) == NULL) {
        FatalError("Memory Allocation failed for InstallMethod() #2");
    }

    if ((ptr->classes = strdup(g_classbuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallMethod() #3");
    }

    if (strlen(file) == 0) {
        yyerror("Missing filename in method");
        return;
    }

    if (strcmp(file,"dispatch") == 0) {
        ptr->invitation = 'y';
    } else {
        ptr->invitation = 'n';
    }

    if (file[0] == '/' || file[0] == '.') {

        snprintf(g_output, CF_BUFSIZE,
                "Method name (%s) was absolute. Must be in trusted "
                "Modules directory (no path prefix)", file);

        yyerror(g_output);
        return;
    }

    ptr->file = strdup(file);
    ptr->servers = SplitStringAsItemList(g_cfserver, ',');
    ptr->bundle = NULL;
    ptr->return_vars = SplitStringAsItemList(g_methodfilename, ',');
    ptr->return_classes = SplitStringAsItemList(g_methodreturnclasses, ',');
    ptr->scope = strdup(g_contextid);
    ptr->useshell = g_useshell;
    ptr->log = g_logp;
    ptr->inform = g_informp;

    if (*g_vuidname == '*') {
        ptr->uid = CF_SAME_OWNER;
    } else if (isdigit((int)*g_vuidname)) {
        sscanf(g_vuidname, "%d", &uid);
        if (uid == CF_NOUSER) {
            yyerror("Unknown or silly user id");
            return;
        } else {
            ptr->uid = uid;
        }
    } else if ((pw = getpwnam(g_vuidname)) == NULL) {
        yyerror("Unknown or silly user id");
        return;
    } else {
        ptr->uid = pw->pw_uid;
    }

    if (*g_vgidname == '*') {
        ptr->gid = CF_SAME_GROUP;
    } else if (isdigit((int)*g_vgidname)) {
        sscanf(g_vgidname,"%d",&gid);
        if (gid == CF_NOUSER) {
            yyerror("Unknown or silly group id");
            return;
        } else {
            ptr->gid = gid;
        }
    } else if ((gw = getgrnam(g_vgidname)) == NULL) {
        yyerror("Unknown or silly group id");
        return;
    } else {
        ptr->gid = gw->gr_gid;
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ExpandVarstring(g_chroot, work, "");

    if ((ptr->chroot = strdup(work)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #4b");
    }

    ExpandVarstring(g_chdir, work, "");

    if ((ptr->chdir = strdup(work)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #4c");
    }

    ExpandVarstring(g_methodforce, work, "");

    if ((ptr->forcereplyto = strdup(work)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #4c");
    }

    ptr->next = NULL;
    g_vmethodstop = ptr;
    InitializeAction();
}

/* ----------------------------------------------------------------- */

void
InstallTidyItem(char *path, char *wild, int rec, short age,
        char travlinks, int tidysize, char type, char ldirs,
        char tidydirs, char *classes)
{
    struct TwoDimList *tp = NULL;
    char *sp;

    if (strcmp(path,"/") != 0) {
        DeleteSlash(path);
    }

    Build2DListFromVarstring(&tp, path, '/');

    Set2DList(tp);

    for (sp = Get2DListEnt(tp); sp != NULL; sp = Get2DListEnt(tp)) {
        if (TidyPathExists(sp)) {
            AddTidyItem(sp, wild, rec, age, travlinks, tidysize, type,
                    ldirs, tidydirs, classes);
        } else {
            InstallTidyPath(sp, wild, rec, age, travlinks, tidysize, type,
                    ldirs, tidydirs, classes);
        }
    }

    Delete2DList(tp);
    InitializeAction();
}

/* ----------------------------------------------------------------- */

void
InstallMakePath(char *path, mode_t plus, mode_t minus,
        char *uidnames, char *gidnames)
{
    struct File *ptr;
    char buffer[CF_EXPANDSIZE];
    char ebuff[CF_EXPANDSIZE];

    Debug1("InstallMakePath (%s) (+%o)(-%o)(%s)(%s)\n",
            path, plus, minus, uidnames, gidnames);

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing directory item, no match\n");
        return;
    }

    /* Expand any variables */
    ExpandVarstring(path,ebuff,"");

    if ((ptr = (struct File *)malloc(sizeof(struct File))) == NULL) {
        FatalError("Memory Allocation failed for InstallMakepath() #1");
    }

    if ((ptr->path = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallMakepath() #2");
    }

    if ((ptr->classes = strdup(g_classbuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallMakepath() #3");
    }

    ExpandVarstring(g_allclassbuffer, buffer, "");

    if ((ptr->defines = strdup(buffer)) == NULL) {
        FatalError("Memory Allocation failed for InstallMakepath() #3a");
    }

    ExpandVarstring(g_elseclassbuffer, buffer, "");

    if ((ptr->elsedef = strdup(buffer)) == NULL) {
        FatalError("Memory Allocation failed for InstallMakepath() #3a");
    }

    AddInstallable(ptr->defines);
    AddInstallable(ptr->elsedef);

    /* First element in the list */
    if (g_vmakepathtop == NULL) {
        g_vmakepath = ptr;
    } else {
        g_vmakepathtop->next = ptr;
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ptr->plus = plus;
    ptr->minus = minus;
    ptr->recurse = 0;
    ptr->action = fixdirs;
    ptr->uid = MakeUidList(uidnames);
    ptr->gid = MakeGidList(gidnames);
    ptr->inclusions = NULL;
    ptr->exclusions = NULL;
    ptr->acl_aliases= g_vaclbuild;
    ptr->log = g_logp;
    ptr->filters = NULL;
    ptr->inform = g_informp;
    ptr->plus_flags = g_plusflag;
    ptr->minus_flags = g_minusflag;
    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);

    ptr->next = NULL;
    g_vmakepathtop = ptr;
    InitializeAction();
}

/* ----------------------------------------------------------------- */

void
HandleTravLinks(char *value)
{
    if (g_action == tidy && strncmp(g_currentobject, "home", 4) == 0) {
        yyerror("Can't use links= option with special variable home in tidy");
        yyerror("Use command line options instead.\n");
    }

    if (g_ptravlinks != '?') {
        Warning("redefinition of links= option");
    }

    if ((strcmp(value, "stop") == 0) || (strcmp(value, "false") == 0)) {
        g_ptravlinks = (short) 'F';
        return;
    }

    if ((strcmp(value,"traverse") == 0) 
            || (strcmp(value, "follow") == 0) 
            || (strcmp(value, "true") == 0)) {
        g_ptravlinks = (short) 'T';
        return;
    }

    if ((strcmp(value, "tidy")) == 0) {
        g_ptravlinks = (short) 'K';
        return;
    }

    yyerror("Illegal links= specifier");
}

/* ----------------------------------------------------------------- */

void
HandleTidySize(char *value)
{
    int num = -1;
    char *sp, units = 'k';

    for (sp = value; *sp != '\0'; sp++) {
        *sp = ToLower(*sp);
    }

    if (strcmp(value, "empty") == 0) {
        g_tidysize = CF_EMPTYFILE;
    } else {
        sscanf(value, "%d%c", &num, &units);

        if (num <= 0) {
            if (*value == '>') {
                sscanf(value+1, "%d%c", &num, &units);
                if (num <= 0) {
                    yyerror("size value must be a decimal number "
                            "with units m/b/k");
                }
            } else {
                yyerror("size value must be a decimal number "
                        "with units m/b/k");
            }
        }

        switch (units) {
        case 'b':
            g_tidysize = num;
            break;
        case 'm':
            g_tidysize = num * 1024 * 1024;
            break;
        default:
            g_tidysize = num * 1024;
        }
    }
}

/* ----------------------------------------------------------------- */

void
HandleUmask(char *value)
{
    int num = -1;

    Debug("HandleUmask(%s)", value);

    sscanf(value, "%o", &num);

    if (num <= 0) {
        yyerror("umask value must be an octal number >= zero");
    }

    g_umask = (mode_t) num;
}

/* ----------------------------------------------------------------- */

void
HandleDisableSize(char *value)
{
    int i = -1;
    char *sp, units = 'b';

    for (sp = value; *sp != '\0'; sp++) {
        *sp = ToLower(*sp);
    }

    switch (*value) {
    case '>':
        g_discomp = '>';
        value++;
        break;
    case '<':
        g_discomp = '<';
        value++;
        break;
    default :
        g_discomp = '=';
    }

    sscanf(value, "%d%c", &i, &units);

    if (i < 1) {
        yyerror("disable size attribute with silly value (must be > 0)");
    }

    switch (units) {
    case 'k':
        g_disablesize = i * 1024;
        break;
    case 'm':
        g_disablesize = i * 1024 * 1024;
        break;
    default:
        g_disablesize = i;
    }
}

/* ----------------------------------------------------------------- */

void
HandleCopySize(char *value)
{
    int i = -1;
    char *sp, units = 'b';

    for (sp = value; *sp != '\0'; sp++) {
        *sp = ToLower(*sp);
    }

    switch (*value) {
    case '>':
        g_imgcomp = '>';
        value++;
        break;
    case '<':
        g_imgcomp = '<';
        value++;
        break;
    default :
        g_imgcomp = '=';
    }

    sscanf(value, "%d%c", &i, &units);

    if (i < 0) {
        yyerror("copy size attribute with silly value "
                "(must be a non-negative number)");
    }

    switch (units) {
    case 'k':
        g_imgsize = i * 1024;
        break;
    case 'm':
        g_imgsize = i * 1024 * 1024;
        break;
    default:
        g_imgsize = i;
    }
}

/* ----------------------------------------------------------------- */

void
HandleRequiredSize(char *value)
{
    int i = -1;
    char *sp, units = 'b';

    for (sp = value; *sp != '\0'; sp++) {
        *sp = ToLower(*sp);
    }

    switch (*value) {
    case '>':
        g_imgcomp = '>';
        value++;
        break;
    case '<':
        g_imgcomp = '<';
        value++;
        break;
    default :
        g_imgcomp = '=';
    }

    sscanf(value, "%d%c", &i, &units);

    if (i < 1) {
        yyerror("disk/required size attribute with silly "
                "value (must be > 0)");
    }

    switch (units) {
    case 'b':
        g_imgsize = i / 1024;
        break;
    case 'm':
        g_imgsize = i * 1024;
        break;
    case '%':
        g_imgsize = -i;       /* -ve number signals percentage */
        break;
    default:
        g_imgsize = i;
    }
}

/* ----------------------------------------------------------------- */

void
HandleTidyType(char *value)
{
    if (strcmp(value, "a") == 0 || strcmp(value, "atime") == 0) {
        g_agetype = 'a';
        return;
    }

    if (strcmp(value, "m") == 0 || strcmp(value, "mtime") == 0) {
        g_agetype = 'm';
        return;
    }

    if (strcmp(value, "c") == 0 || strcmp(value, "ctime") == 0) {
        g_agetype = 'c';
        return;
    }

    yyerror("Illegal age search type, must be atime/ctime/mtime");
}

/* ----------------------------------------------------------------- */

void
HandleTidyLinkDirs(char *value)
{
    if (strcmp(value, "keep") == 0) {
        g_linkdirs = 'k';
        return;
    }

    if ((strcmp(value, "tidy") == 0) || (strcmp(value, "delete") == 0)) {
        g_linkdirs = 'y';
        return;
    }

    yyerror("Illegal linkdirs value, must be keep/delete/tidy");
}

/* ----------------------------------------------------------------- */

void
HandleTidyRmdirs(char *value)
{
    if ((strcmp(value, "true") == 0) || (strcmp(value, "all") == 0)) {
        g_tidydirs = 'y';
        return;
    }

    if ((strcmp(value, "false") == 0) || (strcmp(value, "none") == 0)) {
        g_tidydirs = 'n';
        return;
    }

    if (strcmp(value, "sub") == 0) {
        g_tidydirs = 's';
        return;
    }

    yyerror("Illegal rmdirs value, must be true/false/sub");
}

/* ----------------------------------------------------------------- */

void
HandleTimeOut(char *value)
{
    int num = -1;

    sscanf(value, "%d", &num);

    if (num <= 0) {
        yyerror("timeout value must be a decimal number > 0");
    }

    g_vtimeout = num;
}


/* ----------------------------------------------------------------- */

void
HandleUseShell(char *value)
{
    if (strcmp(value, "true") == 0) {
        g_useshell = 'y';
        return;
    }

    if (strcmp(value, "false") == 0) {
        g_useshell = 'n';
        return;
    }

    if (strcmp(value, "dumb") == 0) {
        g_useshell = 'd';
        return;
    }

    yyerror("Illegal attribute for useshell= ");
}

/* ----------------------------------------------------------------- */

void
HandleChecksum(char *value)
{
    if (strcmp(value, "md5") == 0) {
        g_checksum = 'm';
        return;
    }

    if (strncmp(value, "sha1", strlen(value)) == 0) {
        g_checksum = 's';
        return;
    }

    yyerror("Illegal attribute for checksum= ");
}

/* ----------------------------------------------------------------- */

void
HandleTimeStamps(char *value)
{
    if (strcmp(value, "preserve") == 0 || strcmp(value, "keep") == 0) {
        g_preservetimes = 'y';
        return;
    }

    g_preservetimes = 'n';
}

/* ----------------------------------------------------------------- */

int
GetFileAction(char *action)
{
    int i;

    for (i = 0; g_fileactiontext[i] != '\0'; i++) {
        if (strcmp(action,g_fileactiontext[i]) == 0) {
            return i;
        }
    }

    yyerror("Unknown action type");
    return (int) warnall;
}


/* ----------------------------------------------------------------- */

void InstallFileListItem(char *path, mode_t plus, mode_t minus,
        enum fileactions action, char *uidnames, char *gidnames,
        int recurse, char travlinks, char chksum)
{
    struct File *ptr;
    char *spl;
    char ebuff[CF_EXPANDSIZE];
    struct TwoDimList *tp = NULL;

    Debug1("InstallFileaction (%s) (+%o)(-%o) (%s) (%d) (%c)\n",
            path, plus, minus, g_fileactiontext[action], action, travlinks);

    if (!IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing file item, no match\n");
        return;
    }

    Build2DListFromVarstring(&tp, path, '/');

    Set2DList(tp);

    for (spl = Get2DListEnt(tp); spl != NULL; spl = Get2DListEnt(tp)) {
        if ((ptr = (struct File *)malloc(sizeof(struct File))) == NULL) {
            FatalError("Memory Allocation failed for InstallFileListItem() #1");
        }

        if ((ptr->path = strdup(spl)) == NULL) {
            FatalError("Memory Allocation failed for InstallFileListItem() #2");
        }

        if ((ptr->classes = strdup(g_classbuff)) == NULL) {
            FatalError("Memory Allocation failed for InstallFileListItem() #3");
        }

        ExpandVarstring(g_allclassbuffer,ebuff,"");

        if ((ptr->defines = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for InstallFileListItem() #3");
        }

        ExpandVarstring(g_elseclassbuffer,ebuff,"");

        if ((ptr->elsedef = strdup(ebuff)) == NULL) {
            FatalError("Memory Allocation failed for InstallFileListItem() #3");
        }

        AddInstallable(ptr->defines);
        AddInstallable(ptr->elsedef);

        /* First element in the list */
        if (g_vfiletop == NULL) {
            g_vfile = ptr;
        } else {
            g_vfiletop->next = ptr;
        }

        if (g_pifelapsed != -1) {
            ptr->ifelapsed = g_pifelapsed;
        } else {
            ptr->ifelapsed = g_vifelapsed;
        }

        if (g_pexpireafter != -1) {
            ptr->expireafter = g_pexpireafter;
        } else {
            ptr->expireafter = g_vexpireafter;
        }

        ptr->action = action;
        ptr->plus = plus;
        ptr->minus = minus;
        ptr->recurse = recurse;
        ptr->uid = MakeUidList(uidnames);
        ptr->gid = MakeGidList(gidnames);
        ptr->exclusions = g_vexcludeparse;
        ptr->inclusions = g_vincludeparse;
        ptr->filters = NULL;
        ptr->ignores = g_vignoreparse;
        ptr->travlinks = travlinks;
        ptr->acl_aliases  = g_vaclbuild;
        ptr->filters = g_vfilterbuild;
        ptr->next = NULL;
        ptr->log = g_logp;
        ptr->xdev = g_xdev;
        ptr->inform = g_informp;
        ptr->checksum = chksum;
        ptr->plus_flags = g_plusflag;
        ptr->minus_flags = g_minusflag;
        ptr->done = 'n';
        ptr->scope = strdup(g_contextid);

        g_vfiletop = ptr;
        Debug1("Installed file object %s\n",spl);
    }
    Delete2DList(tp);
    InitializeAction();
}


/* ----------------------------------------------------------------- */

void InstallProcessItem(char *expr, char *restart, short matches,
        char comp, short signal, char action, char *classes,
        char useshell, char *uidname, char *gidname)
{
    struct Process *ptr;
    char ebuff[CF_EXPANDSIZE];
    uid_t uid;
    gid_t gid;
    struct passwd *pw;
    struct group *gw;

    Debug1("InstallProcessItem(%s,%s,%d,%d,%c)\n",
            expr, restart, matches, signal, action);

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing process item, no match\n");
        return;
    }

    if ((ptr = (struct Process *)malloc(sizeof(struct Process))) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #1");
    }

    ExpandVarstring(expr, ebuff, "");

    if ((ptr->expr = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #2");
    }

    ExpandVarstring(restart, ebuff, "");

    if (strcmp(ebuff, "SetOptionString") == 0) {
        if ((strlen(ebuff) > 0) && ebuff[0] != '/') {

            snprintf(g_output, CF_BUFSIZE, "Restart command (%s) does "
                    "not have an absolute pathname", ebuff);

            yyerror(g_output);
        }
    }

    if ((ptr->restart = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #3");
    }

    ExpandVarstring(g_allclassbuffer, ebuff, "");

    if ((ptr->defines = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #4");
    }

    ExpandVarstring(g_elseclassbuffer, ebuff, "");

    if ((ptr->elsedef = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #4a");
    }

    ExpandVarstring(g_chroot, ebuff, "");

    if ((ptr->chroot = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #4b");
    }

    ExpandVarstring(g_chdir, ebuff, "");

    if ((ptr->chdir = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #4c");
    }

    if ((ptr->classes = strdup(g_classbuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallProcItem() #5");
    }

    /* First element in the list */
    if (g_vproctop == NULL) {
        g_vproclist = ptr;
    } else {
        g_vproctop->next = ptr;
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ptr->matches = matches;
    ptr->comp = comp;
    ptr->signal = signal;
    ptr->action = action;
    ptr->umask = g_umask;
    ptr->useshell = useshell;
    ptr->next = NULL;
    ptr->log = g_logp;
    ptr->inform = g_informp;
    ptr->exclusions = g_vexcludeparse;
    ptr->inclusions = g_vincludeparse;
    ptr->filters = ptr->filters = g_vfilterbuild;
    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);

    if (*uidname == '*') {
        ptr->uid = -1;
    } else if (isdigit((int)*uidname)) {
        sscanf(uidname, "%d", &uid);
        if (uid == CF_NOUSER) {
            yyerror("Unknown or silly restart user id");
            return;
        } else {
            ptr->uid = uid;
        }
    } else if ((pw = getpwnam(uidname)) == NULL) {
        yyerror("Unknown or silly restart user id");
        return;
    } else {
        ptr->uid = pw->pw_uid;
    }

    if (*gidname == '*') {
        ptr->gid = -1;
    } else if (isdigit((int)*gidname)) {
        sscanf(gidname, "%d", &gid);
        if (gid == CF_NOUSER) {
            yyerror("Unknown or silly restart group id");
            return;
        } else {
            ptr->gid = gid;
        }
    } else if ((gw = getgrnam(gidname)) == NULL) {
        yyerror("Unknown or silly restart group id");
        return;
    } else {
        ptr->gid = gw->gr_gid;
    }

    g_vproctop = ptr;
    InitializeAction();
    AddInstallable(ptr->defines);
    AddInstallable(ptr->elsedef);
}

/* ----------------------------------------------------------------- */

void InstallPackagesItem(char *name, char *ver,
        enum cmpsense sense, enum pkgmgrs mgr)
{
    struct Package *ptr;
    char buffer[CF_EXPANDSIZE];

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing packages item, no match\n");
        return;
    }

    /* If the package manager is set to pkgmgr_none, then an invalid
    * manager was specified, so we don't need to do anything */
    if ( g_pkgmgr == pkgmgr_none ) {
        InitializeAction();
        Debug1("Package manager set to none.  Not installing package item\n");
        return;
    }

    Debug1("InstallPackagesItem(%s,%s,%s,%s)\n",
            name, ver, g_cmpsensetext[sense], g_pkgmgrtext[mgr]);

    if ((ptr = (struct Package *)malloc(sizeof(struct Package))) == NULL) {
        FatalError("Memory Allocation failed for InstallPackageItem() #1");
    }

    if ((ptr->name = strdup(name)) == NULL) {
        FatalError("Memory Allocation failed for InstallPackageItem() #2");
    }

    if ((ptr->ver = strdup(ver)) == NULL) {
        FatalError("Memory Allocation failed for InstallPackageItem() #3");
    }

    if ((ptr->classes = strdup(g_classbuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallPackageItem() #4");
    }

    ExpandVarstring(g_allclassbuffer, buffer, "");

    if ((ptr->defines = strdup(buffer)) == NULL) {
        FatalError("Memory Allocation failed for InstallPackageItem() #4a");
    }

    ExpandVarstring(g_elseclassbuffer, buffer, "");

    if ((ptr->elsedef = strdup(buffer)) == NULL) {
        FatalError("Memory Allocation failed for InstallPackageItem() #4b");
    }

    AddInstallable(ptr->defines);
    AddInstallable(ptr->elsedef);

    /* First element in the list */
    if (g_vpkgtop == NULL) {
        g_vpkg = ptr;
    } else {
        g_vpkgtop->next = ptr;
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ptr->log = g_logp;
    ptr->inform = g_informp;
    ptr->cmp = sense;
    ptr->pkgmgr = mgr;
    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);

    ptr->next = NULL;
    g_vpkgtop = ptr;
    InitializeAction();
}

/* ----------------------------------------------------------------- */

int
GetCmpSense(char *sense)
{
    int i;

    for (i = 0; g_cmpsensetext[i] != '\0'; i++) {
        if (strcmp(sense,g_cmpsensetext[i]) == 0) {
            return i;
        }
    }

    yyerror("Unknown comparison sense");
    return (int) cmpsense_eq;
}

/* ----------------------------------------------------------------- */

int
GetPkgMgr(char *pkgmgr)
{
    int i;
    for (i = 0; g_pkgmgrtext[i] != '\0'; i++) {
        if (strcmp(pkgmgr, g_pkgmgrtext[i]) == 0) {
            return i;
        }
    }

    yyerror("Unknown package manager");
    return (int) pkgmgr_none;
}

/* ----------------------------------------------------------------- */

void
InstallImageItem(char *cf_findertype, char *path, mode_t plus,mode_t minus,
        char *destination, char *action, char *uidnames, char *gidnames,
        int size, char comp, int rec, char type, char lntype, char *server)
{
    struct Image *ptr;
    char *spl;
    char buf1[CF_EXPANDSIZE], buf2[CF_EXPANDSIZE]; 
    char buf3[CF_EXPANDSIZE], buf4[CF_EXPANDSIZE];
    struct TwoDimList *tp = NULL;
    struct hostent *hp;
    struct Item *expserver = NULL;
    struct Item *ep;

    if (!IsInstallable(g_classbuff)) {
        Debug1("Not installing copy item, no match (%s,%s)\n",
                path, g_classbuff);
        InitializeAction();
        return;
    }

    Debug1("InstallImageItem (%s) (+%o)(-%o) (%s), server=%s\n",
            path, plus, minus, destination, server);

    /* default action */
    if (strlen(action) == 0) {
        strcat(action,"fix");
    }

    if (!(strcmp(action, "silent") == 0 || strcmp(action, "warn") == 0 ||
                strcmp(action, "fix") == 0)) {

        sprintf(g_vbuff, "Illegal action in image/copy item: %s", action);
        yyerror(g_vbuff);
        return;
    }

    ExpandVarstring(path, buf1, "");
    ExpandVarstring(server, buf3, "");

    if (strlen(buf1) > 1) {
        DeleteSlash(buf1);
    }

    expserver = SplitStringAsItemList(buf3,g_listseparator);

    /* Must split on space in comm string */
    Build2DListFromVarstring(&tp, path, '/');

    Set2DList(tp);

    for (spl = Get2DListEnt(tp); spl != NULL; spl = Get2DListEnt(tp))
    {
        for (ep = expserver; ep != NULL; ep = ep->next) {
            Debug("\nBegin pass with server = %s\n",ep->name);

            if ((ptr = (struct Image *)malloc(sizeof(struct Image))) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #1");
            }

            if ((ptr->classes = strdup(g_classbuff)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #3");
            }

            if (!g_forcenetcopy && ((strcmp(ep->name, g_vfqname) == 0) ||
                        (strcmp(ep->name, g_vuqname) == 0) ||
                        (strcmp(ep->name, g_vsysname.nodename) == 0))) {

                Debug("Swapping %s for localhost\n", server);
                free(ep->name);
                strcpy(ep->name, "localhost");
            }

            if (ptr->purge == 'y' && strstr(ep->name,"localhost") == 0) {
                Verbose("Warning: purge in local file copy to file %s\n",
                        ptr->destination);
                Verbose("Warning: Do not risk purge if source % is on NFS\n",
                        ptr->path);
            }

            if (IsDefinedClass(g_classbuff)) {
                if ((strcmp(spl,buf2) == 0) 
                        && (strcmp(ep->name,"localhost") == 0)) {
                    yyerror("Image loop: file/dir copies to itself "
                            "or missing destination file");
                    continue;
                }
            }


            if (strlen(ep->name) > 128) {
                snprintf(g_output, CF_BUFSIZE,
                        "Server name (%s) is too long",ep->name);
                yyerror(g_output);
                DeleteItemList(expserver);
                return;
            }

            if (!IsItemIn(g_vserverlist, ep->name)) {
                AppendItem(&g_vserverlist, ep->name, NULL);
            }

            Debug("Transformed server %s into %s\n",server,ep->name);

            if ((ptr->server = strdup(ep->name)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #5");
            }

            if ((ptr->action = strdup(action)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #6");
            }

            ExpandVarstring(g_allclassbuffer, buf4, "");

            if ((ptr->defines = strdup(buf4)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #7");
            }

            ExpandVarstring(g_elseclassbuffer, buf4, "");

            if ((ptr->elsedef = strdup(buf4)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #7");
            }

            ExpandVarstring(g_failoverbuffer, buf4, "");

            if ((ptr->failover = strdup(buf4)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #8");
            }

            if (strlen(destination) == 0) {
                strcpy(buf2, spl);
            } else {
                ExpandVarstring(destination, buf2, "");
            }

            if (strlen(buf2) > 1) {
                DeleteSlash(buf2);
            }

            if (!IsAbsoluteFileName(buf2)) {
                if (strncmp(buf2, "home", 4) == 0) {
                    if (strlen(buf2) > 4 && buf2[4] != '/') {
                        yyerror("illegal use of home or not absolute pathname");
                        return;
                    }
                } else if (*buf2 == '$') {
                    /* unexpanded variable */
                } else {
                    snprintf(g_output, CF_BUFSIZE,
                            "Image %s needs an absolute pathname", buf2);
                    yyerror(g_output);
                    return;
                }
            }

            if ((ptr->destination = strdup(buf2)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #4");
            }

            if ((ptr->path = strdup(spl)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "InstallImageItem() #2");
            }

            if (g_vimagetop == NULL) {
                g_vimage = ptr;
            } else {
                g_vimagetop->next = ptr;
            }

            if ((ptr->cf_findertype = strdup(cf_findertype)) == NULL) {
                FatalError("Memory Allocation failed for "
                        "cf_findertype ptr in InstallImageItem()");
            }

            ptr->plus = plus;
            ptr->minus = minus;
            ptr->uid = MakeUidList(uidnames);
            ptr->gid = MakeGidList(gidnames);
            ptr->force = g_force;
            ptr->next = NULL;
            ptr->backup = g_imagebackup;
            ptr->done = 'n';
            ptr->scope = strdup(g_contextid);

            if (strlen(g_localrepos) > 0) {
                ExpandVarstring(g_localrepos, buf2, "");
                ptr->repository = strdup(buf2);
            } else {
                ptr->repository = NULL;
            }

            if (g_pifelapsed != -1) {
                ptr->ifelapsed = g_pifelapsed;
            } else {
                ptr->ifelapsed = g_vifelapsed;
            }

            if (g_pexpireafter != -1) {
                ptr->expireafter = g_pexpireafter;
            } else {
                ptr->expireafter = g_vexpireafter;
            }

            ptr->recurse = rec;
            ptr->type = type;
            ptr->stealth = g_stealth;
            ptr->preservetimes = g_preservetimes;
            ptr->encrypt = g_encrypt;
            ptr->verify = g_verify;
            ptr->size = size;
            ptr->comp = comp;
            ptr->linktype = lntype;
            ptr->symlink = g_vcplnparse;
            ptr->exclusions = g_vexcludeparse;
            ptr->inclusions = g_vincludeparse;
            ptr->filters = g_vfilterbuild;
            ptr->ignores = g_vignoreparse;
            ptr->cache = NULL;
            ptr->purge = g_purge;

            if (g_purge == 'y') {
                ptr->forcedirs = 'y';
                ptr->typecheck = 'n';
            } else {
                ptr->forcedirs = g_forcedirs;
                ptr->typecheck = g_typecheck;
            }

            ptr->log = g_logp;
            ptr->inform = g_informp;
            ptr->plus_flags = g_plusflag;
            ptr->minus_flags = g_minusflag;
            ptr->trustkey = g_trustkey;
            ptr->compat = g_compatibility;
            ptr->forceipv4 = g_forceipv4;
            ptr->xdev = g_xdev;

            if (ptr->compat == 'y' && ptr->encrypt == 'y') {
                yyerror("You cannot mix version1 compatibility with "
                        "encrypted transfers");
                return;
            }

            ptr->acl_aliases = g_vaclbuild;
            ptr->inode_cache = NULL;

            g_vimagetop = ptr;

            AddInstallable(ptr->defines);
            AddInstallable(ptr->elsedef);
        }

    }

    DeleteItemList(expserver);
    Delete2DList(tp);
    InitializeAction();
}

/* ----------------------------------------------------------------- */

/* This is the top level interface for installing access rules
   for the server. Picks out the structures by name. */

void
InstallAuthItem(char *path, char *attribute,
        struct Auth **list, struct Auth **listtop, char *classes)
{
    struct TwoDimList *tp = NULL;
    char attribexp[CF_EXPANDSIZE];
    char *sp1;
    struct Item *vlist = NULL,*ip;

    Debug1("InstallAuthItem(%s,%s)\n", path, attribute);

    vlist = SplitStringAsItemList(attribute, g_listseparator);

    Build2DListFromVarstring(&tp, path, '/');

    Set2DList(tp);

    for (sp1 = Get2DListEnt(tp); sp1 != NULL; sp1 = Get2DListEnt(tp)) {
        for (ip = vlist; ip != NULL; ip = ip->next) {
            ExpandVarstring(ip->name, attribexp, "");

            Debug1("InstallAuthItem(%s=%s,%s)\n", path, sp1, attribexp);

            if (AuthPathExists(sp1, *list)) {
                AddAuthHostItem(sp1, attribexp, classes, list);
            } else {
                InstallAuthPath(sp1, attribexp, classes, list, listtop);
            }
        }
    }
    Delete2DList(tp);
    DeleteItemList(vlist);
}

/* ----------------------------------------------------------------- */

int
GetCommAttribute(char *s)
{
    int i;
    char comm[CF_MAXVARSIZE];

    for (i = 0; s[i] != '\0'; i++) {
        s[i] = ToLower(s[i]);
    }

    comm[0] = '\0';

    sscanf(s, "%[^=]", comm);

    Debug1("GetCommAttribute(%s)\n", comm);

    for (i = 0; g_commattributes[i] != NULL; i++) {
        if (strncmp(g_commattributes[i], comm,strlen(comm)) == 0) {
            Debug1("GetCommAttribute - got: %s\n", g_commattributes[i]);
            return i;
        }
    }
    return cfbad;
}

/* ----------------------------------------------------------------- */

void
HandleRecurse(char *value)
{
    int n = -1;

    if (strcmp(value, "inf") == 0) {
        g_vrecurse = CF_INF_RECURSE;
    } else {

        /* Bas
        if (strncmp(g_currentobject,"home",4) == 0)
            {
            Bas
            yyerror("Recursion is always infinite for home");
            return;
            }
        */

        sscanf(value, "%d", &n);

        if (n == -1) {
            yyerror("Illegal recursion specifier");
        } else {
            g_vrecurse = n;
        }
    }
}

/* ----------------------------------------------------------------- */

void
HandleCopyType(char *value)
{
    if (strcmp(value, "ctime") == 0) {
        Debug1("Set copy by ctime\n");
        g_copytype = 't';
        return;
    } else if (strcmp(value, "mtime") == 0) {
        Debug1("Set copy by mtime\n");
        g_copytype = 'm';
        return;
    } else if (strcmp(value, "checksum") == 0 || strcmp(value, "sum") == 0) {
        Debug1("Set copy by md5 checksum\n");
        g_copytype = 'c';
        return;
    } else if (strcmp(value, "byte") == 0 || strcmp(value, "binary") == 0) {
        Debug1("Set copy by byte comaprison\n");
        g_copytype = 'b';
        return;
    }
    yyerror("Illegal copy type");
}

/* ----------------------------------------------------------------- */

void
HandleDisableFileType(char *value)
{
    if (strlen(g_currentitem) != 0) {
        Warning("Redefinition of filetype in disable");
    }

    if (strcmp(value, "link") == 0 || strcmp(value, "links") == 0) {
        strcpy(g_currentitem, "link");
    } else if (strcmp(value, "plain") == 0 || strcmp(value, "file") == 0) {
        strcpy(g_currentitem, "file");
    } else {
        yyerror("Disable filetype unknown");
    }
}

/* ----------------------------------------------------------------- */

void
HandleDisableRotate(char *value)
{
    int num = 0;

    if (strcmp(value, "empty") == 0 || strcmp(value, "truncate") == 0) {
        g_rotate = CF_TRUNCATE;
    } else {
        sscanf(value, "%d", &num);

        if (num == 0) {
            yyerror("disable/rotate value must be a decimal number "
                    "greater than zero or keyword empty");
        }

        if (! g_silent && num > 99) {
            Warning("rotate value looks silly");
        }

        g_rotate = (short) num;
    }
}

/* ----------------------------------------------------------------- */

void
HandleAge(char *days)
{
    sscanf(days, "%d", &g_vage);
    Debug1("HandleAge(%d)\n", g_vage);
}

/* ----------------------------------------------------------------- */

void
HandleProcessMatches(char *value)
{
    int i = -1;

    switch (*value) {
    case '>':
        g_procomp = '>';
        value++;
        break;
    case '<':
        g_procomp = '<';
        value++;
        break;
    default :
        g_procomp = '=';
    }

    sscanf(value, "%d", &i);

    if (i < 0) {
        yyerror("matches attribute with silly value (must be >= 0)");
    }

    g_promatches = (short) i;
    g_actionpending = true;
}

/* ----------------------------------------------------------------- */

void
HandleProcessSignal(char *value)
{
    int i;
    char *sp;

    for (i = 1; i < highest_signal; i++) {
        for (sp = value; *sp != '\0'; sp++) {
            *sp = ToUpper(*sp);
        }

        if (strcmp(g_signals[i], "no signal") == 0) {
            continue;
        }

        /* 3 to cut off "sig" */
        if (strcmp(g_signals[i]+3,value) == 0) {
            g_prosignal = (short) i;
            return;
        }
    }

    i = 0;

    sscanf(value, "%d", &i);

    if (i < 1 && i >= highest_signal) {
        yyerror("Unknown signal in attribute - try using a number");
    }

    g_actionpending = true;
    g_prosignal = (short) i;
}

/* ----------------------------------------------------------------- */

void
HandleNetmask(char *value)
{
    if (strlen(g_destination) == 0) {
        strcpy(g_destination,value);
    } else {
        yyerror("redefinition of netmask");
    }
}

/* ----------------------------------------------------------------- */

void
HandleIPAddress(char *value)
{
    if (strlen(g_linkto) == 0) {
        strcpy(g_linkto,value);
    } else {
        yyerror("redefinition of ip address");
    }
}

/* ----------------------------------------------------------------- */

void
HandleBroadcast(char *value)
{
    if (strlen(g_currentobject) != 0) {
        yyerror("redefinition of broadcast address");
        printf("Previous val = %s\n", g_currentobject);
    }

    if (strcmp("ones",value) == 0) {
        strcpy(g_currentobject, "one");
        return;
    }

    if (strcmp("zeroes",value) == 0) {
        strcpy(g_currentobject, "zero");
        return;
    }

    if (strcmp("zeros",value) == 0) {
        strcpy(g_currentobject, "zero");
        return;
    }

    yyerror("Illegal broadcast specifier (ones/zeros)");
}

/* ----------------------------------------------------------------- */

void
AppendToActionSequence (char *action)
{
    int j = 0;
    char *sp;
    char cbuff[CF_BUFSIZE],actiontxt[CF_BUFSIZE];

    Debug1("Installing item (%s) in the action sequence list\n", action);

    AppendItem(&g_vactionseq, action, g_classbuff);

    if (strstr(action, "module")) {
        g_useenviron = true;
    }

    cbuff[0] = '\0';
    actiontxt[0] = '\0';
    sp = action;

    while (*sp != '\0') {
        ++j;
        sscanf(sp, "%[^. ]", cbuff);

        while ((*sp != '\0') && (*sp !='.')) {
            sp++;
        }

        if (*sp == '.') {
            sp++;
        }

        if (IsHardClass(cbuff)) {
            char *tmp = malloc(strlen(action)+30);
            sprintf(tmp, "Error in action sequence: %s\n", action);
            yyerror(tmp);
            free(tmp);
            yyerror("You cannot add a reserved class!");
            return;
        }

        if (j == 1) {
            strcpy(actiontxt, cbuff);
            continue;
        } else if (!IsSpecialClass(cbuff)) {
            AddInstallable(cbuff);
        }
    }
}

/* ----------------------------------------------------------------- */

void
AppendToAccessList(char *user)
{
    char id[CF_MAXVARSIZE];
    struct passwd *pw;

    Debug1("Adding to access list for %s\n", user);

    if (isalpha((int)user[0])) {
        if ((pw = getpwnam(user)) == NULL) {
            yyerror("No such user in password database");
            return;
        }

        sprintf(id, "%d", pw->pw_uid);
        AppendItem(&g_vaccesslist, id, NULL);
    } else {
        AppendItem(&g_vaccesslist, user, NULL);
    }
}

/* ----------------------------------------------------------------- */

void
HandleLinkAction(char *value)
{
    if (strcmp(value, "silent") == 0) {
        g_linksilent = true;
        return;
    }

    yyerror("Invalid link action");
}

/* ----------------------------------------------------------------- */

void
HandleDeadLinks(char *value)
{
    if (strcmp(value, "kill") == 0) {
        g_deadlinks = false;
        return;
    }

    if (strcmp(value, "force") == 0) {
        g_deadlinks = true;
        return;
    }

    yyerror("Invalid deadlink option");
}

/* ----------------------------------------------------------------- */

void
HandleLinkType(char *value)
{
    if (strcmp(value, "hard") == 0) {
        if (g_action_is_linkchildren) {
            yyerror("hard links inappropriate for multiple linkage");
        }

        if (g_action == image) {
            yyerror("hard links inappropriate for copy operation");
        }

        g_linktype = 'h';
        return;
    }

    if (strcmp(value, "symbolic") == 0 || strcmp(value, "sym") == 0) {
        g_linktype = 's';
        return;
    }

    if (strcmp(value, "abs") == 0 || strcmp(value, "absolute") == 0) {
        g_linktype = 'a';
        return;
    }

    if (strcmp(value, "rel") == 0 || strcmp(value, "relative") == 0) {
        g_linktype = 'r';
        return;
    }

    if (strcmp(value, "none") == 0 || strcmp(value, "copy") == 0) {
        g_linktype = 'n';
        return;
    }

    snprintf(g_output, CF_BUFSIZE*2, "Invalid link type %s\n", value);
    yyerror(g_output);
}

/* ----------------------------------------------------------------- */

void
HandleServer(char *value)
{
    Debug("Server in copy set to : %s\n", value);
    strcpy(g_cfserver, value);
}

/* ----------------------------------------------------------------- */

void
HandleDefine(char *value)
{
    char *sp;

    Debug("Define response classes: %s\n", value);

    if (strlen(value) > CF_BUFSIZE) {
        yyerror("class list too long - can't handle it!");
    }

    /*if (!IsInstallable(value))
    {
    snprintf(g_output,CF_BUFSIZE*2,"Undeclared installable define=%s (see AddInstallable)",value);
    yyerror(g_output);
    }
    */
    strcpy(g_allclassbuffer, value);

    for (sp = value; *sp != '\0'; sp++) {
        if (*sp == ':' || *sp == ',' || *sp == '.') {
            continue;
        }

        if (! isalnum((int)*sp) && *sp != '_') {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Illegal class list in define=%s\n", value);
            yyerror(g_output);
        }
    }
}

/* ----------------------------------------------------------------- */

void
HandleElseDefine(char *value)
{
    char *sp;

    Debug("Define elsedefault classes: %s\n", value);

    if (strlen(value) > CF_BUFSIZE) {
        yyerror("class list too long - can't handle it!");
    }

    strcpy(g_elseclassbuffer, value);

    for (sp = value; *sp != '\0'; sp++) {
        if (*sp == ':' || *sp == ',' || *sp == '.') {
            continue;
        }

        if (! isalnum((int)*sp) && *sp != '_') {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Illegal class list in elsedefine=%s\n", value);
            yyerror(g_output);
        }
    }
}

/* ----------------------------------------------------------------- */

void
HandleFailover(char *value)
{
    char *sp;

    Debug("Define failover classes: %s\n", value);

    if (strlen(value) > CF_BUFSIZE) {
        yyerror("class list too long - can't handle it!");
    }

    strcpy(g_failoverbuffer, value);

    for (sp = value; *sp != '\0'; sp++) {
        if (*sp == ':' || *sp == ',' || *sp == '.') {
            continue;
        }

        if (! isalnum((int)*sp) && *sp != '_') {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Illegal class list in failover=%s\n", value);
            yyerror(g_output);
        }
    }
}

/* ----------------------------------------------------------------- */
/* Level 4                                                         */
/* ----------------------------------------------------------------- */

struct UidList *
MakeUidList(char *uidnames)
{
    struct UidList *uidlist;
    struct Item *ip, *tmplist;
    char uidbuff[CF_BUFSIZE];
    char *sp;
    int offset;
    struct passwd *pw;
    char *machine, *user, *domain; 
    char buffer[CF_EXPANDSIZE]; 
    char *usercopy = NULL;
    int uid;
    int tmp = -1;

    uidlist = NULL;
    buffer[0] = '\0';

    ExpandVarstring(uidnames, buffer, "");

    for (sp = buffer; *sp != '\0'; sp+=strlen(uidbuff)) {
        if (*sp == ',') {
            sp++;
        }

        if (sscanf(sp, "%[^,]", uidbuff)) {

            /* NIS group - have to do this in a roundabout     */
             /* way because calling getpwnam spoils getnetgrent */

            if (uidbuff[0] == '+') {
                offset = 1;
                if (uidbuff[1] == '@') {
                    offset++;
                }

                setnetgrent(uidbuff+offset);
                tmplist = NULL;

                while (getnetgrent(&machine, &user, &domain)) {
                    if (user != NULL) {
                        AppendItem(&tmplist, user, NULL);
                    }
                }

                endnetgrent();

                for (ip = tmplist; ip != NULL; ip=ip->next) {
                    if ((pw = getpwnam(ip->name)) == NULL) {
                        if (!g_parsing) {
                            snprintf(g_output, CF_BUFSIZE*2,
                                    "Unknown user [%s]\n", ip->name);
                            CfLog(cferror, g_output, "");
                        }

                        /* signal user not found */
                        uid = CF_UNKNOWN_OWNER;
                        usercopy = ip->name;

                    } else {
                        uid = pw->pw_uid;
                    }
                    AddSimpleUidItem(&uidlist, uid, usercopy);
                }

                DeleteItemList(tmplist);
                continue;
            }

            if (isdigit((int)uidbuff[0])) {
                sscanf(uidbuff, "%d", &tmp);
                uid = (uid_t)tmp;
            } else {
                if (strcmp(uidbuff, "*") == 0) {

                    /* signals wildcard */
                    uid = CF_SAME_OWNER;

                } else if ((pw = getpwnam(uidbuff)) == NULL) {
                    if (!g_parsing) {
                        snprintf(g_output, CF_BUFSIZE,
                                "Unknown user %s\n", uidbuff);
                        CfLog(cferror, g_output, "");
                    }

                    /* signal user not found */
                    uid = CF_UNKNOWN_OWNER;
                    usercopy = uidbuff;
                } else {
                    uid = pw->pw_uid;
                }
            }
            AddSimpleUidItem(&uidlist, uid, usercopy);
        }
    }

    if (uidlist == NULL) {
        AddSimpleUidItem(&uidlist, CF_SAME_OWNER, (char *) NULL);
    }

    return (uidlist);
}

/* ----------------------------------------------------------------- */

struct GidList *
MakeGidList(char *gidnames)
{
    struct GidList *gidlist;
    char gidbuff[CF_BUFSIZE], buffer[CF_EXPANDSIZE];
    char *sp; 
    char *groupcopy = NULL;
    struct group *gr;
    int gid;
    int tmp = -1;

    gidlist = NULL;
    buffer[0] = '\0';

    ExpandVarstring(gidnames, buffer, "");

    for (sp = buffer; *sp != '\0'; sp+=strlen(gidbuff)) {
        if (*sp == ',') {
            sp++;
        }

        if (sscanf(sp, "%[^,]", gidbuff)) {
            if (isdigit((int)gidbuff[0])) {
                sscanf(gidbuff, "%d", &tmp);
                gid = (gid_t)tmp;
            } else {
                if (strcmp(gidbuff, "*") == 0) {

                    /* signals wildcard */
                    gid = CF_SAME_GROUP;

                } else if ((gr = getgrnam(gidbuff)) == NULL) {
                    if (!g_parsing) {
                        snprintf(g_output, CF_BUFSIZE,
                                "Unknown group %s\n", gidbuff);
                        CfLog(cferror, g_output, "");
                    }

                    gid = CF_UNKNOWN_GROUP;
                    groupcopy = gidbuff;
                } else {
                    gid = gr->gr_gid;
                }
            }
            AddSimpleGidItem(&gidlist, gid, groupcopy);
        }
    }

    if (gidlist == NULL) {
        AddSimpleGidItem(&gidlist, CF_SAME_GROUP, NULL);
    }

    return(gidlist);
}


/* ----------------------------------------------------------------- */

void
InstallTidyPath(char *path, char *wild, int rec, short age,
        char travlinks, int tidysize, char type, char ldirs,
        char tidydirs, char *classes)
{
    struct Tidy *ptr;
    char *sp;
    char ebuff[CF_EXPANDSIZE];
    int no_of_links = 0;

    if (!IsInstallable(classes)) {
        InitializeAction();
        Debug1("Not installing tidy path, no match\n");
        return;
    }

    ExpandVarstring(path, ebuff, "");

    if (strlen(ebuff) != 1) {
        DeleteSlash(ebuff);
    }

    if ((ptr = (struct Tidy *)malloc(sizeof(struct Tidy))) == NULL) {
        FatalError("Memory Allocation failed for InstallTidyItem() #1");
    }

    if ((ptr->path = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallTidyItem() #3");
    }

    /* First element in the list */
    if (g_vtidytop == NULL) {
        g_vtidy = ptr;
    } else {
        g_vtidytop->next = ptr;
    }

    /* Is this a subdir of home wildcard? */
    if (rec != CF_INF_RECURSE && strncmp("home/", ptr->path, 5) == 0) {

        /* Need to make recursion relative to start */
        /* of the search, not relative to home */
        for (sp = ptr->path; *sp != '\0'; sp++) {
            if (*sp == '/') {
                no_of_links++;
            }
        }
    }

    if (g_pifelapsed != -1) {
        ptr->ifelapsed = g_pifelapsed;
    } else {
        ptr->ifelapsed = g_vifelapsed;
    }

    if (g_pexpireafter != -1) {
        ptr->expireafter = g_pexpireafter;
    } else {
        ptr->expireafter = g_vexpireafter;
    }

    ptr->done = 'n';
    ptr->scope = strdup(g_contextid);
    ptr->tidylist = NULL;
    ptr->maxrecurse = rec + no_of_links;
    ptr->next = NULL;
    ptr->xdev = g_xdev;
    ptr->exclusions = g_vexcludeparse;
    ptr->ignores = g_vignoreparse;

    g_vexcludeparse = NULL;
    g_vignoreparse = NULL;
    g_vtidytop = ptr;

    AddTidyItem(path, wild, rec+no_of_links,age, travlinks,
            tidysize, type, ldirs, tidydirs, classes);
}

/* ----------------------------------------------------------------- */

void
AddTidyItem(char *path, char *wild, int rec, short age, char travlinks,
        int tidysize, char type, char ldirs, short tidydirs, char *classes)
{
    char varbuff[CF_EXPANDSIZE];
    struct Tidy *ptr;
    struct Item *ip;

    Debug1("AddTidyItem(%s,pat=%s,size=%d)\n", path, wild, tidysize);

    if ( ! IsInstallable(g_classbuff)) {
        InitializeAction();
        Debug1("Not installing TidyItem no match\n");
        return;
    }

    for (ptr = g_vtidy; ptr != NULL; ptr=ptr->next) {
        ExpandVarstring(path, varbuff, "");

        if (strcmp(ptr->path,varbuff) == 0) {
            PrependTidy(&(ptr->tidylist), wild, rec, age, travlinks,
                    tidysize, type, ldirs, tidydirs, classes);

            for (ip = g_vexcludeparse; ip != NULL; ip=ip->next) {
                AppendItem(&(ptr->exclusions), ip->name, ip->classes);
            }

            for (ip = g_vignoreparse; ip != NULL; ip=ip->next) {
                AppendItem(&(ptr->ignores), ip->name, ip->classes);
            }

            DeleteItemList(g_vexcludeparse);
            DeleteItemList(g_vignoreparse);
            /* Must have the maximum recursion level here */

            if (rec == CF_INF_RECURSE || ((ptr->maxrecurse < rec) &&
                        (ptr->maxrecurse != CF_INF_RECURSE))) {

                ptr->maxrecurse = rec;
            }
            return;
        }
    }
}

/* ----------------------------------------------------------------- */

int
TidyPathExists(char *path)
{
    struct Tidy *tp;
    char ebuff[CF_EXPANDSIZE];

    ExpandVarstring(path, ebuff, "");

    for (tp = g_vtidy; tp != NULL; tp=tp->next) {
        if (strcmp(tp->path, ebuff) == 0) {
            Debug1("TidyPathExists(%s)\n", ebuff);
            return true;
        }
    }

    return false;
}


/* ----------------------------------------------------------------- */
/* Level 5                                                         */
/* ----------------------------------------------------------------- */

void
AddSimpleUidItem(struct UidList **uidlist, int uid, char *uidname)
{
    struct UidList *ulp, *u;
    char *copyuser;

    if ((ulp = (struct UidList *)malloc(sizeof(struct UidList))) == NULL) {
        FatalError("cfagent: malloc() failed #1 in AddSimpleUidItem()");
    }

    ulp->uid = uid;

    /* unknown user */
    if (uid == CF_UNKNOWN_OWNER) {
        if ((copyuser = strdup(uidname)) == NULL) {
            FatalError("cfagent: malloc() failed #2 in AddSimpleUidItem()");
        }
        ulp->uidname = copyuser;
    } else {
        ulp->uidname = NULL;
    }

    ulp->next = NULL;

    if (*uidlist == NULL) {
        *uidlist = ulp;
    } else {
        for (u = *uidlist; u->next != NULL; u = u->next) { }
        u->next = ulp;
    }
}

/* ----------------------------------------------------------------- */

void
AddSimpleGidItem(struct GidList **gidlist, int gid, char *gidname)
{
    struct GidList *glp,*g;
    char *copygroup;

    if ((glp = (struct GidList *)malloc(sizeof(struct GidList))) == NULL) {
        FatalError("cfagent: malloc() failed #1 in AddSimpleGidItem()");
    }

    glp->gid = gid;

    /* unknown group */
    if (gid == CF_UNKNOWN_GROUP) {
        if ((copygroup = strdup(gidname)) == NULL) {
            FatalError("cfagent: malloc() failed #2 in AddSimpleGidItem()");
        }

        glp->gidname = copygroup;
    } else {
        glp->gidname = NULL;
    }

    glp->next = NULL;

    if (*gidlist == NULL) {
        *gidlist = glp;
    } else {
        for (g = *gidlist; g->next != NULL; g = g->next) { }
        g->next = glp;
    }
}


/* ----------------------------------------------------------------- */

void
InstallAuthPath(char *path, char *hostname, char *classes,
        struct Auth **list, struct Auth **listtop)
{
    struct Auth *ptr;
    char ebuff[CF_EXPANDSIZE];

    if (!IsDefinedClass(classes)) {
        Debug1("Not installing Auth path, no match\n");
        InitializeAction();
        return;
    }

    Debug1("InstallAuthPath(%s,%s) for %s\n", path, hostname, classes);

    ExpandVarstring(path, ebuff,"");

    if (strlen(ebuff) != 1) {
        DeleteSlash(ebuff);
    }

    if ((ptr = (struct Auth *)malloc(sizeof(struct Auth))) == NULL) {
        FatalError("Memory Allocation failed for InstallAuthPath() #1");
    }

    if ((ptr->path = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallAuthPath() #3");
    }

    /* First element in the list */
    if (*listtop == NULL) {
        *list = ptr;
    } else {
        (*listtop)->next = ptr;
    }

    ptr->accesslist = NULL;
    ptr->maproot = NULL;
    ptr->encrypt = false;
    ptr->next = NULL;
    *listtop = ptr;

    AddAuthHostItem(ptr->path, hostname, classes, list);
}

/* ----------------------------------------------------------------- */

void
AddAuthHostItem(char *path, char *attribute, char *classes,
        struct Auth **list)
{
    char varbuff[CF_EXPANDSIZE];
    struct Auth *ptr;
    struct Item *ip, *split = NULL;

    Debug1("AddAuthHostItem(%s,%s)\n", path, attribute);

    if (!IsDefinedClass(g_classbuff)) {
        Debug1("Not installing AuthItem no match\n");
        return;
    }

    for (ptr = *list; ptr != NULL; ptr=ptr->next) {
        ExpandVarstring(path, varbuff,"");

        split = SplitStringAsItemList(attribute, g_listseparator);

        if (strcmp(ptr->path,varbuff) == 0) {
            for (ip = split; ip != NULL; ip=ip->next) {
                if (!HandleAdmitAttribute(ptr, ip->name)) {
                    PrependItem(&(ptr->accesslist), ip->name, classes);
                }
            }
            return;
        }
    }
}


/* ----------------------------------------------------------------- */

int
AuthPathExists(char *path, struct Auth *list)
{
    struct Auth *ap;
    char ebuff[CF_EXPANDSIZE];

    Debug1("AuthPathExists(%s)\n", path);

    ExpandVarstring(path, ebuff, "");

    if (ebuff[0] != '/') {
        yyerror("Missing absolute path to a directory");
        FatalError("Cannot continue");
    }

    for (ap = list; ap != NULL; ap=ap->next) {
        if (strcmp(ap->path, ebuff) == 0) {
            return true;
        }
    }

    return false;
}

/* ----------------------------------------------------------------- */

int
HandleAdmitAttribute(struct Auth *ptr, char *attribute)
{
    char *sp;
    char value[CF_MAXVARSIZE];
    char buffer[CF_EXPANDSIZE];
    char host[CF_MAXVARSIZE];
    char ebuff[CF_EXPANDSIZE];

    value[0] = '\0';

    ExpandVarstring(attribute, ebuff, NULL);

    sscanf(ebuff, "%*[^=]=%s", value);

    if (value[0] == '\0') {
        return false;
    }

    Debug1("HandleAdmitFileAttribute(%s)\n", value);

    switch(GetCommAttribute(attribute)) {
    case cfencryp:
        Debug("\nENCRYPTION tag %s\n", value);
        if ((strcmp(value, "on") == 0) || (strcmp(value, "true") == 0)) {
            ptr->encrypt = true;
            return true;
        }
        break;

    case cfroot:
        Debug("\nROOTMAP tag %s\n", value);
                    ExpandVarstring(value, buffer, "");

        for (sp = buffer; *sp != '\0'; sp += strlen(host)) {
            if (*sp == ',') {
                sp++;
            }

            host[0] = '\0';

            if (sscanf(sp, "%[^,\n]", host)) {
                char copyhost[CF_BUFSIZE];

                strncpy(copyhost, host, CF_BUFSIZE-1);

                if (!strstr(copyhost,".")) {
                    if (strlen(copyhost) + strlen(g_vdomain) 
                            < CF_MAXVARSIZE-2) {
                        strcat(copyhost, ".");
                        strcat(copyhost, g_vdomain);
                    } else {
                        yyerror("Server name too long");
                    }
                }

                if (!IsItemIn(ptr->maproot, copyhost)) {
                    PrependItem(&(ptr->maproot), copyhost,NULL);
                } else {
                    Debug("Not installing %s in rootmap\n", host);
                }
            }
        }
        return true;
        break;
    }

    yyerror("Illegal admit/deny attribute");
    return false;
}


/* ----------------------------------------------------------------- */

void
PrependTidy(struct TidyPattern **list, char *wild, int rec, short age,
        char travlinks, int tidysize, char type, char ldirs,
        char tidydirs, char *classes)
{
    struct TidyPattern *tp;
    char *spe = NULL,*sp; 
    char buffer[CF_EXPANDSIZE];

    if ((tp = (struct TidyPattern *)malloc(sizeof(struct TidyPattern))) == NULL) {

        perror("Can't allocate memory in PrependTidy()");
        FatalError("");
    }

    if ((sp = strdup(wild)) == NULL) {
        FatalError("Memory Allocation failed for PrependTidy() #2");
    }

    ExpandVarstring(g_allclassbuffer, buffer, "");

    if ((tp->defines = strdup(buffer)) == NULL) {
        FatalError("Memory Allocation failed for PrependTidy() #2a");
    }

    ExpandVarstring(g_elseclassbuffer, buffer, "");

    if ((tp->elsedef = strdup(buffer)) == NULL) {
        FatalError("Memory Allocation failed for PrependTidy() #2a");
    }

    AddInstallable(tp->defines);
    AddInstallable(tp->elsedef);

    if ((classes!= NULL) && (spe = malloc(strlen(classes) + 2)) == NULL) {
        perror("Can't allocate memory in PrependItem()");
        FatalError("");
    }

    if (travlinks == '?') {
        /* False is default */
        travlinks = 'F';  
    }

    tp->size = tidysize;
    tp->recurse = rec;
    tp->age = age;
    tp->searchtype = type;
    tp->travlinks = travlinks;
    tp->filters = g_vfilterbuild;
    tp->pattern = sp;
    tp->next = *list;
    tp->dirlinks = ldirs;
    tp->log = g_logp;
    tp->inform = g_informp;
    tp->compress=g_compress;

    tp->rmdirs = tidydirs;

    *list = tp;

    if (classes != NULL) {
        strcpy(spe,classes);
        tp->classes = spe;
    } else {
        tp->classes = NULL;
    }
}
