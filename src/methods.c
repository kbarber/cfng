/*
 * $Id: methods.c 756 2004-05-25 18:45:20Z skaar $
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


/* ----------------------------------------------------------------- */

void
DispatchNewMethod(struct Method *ptr)
{
    struct Item *ip;
    char label[CF_BUFSIZE];
    char serverip[64],clientip[64];
    unsigned char digest[EVP_MAX_MD_SIZE+1];

    serverip[0] = clientip[0] = '\0';

    for (ip = ptr->servers; ip != NULL; ip=ip->next) {

        if ((strcmp(ip->name,"localhost") == 0) ||
                (strcmp(ip->name,"::1") == 0) ||
                (strcmp(ip->name,"127.0.0.1") == 0)) {

            Verbose("\nDispatch method localhost:%s to %s/rpc_in\n",
                    ptr->name,g_vlockdir);

            snprintf(label,CF_BUFSIZE-1,"%s/rpc_in/localhost_localhost_%s_%s",
                    g_vlockdir, ptr->name,ChecksumPrint('m',ptr->digest));

            EncapsulateMethod(ptr,label);
        } else {
            strncpy(serverip,Hostname2IPString(ip->name),63);

            if (strlen(ptr->forcereplyto) > 0) {
                strncpy(clientip,ptr->forcereplyto,63);
            } else {
                strncpy(clientip,Hostname2IPString(g_vfqname),63);
            }

            if (strcmp(clientip,serverip) == 0) {

                Verbose("Invitation to accept remote method %s "
                        "on this host\n",
                        ip->name);
                Verbose("(Note that this is not a method to "
                        "be executed as server localhost)\n");
                continue;
            }

            Verbose("Hailing server (%s) from our calling id %s\n",
                    serverip, g_vfqname);

            snprintf(label, CF_BUFSIZE-1,
                    "%s/rpc_out/%s_%s_%s_%s",
                    g_vlockdir, clientip,serverip, ptr->name,
                    ChecksumPrint('m',ptr->digest));

            EncapsulateMethod(ptr,label);
            Debug("\nDispatched method %s to %s/rpc_out\n",label,g_vlockdir);
        }
    }
}

/* ----------------------------------------------------------------- */

struct
Item *GetPendingMethods(int state)
{
    char filename[CF_MAXVARSIZE],path[CF_BUFSIZE],name[CF_BUFSIZE];
    char client[CF_BUFSIZE],server[CF_BUFSIZE];
    char extra[CF_BUFSIZE],digeststring[CF_BUFSIZE];
    DIR *dirh;
    struct dirent *dirp;
    struct Method *mp;
    struct Item *list = NULL, *ip;
    struct stat statbuf;

    Debug("GetPendingMethods(%d) in (%s/rpc_in)\n",state,g_vlockdir);

    Debug("Processing applications...\n");

    snprintf(filename,CF_MAXVARSIZE-1,"%s/rpc_in",g_vlockdir);

    if ((dirh = opendir(filename)) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't open directory %s\n",filename);
        CfLog(cfverbose,g_output,"opendir");
        return NULL;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name,filename,NULL)) {
            continue;
        }

        snprintf(path,CF_BUFSIZE-1,"%s/%s",filename,dirp->d_name);

        if (stat(path,&statbuf) == -1) {
            continue;
        }

        if (statbuf.st_mtime > g_cfstarttime + (g_vexpireafter * 60)) {
            Verbose("Purging expired method %s\n",path);
            unlink(path);
            continue;
        }

        SplitMethodName(dirp->d_name,client,server,name,digeststring,extra);

        if (strlen(name) == 0) {
            snprintf(g_output,CF_BUFSIZE,
                    "Unable to extract method name from package (%s)",
                    dirp->d_name);
            CfLog(cfinform,g_output,"");
            continue;
        }

        if (strlen(extra) != 0) {
            if (state == CF_METHODREPLY) {
                Debug("Found an attachment (%s) in reply to %s\n",extra,name);
            } else {
                Debug("Found an attachment (%s) to incoming method %s\n",
                        extra,name);
            }
            continue;
        }

        if ((state == CF_METHODREPLY) && (strstr(name,":Reply") == 0)) {
            Debug("Ignoring bundle (%s) from waiting function call\n",name);
            continue;
        }

        if ((state == CF_METHODEXEC) && (strstr(name,":Reply"))) {
            Debug("Ignoring bundle (%s) from waiting reply\n",name);
            continue;
        }

        Verbose("Looking at method (%s) from (%s) intended for "
                "exec on (%s) with arghash %s\n",
                name, client,server, digeststring);

        ip = SplitStringAsItemList(name,':');

        if (mp = IsDefinedMethod(ip->name,digeststring)) {

            if (IsItemIn(mp->servers,"localhost") ||
                    IsItemIn(mp->servers,IPString2Hostname(server))) {

                if (state == CF_METHODREPLY) {

                    Verbose("Found a local approval to forward reply "
                            "from local method (%s) to "
                            "final destination sender %s\n",name,client);

                } else {

                    Verbose("Found a local approval to execute this "
                            "method (%s) on behalf of sender %s\n",
                            name, IPString2Hostname(client));

                }
                AppendItem(&list,dirp->d_name,"");
                mp->bundle = strdup(dirp->d_name);
            } else {

                Verbose("No local approval in methods list on this host "
                        "for received request (%s)\n",dirp->d_name);
                unlink(path);

            }
        }

        DeleteItemList(ip);
    }

    closedir(dirh);

    if (list == NULL) {
        return NULL;
    }

    return list;
}


/* ----------------------------------------------------------------- */

/* Actually execute any method that is our responsibilty */
void
EvaluatePendingMethod(char *methodname)
{
    struct Method *ptr;
    char buffer[CF_BUFSIZE],line[CF_BUFSIZE];
    char options[32];
    char client[CF_BUFSIZE],server[CF_BUFSIZE],name[CF_BUFSIZE];
    char digeststring[CF_BUFSIZE],extra[CF_BUFSIZE];
    char *sp;
    char execstr[CF_BUFSIZE];
    int print;
    mode_t maskval = 0;
    FILE *pp;

    SplitMethodName(methodname,client,server,name,digeststring,extra);

    /* Could check client for access granted? */

    if (strcmp(server,"localhost") == 0 ||
            strcmp(IPString2Hostname(server),g_vfqname) == 0) {
        Debug("EvaluatePendingMethod(%s)\n",name);
    } else {
        Debug("Nothing to do for %s\n",methodname);
        return;
    }

    options[0] = '\0';

    if (g_inform) {
        strcat(options,"-I ");
    }

    if (g_verbose) {
        strcat(options,"-v ");
    }

    if (g_debug || g_d2) {
        strcat(options,"-d2 ");
    }

    ptr = IsDefinedMethod(name,digeststring);

    strcat(options,"-Z ");
    strcat(options,digeststring);
    strcat(options," ");

    snprintf(execstr,CF_BUFSIZE-1,"%s/bin/cfagent -f %s %s",
            WORKDIR,GetMethodFilename(ptr),options);

    Verbose("Trying %s\n",execstr);

    if (IsExcluded(ptr->classes)) {
        Verbose("(Excluded on classes (%s))\n",ptr->classes);
        return;
    }

    ResetOutputRoute(ptr->log,ptr->inform);

    if (!GetLock(ASUniqueName("method-exec"),CanonifyName(ptr->name),
                ptr->ifelapsed,ptr->expireafter,g_vuqname,g_cfstarttime)) {
        return;
    }

    snprintf(g_output,CF_BUFSIZE*2,"Executing method %s...(uid=%d,gid=%d)\n",
            execstr,ptr->uid,ptr->gid);
    CfLog(cfinform,g_output,"");

    if (g_dontdo) {
        printf("%s: execute method %s\n",g_vprefix,ptr->name);
    } else {
        switch (ptr->useshell) {
        case 'y':
            pp = cfpopen_shsetuid(execstr,"r",ptr->uid,ptr->gid,
                    ptr->chdir,ptr->chroot);
            break;
        default:
            pp = cfpopensetuid(execstr,"r",ptr->uid,ptr->gid,
                    ptr->chdir,ptr->chroot);
            break;
        }

        if (pp == NULL) {

            snprintf(g_output, CF_BUFSIZE*2,
                    "Couldn't open pipe to command %s\n",
                    execstr);

            CfLog(cferror,g_output,"popen");
            ResetOutputRoute('d','d');
            ReleaseCurrentLock();
            return;
        }

        while (!feof(pp)) {
            if (ferror(pp)) {
                /* abortable */
                snprintf(g_output, CF_BUFSIZE*2,
                        "Shell command pipe %s\n",execstr);
                CfLog(cferror,g_output,"ferror");
                break;
            }

            ReadLine(line,CF_BUFSIZE,pp);

            if (strstr(line,"cfengine-die")) {
                break;
            }

            if (ferror(pp))  {
                /* abortable */
                snprintf(g_output, CF_BUFSIZE*2,
                        "Shell command pipe %s\n", execstr);
                CfLog(cferror,g_output,"ferror");
                break;
            }


            print = false;

            for (sp = line; *sp != '\0'; sp++) {
                if (! isspace((int)*sp)) {
                    print = true;
                    break;
                }
            }

            if (print) {
                printf("%s:%s: %s\n",g_vprefix,ptr->name,line);
            }
        }

        cfpclose(pp);
    }

    snprintf(g_output, CF_BUFSIZE*2, 
            "Finished local method %s processing\n", execstr);
    CfLog(cfinform,g_output,"");

    ResetOutputRoute('d','d');
    ReleaseCurrentLock();

    return;
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

int
ParentLoadReplyPackage(char *methodname)
{
    char client[CF_MAXVARSIZE],server[CF_BUFSIZE];
    char name[CF_BUFSIZE],buf[CF_BUFSIZE];
    char line[CF_BUFSIZE],type[64],arg[CF_BUFSIZE], **methodargv = NULL;
    char basepackage[CF_BUFSIZE],cffilename[CF_BUFSIZE];
    char digeststring[CF_BUFSIZE],extra[CF_MAXVARSIZE];
    unsigned char digest[EVP_MAX_MD_SIZE+1];
    char *sp;
    struct Item *methodargs,*ip;
    struct Method *mp;
    int isreply = false, argnum = 0, i, methodargc = 0;
    FILE *fp,*fin,*fout;

    Debug("\nParentLoadReplyPackage(%s)\n\nLook for method "
            "replies in %s/rpc_in\n", methodname, g_vlockdir);

    SplitMethodName(methodname,client,server,name,digeststring,extra);

    for (sp = name; *sp != '\0'; sp++) {
        if (*sp == ':') {
            /* Strip off :Reply */
            *sp = '\0';
            break;
        }
    }

    if ((mp = IsDefinedMethod(name,digeststring)) == NULL) {
        return false;
    }

    if (!GetLock(ASUniqueName("methods-receive"),CanonifyName(name),
                mp->ifelapsed,mp->expireafter,g_vuqname,g_cfstarttime)) {

        return false;
    }

    i = 1;

    for (ip = mp->return_vars; ip != NULL; ip=ip->next) {
        i++;
    }

    methodargv = (char **)  malloc(i*sizeof(char *));

    i = 0;

    for (ip = mp->return_vars; ip != NULL; ip=ip->next) {
        /* Fill this temporarily with the formal parameters */
        methodargv[i++] = ip->name;
    }

    methodargc = i;

    snprintf(basepackage,CF_BUFSIZE-1,"%s/rpc_in/%s",g_vlockdir,methodname);

    argnum = CountAttachments(basepackage);

    if (argnum != methodargc) {

        Verbose("Number of return arguments (%d) does not match "
                "template (%d)\n", argnum, methodargc);

        Verbose("Discarding method %s\n",name);
        free((char *)methodargv);
        ReleaseCurrentLock();
        return false;
    }

    argnum = 0;
    Debug("Opening bundle %s\n",methodname);

    if ((fp = fopen(basepackage,"r")) == NULL) {

        snprintf(g_output, CF_BUFSIZE, 
                "Could not open a parameter bundle (%s) "
                "for method %s for parent reply", basepackage, name);

        CfLog(cfinform,g_output,"fopen");
        free((char *)methodargv);
        ReleaseCurrentLock();
        return false;
    }

    while (!feof(fp)) {
        memset(line,0,CF_BUFSIZE);
        fgets(line,CF_BUFSIZE,fp);
        type[0] = '\0';
        arg[0] = '\0';

        sscanf(line,"%63s %s",type,arg);

        switch (ConvertMethodProto(type)) {
        case cfmeth_name:
            if (strncmp(arg,name,strlen(name)) != 0) {

                snprintf(g_output, CF_BUFSIZE,
                        "Method name %s did not match package name %s",
                        name, arg);

                CfLog(cfinform,g_output,"");
                fclose(fp);
                free((char *)methodargv);
                ReleaseCurrentLock();
                return false;
            }
            break;

        case cfmeth_isreply:
            isreply = true;
            break;
        case cfmeth_sendclass:

            if (IsItemIn(mp->return_classes,arg)) {
                AddPrefixedMultipleClasses(name,arg);
            } else {

                Verbose("Method returned a class (%s) that was not "
                        "in the return_classes access list\n", arg);

            }
            break;
        case cfmeth_attacharg:
            if (methodargv[argnum][0] == '/') {
                int val;
                struct stat statbuf;
                FILE *fp,*fin,*fout;

                if (stat (arg,&statbuf) == -1) {
                    Verbose("Unable to stat file %s\n",arg);
                    ReleaseCurrentLock();
                    return false;
                }

                val = statbuf.st_size;

                Debug("Install file in %s at %s\n",arg,methodargv[argnum]);

                if ((fin = fopen(arg,"r")) == NULL) {

                    snprintf(g_output,CF_BUFSIZE,
                            "Could open not argument bundle %s\n",arg);

                    CfLog(cferror,g_output,"fopen");
                    ReleaseCurrentLock();
                    return false;
                }

                if ((fout = fopen(methodargv[argnum],"w")) == NULL) {

                    snprintf(g_output,CF_BUFSIZE,
                            "Could not write to local parameter file %s\n",
                            methodargv[argnum]);

                    CfLog(cferror,g_output,"fopen");
                    fclose(fin);
                    ReleaseCurrentLock();
                    return false;
                }

                if (val > CF_BUFSIZE) {
                    int count = 0;
                    while(!feof(fin)) {
                        fputc(fgetc(fin),fout);
                        count++;
                        if (count == val) {
                            break;
                        }
                        Debug("Wrote #\n");
                    }
                } else {
                    memset(buf,0,CF_BUFSIZE);
                    fread(buf,val,1,fin);
                    fwrite(buf,val,1,fout);
                    Debug("Wrote #\n");
                }

                fclose(fin);
                fclose(fout);
            } else {
                char argbuf[CF_BUFSIZE],newname[CF_MAXVARSIZE];
                memset(argbuf,0,CF_BUFSIZE);

                Debug("Read arg from file %s in parent load\n",arg);

                if ((fin = fopen(arg,"r")) == NULL) {
                    snprintf(g_output,CF_BUFSIZE,
                            "Missing method argument package (%s)\n",arg);
                    FatalError(g_output);
                }

                fread(argbuf,CF_BUFSIZE-1,1,fin);
                fclose(fin);

                snprintf(newname, CF_MAXVARSIZE-1,
                        "%s.%s", name, methodargv[argnum]);
                Debug("Setting variable %s to %s\n",newname,argbuf);
                AddMacroValue(g_contextid,newname,argbuf);
            }

            Debug("Unlink %s\n",arg);
            unlink(arg);
            argnum++;
            break;
        default:
                break;
        }
    }

    fclose(fp);
    free((char *)methodargv);

    unlink(basepackage);   /* Now remove the invitation */

    if (!isreply) {
        Verbose("Reply package was not marked as a reply\n");
        ReleaseCurrentLock();
        return false;
    }

    ReleaseCurrentLock();
    return true;
}


/* ----------------------------------------------------------------- */

int
ChildLoadMethodPackage(char *name, char *digeststring)
{
    char line[CF_BUFSIZE],type[64],arg[CF_BUFSIZE];
    char basepackage[CF_BUFSIZE],cffilename[CF_BUFSIZE],buf[CF_BUFSIZE];
    char ebuff[CF_EXPANDSIZE];
    char clientip[CF_BUFSIZE],ipaddress[CF_BUFSIZE];
    struct stat statbuf;
    FILE *fp,*fin,*fout;
    int argnum = 0, gotmethod = false;
    struct Method *mp;

    Debug("Look for all incoming methods in %s/rpc_in\n",g_vlockdir);

    g_methodfilename[0] = '\0';

    if (!CheckForMethodPackage(name)) {
        Verbose("Could not find anything looking like a current invitation\n");
        return false;
    }

    if (strcmp(g_methodreplyto, "localhost") != 0) {
        strncpy(clientip,Hostname2IPString(g_methodreplyto),63);
        strncpy(ipaddress,Hostname2IPString(g_vfqname),63);
    } else {
        strncpy(clientip,"localhost",63);
        strncpy(ipaddress,"localhost",63);
    }

    snprintf(basepackage,CF_BUFSIZE-1,"%s/rpc_in/%s_%s_%s_%s",
            g_vlockdir,clientip,ipaddress,g_methodname,g_methodmd5);

    argnum = CountAttachments(basepackage);

    if (argnum != g_methodargc) {

        Verbose("Number of return arguments (%d) does not match "
                "template (%d)\n", argnum, g_methodargc);

        Verbose("Discarding method %s\n",name);
        return false;
    }

    argnum = 0;

    if ((fp = fopen(basepackage,"r")) == NULL) {

        snprintf(g_output, CF_BUFSIZE,
                "Could not open a parameter bundle (%s) for "
                "method %s for child loading", basepackage, name);

        CfLog(cfinform,g_output,"fopen");
        return false;
    }

    while (!feof(fp)) {
        memset(line,0,CF_BUFSIZE);
        fgets(line,CF_BUFSIZE,fp);
        type[0] = '\0';
        arg[0] = '\0';

        sscanf(line,"%63s %s",type,arg);

        switch (ConvertMethodProto(type)) {
        case cfmeth_name:
            if (strcmp(arg,name) != 0) {

                snprintf(g_output,CF_BUFSIZE,
                        "Method name %s did not match package name %s",
                        name,arg);

                CfLog(cfinform,g_output,"");
                fclose(fp);
                return false;
            }
            break;

        case cfmeth_file:

            if (GetMacroValue(g_contextid,"methoddirectory")) {
                ExpandVarstring("$(methoddirectory)",ebuff,NULL);
            } else {
                snprintf(ebuff, CF_BUFSIZE, "%s/methods", WORKDIR);
            }

            snprintf(cffilename,CF_BUFSIZE-1,"%s/%s",ebuff,arg);

            if (lstat(cffilename,&statbuf) != -1) {
                if (S_ISLNK(statbuf.st_mode)) {

                    snprintf(g_output, CF_BUFSIZE, 
                            "SECURITY ALERT. Method (%s) executable "
                            "is a symbolic link",name);

                    CfLog(cferror,g_output,"");
                    fclose(fp);
                    continue;
                }
            }

            if (stat(cffilename,&statbuf) == -1) {

                snprintf(g_output,CF_BUFSIZE,
                        "Method name %s did not find executable name %s",
                        name,cffilename);

                CfLog(cfinform,g_output,"stat");
                fclose(fp);
                continue;
            }

            strncpy(g_methodfilename,cffilename,CF_MAXVARSIZE);
            break;
        case cfmeth_replyto:

            Debug("Found reply to host: %s\n",arg);
            strncpy(g_methodreplyto,arg,CF_MAXVARSIZE-1);
            break;

        case cfmeth_sendclass:
            Debug("Defining class: %s\n",arg);
            strncpy(g_methodreturnclasses,arg,CF_BUFSIZE-1);
            break;

        case cfmeth_attacharg:

            Debug("Handling expected local argument (%s)\n",
                    g_methodargv[argnum]);

            if (g_methodargv[argnum][0] == '/') {
                int val;
                struct stat statbuf;

                if (stat (arg,&statbuf) == -1) {
                    Verbose("Unable to stat file %s\n",arg);
                    return false;
                }

                val = statbuf.st_size;

                Debug("Install file in %s at %s\n",arg,g_methodargv[argnum]);

                if ((fin = fopen(arg,"r")) == NULL) {

                    snprintf(g_output,CF_BUFSIZE,
                            "Could open not argument bundle %s\n",arg);

                    CfLog(cferror,g_output,"fopen");
                    return false;
                }

                if ((fout = fopen(g_methodargv[argnum],"w")) == NULL) {

                    snprintf(g_output,CF_BUFSIZE,
                            "Could not write to local parameter file %s\n",
                            g_methodargv[argnum]);

                    CfLog(cferror,g_output,"fopen");
                    fclose(fin);
                    return false;
                }

                if (val > CF_BUFSIZE) {
                    int count = 0;
                    while(!feof(fin)) {
                        fputc(fgetc(fin),fout);
                        count++;
                        if (count == val) {
                            break;
                        }
                        Debug("Wrote #\n");
                    }
                } else {
                    memset(buf,0,CF_BUFSIZE);
                    fread(buf,val,1,fin);
                    fwrite(buf,val,1,fout);
                    Debug("Wrote #\n");
                }

                fclose(fin);
                fclose(fout);
            } else {
                char argbuf[CF_BUFSIZE];
                memset(argbuf,0,CF_BUFSIZE);

                Debug("Read arg from file %s\n",arg);

                if ((fin = fopen(arg,"r")) == NULL) {
                    snprintf(g_output,CF_BUFSIZE,
                            "Missing method argument package (%s)\n",arg);
                    FatalError(g_output);
                }

                fread(argbuf,CF_BUFSIZE-1,1,fin);
                fclose(fin);

                if (strcmp(g_methodargv[argnum],"void") != 0) {
                    Debug("Setting variable %s to %s\n",
                            g_methodargv[argnum],argbuf);
                    AddMacroValue(g_contextid,g_methodargv[argnum],argbuf);
                } else {
                    Verbose("Method had no arguments (void)\n");
                }
            }

            Debug("Removing arg %s\n",arg);
            unlink(arg);
            argnum++;
            break;

        default:
            break;
        }
    }

    fclose(fp);

    if (strlen(g_methodfilename) == 0) {
        snprintf(g_output,CF_BUFSIZE,
                "Method (%s) package had no filename to execute",name);
        CfLog(cfinform,g_output,"");
        return false;
    }

    if (strlen(g_methodreplyto) == 0) {
        snprintf(g_output,CF_BUFSIZE,
                "Method (%s) package had no replyto address",name);
        CfLog(cfinform,g_output,"");
        return false;
    }

    Debug("Done loading %s\n",basepackage);
    unlink(basepackage);   /* Now remove the invitation */
    return true;
}

/* ----------------------------------------------------------------- */

void
DispatchMethodReply(void)
{
    struct Item *ip;
    char label[CF_BUFSIZE];
    char ipaddress[64],clientip[64];

    Verbose("DispatchMethodReply()\n\n");

    if (strcmp(g_methodreplyto,"localhost") == 0 ||
            strcmp(g_methodreplyto,"::1") == 0 ||
            strcmp(g_methodreplyto,"127.0.0.1") == 0) {

        snprintf(label,CF_BUFSIZE-1,
                "%s/rpc_in/localhost_localhost_%s:Reply_%s",
                g_vlockdir,g_methodname,g_methodmd5);

        EncapsulateReply(label);
    } else {
        strncpy(clientip,Hostname2IPString(g_methodreplyto),63);
        strncpy(ipaddress,Hostname2IPString(g_vfqname),63);

        Verbose("\nDispatch method %s:%s to %s/rpc_out\n",
                g_methodreplyto,g_methodname,g_vlockdir);

        snprintf(label,CF_BUFSIZE-1,"%s/rpc_out/%s_%s_%s:Reply_%s",
                g_vlockdir,clientip,ipaddress,g_methodname,g_methodmd5);

        EncapsulateReply(label);
    }
}


/* ----------------------------------------------------------------- */

char *
GetMethodFilename(struct Method *ptr)
{
    char line[CF_BUFSIZE],type[64],arg[CF_BUFSIZE];
    char basepackage[CF_BUFSIZE],cffilename[CF_BUFSIZE];
    char ebuff[CF_EXPANDSIZE];
    static char returnfile[CF_BUFSIZE];
    FILE *fp;
    struct stat statbuf;
    int numargs = 0, argnum = 0;

    numargs = ListLen(ptr->send_args);

    snprintf(basepackage,CF_BUFSIZE-1,"%s/rpc_in/%s",g_vlockdir,ptr->bundle);

    if ((fp = fopen(basepackage,"r")) == NULL) {

        snprintf(g_output, CF_BUFSIZE, "Could not open a parameter "
                "bundle (%s) for method %s to get filenames",
                ptr->bundle, ptr->name);

        CfLog(cfinform,g_output,"fopen");
        return returnfile;
    }

    argnum = 0;

    while (!feof(fp)) {
        memset(line,0,CF_BUFSIZE);
        fgets(line,CF_BUFSIZE,fp);
        type[0] = '\0';
        arg[0] = '\0';
        sscanf(line,"%63s %s",type,arg);

        switch (ConvertMethodProto(type)) {
        case cfmeth_name:
            if (strcmp(arg,ptr->name) != 0) {

                snprintf(g_output,CF_BUFSIZE,
                        "Method name %s did not match package name %s",
                        ptr->name,arg);

                CfLog(cfinform,g_output,"");
                fclose(fp);
                return returnfile;
            }
            break;
        case cfmeth_file:
            if (GetMacroValue(g_contextid,"methoddirectory")) {
                ExpandVarstring("$(methoddirectory)",ebuff,NULL);
            } else {
                snprintf(ebuff, CF_BUFSIZE, "%s/methods", WORKDIR);
            }

            snprintf(cffilename,CF_BUFSIZE-1,"%s/%s",ebuff,arg);

            if (lstat(cffilename,&statbuf) != -1) {
                if (S_ISLNK(statbuf.st_mode)) {

                    snprintf(g_output, CF_BUFSIZE,
                            "SECURITY ALERT. Method (%s) source is "
                            "a symbolic link", ptr->name);

                    CfLog(cferror,g_output,"");
                    fclose(fp);
                    continue;
                }
            }

            if (stat(cffilename,&statbuf) == -1) {

                snprintf(g_output,CF_BUFSIZE,
                        "Method name %s did not find package name %s",
                        ptr->name,cffilename);

                CfLog(cfinform,g_output,"stat");
                fclose(fp);
                continue;
            }

            strncpy(returnfile,cffilename,CF_MAXVARSIZE);
            break;
        case cfmeth_attacharg:
            argnum++;
            break;
        default:
            break;
        }
    }
    fclose(fp);

    if (numargs != argnum) {

        Verbose("Method (%s) arguments did not agree with "
                "the agreed template in the config file (%d/%d)\n",
                ptr->name, argnum, numargs);

        return returnfile;
    }

    return returnfile;
}

/*
 * --------------------------------------------------------------------
 * Level 3
 * --------------------------------------------------------------------
 */

enum methproto
ConvertMethodProto(char *name)
{
    int i;
    for (i = 0; g_vmethodproto[i] != NULL; i++) {
        if (strcmp(name,g_vmethodproto[i]) == 0) {
            break;
        }
    }

    return (enum methproto) i;
}

/* ----------------------------------------------------------------- */

int
CheckForMethodPackage(char *methodname)
{
    DIR *dirh;
    struct Method *mp = NULL;
    struct dirent *dirp;
    struct Item *methodID = NULL;
    char dirname[CF_MAXVARSIZE],path[CF_BUFSIZE];
    char name[CF_BUFSIZE],client[CF_BUFSIZE],server[CF_BUFSIZE];
    char digeststring[CF_BUFSIZE],extra[256];
    struct stat statbuf;
    int gotmethod = false;

    snprintf(dirname,CF_MAXVARSIZE-1,"%s/rpc_in",g_vlockdir);

    if ((dirh = opendir(dirname)) == NULL) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't open directory %s\n",dirname);
        CfLog(cfverbose,g_output,"opendir");
        return false;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name,dirname,NULL)) {
            continue;
        }

        snprintf(path,CF_BUFSIZE-1,"%s/%s",dirname,dirp->d_name);

        if (stat(path,&statbuf) == -1) {
            continue;
        }

        Debug("Examining child's incoming (%s)\n",dirp->d_name);

        SplitMethodName(dirp->d_name,client,server,name,digeststring,extra);

        Debug("This request came from %s - "
                "our reply should be sent there!\n", client);

        strcpy(g_methodreplyto,client);

        if (strcmp(methodname,name) == 0) {
            gotmethod = true;
            break;
        }

        if (mp = IsDefinedMethod(name,digeststring)) {
            if (statbuf.st_mtime > (g_cfstarttime - (mp->expireafter * 60))) {
                Debug("Purging expired method %s\n",path);
                unlink(path);
                continue;
            }
        } else {
            if (statbuf.st_mtime > (g_cfstarttime - 2*3600)) {
                /* Default 2 hr expiry */
                Debug("Purging expired method %s\n",path);
                unlink(path);
                continue;
            }
        }

        DeleteItemList(methodID);
    }
    closedir(dirh);

    if (gotmethod) {
        return true;
    } else {
        return false;
    }
}

/* ----------------------------------------------------------------- */

struct Method *
IsDefinedMethod(char *name,char *digeststring)
{
    struct Method *mp;

    for (mp = g_vmethods; mp != NULL; mp=mp->next) {

        Debug("Check for defined method (%s=%s)(%s)\n",name,
                mp->name,digeststring);

        if (strncmp(mp->name,name,strlen(mp->name)) == 0) {

            Debug("  Comparing digests: %s=%s\n",digeststring,
                    ChecksumPrint('m',mp->digest));

            if (strcmp(digeststring,ChecksumPrint('m',mp->digest)) == 0) {
                Verbose("Method %s is defined\n",name);
                return mp;
            }
        }
    }

    Verbose("Method %s not defined\n",name);
    return NULL;
}

/* ----------------------------------------------------------------- */

int
CountAttachments(char *basepackage)
{
    FILE *fp;
    int argnum = 0;
    char line[CF_BUFSIZE],type[CF_BUFSIZE],arg[CF_BUFSIZE];

    Debug("CountAttachments(%s)\n",basepackage);

    if ((fp = fopen(basepackage,"r")) == NULL) {

        snprintf(g_output, CF_BUFSIZE,
                "Could not open a parameter bundle (%s) while "
                "counting attachments\n", basepackage);

        CfLog(cfinform,g_output,"fopen");
        return 0;
    }

    while (!feof(fp)) {
        memset(line,0,CF_BUFSIZE);
        fgets(line,CF_BUFSIZE,fp);
        type[0] = '\0';
        arg[0] = '\0';

        sscanf(line,"%63s %s",type,arg);

        switch (ConvertMethodProto(type)) {
        case cfmeth_attacharg:
            argnum++;
            break;
        }
    }

    fclose(fp);
    return argnum;
}


/*
 * --------------------------------------------------------------------
 * Tool level
 * --------------------------------------------------------------------
 */

void
EncapsulateMethod(struct Method *ptr,char *name)
{
    struct Item *ip;
    char expbuf[CF_EXPANDSIZE],filename[CF_BUFSIZE];
    int i = 0, isreply = false;
    FILE *fp;

    if (strstr(name,":Reply")) {
        /* TODO - check if remote spoofing this could be dangerous*/

        snprintf(g_output,CF_BUFSIZE,
                "Method name (%s) should not contain a reply signal",name);

        CfLog(cferror,g_output,"");
    }

    if ((fp = fopen(name,"w")) == NULL) {
        snprintf(g_output,CF_BUFSIZE,"Could dispatch method to %s\n",name);
        CfLog(cferror,g_output,"fopen");
        return;
    }

    Debug("EncapsulatingMethod(%s)\n",name);

    fprintf(fp,"%s %s\n",g_vmethodproto[cfmeth_name],ptr->name);
    fprintf(fp,"%s %s\n",g_vmethodproto[cfmeth_file],ptr->file);
    fprintf(fp, "%s %lu\n", g_vmethodproto[cfmeth_time],
            (unsigned long)g_cfstarttime);

    if (ptr->servers == NULL || ptr->servers->name == NULL) {
        fprintf(fp,"%s localhost\n",g_vmethodproto[cfmeth_replyto]);
    } else if (strcmp(ptr->servers->name,"localhost") == 0) {
        fprintf(fp,"%s localhost\n",g_vmethodproto[cfmeth_replyto]);
    } else {

        /* 
         * Send FQNAME or g_vipaddress or IPv6? This is difficult. Best to
         * send IP to avoid lookups, and hosts might have a fixed name
         * but variable IP ... get it from interface?  unfortunately
         * can't do this for IPv6 yet -- leave that for another day.
         */

        if (strlen(ptr->forcereplyto) > 0) {
            fprintf(fp,"%s %s\n",g_vmethodproto[cfmeth_replyto],
                    ptr->forcereplyto);
        } else {
            fprintf(fp,"%s %s\n",g_vmethodproto[cfmeth_replyto],g_vipaddress);
        }
    }

    for (ip = ptr->send_classes; ip != NULL; ip=ip->next) {

        /* 
         * If classX is defined locally, activate class method_classX in
         * method.
         */
        if (IsDefinedClass(ip->name)) {
            fprintf(fp,"%s %s\n",g_vmethodproto[cfmeth_sendclass],ip->name);
        }
    }

    for (ip = ptr->send_args; ip != NULL; ip=ip->next) {
        i++;

        if (strstr(name,"rpc_out"))  {
            /* Remote copy needs to be transferred to incoming for later .. */
            char correction[CF_BUFSIZE];

            snprintf(correction,CF_BUFSIZE-1,"%s/rpc_in/%s",
                    g_vlockdir,name+strlen(g_vlockdir)+2+strlen("rpc_out"));

            fprintf(fp,"%s %s_%d\n",
                    g_vmethodproto[cfmeth_attacharg],correction,i);
        } else {
            fprintf(fp,"%s %s_%d\n",
                    g_vmethodproto[cfmeth_attacharg],name,i);
        }
    }

    fclose(fp);

    Debug("Packaging done\n");

    /* 
     * Now open a new file for each argument - this provides more policy
     * control and security for remote access, since file sizes can be
     * limited and there is less chance of buffer overflows 
     */

    i = 0;

    for (ip = ptr->send_args; ip != NULL; ip=ip->next) {
        i++;
        snprintf(filename,CF_BUFSIZE-1,"%s_%d",name,i);
        Debug("Create arg_file %s\n",filename);

        if (IsBuiltinFunction(ip->name)) {
            char name[CF_BUFSIZE],args[CF_BUFSIZE];
            char value[CF_BUFSIZE],sourcefile[CF_BUFSIZE];
            char *sp,maxbytes[CF_BUFSIZE],*nf,*nm;
            int count = 0,val = 0;
            FILE *fin,*fout;

            Debug("Evaluating readfile inclusion...\n");
            sscanf(ip->name,"%255[^(](%255[^)])",name,args);
            ExpandVarstring(name,expbuf,NULL);

            switch (FunctionStringToCode(expbuf)) {
            case fn_readfile:

                sourcefile[0] = '\0';
                maxbytes[0] = '\0';

                for (sp = args; *sp != '\0'; sp++) {
                    if (*sp == ',') {
                        count++;
                    }
                }

                if (count != 1) {
                    yyerror("ReadFile(filename,maxbytes): argument error");
                    return;
                }

                sscanf(args,"%[^,],%[^)]",sourcefile,maxbytes);
                Debug("ReadFile [%s] < [%s]\n",sourcefile,maxbytes);

                if (sourcefile[0]=='\0' || maxbytes[0] == '\0') {
                    yyerror("Argument error in class-function");
                    return;
                }

                nf = UnQuote(sourcefile);
                nm = UnQuote(maxbytes);

                val = atoi(nm);

                if ((fin = fopen(nf,"r")) == NULL) {
                    snprintf(g_output, CF_BUFSIZE, 
                            "Could not open ReadFile(%s)\n",nf);
                    CfLog(cferror,g_output,"fopen");
                    return;
                }

                Debug("Writing file to %s\n",filename);

                if ((fout = fopen(filename,"w")) == NULL) {
                    snprintf(g_output, CF_BUFSIZE,
                            "Could not open argument attachment %s\n",
                            filename);
                    CfLog(cferror,g_output,"fopen");
                    fclose(fin);
                    return;
                }

                if (val > CF_BUFSIZE) {
                    int count = 0;
                    while(!feof(fin)) {
                        fputc(fgetc(fin),fout);
                        count++;
                        if (count == val) {
                            break;
                        }
                        Debug("Wrote #\n");
                    }
                } else {
                    memset(value,0,CF_BUFSIZE);
                    fread(value,val,1,fin);
                    fwrite(value,val,1,fout);
                    Debug("Wrote #\n");
                }

                fclose(fin);
                fclose(fout);
                Debug("Done with file dispatch\n");
                break;

            default:

                snprintf(g_output, CF_BUFSIZE,
                        "Invalid function (%s) used in "
                        "function argument processing", expbuf);

                CfLog(cferror,g_output,"");

            }

        } else {
            if ((fp = fopen(filename,"w")) == NULL) {
                snprintf(g_output, CF_BUFSIZE,
                        "Could dispatch method to %s\n", name);
                CfLog(cferror,g_output,"fopen");
                return;
            }

            ExpandVarstring(ip->name,expbuf,NULL);
            fwrite(expbuf,strlen(expbuf),1,fp);
            fclose(fp);
        }
    }
}


/* ----------------------------------------------------------------- */

void
EncapsulateReply(char *name)
{
    FILE *fp;
    struct Item *ip;
    char expbuf[CF_EXPANDSIZE],filename[CF_BUFSIZE];
    int i = 0, isreply = false;;

    if ((fp = fopen(name,"w")) == NULL) {
        snprintf(g_output,CF_BUFSIZE,"Could dispatch method to %s\n",name);
        CfLog(cferror,g_output,"fopen");
        return;
    }

    Debug("EncapsulatingReply(%s)\n",name);

    fprintf(fp,"%s %s\n",g_vmethodproto[cfmeth_name],g_methodname);
    fprintf(fp, "%s %lu\n", g_vmethodproto[cfmeth_time],
            (unsigned long)g_cfstarttime);
    fprintf(fp,"%s true\n",g_vmethodproto[cfmeth_isreply]);

    for (ip = SplitStringAsItemList(g_methodreturnclasses,',') ;
            ip != NULL; ip=ip->next) {

        /* 
         * If classX is defined locally, activate class method_classX in
         * method.
         */

        if (IsDefinedClass(ip->name)) {
            Debug("Packaging return class %s\n",ip->name);
            fprintf(fp,"%s %s\n",g_vmethodproto[cfmeth_sendclass],ip->name);
        }
    }

    DeleteItemList(ip);

    for (ip = ListFromArgs(g_methodreturnvars); ip != NULL; ip=ip->next) {
        i++;
        ExpandVarstring(ip->name,expbuf,NULL);

        if (strstr(name,"rpc_out")) {
            /* Remote copy needs to be transferred to incoming for later .. */
            char correction[CF_BUFSIZE];

            snprintf(correction,CF_BUFSIZE-1,"%s/rpc_in/%s",
                    g_vlockdir,name+strlen(g_vlockdir)+2+strlen("rpc_out"));

            fprintf(fp,"%s %s_%d (%s)\n",
                    g_vmethodproto[cfmeth_attacharg],correction,i,expbuf);

        } else {
            fprintf(fp,"%s %s_%d (%s)\n",
                    g_vmethodproto[cfmeth_attacharg],name,i,expbuf);
        }
    }

    DeleteItemList(ip);
    fclose(fp);

    Debug("Packaging done\n");

    /* 
     * Now open a new file for each argument - this provides more policy
     * control and security for remote access, since file sizes can be
     * limited and there is less chance of buffer overflows 
     */


    i = 0;

    for (ip = ListFromArgs(g_methodreturnvars); ip != NULL; ip=ip->next) {
        i++;
        snprintf(filename,CF_BUFSIZE-1,"%s_%d",name,i);
        Debug("Create arg_file %s\n",filename);

        if (IsBuiltinFunction(ip->name)) {
            char name[CF_BUFSIZE],args[CF_BUFSIZE];
            char value[CF_BUFSIZE],sourcefile[CF_BUFSIZE];
            char *sp,maxbytes[CF_BUFSIZE],*nf,*nm;
            int count = 0,val = 0;
            FILE *fin,*fout;

            Debug("Evaluating readfile inclusion...\n");
            sscanf(ip->name,"%255[^(](%255[^)])",name,args);
            ExpandVarstring(name,expbuf,NULL);

            switch (FunctionStringToCode(expbuf)) {
            case fn_readfile:

                sourcefile[0] = '\0';
                maxbytes[0] = '\0';

                for (sp = args; *sp != '\0'; sp++) {
                    if (*sp == ',') {
                        count++;
                    }
                }

                if (count != 1) {
                    yyerror("ReadFile(filename,maxbytes): argument error");
                    return;
                }

                sscanf(args,"%[^,],%[^)]",sourcefile,maxbytes);
                Debug("ReadFile [%s] < [%s]\n",sourcefile,maxbytes);

                if (sourcefile[0]=='\0' || maxbytes[0] == '\0') {
                    yyerror("Argument error in class-function");
                    return;
                }

                nf = UnQuote(sourcefile);
                nm = UnQuote(maxbytes);

                val = atoi(nm);

                if ((fin = fopen(nf,"r")) == NULL) {
                    snprintf(g_output, CF_BUFSIZE,
                            "Could not open ReadFile(%s)\n", nf);
                    CfLog(cferror,g_output,"fopen");
                    return;
                }

                if ((fout = fopen(filename,"w")) == NULL) {
                    snprintf(g_output,CF_BUFSIZE,
                            "Could not open argument attachment %s\n",
                            filename);
                    CfLog(cferror,g_output,"fopen");
                    fclose(fin);
                    return;
                }

                if (val > CF_BUFSIZE) {
                    int count = 0;
                    while(!feof(fin)) {
                        fputc(fgetc(fin),fout);
                        count++;
                        if (count == val) {
                            break;
                        }
                    }
                } else {
                    memset(value,0,CF_BUFSIZE);
                    fread(value,val,1,fin);
                    fwrite(value,val,1,fout);
                }
                fclose(fin);
                fclose(fout);
                break;

            default:

                snprintf(g_output, CF_BUFSIZE, 
                        "Invalid function (%s) used in "
                        "function argument processing", expbuf);

                CfLog(cferror,g_output,"");

            }
        } else {
            if ((fp = fopen(filename,"w")) == NULL) {
                snprintf(g_output, CF_BUFSIZE,
                        "Could dispatch method to %s\n", name);
                CfLog(cferror,g_output,"fopen");
                return;
            }

            ExpandVarstring(ip->name,expbuf,NULL);
            fwrite(expbuf,strlen(expbuf),1,fp);
            fclose(fp);
        }
    }

    DeleteItemList(ip);
}

/* ----------------------------------------------------------------- */

void
SplitMethodName(char *name, char *client, char *server,
        char *methodname,char *digeststring,char *extra)
{
    methodname[0] = '\0';
    client[0] = '\0';
    server[0] = '\0';
    digeststring[0] = '\0';
    extra[0] = '\0';

    sscanf(name,"%1024[^_]_%1024[^_]_%1024[^_]_%[^_]_%64s",
            client,server,methodname,digeststring,extra);
}
