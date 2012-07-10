/*
 * $Id: globals.c 719 2004-05-23 01:29:46Z skaar $
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

/*
 * --------------------------------------------------------------------
 * Global variables for cfng/cfengine (designated with g_ prefix)
 * --------------------------------------------------------------------
 */


/* General workspace, contents not guaranteed */
char g_vbuff[CF_BUFSIZE]; 

char g_output[CF_BUFSIZE*2];
int  g_authenticated = false;
int  g_checksumupdates = false;

int  g_pass;

char *g_checksumdb;
char g_padchar = ' ';
char g_contextid[32];
char g_cfpubkeyfile[CF_BUFSIZE];
char g_cfprivkeyfile[CF_BUFSIZE];
char g_avdb[1024];

RSA *PRIVKEY = NULL, *PUBKEY = NULL;

#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
pthread_mutex_t MUTEX_SYSCALL = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t MUTEX_LOCK = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * --------------------------------------------------------------------
 * cfservd.main object: root application object.
 * --------------------------------------------------------------------
 */

struct Auth *g_vadmit = NULL;
struct Auth *g_vadmittop = NULL;
struct Auth *g_vdeny = NULL;
struct Auth *g_vdenytop = NULL;

/*
 * --------------------------------------------------------------------
 * cfng.main object: root application object.
 * --------------------------------------------------------------------
 */

char *g_copyright = "Copyright (C) 1994-2004 Free Software Foundation, Inc.";

char *g_vpreconfig = "cf.preconf";
char *g_vrcfile = "cfrc";

char *g_vsetuidlog = NULL;
char *g_varch = NULL;
char *g_varch2 = NULL;
char *g_vrepository = NULL;
char *g_compresscommand = NULL;

char g_vprefix[CF_MAXVARSIZE];

char g_vinputfile[CF_BUFSIZE];
char g_vcurrentfile[CF_BUFSIZE];
char g_vlogfile[CF_BUFSIZE];
char g_allclassbuffer[4*CF_BUFSIZE];
char g_elseclassbuffer[CF_BUFSIZE];
char g_failoverbuffer[CF_BUFSIZE];
char g_chroot[CF_BUFSIZE];
char g_editbuff[CF_BUFSIZE];

short g_debug = false;
short g_d1 = false;
short g_d2 = false;
short g_d3 = false;
short g_d4 = false;
short g_verbose = false;
short g_inform = false;
short g_check = false;
short g_exclaim = true;
short g_compatibility_mode = false;
short g_logging = false;
short g_inform_save;
short g_logging_save;
short g_cfparanoid = false;
short g_showactions = false;
short g_logtidyhomefiles = true;
short g_updateonly = false;

char g_fork = 'n';

int g_rpctimeout = 60;          /* seconds */
int g_sensiblefilecount = 2;
int g_sensiblefssize = 1000;

time_t g_cfstarttime;
time_t g_cfinitstarttime;

dev_t g_rootdevice = 0;

enum classes g_vsystemhardclass;

/* see GetNameInfo(), main.c */
struct Item g_vdefaultbinserver = {
    'n',
    NULL,
    NULL,
    0,
    0,
    0,
    NULL
};

/* For uname (2) */
struct utsname g_vsysname;

mode_t g_defaultmode = (mode_t) 0755;
mode_t g_defaultsystemmode = (mode_t) 0644;

int g_vifelapsed = 1;
int g_vexpireafter = 120;
int g_vdefaultifelapsed = 1;
int g_vdefaultexpireafter = 120; /* minutes */

struct cfagent_connection *g_conn = NULL;
struct Item *g_vexcludecache = NULL;

struct cfObject *g_objectlist = NULL;

struct Item *g_ipaddresses = NULL;

/*
 * --------------------------------------------------------------------
 * Anomaly data structures
 * --------------------------------------------------------------------
 */
char *g_ecgsocks[CF_ATTR][2] = {
    {"137", "netbiosns"},
    {"138", "netbiosdgm"},
    {"139", "netbiosssn"},
    {"194", "irc"},
    {"5308", "cfengine"},
    {"2049", "nfsd"},
    {"25", "smtp"},
    {"80", "www"},
    {"21", "ftp"},
    {"22", "ssh"},
    {"443", "wwws"}
};

char *g_tcpnames[CF_NETATTR] = {
    "icmp",
    "udp",
    "dns",
    "tcpsyn",
    "tcpack",
    "tcpfin",
    "misc"
};

/*
 * --------------------------------------------------------------------
 * Methods
 * --------------------------------------------------------------------
 */
struct Item *g_methodargs = NULL;
char ** g_methodargv = NULL;
int  g_methodargc = 0;
char *g_vmethodproto[] = {
    "NAME:",
    "TRUSTEDFILE:",
    "TIMESTAMP:",
    "REPLYTO:",
    "SENDCLASS:",
    "ATTACH-ARG:",
    "ISREPLY:",
    NULL
};

char g_methodname[CF_BUFSIZE];
char g_methodfilename[CF_BUFSIZE];
char g_methodreplyto[CF_BUFSIZE];
char g_methodforce[CF_BUFSIZE];
char g_methodreturnvars[CF_BUFSIZE];
char g_methodreturnclasses[CF_BUFSIZE];
char g_methodmd5[CF_BUFSIZE];

/*
 * --------------------------------------------------------------------
 * Data/list structures - root pointers
 * --------------------------------------------------------------------
 */
struct Item *g_vtimezone = NULL;
struct Item *g_vmountlist = NULL;
struct Item *g_vexcludecopy = NULL;
struct Item *g_vautodefine = NULL;
struct Item *g_vsinglecopy = NULL;
struct Item *g_vexcludelink = NULL;
struct Item *g_vcopylinks = NULL;
struct Item *g_vlinkcopies = NULL;
struct Item *g_vexcludeparse = NULL;
struct Item *g_vcplnparse = NULL;
struct Item *g_vincludeparse = NULL;
struct Item *g_vignoreparse = NULL;
struct Item *g_vserverlist = NULL;
struct Item *g_vrpcpeerlist = NULL;
struct Item *g_vredefines = NULL;

/* Points to the base of the attribute heap */
struct Item *g_vheap = NULL;
struct Item *g_vnegheap = NULL;

/* Points to the list of mountables */
struct Mountables *g_vmountables = NULL;
struct Mountables *g_vmountablestop = NULL;

struct cfObject *g_vobjtop = NULL;
struct cfObject *g_vobj = NULL;

struct Item *g_valerts = NULL;
struct Item *g_vmounted = NULL;

/* Points to the list of tidy specs */
struct Tidy *g_vtidy = NULL;
struct Tidy *g_vtidytop = NULL;

/* Points to proc list */
struct Item *g_vprocesses = NULL;

/* List of required file systems */
struct Disk *g_vrequired = NULL;
struct Disk *g_vrequiredtop = NULL;

/* List of scripts to execute */
struct ShellComm *g_vscript = NULL;
struct ShellComm *g_vscripttop = NULL;
struct Interface *g_viflist = NULL;
struct Interface *g_viflisttop = NULL;

/* Files systems already mounted */
struct Mounted *g_mounted = NULL;

struct MiscMount *g_vmiscmount = NULL;
struct MiscMount *g_vmiscmounttop = NULL;
struct Item *g_vbinservers = &g_vdefaultbinserver;
struct Link *g_vlink = NULL;
struct Link *g_vlinktop = NULL;
struct File *g_vfile = NULL;
struct File *g_vfiletop = NULL;
struct Image *g_vimage = NULL;
struct Image *g_vimagetop=NULL;
struct Method *g_vmethods = NULL;
struct Method *g_vmethodstop=NULL;
struct Item *g_vhomeservers = NULL;
struct Item *g_vsetuidlist = NULL;
struct Disable *g_vdisablelist = NULL;
struct Disable *g_vdisabletop = NULL;
struct File *g_vmakepath = NULL;
struct File *g_vmakepathtop = NULL;
struct Link *g_vchlink = NULL;
struct Link *g_vchlinktop = NULL;
struct Item *g_vignore = NULL;
struct Item *g_vhomepatlist = NULL;
struct Item *g_extensionlist = NULL;
struct Item *g_suspiciouslist = NULL;
struct Item *g_schedule = NULL;
struct Item *g_spooldirlist = NULL;
struct Item *g_nonattackerlist = NULL;
struct Item *g_multiconnlist = NULL;
struct Item *g_trustkeylist = NULL;
struct Item *g_dhcplist = NULL;
struct Item *g_allowuserlist = NULL;
struct Item *g_skipverify = NULL;
struct Item *g_attackerlist = NULL;
struct Item *g_mountoptlist = NULL;
struct Item *g_vresolve = NULL;
struct Item *g_vimport = NULL;
struct Item *g_vactionseq=NULL;
struct Item *g_vaccesslist=NULL;
struct Item *g_vaddclasses=NULL;           /* Action sequence defs  */
struct Item *g_valladdclasses=NULL;        /* All classes */
struct Item *g_vjustactions=NULL;
struct Item *g_vavoidactions=NULL;
struct UnMount *g_vunmount=NULL;
struct UnMount *g_vunmounttop=NULL;
struct Edit *g_veditlist=NULL;
struct Edit *g_veditlisttop=NULL;
struct Filter *g_vfilterlist=NULL;
struct Filter *g_vfilterlisttop=NULL;
struct CFACL  *g_vacllist=NULL;
struct CFACL  *g_vacllisttop=NULL;
struct Strategy *g_vstrategylist=NULL;
struct Strategy *g_vstrategylisttop=NULL;

struct Item *g_vclassdefine=NULL;
struct Process *g_vproclist=NULL;
struct Process *g_vproctop=NULL;
struct Item *g_vreposlist=NULL;

struct Package *g_vpkg=NULL;    /* Head of the packages item list */
struct Package *g_vpkgtop=NULL; /* The last packages item we added */

/*
 * --------------------------------------------------------------------
 * Resource names
 * --------------------------------------------------------------------
 */

/* one for each major variable in class.c */
char *g_vresources[] = {
    "mountcomm",
    "unmountcomm",
    "ethernet",
    "mountopts",
    "unused",
    "fstab",
    "maildir",
    "netstat",
    "pscomm",
    "psopts",
    NULL
};


/*
 * --------------------------------------------------------------------
 * Reserved variables
 * --------------------------------------------------------------------
 */

char   g_vmailserver[CF_BUFSIZE];

char      g_vfaculty[CF_MAXVARSIZE];
char       g_vdomain[CF_MAXVARSIZE];
char       g_vsysadm[CF_MAXVARSIZE];
char      g_vnetmask[CF_MAXVARSIZE];
char    g_vbroadcast[CF_MAXVARSIZE];
char g_vdefaultroute[CF_MAXVARSIZE];
char      g_vnfstype[CF_MAXVARSIZE];
char       g_vfqname[CF_MAXVARSIZE];
char       g_vuqname[CF_MAXVARSIZE];
char       g_logfile[CF_MAXVARSIZE];

char         g_vyear[5];
char         g_vday[3];
char         g_vmonth[4];
char         g_vhr[3];
char         g_vminute[3];
char         g_vsec[3];


/*
 * --------------------------------------------------------------------
 * Command line options (#include "getopt.h")
 * --------------------------------------------------------------------
 */

struct option g_options[] = {
    { "help", no_argument, 0, 'h' },
    { "debug", optional_argument, 0, 'd' },
    { "method", required_argument, 0, 'Z' },
    { "verbose", no_argument, 0, 'v' },
    { "traverse-links", no_argument, 0, 'l' },
    { "recon", no_argument, 0, 'n' },
    { "dry-run", no_argument, 0, 'n'},
    { "just-print", no_argument, 0, 'n'},
    { "no-ifconfig", no_argument, 0, 'i' },
    { "file", required_argument, 0, 'f' },
    { "parse-only", no_argument, 0, 'p' },
    { "no-mount", no_argument, 0, 'm' },
    { "no-check-files", no_argument, 0, 'c' },
    { "no-check-mounts", no_argument, 0, 'C' },
    { "no-tidy", no_argument, 0, 't' },
    { "no-commands", no_argument, 0, 's' },
    { "sysadm", no_argument, 0, 'a' },
    { "version", no_argument, 0, 'V' },
    { "define", required_argument, 0, 'D' },
    { "negate", required_argument, 0, 'N' },
    { "undefine", required_argument, 0, 'N' },
    { "delete-stale-links", no_argument, 0, 'L' },
    { "no-warn", no_argument, 0, 'w' },
    { "silent", no_argument, 0, 'S' },
    { "quiet", no_argument, 0, 'w' },
    { "no-preconf", no_argument, 0, 'x' },
    { "no-links", no_argument, 0, 'X'},
    { "no-edits", no_argument, 0, 'e'},
    { "enforce-links", no_argument, 0, 'E'},
    { "no-copy", no_argument, 0, 'k'},
    { "use-env", no_argument, 0, 'u'},
    { "no-processes", no_argument, 0, 'P'},
    { "underscore-classes", no_argument, 0, 'U'},
    { "no-hard-classes", no_argument, 0, 'H'},
    { "no-splay", no_argument, 0, 'q'},
    { "no-lock", no_argument, 0, 'K'},
    { "auto", no_argument, 0, 'A'},
    { "inform", no_argument, 0, 'I'},
    { "no-modules", no_argument, 0, 'M'},
    { "force-net-copy", no_argument, 0, 'b'},
    { "secure-input", no_argument, 0, 'Y'},
    { "zone-info", no_argument, 0, 'z'},
    { "update-only", no_argument, 0, 'B'},
    { "check-contradictions", no_argument, 0, 'g'},
    { "just", required_argument, 0, 'j'},
    { "avoid", required_argument, 0, 'o'},
    { NULL, 0, 0, 0 }
};


 /* ----------------------------------------------------------------- */
 /* Actions                                                           */
 /* ----------------------------------------------------------------- */

/*
 * --------------------------------------------------------------------
 * Actions
 * --------------------------------------------------------------------
 */
char *g_actiontext[] = {
    "",
    "Control Defintions:",
    "Alerts:",
    "Groups:",
    "File Imaging:",
    "Resolve:",
    "Processes:",
    "Files:",
    "Tidy:",
    "Home Servers:",
    "Binary Servers:",
    "Mail Server:",
    "Required Filesystems",
    "Disks (Required)",
    "Reading Mountables",
    "Links:",
    "Import files:",
    "User Shell Commands:",
    "Rename or Disable Files:",
    "Rename files:",
    "Make Directory Path:",
    "Ignore File Paths:",
    "Broadcast Mode:",
    "Default Packet Route:",
    "Miscellaneous Mountables:",
    "Edit Simple Text File:",
    "Unmount filesystems:",
    "Admit network access:",
    "Deny network access:",
    "Access control lists:",
    "Additional network interfaces:",
    "Search filter objects:",
    "Strategies:",
    "Package Checks:",
    "Method Function Calls",
    NULL
};


/* 
 * The actions which may be specified as indexed macros in the "special"
 * section of the file  
 */
char *g_actionid[] = {
    "",
    "control",
    "alerts",
    "groups",
    "copy",
    "resolve",
    "processes",
    "files",
    "tidy",
    "homeservers",
    "binservers",
    "mailserver",
    "required",
    "disks",
    "mountables",
    "links",
    "import",
    "shellcommands",
    "disable",
    "rename",
    "directories",
    "ignore",
    "broadcast",
    "defaultroute",
    "miscmounts",
    "editfiles",
    "unmount",
    "admit",
    "deny",
    "acl",
    "interfaces",
    "filters",
    "strategies",
    "packages",
    "methods",
    NULL
};

/* The actions which may be specified as indexed */
char *g_builtins[] = {
    "",
    "randomint",
    "isnewerthan",
    "accessedbefore",
    "changedbefore",
    "fileexists",
    "isdir",
    "islink",
    "isplain",
    "execresult",
    "returnszero",
    "iprange",
    "hostrange",
    "isdefined",
    "strcmp",
    "regcmp",
    "showstate",
    "friendstatus",
    "readfile",
    "returnvariables",
    "returnclasses",
    "syslog",
    "setstate",
    "unsetstate",
    "prepmodule",
    "a",
    "readarray",
    "readtable",
    NULL
};


/*
 * --------------------------------------------------------------------
 * File/image actions
 * --------------------------------------------------------------------
 */
char *g_fileactiontext[] = {
    "warnall",
    "warnplain",
    "warndirs",
    "fixall",
    "fixplain",
    "fixdirs",
    "touch",
    "linkchildren",
    "create",
    "compress",
    "alert",
    NULL
};

/* ----------------------------------------------------------------- */

char *g_actionseqtext[] = {
    "directories",
    "links",
    "mailcheck",
    "required",
    "disks",
    "tidy",
    "shellcommands",
    "files",
    "disable",
    "rename",
    "addmounts",
    "editfiles",
    "mountall",
    "unmount",
    "resolve",
    "copy",
    "netconfig",
    "checktimezone",
    "mountinfo",
    "processes",
    "packages",
    "methods",
    "none",
    NULL
};


/*
 * --------------------------------------------------------------------
 * Package check actions (comparisons)
 * --------------------------------------------------------------------
 */
char *g_cmpsensetext[] = {
    "eq",
    "gt",
    "lt",
    "ge",
    "le",
    "ne",
    NULL
};

/*
 * --------------------------------------------------------------------
 * Available package managers
 * --------------------------------------------------------------------
 */
char *g_pkgmgrtext[] = {
    "rpm",
    "dpkg",   /* aptget ? */
    NULL
};


/*
 * --------------------------------------------------------------------
 * Parse object: Variables belonging to the parse object
 * --------------------------------------------------------------------
 */


/* for re-using parser code in cfservd */
short g_iscfengine;  

short g_parsing = false;
short g_noabspath = false;
short g_travlinks = false;
short g_ptravlinks = false;
short g_deadlinks = true;
short g_dontdo = false;
short g_ifconf = true;
short g_parseonly = false;
short g_gotmountinfo = true;
short g_nomounts = false;
short g_nomodules = false;
short g_nofilecheck = false;
short g_notidy = false;
short g_noscripts = false;
short g_prsysadm = false;
short g_prmailserver = false;
short g_mountcheck = false;
short g_noprocs = false;
short g_nomethods = false;
short g_noedits = false;
short g_killoldlinks = false;
short g_ignorelock = false;
short g_nopreconfig = false;
short g_warnings = true;
short g_nonalphafiles = false;
short g_minusf = false;
short g_nolinks = false;
short g_enforcelinks = false;
short g_nocopy = false;
short g_forcenetcopy = false;
short g_silent = false;
short g_editverbose = false;
short g_linksilent;

short g_rotate = 0;
short g_useenviron = false;
short g_promatches = -1;
short g_edabortmode = false;
short g_underscore_classes = false;
short g_nohardclasses = false;
short g_nosplay = false;
short g_donesplay = false;

struct Item *g_vaclbuild = NULL;
struct Item *g_vfilterbuild = NULL;
struct Item *g_vstrategybuild = NULL;

char g_tidydirs = 'n';
char g_xdev = false;
char g_imagebackup='y';
char g_trustkey = 'n';
char g_preservetimes = 'n';
char g_typecheck = 'y';
char g_scan = 'n';
char g_linktype = 's';
char g_agetype = 'a';
char g_copytype = 't';
char g_defaultcopytype = 't';
char g_reposchar = '_';
char g_listseparator = ':';
char g_linkdirs = 'k';
char g_discomp = '=';
char g_useshell = 'y';  /* yes or no or dumb */
char g_preview = 'n';  /* yes or no */
char g_purge = 'n';
char g_logp = 'd';  /* y,n,d=default*/
char g_informp = 'd';
char g_mountmode = 'w';   /* o or w for rw/ro*/
char g_deletedir = 'y';   /* t=true */
char g_deletefstab = 'y';
char g_force = 'n';
char g_forceipv4 = 'n';
char g_forcelink = 'n';
char g_forcedirs = 'n';
char g_stealth = 'n';
char g_checksum = 'n'; /* n,m,s */
char g_compress = 'n';

char *g_findertype;
char *g_vuidname;
char *g_vgidname;
char *g_filtername;
char *g_strategyname;
char *g_groupbuff;
char *g_actionbuff;
char *g_currentobject;
char *g_currentitem;
char *g_classbuff;
char *g_linkfrom;
char *g_linkto;
char *g_error;
char *g_mountfrom;
char *g_mountonto;
char *g_mountopts;
char *g_destination;
char *g_imageaction;
char *g_vifname[16];
char *g_vifnameoverride[16];
char *g_chdir;
char *g_localrepos;
char *g_expr;
char *g_currentauthpath;
char *g_restart;
char *g_filterdata;
char *g_strategydata;
char *g_pkgver;     /* value of ver option in packages: */

short g_prosignal;
char  g_proaction;

char g_procomp;
char g_imgcomp;
int  g_imgsize;

int g_errorcount = 0;
int g_linenumber = 1;

int g_haveuid = 0;
int g_disablesize = 99999999;
int g_tidysize=0;
int g_vrecurse;
int g_vage;
int g_vtimeout = 0;
int g_pifelapsed = 0;
int g_pexpireafter = 0;

mode_t g_umask = 0;
mode_t g_plusmask;
mode_t g_minusmask;

u_long g_plusflag;
u_long g_minusflag;

/* Parsing flags etc */
enum actions g_action = none;
enum vnames g_controlvar = nonexistentvar;
enum fileactions g_fileaction = warnall;
enum cmpsense g_cmpsense = cmpsense_eq; /* Comparison for packages: */
enum pkgmgrs g_pkgmgr = pkgmgr_none;  /* Which package mgr to query */
enum pkgmgrs g_defaultpkgmgr = pkgmgr_none;

flag g_action_is_link = false;
flag g_action_is_linkchildren = false;
flag g_mount_onto = false;
flag g_mount_from = false;
flag g_have_restart = false;
flag g_actionpending = false;
flag g_homecopy = false;
char g_encrypt = 'n';
char g_verify = 'n';
char g_compatibility = 'n';

flag g_mount_ro = false;

char *g_commattributes[] = {
    "findertype",
    "recurse",
    "mode",
    "owner",
    "group",
    "age",
    "action",
    "pattern",
    "links",
    "type",
    "destination",
    "force",
    "forcedirs",
    "forceipv4",
    "forcereplyto",
    "backup",
    "rotate",
    "size",
    "matches",
    "signal",
    "exclude",
    "copy",
    "symlink",
    "copytype",
    "linktype",
    "include",
    "dirlinks",
    "rmdirs",
    "server",
    "define",
    "elsedefine",
    "failover",
    "timeout",
    "freespace",
    "nofile",
    "acl",
    "purge",
    "useshell",
    "syslog",
    "inform",
    "ipv4",
    "netmask",
    "broadcast",
    "ignore",
    "deletedir",
    "deletefstab",
    "stealth",
    "checksum",
    "flags",
    "encrypt",
    "verify",
    "root",
    "typecheck",
    "umask",
    "compress",
    "filter",
    "background",
    "chdir",
    "chroot",
    "preview",
    "repository",
    "timestamps",
    "trustkey",
    "oldserver",
    "mountoptions",      /* HvB : Bas van der Vlies */
    "readonly",          /* HvB : Bas van der Vlies */
    "version",
    "cmp",
    "pkgmgr",
    "xdev",
    "returnvars",
    "returnclasses",
    "sendclasses",
    "ifelapsed",
    "expireafter",
    "scanarrivals",
    "noabspath",
    NULL
};

char *g_vfilternames[] = {
    "Result", /* quoted string of combinatorics, classes of each result */
    "Owner",
    "Group",
    "Mode",
    "Type",
    "FromCtime",
    "ToCtime",
    "FromMtime",
    "ToMtime",
    "FromAtime",
    "ToAtime",
    "FromSize",
    "ToSize",
    "ExecRegex",
    "NameRegex",
    "DefineClasses",
    "ElseDefineClasses",
    "ExecProgram",
    "IsSymLinkTo",
    "PID",
    "PPID",
    "PGID",
    "RSize",
    "VSize",
    "Status",
    "Command",
    "FromTTime",
    "ToTTime",
    "FromSTime",
    "ToSTime",
    "TTY",
    "Priority",
    "Threads",
    "NoFilter",
    NULL
};


/*
 * --------------------------------------------------------------------
 * Editfiles object / variables ( uses Item )
 * --------------------------------------------------------------------
 */

char g_veditabort[CF_BUFSIZE];

int g_editfilesize = 10000;
int g_editbinfilesize = 10000000;

int g_numberofedits = 0;
int g_currentlinenumber = 1;           /* current line number in file */
struct Item *g_currentlineptr = NULL;  /* Ptr to current line */

struct re_pattern_buffer *g_searchpattbuff;
struct re_pattern_buffer *g_pattbuffer;

int g_editgrouplevel=0;
int g_searchreplacelevel=0;
int g_foreachlevel = 0;

int g_autocreated = 0;

char g_commentstart[CF_MAXVARSIZE];
char g_commentend[CF_MAXVARSIZE];

char *g_veditnames[] = {
    "NoEdit",
    "DeleteLinesStarting",
    "DeleteLinesNotStarting",
    "DeleteLinesContaining",
    "DeleteLinesNotContaining",
    "DeleteLinesMatching",
    "DeleteLinesNotMatching",
    "DeleteLinesStartingFileItems",
    "DeleteLinesContainingFileItems",
    "DeleteLinesMatchingFileItems",
    "DeleteLinesNotStartingFileItems",
    "DeleteLinesNotContainingFileItems",
    "DeleteLinesNotMatchingFileItems",
    "AppendIfNoSuchLine",
    "PrependIfNoSuchLine",
    "WarnIfNoSuchLine",
    "WarnIfLineMatching",
    "WarnIfNoLineMatching",
    "WarnIfLineStarting",
    "WarnIfLineContaining",
    "WarnIfNoLineStarting",
    "WarnIfNoLineContaining",
    "HashCommentLinesContaining",
    "HashCommentLinesStarting",
    "HashCommentLinesMatching",
    "SlashCommentLinesContaining",
    "SlashCommentLinesStarting",
    "SlashCommentLinesMatching",
    "PercentCommentLinesContaining",
    "PercentCommentLinesStarting",
    "PercentCommentLinesMatching",
    "ResetSearch",
    "SetSearchRegExp",
    "LocateLineMatching",
    "InsertLine",
    "AppendIfNoSuchLinesFromFile",
    "IncrementPointer",
    "ReplaceLineWith",
    "DeleteToLineMatching",
    "HashCommentToLineMatching",
    "PercentCommentToLineMatching",
    "SetScript",
    "RunScript",
    "RunScriptIfNoLineMatching",
    "RunScriptIfLineMatching",
    "AppendIfNoLineMatching",
    "PrependIfNoLineMatching",
    "DeleteNLines",
    "EmptyEntireFilePlease",
    "GotoLastLine",
    "BreakIfLineMatches",
    "BeginGroupIfNoMatch",
    "BeginGroupIfNoLineMatching",
    "BeginGroupIfNoSuchLine",
    "EndGroup",
    "Append",
    "Prepend",
    "SetCommentStart",
    "SetCommentEnd",
    "CommentLinesMatching",
    "CommentLinesStarting",
    "CommentToLineMatching",
    "CommentNLines",
    "UnCommentNLines",
    "ReplaceAll",
    "With",
    "SetLine",
    "FixEndOfLine",
    "AbortAtLineMatching",
    "UnsetAbort",
    "AutomountDirectResources",
    "UnCommentLinesContaining",
    "UnCommentLinesMatching",
    "InsertFile",
    "CommentLinesContaining",
    "BeginGroupIfFileIsNewer",
    "BeginGroupIfFileExists",
    "BeginGroupIfNoLineContaining",
    "BeginGroupIfDefined",
    "BeginGroupIfNotDefined",
    "AutoCreate",
    "ForEachLineIn",
    "EndLoop",
    "ReplaceLinesMatchingField",
    "SplitOn",
    "AppendToLineIfNotContains",
    "DeleteLinesAfterThisMatching",
    "DefineClasses",
    "ElseDefineClasses",
    "CatchAbort",
    "Backup",
    "Syslog",
    "Inform",
    "Recurse",
    "EditMode",
    "WarnIfContainsString",
    "WarnIfContainsFile",
    "Ignore",
    "Exclude",
    "Include",
    "Repository",
    "Umask",
    "UseShell",
    "Filter",
    "DefineInGroup",
    "IfElapsed",
    "ExpireAfter",
    NULL
};


/*
 * --------------------------------------------------------------------
 * Processes object
 * --------------------------------------------------------------------
 */
char *g_signals[highest_signal];  /* This is initialized to zero */


/*
 * --------------------------------------------------------------------
 * Network client-server object
 * --------------------------------------------------------------------
 */
char g_cfserver[CF_MAXVARSIZE];
char g_bindinterface[CF_BUFSIZE];
unsigned short g_portnumber = 0;
char g_vipaddress[18];
int  g_cf_timeout = 10;

int  g_cfsignature = 0;
char g_cfdes1[9];
char g_cfdes2[9];
char g_cfdes3[9];

char *g_protocol[] = {
    "EXEC",
    "AUTH",  /* old protocol */
    "GET",
    "OPENDIR",
    "SYNCH",
    "CLASSES",
    "MD5",
    "SMD5",
    "CAUTH",
    "SAUTH",
    "SSYNCH",
    "SGET",
    NULL
};


/*
 * --------------------------------------------------------------------
 * Adaptive lock object
 * --------------------------------------------------------------------
 */
char g_vlockdir[CF_BUFSIZE];
char g_vlogdir[CF_BUFSIZE];

char *g_vcanonicalfile = NULL;

FILE *g_vlogfp = NULL;

char g_cflock[CF_BUFSIZE];
char g_savelock[CF_BUFSIZE];
char g_cflog[CF_BUFSIZE];
char g_cflast[CF_BUFSIZE];
char g_lockdb[CF_BUFSIZE];
