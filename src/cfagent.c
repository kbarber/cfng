/*
 * $Id: cfagent.c 748 2004-05-25 14:47:10Z skaar $
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


#include "../pub/getopt.h"
#include "cf.defs.h"
#include "cf.extern.h"

/*
 * --------------------------------------------------------------------
 * Fuctions internal to cfagent.c
 * --------------------------------------------------------------------
 */
int   main(int argc,char *argv[]);
void  Initialize(int argc, char **argv);
void  PreNetConfig(void);
void  ReadRCFile(void);
void  EchoValues(void);
void  CheckSystemVariables(void);
void  SetReferenceTime(int setclasses);
void  SetStartTime(int setclasses);
void  DoTree(int passes, char *info);
enum  aseq EvaluateAction(char *action, struct Item **classlist, int pass);
void  CheckOpts(int argc, char **argv);
int   GetResource(char *var);
void  BuildClassEnvironment(void);
void  Syntax(void);
void  EmptyActionSequence(void);
void  GetEnvironment(void);
int   NothingLeftToDo(void);
void  SummarizeObjects(void);
void  SetContext(char *id);
void  DeleteCaches(void);
void  CheckForMethod(void);
void  CheckMethodReply(void);

/*
 * --------------------------------------------------------------------
 * Level 0: Main
 * --------------------------------------------------------------------
 */
int
main(int argc, char *argv[])
{
    struct Item *ip;

    SetContext("global");

    /* Signal Handler */
    SetSignals();
    signal(SIGTERM, HandleSignal);                   
    signal(SIGHUP, HandleSignal);
    signal(SIGINT, HandleSignal);
    signal(SIGPIPE, HandleSignal);

    Initialize(argc, argv);
    SetReferenceTime(true);
    SetStartTime(false);

    if (! g_nohardclasses) {
        GetNameInfo();
        GetInterfaceInfo();
        GetV6InterfaceInfo();
        GetEnvironment();
    }

    PreNetConfig();

    /* Should come before parsing so that it can be overridden */
    ReadRCFile(); 

    if (IsPrivileged() && !g_minusf && !g_prmailserver) {
        SetContext("update");
        if (ParseInputFile("update.conf")) {
            CheckSystemVariables();
            if (!g_parseonly) {
                DoTree(1, "Update");
                EmptyActionSequence();
                DeleteClassesFromContext("update");
                DeleteCaches();
            }
        }

        if (g_errorcount > 0) {
            exit(1);
        }
    }

    if (g_updateonly) {
        return 0;
    }

    SetContext("main");

    if (!g_parseonly) {
        PersistentClassesToHeap();
        GetEnvironment();
    }

    ParseInputFile(g_vinputfile);
    CheckFilters();
    EchoValues();
    CheckForMethod();

    /* -a option */
    if (g_prsysadm) {
        printf("%s\n", g_vsysadm);
        exit (0);
    }

    if (g_prmailserver) {
        if (GetMacroValue(g_contextid, "smtpserver")) {
            printf("%s\n", GetMacroValue(g_contextid, "smtpserver"));
        } else {
            printf("No SMTP server defined\n");
        }
        printf("%s\n", g_vsysadm);
        printf("%s\n", g_vfqname);
        printf("%s\n", g_vipaddress);

        if (GetMacroValue(g_contextid, "EmailMaxLines")) {
            printf("%s\n", GetMacroValue(g_contextid, "EmailMaxLines"));
        } else {
            /* User has not expressed a preference -- let cfexecd decide */
            printf("%s", "-1\n");
        }

        for (ip = g_schedule; ip != NULL; ip=ip->next) {
            printf("[%s]\n", ip->name);
        }

        printf("\n");
        exit(0);
    }

    if (g_errorcount > 0) {
        exit(1);
    }

    /* Establish lock for root */
    if (g_parseonly) {
        exit(0);
    }

    CfOpenLog();
    CheckSystemVariables();

    SetReferenceTime(false); /* Reset */

    DoTree(2, "Main Tree");
    DoAlerts();

    CheckMethodReply();

    if (OptionIs(g_contextid, "ChecksumPurge", true)) {
        ChecksumPurge();
    }

    SummarizeObjects();
    closelog();
    return 0;
}

/*
 * --------------------------------------------------------------------
 * Level 1
 * --------------------------------------------------------------------
 */
void
Initialize(int argc, char *argv[])
{
    char *sp; 
    char *cfargv[CF_MAXARGS];
    int i, cfargc, seed;
    struct stat statbuf;
    unsigned char s[16];
    char ebuff[CF_EXPANDSIZE];

    strcpy(g_vdomain, CF_START_DOMAIN);

    PreLockState();

    g_iscfengine = true;
    g_vfaculty[0] = '\0';
    g_vsysadm[0] = '\0';
    g_vnetmask[0]= '\0';
    g_vbroadcast[0] = '\0';
    g_vmailserver[0] = '\0';
    g_vdefaultroute[0] = '\0';
    g_allclassbuffer[0] = '\0';
    g_vrepository = strdup("\0");

    strcpy(g_methodname,"cf-nomethod");
    g_methodreplyto[0] = '\0';
    g_methodreturnvars[0] = '\0';
    g_methodreturnclasses[0] = '\0';

#ifndef HAVE_REGCOMP
    re_syntax_options |= RE_INTERVALS;
#endif

    strcpy(g_vinputfile, "cfagent.conf");
    strcpy(g_vnfstype, "nfs");

    IDClasses();

    /* 
     * Note we need to fix the options since the argv mechanism doesn't
     * work when the shell #!/bla/cfagent -v -f notation is used.
     * Everything ends up inside a single argument! Here's the fix.
     */

    cfargc = 1;
    cfargv[0]="cfagent";

    for (i = 1; i < argc; i++) {
        sp = argv[i];

        while (*sp != '\0') {
            /* Skip to arg */
            while (*sp == ' ' && *sp != '\0') {
                if (*sp == ' ') {
                    *sp = '\0'; /* Break argv string */
                }
                sp++;
            }

            cfargv[cfargc++] = sp;

            /* Skip to white space */
            while (*sp != ' ' && *sp != '\0') {
                sp++;
            }
        }
    }

    g_vdefaultbinserver.name = "";

    g_vexpireafter = g_vdefaultexpireafter;
    g_vifelapsed = g_vdefaultifelapsed;
    g_travlinks = false;

    sprintf(ebuff, "%s/state/cfng_procs", WORKDIR);

    if (stat(ebuff, &statbuf) == -1) {
        CreateEmptyFile(ebuff);
    }

    sprintf(g_vbuff, "%s/logs", WORKDIR);
    strcpy(g_vlogdir, g_vbuff);

    sprintf(g_vbuff, "%s/state",WORKDIR);
    strcpy(g_vlockdir, g_vbuff);  
    

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    CheckWorkDirectories();
    RandomSeed();

    RAND_bytes(s,16);
    s[15] = '\0';
    seed = ElfHash(s);
    srand48((long)seed);
    CheckOpts(cfargc, cfargv);
}

/* ----------------------------------------------------------------- */

/* Execute a shell script */
void
PreNetConfig(void)
{
    struct stat buf;
    char comm[CF_BUFSIZE], ebuff[CF_EXPANDSIZE];
    char *sp;
    FILE *pp;

    if (g_nopreconfig) {
        CfLog(cfverbose, "Ignoring the cf.preconf file: option set", "");
        return;
    }

    strcpy(g_vprefix, "cfng:");
    strcat(g_vprefix, g_vuqname);

    if ((sp = getenv(CF_INPUTSVAR)) != NULL) {
        snprintf(comm, CF_BUFSIZE, "%s/%s", sp, g_vpreconfig);

        if (stat(comm, &buf) == -1) {
            CfLog(cfverbose, "No preconfiguration file", "");
            return;
        }

        snprintf(comm, CF_BUFSIZE, "%s/%s %s 2>&1",
                sp, g_vpreconfig, g_classtext[g_vsystemhardclass]);

    } else {
        snprintf(comm, CF_BUFSIZE, "%s/%s", WORKDIR, g_vpreconfig);

        if (stat(comm, &buf) == -1) {
            CfLog(cfverbose, "No preconfiguration file\n", "");
            return;
        }

        snprintf(comm, CF_BUFSIZE, "%s/%s %s",
                WORKDIR, g_vpreconfig, g_classtext[g_vsystemhardclass]);
    }

    if (S_ISDIR(buf.st_mode) || S_ISCHR(buf.st_mode) 
            || S_ISBLK(buf.st_mode)) {

        snprintf(g_output, CF_BUFSIZE*2,
                "Error: %s was not a regular file\n", g_vpreconfig);

        CfLog(cferror, g_output, "");
        FatalError("Aborting.");
    }

    Verbose("\n\nExecuting Net Preconfiguration script...%s\n\n",
            g_vpreconfig);

    if ((pp = cfpopen(comm, "r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Failed to open pipe to %s\n", comm);
        CfLog(cferror, g_output, "");
        return;
    }

    while (!feof(pp)) {
        /* abortable */
        if (ferror(pp)) {
            CfLog(cferror, "Error running preconfig\n", "ferror");
            break;
        }

        ReadLine(ebuff, CF_BUFSIZE, pp);

        if (feof(pp)) {
            break;
        }

        /* abortable */
        if (ferror(pp)) {
            CfLog(cferror, "Error running preconfig\n", "ferror");
            break;
        }

        CfLog(cfinform, ebuff, "");
    }

    cfpclose(pp);
}

/* ----------------------------------------------------------------- */

void
DeleteCaches(void)
{
    /* DeleteItemList(g_vexcludecache); ?? */
}

/* ----------------------------------------------------------------- */

void
ReadRCFile(void)
{
    char  *mp;
    int   c;
    char  filename[CF_BUFSIZE]; 
    char  buffer[CF_BUFSIZE]; 
    char  class[CF_MAXVARSIZE]; 
    char  variable[CF_MAXVARSIZE]; 
    char  value[CF_MAXVARSIZE];
    FILE  *fp;

    filename[0] = buffer[0] = class[0] = variable[0] = value[0] = '\0';
    g_linenumber = 0;

    snprintf(filename, CF_BUFSIZE, "%s/inputs/%s", WORKDIR, g_vrcfile);
    /* Open root file */
    if ((fp = fopen(filename, "r")) == NULL) {
        return;
    }

    while (!feof(fp)) {
        ReadLine(buffer, CF_BUFSIZE, fp);
        g_linenumber++;
        class[0] = '\0';
        variable[0] = '\0';
        value[0] = '\0';

        if (strlen(buffer) == 0 || buffer[0] == '#') {
            continue;
        }

        if (strstr(buffer,":") == 0) {

            snprintf(g_output, CF_BUFSIZE*2,
                "Malformed line (missing :) in resource file %s - skipping\n",
                g_vrcfile);

            CfLog(cferror, g_output,"");
            continue;
        }

        sscanf(buffer, "%[^.].%[^:]:%[^\n]", class, variable, value);

        if (class[0] == '\0' || variable[0] == '\0' || value[0] == '\0') {

            snprintf(g_output, CF_BUFSIZE*2, "%s:%s - Bad resource\n",
                    g_vrcfile, buffer);
            CfLog(cferror, g_output, "");

            snprintf(g_output, CF_BUFSIZE*2,
                    "class=%s,variable=%s,value=%s\n", 
                    class, variable, value);
            CfLog(cferror, g_output,"");

            FatalError("Bad resource");
        }

        if (strcmp(g_classtext[g_vsystemhardclass], class) != 0) {
            /* No point if not equal*/
            continue;  
        }

        if ((mp = strdup(value)) == NULL) {
            perror("malloc");
            FatalError("malloc in ReadRCFile");
        }

        snprintf(g_output, CF_BUFSIZE*2,
                "Redefining resource %s as %s (%s)\n", 
                variable, value, class);

        CfLog(cfverbose, g_output, "");

        c = g_vsystemhardclass;

        switch (GetResource(variable)) {
        case rmountcom:
            g_vmountcomm[c] = mp;
            break;
        case runmountcom:
            g_vunmountcomm[c] = mp;
            break;
        case rethernet:
            g_vifdev[c] = mp;
            break;
        case rmountopts:
            g_vmountopts[c] = mp;
            break;
        case rfstab:
            g_vfstab[c] = mp;
            break;
        case rmaildir:
            g_vmaildir[c] = mp;
            break;
        case rnetstat:
            g_vnetstat[c] = mp;
            break;
        case rpscomm:
            g_vpscomm[c] = mp;
            break;
        case rpsopts:
            g_vpsopts[c] = mp;
            break;
        default:
            snprintf(g_output, CF_BUFSIZE, "Bad resource %s in %s\n",
                    variable, g_vrcfile);
            FatalError(g_output);
            break;
        }
    }

    fclose(fp);
}

/* ----------------------------------------------------------------- */

void
EmptyActionSequence(void)
{
    DeleteItemList(g_vactionseq);
    g_vactionseq = NULL;
}

/* ----------------------------------------------------------------- */

void
GetEnvironment(void)
{
    char env[CF_BUFSIZE], class[CF_BUFSIZE];
    char name[CF_MAXVARSIZE], value[CF_MAXVARSIZE];
    FILE *fp;
    struct stat statbuf;
    time_t now = time(NULL);

    Verbose("Looking for environment from cfenvd...\n");
    snprintf(env, CF_BUFSIZE, "%s/state/%s", WORKDIR, CF_ENV_FILE);

    if (stat(env,&statbuf) == -1) {
        Verbose("\nUnable to detect environment from cfenvd\n\n");
        return;
    }

    if (statbuf.st_mtime < (now - 60*60)) {
        Verbose("Environment data are too old - discarding\n");
        unlink(env);
        return;
    }

    if (!GetMacroValue(g_contextid,"env_time")) {
        snprintf(value, CF_MAXVARSIZE-1, "%s", ctime(&statbuf.st_mtime));
        Chop(value);
        AddMacroValue(g_contextid, "env_time", value);
    } else {
        CfLog(cferror,"Reserved variable $(env_time) in use","");
    }

    Verbose("Loading environment...\n");

    if ((fp = fopen(env, "r")) == NULL) {
        Verbose("\nUnable to detect environment from cfenvd\n\n");
        return;
    }

    while (!feof(fp)) {
        class[0] = '\0';
        name[0] = '\0';
        value[0] = '\0';
        fscanf(fp, "%256s", class);

        if (feof(fp)) {
            break;
        }

        if (strstr(class,"=")) {
            sscanf(class, "%255[^=]=%255s", name, value);
            if (!GetMacroValue(g_contextid, name)) {
                AddMacroValue(g_contextid, name, value);
            }
        } else {
            AddClassToHeap(class);
        }
    }

    fclose(fp);
    Verbose("Environment data loaded\n\n");
}

/* ----------------------------------------------------------------- */

void
EchoValues(void)
{
    struct Item *ip;
    int n = 0;
    char ebuff[CF_EXPANDSIZE];

    Verbose("Accepted domain name: %s\n\n", g_vdomain);

    if (GetMacroValue(g_contextid, "OutputPrefix")) {
        ExpandVarstring("$(OutputPrefix)", ebuff, NULL);
    }

    if (strlen(ebuff) != 0) {
        /* No more than 40 char prefix (!) */
        strncpy(g_vprefix, ebuff, 40);  
    } else {
        strcpy(g_vprefix, "cfng:");
        strcat(g_vprefix, g_vuqname);
    }

    if (g_verbose || g_debug || g_d2 || g_d3) {
        struct Item *ip;

        ListDefinedClasses();

        printf("\nGlobal expiry time for locks: %d minutes\n",
                g_vexpireafter);
        printf("\nGlobal anti-spam elapse time: %d minutes\n\n",
                g_vifelapsed);

        printf("Extensions which should not be directories = ( ");
        for (ip = g_extensionlist; ip != NULL; ip=ip->next) {
            printf("%s ", ip->name);
        }
        printf(")\n");

        printf("Suspicious filenames to be warned about = ( ");
        for (ip = g_suspiciouslist; ip != NULL; ip=ip->next) {
            printf("%s ", ip->name);
        }
        printf(")\n");
    }

    if (g_debug || g_d2 || g_d3) {
        printf("\nFully qualified hostname is: %s\n", g_vfqname);
        printf("Unqualified hostname is: %s\n", g_vuqname);
        printf("\nSystem administrator mail address is: %s\n", g_vsysadm);
        printf("Sensible size = %d\n", g_sensiblefssize);
        printf("Sensible count = %d\n", g_sensiblefilecount);
        printf("Edit File (Max) Size = %d\n\n", g_editfilesize);
        printf("Edit Binary File (Max) Size = %d\n\n", g_editbinfilesize);
        printf("------------------------------------------------------------\n");
        ListDefinedInterfaces();
        printf("------------------------------------------------------------\n");
        ListDefinedBinservers();
        printf("------------------------------------------------------------\n");
        ListDefinedHomeservers();
        printf("------------------------------------------------------------\n");
        ListDefinedHomePatterns();
        printf("------------------------------------------------------------\n");
        ListActionSequence();

        printf("\nWill need to copy from the following trusted sources = ( ");

        for (ip = g_vserverlist; ip !=NULL; ip=ip->next) {
            printf("%s ",ip->name);
        }
        printf(")\n");
        printf("\nUsing mailserver %s\n", g_vmailserver);
        printf("\nLocal mountpoints: ");
        for (ip = g_vmountlist; ip != NULL; ip=ip->next) {
            printf ("%s ",ip->name);
        }
        printf("\n");
        printf("\nDefault route for packets %s\n\n", g_vdefaultroute);
        printf("\nFile repository = %s\n\n", g_vrepository);
        printf("\nNet interface name = %s\n", g_vifdev[g_vsystemhardclass]);
        printf("------------------------------------------------------------\n");
        ListDefinedVariables();
        printf("------------------------------------------------------------\n");
        ListDefinedAlerts();
        printf("------------------------------------------------------------\n");
        ListDefinedStrategies();
        printf("------------------------------------------------------------\n");
        ListDefinedResolvers();
        printf("------------------------------------------------------------\n");
        ListDefinedRequired();
        printf("------------------------------------------------------------\n");
        ListDefinedMountables();
        printf("------------------------------------------------------------\n");
        ListMiscMounts();
        printf("------------------------------------------------------------\n");
        ListUnmounts();
        printf("------------------------------------------------------------\n");
        ListDefinedMakePaths();
        printf("------------------------------------------------------------\n");
        ListDefinedImports();
        printf("------------------------------------------------------------\n");
        ListFiles();
        printf("------------------------------------------------------------\n");
        ListACLs();
        printf("------------------------------------------------------------\n");
        ListFilters();
        printf("------------------------------------------------------------\n");
        ListDefinedIgnore();
        printf("------------------------------------------------------------\n");
        ListFileEdits();
        printf("------------------------------------------------------------\n");
        ListProcesses();
        printf("------------------------------------------------------------\n");
        ListDefinedImages();
        printf("------------------------------------------------------------\n");
        ListDefinedTidy();
        printf("------------------------------------------------------------\n");
        ListDefinedDisable();
        printf("------------------------------------------------------------\n");
        ListDefinedLinks();
        printf("------------------------------------------------------------\n");
        ListDefinedLinkchs();
        printf("------------------------------------------------------------\n");
        ListDefinedScripts();
        printf("------------------------------------------------------------\n");
        ListDefinedPackages();
        printf("------------------------------------------------------------\n");
        ListDefinedMethods();
        printf("------------------------------------------------------------\n");


        if (g_ignorelock) {
            printf("\nIgnoring locks...\n");
        }
    }
}

/* ----------------------------------------------------------------- */

void
CheckSystemVariables(void)
{
    char id[CF_MAXVARSIZE]; 
    char ebuff[CF_EXPANDSIZE];
    int  time, hash, activecfs, locks;

    Debug2("\n\n");

    if (g_vactionseq == NULL) {
        Warning("actionsequence is empty ");
        Warning("perhaps cfagent.conf/update.conf have not yet been set up?");
    }

    /* get effective user id */
    sprintf(id,"%d",geteuid());   

    if (g_vaccesslist != NULL && !IsItemIn(g_vaccesslist,id)) {
        FatalError("Access denied");
    }

    Debug2("cfagent -d : Debugging output enabled.\n");

    if (g_dontdo && (g_verbose || g_debug || g_d2)) {
        printf("cfagent -n: Running in ``All talk and no action'' mode\n");
    }

    if (g_travlinks && (g_verbose || g_debug || g_d2)) {
        printf("cfagent -l : will traverse symbolic links\n");
        printf("             WARNING: This is inherently insecure, "
                "if there are untrusted users!\n");
    }

    if ( ! g_ifconf && (g_verbose || g_debug || g_d2)) {
        printf("cfagent -i : suppressing interface configuration\n");
    }

    if ( g_nofilecheck && (g_verbose || g_debug || g_d2)) {
        printf("cfagent -c : suppressing file checks\n");
    }

    if ( g_noscripts && (g_verbose || g_debug || g_d2)) {
        printf("cfagent -s : suppressing script execution\n");
    }

    if ( g_notidy && (g_verbose || g_debug || g_d2)) {
        printf("cfagent -t : suppressing tidy function\n");
    }

    if ( g_nomounts && (g_verbose || g_debug || g_d2)) {
        printf("cfagent -m : suppressing mount operations\n");
    }

    if ( g_mountcheck && (g_verbose || g_debug || g_d2)) {
        printf("cfagent -C : check mount points\n");
    }


    if (IsDefinedClass("nt")) {
        AddClassToHeap("windows");
    }

    if (g_errorcount > 0) {
        FatalError("Execution terminated after parsing due to errors "
                "in program");
    }

    g_vcanonicalfile = strdup(CanonifyName(g_vinputfile));

    if (GetMacroValue(g_contextid, "LockDirectory")) {
        Verbose("\n[LockDirectory is no longer used - "
                "same as LogDirectory]\n\n");
    }

    if (GetMacroValue(g_contextid,"LogDirectory")) {
        Verbose("\n[LogDirectory is no longer runtime configurable: "
                "use configure --with-workdir=WORKDIR ]\n\n");
    }

    Verbose("LogDirectory = %s\n", g_vlogdir);

    LoadSecretKeys();

    if (GetMacroValue(g_contextid, "childlibpath")) {

        snprintf(g_output, CF_BUFSIZE, "LD_LIBRARY_PATH=%s",
                GetMacroValue(g_contextid, "childlibpath"));

        if (putenv(strdup(g_output)) == 0) {
            Verbose("Setting %s\n", g_output);
        } else {

            Verbose("Failed to set %s\n",
                    GetMacroValue(g_contextid, "childlibpath"));

        }
    }

    if (GetMacroValue(g_contextid,"BindToInterface")) {
        memset(ebuff, 0, CF_BUFSIZE);
        ExpandVarstring("$(BindToInterface)", ebuff, NULL);
        strncpy(g_bindinterface, ebuff, CF_BUFSIZE-1);
        Debug("$(BindToInterface) Expanded to %s\n", g_bindinterface);
    }

    if (GetMacroValue(g_contextid, "MaxCfengines")) {
        activecfs = atoi(GetMacroValue(g_contextid, "MaxCfengines"));

        locks = CountActiveLocks();

        if (locks >= activecfs) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Too many cfagents running (%d/%d)\n", 
                    locks, activecfs);
            CfLog(cferror,g_output,"");
            closelog();
            exit(1);
        }
    }

    if (OptionIs(g_contextid, "Verbose", true)) {
        g_verbose = true;
    }

    if (OptionIs(g_contextid, "Inform", true)) {
        g_inform = true;
    }

    if (OptionIs(g_contextid,"Exclamation", false)) {
        g_exclaim = false;
    }

    g_inform_save = g_inform;

    if (OptionIs(g_contextid, "Syslog", true)) {
        g_logging = true;
    }

    g_logging_save = g_logging;

    if (OptionIs(g_contextid, "DryRun", true)) {
        g_dontdo = true;
        AddClassToHeap("opt_dry_run");
    }

    if (GetMacroValue(g_contextid, "BinaryPaddingChar")) {
        strcpy(ebuff, GetMacroValue(g_contextid, "BinaryPaddingChar"));

        if (ebuff[0] == '\\') {
            switch (ebuff[1]) {
            case '0':
                g_padchar = '\0';
                break;
            case '\n':
                g_padchar = '\n';
                break;
            case '\\':
                g_padchar = '\\';
            }
        } else {
            g_padchar = ebuff[0];
        }
    }


    if (OptionIs(g_contextid, "Warnings", true)) {
        g_warnings = true;
    }

    if (OptionIs(g_contextid, "NonAlphaNumFiles", true)) {
        g_nonalphafiles = true;
    }

    if (OptionIs(g_contextid, "SecureInput", true)) {
        g_cfparanoid = true;
    }

    if (OptionIs(g_contextid, "ShowActions", true)) {
        g_showactions = true;
    }

    if (GetMacroValue(g_contextid,"Umask")) {
        mode_t val;
        val = (mode_t)atoi(GetMacroValue(g_contextid, "Umask"));
        if (umask(val) == (mode_t)-1) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Can't set umask to %o\n", val);
            CfLog(cferror, g_output, "umask");
        }
    }

    if (GetMacroValue(g_contextid, "DefaultCopyType")) {
        if (strcmp(GetMacroValue(g_contextid, "DefaultCopyType"),
                    "mtime") == 0) {
            g_defaultcopytype = 'm';
        }
        if (strcmp(GetMacroValue(g_contextid, "DefaultCopyType"),
                    "checksum") == 0) {
            g_defaultcopytype = 'c';
        }
        if (strcmp(GetMacroValue(g_contextid, "DefaultCopyType"),
                    "binary") == 0) {
            g_defaultcopytype = 'b';
        }
        if (strcmp(GetMacroValue(g_contextid, "DefaultCopyType"),
                    "ctime") == 0) {
            g_defaultcopytype = 't';
        }
    }

    if (GetMacroValue(g_contextid, "ChecksumDatabase")) {
        ExpandVarstring("$(ChecksumDatabase)", ebuff, NULL);

        g_checksumdb = strdup(ebuff);

        if (*g_checksumdb != '/') {
            FatalError("$(ChecksumDatabase) does not expand to "
                    "an absolute filename\n");
        }
    } else {
        snprintf(ebuff, CF_BUFSIZE, "%s/state/cfng_checksum.db", WORKDIR);
        g_checksumdb = strdup(ebuff);
    }

    Verbose("Checksum database is %s\n", g_checksumdb);

    if (GetMacroValue(g_contextid, "CompressCommand")) {
        ExpandVarstring("$(CompressCommand)", ebuff, NULL);

        g_compresscommand = strdup(ebuff);

        if (*g_compresscommand != '/') {
            FatalError("$(ChecksumDatabase) does not expand to "
                    "an absolute filename\n");
        }
    }

    if (OptionIs(g_contextid,"ChecksumUpdates",true)) {
        g_checksumupdates = true;
    }

    if (GetMacroValue(g_contextid,"TimeOut")) {
        time = atoi(GetMacroValue(g_contextid,"TimeOut"));

        if (time < 3 || time > 60) {
            CfLog(cfinform,
                    "TimeOut not between 3 and 60 seconds, ignoring.\n","");
        } else {
            g_cf_timeout = time;
        }
    }

    if (g_nosplay) {
        return;
    }

    time = 0;
    hash = Hash(g_vfqname);

    if (!g_nosplay) {
        if (GetMacroValue(g_contextid, "SplayTime")) {
            time = atoi(GetMacroValue(g_contextid, "SplayTime"));

            if (time < 0) {
                CfLog(cfinform,"SplayTime with negative value, ignoring.\n","");
                return;
            }

            if (!g_donesplay) {
                if (!g_parseonly) {
                    g_donesplay = true;

                    Verbose("Sleeping for SplayTime %d seconds\n\n",
                            (int)(time*60*hash/CF_HASHTABLESIZE));

                    sleep((int)(time*60*hash/CF_HASHTABLESIZE));
                }
            } else {
                Verbose("Time splayed once already - not repeating\n");
            }
        }
    }

    if (OptionIs(g_contextid, "LogTidyHomeFiles", false)) {
        g_logtidyhomefiles = false;
    }
}

/* ----------------------------------------------------------------- */

void
SetReferenceTime(int setclasses)
{
    time_t tloc;
    char vbuff[CF_MAXVARSIZE];

    if ((tloc = time((time_t *)NULL)) == -1) {
        CfLog(cferror,"Couldn't read system clock\n","");
    }

    g_cfstarttime = tloc;

    snprintf(vbuff, CF_BUFSIZE, "%s", ctime(&tloc));

    Verbose("Reference time set to %s\n", ctime(&tloc));

    if (setclasses) {
        AddTimeClass(vbuff);
    }
}


/* ----------------------------------------------------------------- */

void
DoTree(int passes, char *info)
{
    struct Item *action;

    for (g_pass = 1; g_pass <= passes; g_pass++) {
        for (action = g_vactionseq; action !=NULL; action=action->next) {
            SetStartTime(false);

            if (g_vjustactions && (!IsItemIn(g_vjustactions, action->name))) {
                continue;
            }

            if (g_vavoidactions && IsItemIn(g_vavoidactions, action->name)) {
                continue;
            }

            if (IsExcluded(action->classes)) {
                continue;
            }

            if ((g_pass > 1) && NothingLeftToDo()) {
                continue;
            }

            Verbose("\n*********************************************************************\n");
            Verbose(" %s Sched: %s pass %d @ %s",
                    info,action->name, g_pass,ctime(&g_cfinitstarttime));
            Verbose("*********************************************************************\n\n");

            switch(g_action = EvaluateAction(action->name,&g_vaddclasses,g_pass)) {
            case mountinfo:
                if (g_pass == 1) {
                    GetHomeInfo();
                    GetMountInfo();
                }
                break;

            case mkpaths:
                if (!g_nofilecheck) {
                    GetSetuidLog();
                    MakePaths();
                    SaveSetuidLog();
                }
                break;

            case lnks:
                MakeChildLinks();
                MakeLinks();
                break;

            case chkmail:
                MailCheck();
                break;

            case mountall:
                if (!g_nomounts && g_pass == 1) {
                    MountFileSystems();
                }
                break;

            case requir:
            case diskreq:

                CheckRequired();
                break;

            case tidyf:
                if (!g_notidy) {
                    TidyFiles();
                }
                break;

            case shellcom:
                if (!g_noscripts) {
                    Scripts();
                }
                break;

            case chkfiles:
                if (!g_nofilecheck) {
                    GetSetuidLog();
                    CheckFiles();
                    SaveSetuidLog();
                }
                break;

            case disabl:
            case renam:
                DisableFiles();
                break;

            case mountresc:
                if (!g_nomounts) {
                    if (!GetLock("Mountres",
                            CanonifyName(g_vfstab[g_vsystemhardclass]), 0,
                            g_vexpireafter, g_vuqname, 0)) {

                        exit(0);

                        /* Note IFELAPSED must be zero to avoid
                         * conflict with mountresc */
                    }

                    MountHomeBinServers();
                    MountMisc();
                    ReleaseCurrentLock();
                }
                break;

            case umnt:
                Unmount();
                break;

            case edfil:
                if (!g_noedits) {
                    EditFiles();
                }
                break;


            case resolv:

                if (g_pass > 1) {
                    continue;
                }

                if (!GetLock(ASUniqueName("resolve"), "",
                        g_vifelapsed, g_vexpireafter, 
                        g_vuqname, g_cfstarttime)) {
                    continue;
                }

                CheckResolv();
                ReleaseCurrentLock();
                break;

            case imag:
                if (!g_nocopy) {
                    GetSetuidLog();
                    MakeImages();
                    SaveSetuidLog();
                }
                break;

            case netconfig:

                if (g_ifconf) {
                    ConfigureInterfaces();
                }
                break;

            case tzone:
                CheckTimeZone();
                break;

            case procs:
                if (!g_noprocs) {
                    CheckProcesses();
                }
                break;

            case meths:
                if (!g_nomethods) {
                    DoMethods();
                }
                break;

            case pkgs:
                CheckPackages();
                break;

            case plugin:
                break;

            default:
                snprintf(g_output, CF_BUFSIZE*2,
                        "Undefined action %s in sequence\n", action->name);
                FatalError(g_output);
                break;
            }

            DeleteItemList(g_vaddclasses);
            g_vaddclasses = NULL;
        }
    }
}


/* ----------------------------------------------------------------- */

/* 
 * Check the *probable* source for additional action 
 */
int
NothingLeftToDo(void)
{
    struct ShellComm *vscript;
    struct Link *vlink;
    struct File *vfile;
    struct Disable *vdisablelist;
    struct File *vmakepath;
    struct Link *vchlink;
    struct UnMount *vunmount;
    struct Edit *veditlist;
    struct Process *vproclist;
    struct Tidy *vtidy;
    struct Package *vpkg;

    for (vproclist = g_vproclist; vproclist != NULL; vproclist=vproclist->next) {
        if (vproclist->done == 'n') {
            return false;
        }
    }

    for (vscript = g_vscript; vscript != NULL; vscript=vscript->next) {
        if (vscript->done == 'n') {
        return false;
        }
    }

    for (vfile = g_vfile; vfile != NULL; vfile=vfile->next) {
        if (vfile->done == 'n') {
            return false;
        }
    }

    for (vtidy = g_vtidy; vtidy != NULL; vtidy=vtidy->next) {
        if (vtidy->done == 'n') {
            return false;
        }
    }

    for (veditlist = g_veditlist; veditlist != NULL; veditlist=veditlist->next) {
        if (veditlist->done == 'n') {
            return false;
        }
    }

    for (vdisablelist = g_vdisablelist; vdisablelist != NULL;
            vdisablelist=vdisablelist->next) {

        if (vdisablelist->done == 'n') {
            return false;
        }
    }

    for (vmakepath = g_vmakepath; vmakepath != NULL; 
            vmakepath=vmakepath->next) {
        if (vmakepath->done == 'n') {
            return false;
        }
    }

    for (vlink = g_vlink; vlink != NULL; vlink=vlink->next) {
        if (vlink->done == 'n') {
            return false;
        }
    }

    for (vchlink = g_vchlink; vchlink != NULL; vchlink=vchlink->next) {
        if (vchlink->done == 'n') {
            return false;
        }
    }


    for (vunmount = g_vunmount; vunmount != NULL; vunmount=vunmount->next) {
        if (vunmount->done == 'n') {
            return false;
        }
    }

    for (vpkg = g_vpkg; vpkg != NULL; vpkg=vpkg->next) {
        if (vpkg->done == 'n') {
            return false;
        }
    }

    return true;
}

/* ----------------------------------------------------------------- */

enum aseq
EvaluateAction(char *action, struct Item **classlist, int pass)
{
    int i, j = 0;
    char *sp,cbuff[CF_BUFSIZE], actiontxt[CF_BUFSIZE];
    char mod[CF_BUFSIZE], args[CF_BUFSIZE];
    struct Item *ip;

    cbuff[0]='\0';
    actiontxt[0]='\0';
    mod[0] = args[0] = '\0';
    sscanf(action,"%s %[^\n]",mod,args);
    sp = mod;

    while (*sp != '\0') {
        ++j;
        sscanf(sp,"%[^.]",cbuff);

        while ((*sp != '\0') && (*sp !='.')) {
            sp++;
        }

        if (*sp == '.') {
            sp++;
        }

        if (IsHardClass(cbuff)) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Error in action sequence: %s\n", action);
            CfLog(cferror, g_output,"");
            FatalError("You cannot add a reserved class!");
        }

        if (j == 1) {
            g_vifelapsed = g_vdefaultifelapsed;
            g_vexpireafter = g_vdefaultexpireafter;
            strcpy(actiontxt, cbuff);
            continue;

        } else {
            if ((strncmp(actiontxt, "module:", 7) != 0) &&
                    ! IsSpecialClass(cbuff)) {

                AppendItem(classlist, cbuff, NULL);

            }
        }
    }

    BuildClassEnvironment();

    if ((g_verbose || g_debug || g_d2) && *classlist != NULL) {
        printf("\n                  New temporary class additions\n");
        printf("                  --------( Pass %d )-------\n",pass);
        for (ip = *classlist; ip != NULL; ip=ip->next) {
            printf("                             %s\n",ip->name);
        }
    }

    g_action = none;

    for (i = 0; g_actionseqtext[i] != NULL; i++) {
        if (strcmp(g_actionseqtext[i],actiontxt) == 0) {
            Debug("Actionsequence item %s\n", actiontxt);
            g_action = i;
            return (enum aseq) i;
        }
    }

    Debug("Checking if entry is a module\n");

    if (strncmp(actiontxt, "module:", 7) == 0) {
        if (pass == 1) {
            CheckForModule(actiontxt, args);
        }
        return plugin;
    }

    Verbose("No action matched %s\n", actiontxt);
    return(non);
}


/* ----------------------------------------------------------------- */

void
SummarizeObjects(void)
{
    struct cfObject *op;

    Verbose("\n\n++++++++++++++++++++++++++++++++++++++++\n");
    Verbose("Summary of objects involved\n");
    Verbose("++++++++++++++++++++++++++++++++++++++++\n\n");

    for (op = g_vobj; op != NULL; op=op->next) {
        Verbose("    %s\n",op->scope);
    }
}

/*
 * ---------------------------------------------------------------------
 * Level 2
 * ---------------------------------------------------------------------
 */

void
CheckOpts(int argc, char **argv)
{
    extern char *optarg;
    struct Item *actionList;
    int optindex = 0;
    int c;

    while ((c = getopt_long(argc, argv,
            "bBzMgAbKqkhYHd:vlniIf:pPmcCtsSaeEVD:N:LwxXuUj:o:Z:",
            g_options, &optindex)) != EOF) {

        switch ((char) c) {
        case 'E':
            printf("%s: the enforce-links option can only be used "
                    "in interactive mode.\n", g_vprefix);
            printf("%s: Do you really want to blindly enforce ALL "
                    "links? (y/n)\n", g_vprefix);
            if (getchar() != 'y') {
                printf("cfagent: aborting\n");
                closelog();
                exit(1);
            }

            g_enforcelinks = true;
            break;
        case 'B':
            g_updateonly = true;
            break;
        case 'f':
            strncpy(g_vinputfile, optarg, CF_BUFSIZE-1);
            g_vinputfile[CF_BUFSIZE-1] = '\0';
            g_minusf = true;
            break;
        case 'g':
            g_check  = true;
            break;
        case 'd':
            AddClassToHeap("opt_debug");
            switch ((optarg==NULL) ? '3' : *optarg) {
            case '1':
                g_d1 = true;
                break;
            case '2':
                g_d2 = true;
                break;
            case '3':
                g_d3 = true;
                g_verbose = true;
                break;
            case '4':
                g_d4 = true;
                break;
            default:
                g_debug = true;
                break;
            }
            break;
        case 'M':
            g_nomodules = true;
            break;
        case 'K':
            g_ignorelock = true;
            break;
        case 'A':
            g_ignorelock = true;
            break;
        case 'D':
            AddMultipleClasses(optarg);
            break;
        case 'N':
            NegateCompoundClass(optarg, &g_vnegheap);
            break;
        case 'b':
            g_forcenetcopy = true;
            break;
        case 'e':
            g_noedits = true;
            break;
        case 'i':
            g_ifconf = false;
            break;
        case 'I':
            g_inform = true;
            break;
        case 'v':
            g_verbose = true;
            break;
        case 'l':
            FatalError("Option -l is deprecated -- too dangerous");
            break;
        case 'n':
            g_dontdo = true;
            g_ignorelock = true;
            AddClassToHeap("opt_dry_run");
            break;
        case 'p': g_parseonly = true;
            g_ignorelock = true;
            break;
        case 'm':
            g_nomounts = true;
            break;
        case 'c':
            g_nofilecheck = true;
            break;
        case 'C':
            g_mountcheck = true;
            break;
        case 't':
            g_notidy = true;
            break;
        case 's':
            g_noscripts = true;
            break;
        case 'a':
            g_prsysadm = true;
            g_ignorelock = true;
            g_parseonly = true;
            break;
        case 'z':
            g_prmailserver = true;
            g_ignorelock = true;
            g_parseonly = true;
            break;
        case 'Z':
            strncpy(g_methodmd5, optarg, CF_BUFSIZE-1);
            Debug("Got method call reference %s\n", g_methodmd5);
            break;
        case 'L':
            g_killoldlinks = true;
            break;
        case 'V':
            printf("cfng %s\n%s\n", VERSION, g_copyright);
            printf("This program is covered by the GNU Public License "
                    "and may be\n");
            printf("copied free of charge.  No warranty is implied.\n\n");
            exit(0);
        case 'h':
            Syntax();
            exit(0);
        case 'x':
            g_nopreconfig = true;
            break;
        case 'w':
            g_warnings = false;
            break;
        case 'X':
            g_nolinks = true;
            break;
        case 'k':
            g_nocopy = true;
            break;
        case 'S':
            g_silent = true;
            break;
        case 'u':
            g_useenviron = true;
            break;
        case 'U':
            g_underscore_classes = true;
            break;
        case 'H':
            g_nohardclasses = true;
            break;
        case 'P':
            g_noprocs = true;
            break;
        case 'q':
            g_nosplay = true;
            break;
        case 'Y':
            g_cfparanoid = true;
            break;
        case 'j':
            actionList = SplitStringAsItemList(optarg, ',');
            g_vjustactions = ConcatLists(actionList, g_vjustactions);
            break;
        case 'o':
            actionList = SplitStringAsItemList(optarg, ',');
            g_vavoidactions = ConcatLists(actionList, g_vavoidactions);
            break;
        default:
            Syntax();
            exit(1);
        }
    }
}

/* ----------------------------------------------------------------- */

void
CheckForMethod(void)
{
    struct Item *ip, *ip1, *ip2, *args = NULL;
    char argbuffer[CF_BUFSIZE];
    struct Method *mp;
    int i = 0;

    if (strcmp(g_methodname, "cf-nomethod") == 0) {
        return;
    }

    if (! g_minusf) {
        FatalError("Input files claim to be a module "
                "but this is a parent process\n");
    }

    Verbose("\n++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n+\n");
    Verbose("+ This is a private method: %s\n", g_methodname);

    if (g_methodargs == NULL) {
        FatalError("This module was declared a method "
                "but no MethodParameters declaration was given");
    } else {
        Verbose("+\n+ Method argument prototype = (");

        i = 1;

        for (ip = g_methodargs; ip != NULL; ip = ip->next) {
            i++;
        }

        g_methodargv = (char **) malloc(sizeof(char *) * i);

        i = 0;

        for (ip = g_methodargs; ip != NULL; ip = ip->next) {
            /* Fill this temporarily with the formal parameters */
            g_methodargv[i++] = ip->name;
        }

        g_methodargc = i;

        Verbose(" )\n+\n");
    }

    Verbose("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

    Verbose("Looking for a data package for this method (%s)\n", g_methodmd5);

    if (!ChildLoadMethodPackage(g_methodname, g_methodmd5)) {

        snprintf(g_output, CF_BUFSIZE,
            "No valid incoming request to execute method (%s)\n",
            g_methodname);

        CfLog(cfinform,g_output,"");
        exit(0);
    }
    Debug("Method package looks ok -- proceeding\n");
}


/* ----------------------------------------------------------------- */

void
CheckMethodReply(void)
{
    if (ScopeIsMethod()) {
        if (strlen(g_methodreplyto) > 0) {
            Banner("Method reply message");
            DispatchMethodReply();
        }
    }
}

/* ----------------------------------------------------------------- */

int
GetResource(char *var)
{
    int i;

    for (i = 0; g_vresources[i] != '\0'; i++) {
        if (strcmp(g_vresources[i],var)==0) {
            return i;
        }
    }

    snprintf(g_output, CF_BUFSIZE, "Unknown resource %s in %s",
            var, g_vrcfile);
    FatalError(g_output);
    return 0;
}

/* ----------------------------------------------------------------- */

void
SetStartTime(int setclasses)
{
    time_t tloc;

    if ((tloc = time((time_t *)NULL)) == -1) {
        CfLog(cferror,"Couldn't read system clock\n","");
    }

    g_cfinitstarttime = tloc;

    Debug("Job start time set to %s\n",ctime(&tloc));
}

/* ----------------------------------------------------------------- */

void
BuildClassEnvironment(void)
{
    struct Item *ip;
    int size = 0;

    Debug("(BuildClassEnvironment)\n");

    snprintf(g_allclassbuffer, CF_BUFSIZE, "%s=", CF_ALLCLASSESVAR);

    for (ip = g_vheap; ip != NULL; ip=ip->next) {
        if (IsDefinedClass(ip->name)) {
            if ((size += strlen(ip->name)) > CF_BUFSIZE - BUFFER_MARGIN) {
                Verbose("Class buffer overflowed, dumping class "
                        "environment for modules\n");
                Verbose("This would probably crash the exec interface "
                        "on most machines\n");
                return;
            } else {
                size++; /* Allow for : separator */
            }
            strcat(g_allclassbuffer, ip->name);
            strcat(g_allclassbuffer, ":");
        }
    }

    for (ip = g_valladdclasses; ip != NULL; ip=ip->next) {
        if (IsDefinedClass(ip->name)) {

            if ((size += strlen(ip->name)) > 4 * CF_BUFSIZE - BUFFER_MARGIN) {
                Verbose("Class buffer overflowed, dumping class "
                        "environment for modules\n");
                Verbose("This would probably crash the exec interface "
                        "on most machines\n");
                return;
            } else {
                size++; /* Allow for : separator */
            }

            strcat(g_allclassbuffer, ip->name);
            strcat(g_allclassbuffer, ":");
        }
    }

    Debug2("---\nENVIRONMENT: %s\n---\n", g_allclassbuffer);

    if (g_useenviron) {
        if (putenv(g_allclassbuffer) == -1) {
            perror("putenv");
        }
    }
}

/* ----------------------------------------------------------------- */

void
Syntax(void)
{
    int i;

    printf("cfng: A system configuration engine (cfagent)\n%s\n%s\n",
            VERSION, g_copyright);
    printf("\n");
    printf("Options:\n\n");

    for (i=0; g_options[i].name != NULL; i++) {
        printf("--%-20s    (-%c)\n", g_options[i].name,
                (char)g_options[i].val);
    }

    printf("\nDebug levels: 1=parsing, 2=running, 3=summary, "
            "4=expression eval\n");

    printf("\nReport issues at http://cfng.tigris.org or to "
            "issues@cfng.tigris.org\n");
}
