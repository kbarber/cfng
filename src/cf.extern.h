/*
 * $Id:$
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


#if defined HAVE_PTHREAD_H && (defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD)
extern pthread_mutex_t MUTEX_SYSCALL;
extern pthread_mutex_t MUTEX_LOCK;
#endif

extern int g_pass;
extern RSA *PRIVKEY, *PUBKEY;

extern char g_bindinterface[CF_BUFSIZE];
extern char *g_ecgsocks[CF_ATTR][2];
extern char *g_tcpnames[CF_NETATTR];
extern char **g_methodargv;
extern char g_methodreturnvars[CF_BUFSIZE];
extern char g_methodreturnclasses[CF_BUFSIZE];
extern char g_methodfilename[CF_BUFSIZE];
extern char *g_vmethodproto[];
extern int g_methodargc;
extern char g_methodreplyto[CF_BUFSIZE];
extern char g_methodforce[CF_BUFSIZE];
extern char g_contextid[32];
extern char g_methodname[CF_BUFSIZE];
extern char g_methodmd5[CF_BUFSIZE];
extern char g_padchar;
extern struct cfagent_connection *g_conn;
extern int g_authenticated;
extern struct Item *g_ipaddresses;

extern char g_cflock[CF_BUFSIZE];
extern char g_savelock[CF_BUFSIZE];
extern char g_cflog[CF_BUFSIZE];
extern char g_cflast[CF_BUFSIZE];
extern char g_lockdb[CF_BUFSIZE];
extern char g_editbuff[CF_BUFSIZE];

extern char *tzname[2];
extern char *optarg;
extern int optind;
extern struct option g_options[];
extern int g_cfsignature;
extern char g_cfdes1[8];
extern char g_cfdes2[8];
extern char g_cfdes3[8];

extern char g_cfpubkeyfile[CF_BUFSIZE];
extern char g_cfprivkeyfile[CF_BUFSIZE];
extern char g_avdb[1024];

extern dev_t g_rootdevice;
extern char *g_vpreconfig;
extern char *g_vrcfile;

extern char *g_varch;
extern char *g_varch2;
extern char g_vyear[];
extern char g_vday[];
extern char g_vmonth[];
extern char g_vhr[];
extern char g_vminute[];
extern char g_vsec[];
extern char *g_actiontext[];
extern char *g_actionid[];
extern char *g_builtins[];
extern char *g_classtext[];
extern char *g_classattributes[CF_CLASSATTR][CF_ATTRDIM];
extern char *g_fileactiontext[];
extern char *g_commattributes[];
extern char g_vinputfile[];
extern char *g_vcanonicalfile;
extern char g_vcurrentfile[];
extern char g_vlogfile[];
extern char *g_chdir;
extern char *g_vsetuidlog;
extern FILE *g_vlogfp;
extern char g_veditabort[];
extern char g_listseparator;
extern char g_reposchar;
extern char g_discomp;
extern char g_useshell;
extern char g_preview;
extern char g_purge;
extern char g_checksum;
extern char g_compress;
extern int  g_checksumupdates;
extern int  g_disablesize;

extern char g_vlogdir[];
extern char g_vlockdir[];

extern struct tm TM1;
extern struct tm TM2;

extern int g_errorcount;
extern int g_numberofedits;
extern time_t g_cfstarttime;
extern time_t g_cfinitstarttime;
extern int g_cf_timeout;

extern struct utsname g_vsysname;

extern int g_linenumber;
extern mode_t g_defaultmode;
extern mode_t g_defaultsystemmode;
extern int g_haveuid;
extern char *g_findertype;
extern char *g_vuidname;
extern char *g_vgidname;
extern char g_cfserver[];
extern char *g_protocol[];
extern char g_vipaddress[];
extern char g_vprefix[];
extern int g_vrecurse;
extern int g_vage;
extern int g_rpctimeout;
extern char g_mountmode;
extern char g_deletedir;
extern char g_deletefstab;
extern char g_force;
extern char g_forceipv4;
extern char g_forcelink;
extern char g_forcedirs;
extern char g_stealth;
extern char g_preservetimes;
extern char g_trustkey;
extern char g_fork;

extern short g_compatibility_mode;
extern short g_linksilent;
extern short g_updateonly;
extern char  g_linktype;
extern char  g_agetype;
extern char  g_copytype;
extern char  g_defaultcopytype;
extern char  g_linkdirs;
extern char  g_logp;
extern char  g_informp;

extern char *g_filtername;
extern char *g_strategyname;
extern char *g_currentobject;
extern char *g_currentitem;
extern char *g_groupbuff;
extern char *g_actionbuff;
extern char *g_classbuff;
extern char g_allclassbuffer[CF_BUFSIZE];
extern char g_chroot[CF_BUFSIZE];
extern char g_elseclassbuffer[CF_BUFSIZE];
extern char g_failoverbuffer[CF_BUFSIZE];
extern char *g_linkfrom;
extern char *g_linkto;
extern char *g_error;
extern char *g_mountfrom;
extern char *g_mountonto;
extern char *g_mountopts;
extern char *g_destination;
extern char *g_imageaction;

extern char *g_expr;
extern char *g_currentauthpath;
extern char *g_restart;
extern char *g_filterdata;
extern char *g_strategydata;
extern char *g_pkgver;

extern short g_prosignal;
extern char  g_proaction;
extern char g_procomp;
extern char g_imgcomp;

extern int g_imgsize;


extern char *g_checksumdb;
extern char *g_compresscommand;

extern char *g_hash[CF_HASHTABLESIZE];

extern char g_vbuff[CF_BUFSIZE];
extern char g_output[CF_BUFSIZE*2];

extern char g_vfaculty[CF_MAXVARSIZE];
extern char g_vdomain[CF_MAXVARSIZE];
extern char g_vsysadm[CF_MAXVARSIZE];
extern char g_vnetmask[CF_MAXVARSIZE];
extern char g_vbroadcast[CF_MAXVARSIZE];
extern char g_vmailserver[CF_BUFSIZE];
extern struct Item *g_vtimezone;
extern char g_vdefaultroute[CF_MAXVARSIZE];
extern char g_vnfstype[CF_MAXVARSIZE];
extern char *g_vrepository;
extern char *g_localrepos;
extern char g_vifname[16];
extern char g_vifnameoverride[16];
extern enum classes g_vsystemhardclass;
extern char g_vfqname[];
extern char g_vuqname[];
extern char g_logfile[];

extern short g_noabspath;

extern struct Item *g_vexcludecache;
extern struct Item *g_vsinglecopy;
extern struct Item *g_vautodefine;
extern struct Item *g_vexcludecopy;
extern struct Item *g_vexcludelink;
extern struct Item *g_vcopylinks;
extern struct Item *g_vlinkcopies;
extern struct Item *g_vexcludeparse;
extern struct Item *g_vcplnparse;
extern struct Item *g_vincludeparse;
extern struct Item *g_vignoreparse;
extern struct Item *g_vaclbuild;
extern struct Item *g_vfilterbuild;
extern struct Item *g_vstrategybuild;

extern struct Item *g_vmountlist;
extern struct Item *g_vheap;      /* Points to the base of the attribute heap */
extern struct Item *g_vnegheap;

/* For packages: */
extern struct Package *g_vpkg;
extern struct Package *g_vpkgtop;

/* HvB : Bas van der Vlies */
extern struct Mountables *g_vmountables;  /* Points to the list of mountables */
extern struct Mountables *g_vmountablestop;

extern struct cfObject *g_vobjtop;
extern struct cfObject *g_vobj;


extern struct Item *g_methodargs;
extern flag  g_mount_ro;                  /* mount directory readonly */

extern struct Item *g_valerts;
extern struct Item *g_vmounted;

/* Points to the list of tidy specs */
extern struct Tidy *g_vtidy;

/* List of required file systems */
extern struct Disk *g_vrequired;

extern struct Disk *g_vrequiredtop;

/* List of scripts to execute */
extern struct ShellComm *g_vscript;

extern struct ShellComm *g_vscripttop;
extern struct Interface *g_viflist;
extern struct Interface *g_viflisttop;

/* Files systems already mounted */
extern struct Mounted *g_mounted;

extern struct Item g_vdefaultbinserver;
extern struct Item *g_vbinservers;
extern struct Link *g_vlink;
extern struct File *g_vfile;
extern struct Item *g_vhomeservers;
extern struct Item *g_vsetuidlist;
extern struct Disable *g_vdisablelist;
extern struct Disable *g_vdisabletop;
extern struct File *g_vmakepath;
extern struct File *g_vmakepathtop;
extern struct Link *g_vchlink;
extern struct Item *g_vignore;
extern struct Item *g_vhomepatlist;
extern struct Item *g_extensionlist;
extern struct Item *g_suspiciouslist;
extern struct Item *g_schedule;
extern struct Item *g_spooldirlist;
extern struct Item *g_nonattackerlist;
extern struct Item *g_multiconnlist;
extern struct Item *g_trustkeylist;
extern struct Item *g_dhcplist;
extern struct Item *g_allowuserlist;
extern struct Item *g_skipverify;
extern struct Item *g_attackerlist;
extern struct Item *g_mountoptlist;
extern struct Item *g_vresolve;
extern struct MiscMount *g_vmiscmount;
extern struct MiscMount *g_vmiscmounttop;
extern struct Item *g_vimport;
extern struct Item *g_vactionseq;
extern struct Item *g_vaccesslist;
extern struct Item *g_vaddclasses;
extern struct Item *g_valladdclasses;
extern struct Item *g_vjustactions;
extern struct Item *g_vavoidactions;
extern struct Edit *g_veditlist;
extern struct Edit *g_veditlisttop;
extern struct Filter *g_vfilterlist;
extern struct Filter *g_vfilterlisttop;
extern struct Strategy *g_vstrategylist;
extern struct Strategy *g_vstrategylisttop;

extern struct CFACL  *g_vacllist;
extern struct CFACL  *g_vacllisttop;
extern struct UnMount *g_vunmount;
extern struct UnMount *g_vunmounttop;
extern struct Item *g_vclassdefine;
extern struct Image *g_vimage;
extern struct Image *g_vimagetop;
extern struct Method *g_vmethods;
extern struct Method *g_vmethodstop;
extern struct Process *g_vproclist;
extern struct Process *g_vproctop;
extern struct Item *g_vserverlist;
extern struct Item *g_vrpcpeerlist;
extern struct Item *g_vredefines;

extern struct Item *g_vreposlist;

extern struct Auth *g_vadmit;
extern struct Auth *g_vdeny;
extern struct Auth *g_vadmittop;
extern struct Auth *g_vdenytop;

/* Associated variables which simplify logic */

extern struct Link *g_vlinktop;
extern struct Link *g_vchlinktop;
extern struct Tidy *g_vtidytop;
extern struct File *g_vfiletop;

extern char *g_copyright;

extern short g_debug;
extern short g_d1;
extern short g_d2;
extern short g_d3;
extern short g_d4;

extern short g_parsing;
extern short g_iscfengine;

extern short g_verbose;
extern short g_exclaim;
extern short g_inform;
extern short g_check;

extern int g_pifelapsed;
extern int g_pexpireafter;
extern short g_logging;
extern short g_inform_save;
extern short g_logging_save;
extern short g_cfparanoid;
extern short g_showactions;
extern short g_logtidyhomefiles;

extern char  g_tidydirs;
extern short g_travlinks;
extern short g_deadlinks;
extern short g_ptravlinks;
extern short g_dontdo;
extern short g_ifconf;
extern short g_parseonly;
extern short g_gotmountinfo;
extern short g_nomounts;
extern short g_nomodules;
extern short g_noprocs;
extern short g_nomethods;
extern short g_nofilecheck;
extern short g_notidy;
extern short g_noscripts;
extern short g_prsysadm;
extern short g_prmailserver;
extern short g_mountcheck;
extern short g_noedits;
extern short g_killoldlinks;
extern short g_ignorelock;
extern short g_nopreconfig;
extern short g_warnings;
extern short g_nonalphafiles;
extern short g_minusf;
extern short g_nolinks;
extern short g_enforcelinks;
extern short g_nocopy;
extern short g_forcenetcopy;
extern short g_silent;
extern short g_editverbose;
extern char g_imagebackup;
extern short g_rotate;
extern int   g_tidysize;
extern short g_useenviron;
extern short g_promatches;
extern short g_edabortmode;
extern short g_noprocs;
extern short g_underscore_classes;
extern short g_nohardclasses;
extern short g_nosplay;
extern short g_donesplay;
extern char g_xdev;
extern char g_typecheck;
extern char g_scan;

extern enum actions g_action;
extern enum vnames g_controlvar;

extern mode_t g_plusmask;
extern mode_t g_minusmask;

extern u_long g_plusflag;
extern u_long g_minusflag;

extern flag  g_action_is_link;
extern flag  g_action_is_linkchildren;
extern flag  g_mount_onto;
extern flag  g_mount_from;
extern flag  g_have_restart;
extern flag  g_actionpending;
extern flag  g_homecopy;
extern char g_encrypt;
extern char g_verify;
extern char g_compatibility;

extern char *g_vpscomm[];
extern char *g_vpsopts[];
extern char *g_vmountcomm[];
extern char *g_vmountopts[];
extern char *g_vifdev[];
extern char *g_vetcshells[];
extern char *g_vresolvconf[];
extern char *g_vhostequiv[];
extern char *g_vfstab[];
extern char *g_vmaildir[];
extern char *g_vnetstat[];
extern char *g_vfilecomm[];
extern char *g_actionseqtext[];
extern char *g_veditnames[];
extern char *g_vfilternames[];
extern char *g_vunmountcomm[];
extern char *g_vresources[];
extern char *g_cmpsensetext[];
extern char *g_pkgmgrtext[];

extern int g_vtimeout;
extern mode_t g_umask;

extern char *g_signals[];

extern char *tzname[2]; /* see man ctime */

extern int g_sensiblefilecount;
extern int g_sensiblefssize;
extern int g_editfilesize;
extern int g_editbinfilesize;
extern int g_vifelapsed;
extern int g_vexpireafter;
extern int g_vdefaultifelapsed;
extern int g_vdefaultexpireafter;
extern int g_autocreated;

extern enum fileactions g_fileaction;

extern enum cmpsense g_cmpsense;
extern enum pkgmgrs g_pkgmgr;
extern enum pkgmgrs g_defaultpkgmgr;

extern unsigned short g_portnumber;

extern int g_currentlinenumber;
extern struct Item *g_currentlineptr;

extern int g_editgrouplevel;
extern int g_searchreplacelevel;
extern int g_foreachlevel;

extern char g_commentstart[], g_commentend[];

/* GNU REGEXP */

extern struct re_pattern_buffer *g_searchpattbuff;
extern struct re_pattern_buffer *g_pattbuffer;
