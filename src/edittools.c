/*
 * $Id: edittools.c 743 2004-05-23 07:27:32Z skaar $
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
 * Toolkit: Editing of simple textfiles
 */


#include "cf.defs.h"
#include "cf.extern.h"


/*
 * Edit: data structure routines
 */

int
DoRecursiveEditFiles(char *name, int level, struct Edit *ptr, struct stat *sb)
{
    DIR *dirh;
    struct dirent *dirp;
    char pcwd[CF_BUFSIZE];
    struct stat statbuf;
    int goback;

    if (level == -1) {
        return false;
    }

    Debug("RecursiveEditFiles(%s)\n",name);

    if (!DirPush(name,sb)) {
        return false;
    }

    if ((dirh = opendir(".")) == NULL) {
        return true;
    }

    for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
        if (!SensibleFile(dirp->d_name,name,NULL)) {
            continue;
        }

        if (IgnoreFile(name,dirp->d_name,ptr->ignores)) {
            continue;
        }

        /* Assemble pathname */
        strcpy(pcwd,name);
        AddSlash(pcwd);

        if (BufferOverflow(pcwd,dirp->d_name)) {
            return true;
        }

        strcat(pcwd,dirp->d_name);

        if (!FileObjectFilter(pcwd,&statbuf,ptr->filters,editfiles)) {
            Verbose("Skipping filtered file %s\n",pcwd);
            continue;
        }

        if (g_travlinks) {
            if (lstat(dirp->d_name,&statbuf) == -1) {
                snprintf(g_output,CF_BUFSIZE*2,"Can't stat %s\n",pcwd);
                CfLog(cferror,g_output,"stat");
                continue;
            }

            if (S_ISLNK(statbuf.st_mode) && (statbuf.st_mode != getuid())) {
                snprintf(g_output, CF_BUFSIZE, 
                        "File %s is an untrusted link. cfagent will "
                        "not follow it with a destructive operation (tidy)",
                        pcwd);
                continue;
            }

            if (stat(dirp->d_name,&statbuf) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "RecursiveCheck was working on %s when "
                        "this happened:\n", pcwd);
                CfLog(cferror,g_output,"stat");
                continue;
            }
        } else {
            if (lstat(dirp->d_name,&statbuf) == -1) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "RecursiveCheck was working in %s when "
                        "this happened:\n",pcwd);
                CfLog(cferror,g_output,"lstat");
                continue;
            }
        }

        if (S_ISDIR(statbuf.st_mode)) {
            if (IsMountedFileSystem(&statbuf,pcwd,level)) {
                continue;
            } else {
                if ((ptr->recurse > 1) || (ptr->recurse == CF_INF_RECURSE)) {
                    goback = DoRecursiveEditFiles(pcwd,level-1,ptr,&statbuf);
                    DirPop(goback,name,sb);
                } else {
                    WrapDoEditFile(ptr,pcwd);
                }
            }
        } else {
            WrapDoEditFile(ptr,pcwd);
        }
    }
    closedir(dirh);
    return true;
}

/* ----------------------------------------------------------------- */

void
DoEditHomeFiles(struct Edit *ptr)
{
    DIR *dirh, *dirh2;
    struct dirent *dirp, *dirp2;
    char *sp,homedir[CF_BUFSIZE],dest[CF_BUFSIZE];
    struct passwd *pw;
    struct stat statbuf;
    struct Item *ip;
    uid_t uid;

    if (!MountPathDefined()) {
        CfLog(cfinform,"Mountpattern is undefined\n","");
        return;
    }

    for (ip = g_vmountlist; ip != NULL; ip=ip->next) {
        if (IsExcluded(ip->classes)) {
            continue;
        }

        if ((dirh = opendir(ip->name)) == NULL) {
            snprintf(g_output, CF_BUFSIZE*2, 
                    "Can't open directory %s\n",ip->name);
            CfLog(cferror,g_output,"opendir");
            return;
        }

        for (dirp = readdir(dirh); dirp != NULL; dirp = readdir(dirh)) {
            if (!SensibleFile(dirp->d_name,ip->name,NULL)) {
                continue;
            }

            strcpy(homedir,ip->name);
            AddSlash(homedir);
            strcat(homedir,dirp->d_name);

            if (! IsHomeDir(homedir)) {
                continue;
            }

            if ((dirh2 = opendir(homedir)) == NULL) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Can't open directory%s\n",homedir);
                CfLog(cferror,g_output,"opendir");
                return;
            }

            for (dirp2 = readdir(dirh2); dirp2 != NULL; dirp2 = readdir(dirh2)) {
                if (!SensibleFile(dirp2->d_name,homedir,NULL)) {
                    continue;
                }

                strcpy(dest,homedir);
                AddSlash(dest);
                strcat(dest,dirp2->d_name);
                AddSlash(dest);
                sp = ptr->fname + strlen("home/");
                strcat(dest,sp);

                if (stat(dest,&statbuf)) {
                    EditVerbose("File %s doesn't exist for editing, "
                            "skipping\n", dest);
                    continue;
                }

                if ((pw = getpwnam(dirp2->d_name)) == NULL) {
                    Debug2("cfng: directory corresponds to "
                            "no user %s - ignoring\n", dirp2->d_name);
                    continue;
                } else {
                    Debug2("(Setting user id to %s)\n", dirp2->d_name);
                }

                uid = statbuf.st_uid;

                WrapDoEditFile(ptr,dest);

                chown(dest,uid,CF_SAME_OWNER);
            }
            closedir(dirh2);
        }
        closedir(dirh);
    }
}

/* ----------------------------------------------------------------- */

void
WrapDoEditFile(struct Edit *ptr, char *filename)
{
    struct stat statbuf,statbuf2;
    char linkname[CF_BUFSIZE];
    char realname[CF_BUFSIZE];

    Debug("WrapDoEditFile(%s,%s)\n",ptr->fname,filename);

    if (lstat(filename,&statbuf) != -1) {
        if (S_ISLNK(statbuf.st_mode)) {
            EditVerbose("File %s is a link, editing real file instead\n",
                    filename);

            memset(linkname,0,CF_BUFSIZE);
            memset(realname,0,CF_BUFSIZE);

            if (readlink(filename,linkname,CF_BUFSIZE-1) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Cannot read link %s\n",filename);
                CfLog(cferror,g_output,"readlink");
                return;
            }

            if (linkname[0] != '/') {
                strcpy(realname,filename);
                ChopLastNode(realname);
                AddSlash(realname);
            }

            if (BufferOverflow(realname,linkname)) {
                snprintf(g_output,CF_BUFSIZE*2,"(culprit %s in editfiles)\n",
                        filename);
                CfLog(cferror,g_output,"");
                return;
            }

            if (stat(filename,&statbuf2) != -1) {
                if (statbuf2.st_uid != statbuf.st_uid) {
                    /* Link to /etc/passwd? ouch! */
                    snprintf(g_output, CF_BUFSIZE*2, 
                            "Forbidden to edit a link to another "
                            "user's file with privilege (%s)",filename);
                    CfLog(cfinform,g_output,"");
                    return;
                }
            }

            strcat(realname,linkname);

            if (!FileObjectFilter(realname,&statbuf2,ptr->filters,editfiles)) {
                Debug("Skipping filtered editfile %s\n",filename);
                return;
            }
            DoEditFile(ptr,realname);
            return;
        } else {
            if (!FileObjectFilter(filename,&statbuf,ptr->filters,editfiles)) {
                Debug("Skipping filtered editfile %s\n",filename);
                return;
            }
            DoEditFile(ptr,filename);
            return;
        }
    } else {
        if (!FileObjectFilter(filename,&statbuf,ptr->filters,editfiles)) {
            Debug("Skipping filtered editfile %s\n",filename);
            return;
        }
        DoEditFile(ptr,filename);
    }
}

/* ----------------------------------------------------------------- */

/* 
 * Many of the functions called here are defined in the item.c toolkit
 * since they operate on linked lists
 */

void
DoEditFile(struct Edit *ptr, char *filename)
{
    struct Edlist *ep, *loopstart, *loopend, *ThrowAbort();
    struct Item *filestart = NULL, *newlineptr = NULL;
    char currenteditscript[CF_BUFSIZE], searchstr[CF_BUFSIZE];
    char expdata[CF_EXPANDSIZE];
    char *sp, currentitem[CF_MAXVARSIZE];
    struct stat tmpstat;
    char spliton = ':';
    mode_t maskval;
    int todo = 0, potentially_outstanding = false;
    FILE *loop_fp = NULL;
    FILE *read_fp = NULL;
    int DeleteItemNotContaining();
    int DeleteItemNotStarting();
    int DeleteItemNotMatching();

    Debug("DoEditFile(%s)\n",filename);
    filestart = NULL;
    currenteditscript[0] = '\0';
    searchstr[0] = '\0';
    memset(g_editbuff,0,CF_BUFSIZE);
    g_autocreated = false;
    g_imagebackup = 's';

    if (IgnoredOrExcluded(editfiles,filename,ptr->inclusions,ptr->exclusions)) {
        Debug("Skipping excluded file %s\n",filename);
        return;
    }

    for (ep = ptr->actions; ep != NULL; ep=ep->next) {
        if (!IsExcluded(ep->classes)) {
            todo++;
        }
    }

    /* Because classes are stored per edit, not per file */
    if (todo == 0) {
        return;
    }

    if (!GetLock(ASUniqueName("editfile"),CanonifyName(filename),
                g_vifelapsed,g_vexpireafter,g_vuqname,g_cfstarttime)) {
        return;
    }

    if (ptr->binary == 'y') {
        BinaryEditFile(ptr,filename);
        ReleaseCurrentLock();
        return;
    }

    CheckEditSwitches(filename,ptr);

    if (! LoadItemList(&filestart,filename)) {
        CfLog(cfverbose,"File was marked for editing\n","");
        ReleaseCurrentLock();
        return;
    }

    g_numberofedits = 0;
    g_editverbose = g_verbose;
    g_currentlinenumber = 1;
    g_currentlineptr = filestart;
    strcpy(g_commentstart,"# ");
    strcpy(g_commentend,"");
    g_editgrouplevel = 0;
    g_searchreplacelevel = 0;
    g_foreachlevel = 0;
    loopstart = NULL;

    Verbose("Begin editing %s\n",filename);
    ep = ptr->actions;

    while (ep != NULL) {
        if (IsExcluded(ep->classes)) {
            ep = ep->next;
            potentially_outstanding = true;
            continue;
        }

        ExpandVarstring(ep->data,expdata,NULL);

        Debug2("Edit action: %s\n",g_veditnames[ep->code]);

        switch(ep->code) {
        case NoEdit:
        case EditInform:
        case EditBackup:
        case EditUmask:
        case AutoCreate:
        case EditInclude:
        case EditExclude:
        case EditFilter:
        case DefineClasses:
        case ElseDefineClasses:
            break;

        case EditUseShell:
            if (strcmp(expdata,"false") == 0) {
                ptr->useshell = 'n';
            }
            break;

        case DefineInGroup:
            for (sp = expdata; *sp != '\0'; sp++) {
                currentitem[0] = '\0';
                sscanf(sp,"%[^,:.]",currentitem);
                sp += strlen(currentitem);
                AddClassToHeap(currentitem);
            }
            break;

        case CatchAbort:
            EditVerbose("Caught Exception\n");
            break;

        case SplitOn:
            spliton = *(expdata);
            EditVerbose("Split lines by %c\n",spliton);
            break;

        case DeleteLinesStarting:
            while (DeleteItemStarting(&filestart,expdata)) { }
            break;

        case DeleteLinesContaining:
            while (DeleteItemContaining(&filestart,expdata)) { }
            break;

        case DeleteLinesNotStarting:
            while (DeleteItemNotStarting(&filestart,expdata)) { }
            break;

        case DeleteLinesNotContaining:
            while (DeleteItemNotContaining(&filestart,expdata)) { }
            break;

        case DeleteLinesStartingFileItems:
        case DeleteLinesContainingFileItems:
        case DeleteLinesMatchingFileItems:
        case DeleteLinesNotStartingFileItems:
        case DeleteLinesNotContainingFileItems:
        case DeleteLinesNotMatchingFileItems:
            if (!DeleteLinesWithFileItems(&filestart,expdata,ep->code)) {
                goto abort;
            }
            break;

        case DeleteLinesAfterThisMatching:
            if ((filestart == NULL) || (g_currentlineptr == NULL)) {
                break;
            } else if (g_currentlineptr->next != NULL) {
                while (DeleteItemMatching(&(g_currentlineptr->next),expdata)) { }
            }
            break;

        case DeleteLinesMatching:
            while (DeleteItemMatching(&filestart,expdata)) { }
            break;

        case DeleteLinesNotMatching:
            while (DeleteItemNotMatching(&filestart,expdata)) { }
            break;

        case Append:
            AppendItem(&filestart,expdata,NULL);
            break;

        case AppendIfNoSuchLine:
            if (! IsItemIn(filestart,expdata)) {
                AppendItem(&filestart,expdata,NULL);
            }
            break;

        case AppendIfNoSuchLinesFromFile:
            if (!AppendLinesFromFile(&filestart,expdata)) {
                goto abort;
            }
            break;

        case SetLine:
            strcpy(g_editbuff,expdata);
            EditVerbose("Set current line to %s\n",g_editbuff);
            break;

        case AppendIfNoLineMatching:

            Debug("AppendIfNoLineMatching : %s\n",g_editbuff);

            if (strcmp(g_editbuff,"") == 0) {
                snprintf(g_output, CF_BUFSIZE*2,
                        "SetLine not set when calling "
                        "AppendIfNoLineMatching %s\n",expdata);
                CfLog(cferror,g_output,"");
                break;
            }

            if (strcmp(expdata,"ThisLine") == 0) {
                if (LocateNextItemMatching(filestart,g_editbuff) == NULL) {
                    AppendItem(&filestart,g_editbuff,NULL);
                }
                break;
            }

            if (LocateNextItemMatching(filestart,expdata) == NULL) {
                AppendItem(&filestart,g_editbuff,NULL);
            }
            break;

        case Prepend:
            PrependItem(&filestart,expdata,NULL);
            break;

        case PrependIfNoSuchLine:
            if (! IsItemIn(filestart,expdata)) {
                PrependItem(&filestart,expdata,NULL);
            }
            break;

        case PrependIfNoLineMatching:

            if (strcmp(g_editbuff,"") == 0) {
                snprintf(g_output, CF_BUFSIZE, 
                        "SetLine not set when calling "
                        "PrependIfNoLineMatching %s\n",expdata);
                CfLog(cferror,g_output,"");
                break;
            }

            if (LocateNextItemMatching(filestart,expdata) == NULL) {
                PrependItem(&filestart,g_editbuff,NULL);
            }
            break;

        case WarnIfNoSuchLine:
            if ((g_pass == 1) &&
                    (LocateNextItemMatching(filestart,expdata) == NULL)) {
                printf("Warning, file %s has no line matching %s\n",
                        filename,expdata);
            }
            break;

        case WarnIfLineMatching:
            if ((g_pass == 1) &&
                    (LocateNextItemMatching(filestart,expdata) != NULL)) {
                printf("Warning, file %s has a line matching %s\n",
                        filename,expdata);
            }
            break;

        case WarnIfNoLineMatching:
            if ((g_pass == 1) &&
                    (LocateNextItemMatching(filestart,expdata) == NULL)) {
                printf("Warning, file %s has a no line matching %s\n",
                        filename,expdata);
            }
            break;

        case WarnIfLineStarting:
            if ((g_pass == 1) &&
                    (LocateNextItemStarting(filestart,expdata) != NULL)) {
                printf("Warning, file %s has a line starting %s\n",
                        filename,expdata);
            }
            break;

        case WarnIfNoLineStarting:
            if ((g_pass == 1) &&
                    (LocateNextItemStarting(filestart,expdata) == NULL)) {
                printf("Warning, file %s has no line starting %s\n",
                        filename,expdata);
            }
            break;

        case WarnIfLineContaining:
            if ((g_pass == 1) &&
                    (LocateNextItemContaining(filestart,expdata) != NULL)) {
                printf("Warning, file %s has a line containing %s\n",
                        filename,expdata);
            }
            break;

        case WarnIfNoLineContaining:
            if ((g_pass == 1) &&
                    (LocateNextItemContaining(filestart,expdata) == NULL)) {
                printf("Warning, file %s has no line containing %s\n",
                        filename,expdata);
            }
            break;

        case SetCommentStart:
            strncpy(g_commentstart,expdata,CF_MAXVARSIZE);
            g_commentstart[CF_MAXVARSIZE-1] = '\0';
            break;

        case SetCommentEnd:
            strncpy(g_commentend,expdata,CF_MAXVARSIZE);
            g_commentend[CF_MAXVARSIZE-1] = '\0';
            break;

        case CommentLinesMatching:
            while (CommentItemMatching(&filestart,expdata,
                        g_commentstart,g_commentend)) { }
            break;

        case CommentLinesStarting:
            while (CommentItemStarting(&filestart,expdata,
                        g_commentstart,g_commentend)) { }
            break;

        case CommentLinesContaining:
            while (CommentItemContaining(&filestart,expdata,
                        g_commentstart,g_commentend)) { }
            break;

        case HashCommentLinesContaining:
            while (CommentItemContaining(&filestart,expdata,"# ","")) { }
            break;

        case HashCommentLinesStarting:
            while (CommentItemStarting(&filestart,expdata,"# ","")) { }
            break;

        case HashCommentLinesMatching:
            while (CommentItemMatching(&filestart,expdata,"# ","")) { }
            break;

        case SlashCommentLinesContaining:
            while (CommentItemContaining(&filestart,expdata,"//","")) { }
            break;

        case SlashCommentLinesStarting:
            while (CommentItemStarting(&filestart,expdata,"//","")) { }
            break;

        case SlashCommentLinesMatching:
            while (CommentItemMatching(&filestart,expdata,"//","")) { }
            break;

        case PercentCommentLinesContaining:
            while (CommentItemContaining(&filestart,expdata,"%","")) { }
            break;

        case PercentCommentLinesStarting:
            while (CommentItemStarting(&filestart,expdata,"%","")) { }
            break;

        case PercentCommentLinesMatching:
            while (CommentItemMatching(&filestart,expdata,"%","")) { }
            break;

        case ResetSearch:
            if (!ResetEditSearch(expdata,filestart)) {
                printf("ResetSearch Failed in %s, aborting editing\n",
                        filename);
                goto abort;
            }
            break;

        case LocateLineMatching:

            if (g_currentlineptr == NULL) {
                newlineptr == NULL;
            } else {
                newlineptr = LocateItemMatchingRegExp(g_currentlineptr,expdata);
            }

            if (newlineptr == NULL) {
                EditVerbose("LocateLineMatchingRegexp failed in %s, "
                        "aborting editing\n",filename);
                ep = ThrowAbort(ep);
            }
            break;

        case InsertLine:
            if (filestart == NULL) {
                AppendItem(&filestart,expdata,NULL);
            } else {
                InsertItemAfter(&filestart,g_currentlineptr,expdata);
            }
            break;

        case InsertFile:
            InsertFileAfter(&filestart,g_currentlineptr,expdata);
            break;

        case IncrementPointer:
                /* edittools */
            if (! IncrementEditPointer(expdata,filestart)) {
                printf ("IncrementPointer failed in %s, aborting editing\n",
                        filename);
                ep = ThrowAbort(ep);
            }
            break;

        case ReplaceLineWith:
            if (!ReplaceEditLineWith(expdata)) {
                printf("Aborting edit of file %s\n",filename);
                continue;
            }
            break;

        case DeleteToLineMatching:
            if (! DeleteToRegExp(&filestart,expdata)) {
                EditVerbose("Nothing matched DeleteToLineMatching "
                        "regular expression\n");
                EditVerbose("Aborting file editing of %s.\n" ,filename);
                ep = ThrowAbort(ep);
            }
            break;

        case DeleteNLines:
            if (! DeleteSeveralLines(&filestart,expdata)) {
                EditVerbose("Could not delete %s lines from file\n",expdata);
                EditVerbose("Aborting file editing of %s.\n",filename);
                ep = ThrowAbort(ep);
            }
            break;

        case HashCommentToLineMatching:
            if (! CommentToRegExp(&filestart,expdata,"#","")) {
                EditVerbose("Nothing matched HashCommentToLineMatching "
                        "regular expression\n");
                EditVerbose("Aborting file editing of %s.\n",filename);
                ep = ThrowAbort(ep);
            }
            break;

        case PercentCommentToLineMatching:
            if (! CommentToRegExp(&filestart,expdata,"%","")) {
                EditVerbose("Nothing matched PercentCommentToLineMatching "
                        "regular expression\n");
                EditVerbose("Aborting file editing of %s.\n",filename);
                ep = ThrowAbort(ep);
            }
            break;

        case CommentToLineMatching:
            if (! CommentToRegExp(&filestart,expdata, 
                        g_commentstart,g_commentend)) {
                EditVerbose("Nothing matched CommentToLineMatching "
                        "regular expression\n");
                EditVerbose("Aborting file editing of %s.\n",filename);
                ep = ThrowAbort(ep);
            }
            break;

        case CommentNLines:
            if (! CommentSeveralLines(&filestart, expdata,
                        g_commentstart,g_commentend)) {
                EditVerbose("Could not comment %s lines from file\n",expdata);
                EditVerbose("Aborting file editing of %s.\n",filename);
                ep = ThrowAbort(ep);
            }
            break;

        case UnCommentNLines:
            if (! UnCommentSeveralLines(&filestart,expdata,
                        g_commentstart,g_commentend)) {
                EditVerbose("Could not comment %s lines from file\n",expdata);
                EditVerbose("Aborting file editing of %s.\n",filename);
                ep = ThrowAbort(ep);
            }
            break;

        case UnCommentLinesContaining:
            while (UnCommentItemContaining(&filestart,expdata,
                        g_commentstart,g_commentend)) { }
            break;

        case UnCommentLinesMatching:
            while (UnCommentItemMatching(&filestart,expdata,
                        g_commentstart,g_commentend)) { }
            break;

        case SetScript:
            strncpy(currenteditscript, expdata, CF_BUFSIZE);
            currenteditscript[CF_BUFSIZE-1] = '\0';
            break;

        case RunScript:
            if (! RunEditScript(expdata,filename,&filestart,ptr)) {
                printf("Aborting further edits to %s\n",filename);
                ep = ThrowAbort(ep);
            }
            break;

        case RunScriptIfNoLineMatching:
            if (! LocateNextItemMatching(filestart,expdata)) {
                if (! RunEditScript(currenteditscript,
                            filename,&filestart,ptr)) {

                    printf("Aborting further edits to %s\n",filename);
                    ep = ThrowAbort(ep);
                }
            }
            break;

        case RunScriptIfLineMatching:
            if (LocateNextItemMatching(filestart,expdata)) {
                if (! RunEditScript(currenteditscript,filename,&filestart,ptr)) {
                    printf("Aborting further edits to %s\n",filename);
                    ep = ThrowAbort(ep);
                }
            }
            break;

        case EmptyEntireFilePlease:
            EditVerbose("Emptying entire file\n");
            DeleteItemList(filestart);
            filestart = NULL;
            g_currentlineptr = NULL;
            g_currentlinenumber=0;
            g_numberofedits++;
            break;

        case GotoLastLine:
            GotoLastItem(filestart);
            break;

        case BreakIfLineMatches:
            if (g_currentlineptr == NULL || g_currentlineptr->name == NULL ) {
                EditVerbose("(BreakIfLIneMatches - no match for %s - "
                        "file empty)\n",expdata);
                break;
            }

            if (LineMatches(g_currentlineptr->name,expdata)) {
                EditVerbose("Break! %s\n",expdata);
                goto abort;
            }
            break;

        case BeginGroupIfNoMatch:
            if (g_currentlineptr == NULL || g_currentlineptr->name == NULL ) {
                EditVerbose("(Begin Group - no match for %s - file empty)\n",
                        expdata);
                break;
            }

            if (LineMatches(g_currentlineptr->name,expdata)) {
                EditVerbose("(Begin Group - skipping %s)\n",expdata);
                ep = SkipToEndGroup(ep,filename);
            } else {
                EditVerbose("(Begin Group - no match for %s)\n",expdata);
            }
            break;

        case BeginGroupIfNoLineMatching:
            if (LocateItemMatchingRegExp(filestart,expdata) != 0) {
                EditVerbose("(Begin Group - skipping %s)\n",expdata);
                ep = SkipToEndGroup(ep,filename);
            } else {
                EditVerbose("(Begin Group - no line matching %s)\n",expdata);
            }
            break;

        case BeginGroupIfNoLineContaining:
            if (LocateNextItemContaining(filestart,expdata) != 0) {
                EditVerbose("(Begin Group - skipping, string matched)\n");
                ep = SkipToEndGroup(ep,filename);
            } else {
                EditVerbose("(Begin Group - no line containing %s)\n",expdata);
            }
            break;

        case BeginGroupIfNoSuchLine:
            if (IsItemIn(filestart,expdata)) {
                EditVerbose("(Begin Group - skipping, line exists)\n");
                ep = SkipToEndGroup(ep,filename);
            } else {
                EditVerbose("(Begin Group - no line %s)\n",expdata);
            }
            break;

        case BeginGroupIfFileIsNewer:
            if ((!g_autocreated) && (!FileIsNewer(filename,expdata))) {
                EditVerbose("(Begin Group - skipping, file is older)\n");
                while(ep->code != EndGroup) {
                    ep=ep->next;
                }
            } else {
                EditVerbose("(Begin Group - new file %s)\n",expdata);
            }
            break;


        case BeginGroupIfDefined:
            if (!IsExcluded(expdata)) {
                EditVerbose("(Begin Group - class %s defined)\n", expdata);
            } else {
                EditVerbose("(Begin Group - class %s not defined - "
                        "skipping)\n", expdata);
                ep = SkipToEndGroup(ep,filename);
            }
            break;

        case BeginGroupIfNotDefined:
            if (IsExcluded(expdata)) {
                EditVerbose("(Begin Group - class %s not defined)\n", expdata);
            } else {
                EditVerbose("(Begin Group - class %s defined - skipping)\n", 
                        expdata);
                ep = SkipToEndGroup(ep,filename);
            }
            break;

        case BeginGroupIfFileExists:
            if (stat(expdata,&tmpstat) == -1) {
                EditVerbose("(Begin Group - file unreadable/no such file - "
                        "skipping)\n");
                ep = SkipToEndGroup(ep,filename);
            } else {
                EditVerbose("(Begin Group - found file %s)\n",expdata);
            }
            break;

        case EndGroup:
            EditVerbose("(End Group)\n");
            break;

        case ReplaceAll:
            strncpy(searchstr,expdata,CF_BUFSIZE);
            break;

        case With:
            if (!GlobalReplace(&filestart,searchstr,expdata)) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Error editing file %s",filename);
                CfLog(cferror,g_output,"");
            }
            break;

        case FixEndOfLine:
            DoFixEndOfLine(filestart,expdata);
            break;

        case AbortAtLineMatching:
            g_edabortmode = true;
            strncpy(g_veditabort,expdata,CF_BUFSIZE);
            break;

        case UnsetAbort:
            g_edabortmode = false;
            break;

        case AutoMountDirectResources:
            HandleAutomountResources(&filestart,expdata);
            break;

        case ForEachLineIn:
            if (loopstart == NULL) {
                loopstart = ep;

                if ((loop_fp = fopen(expdata,"r")) == NULL) {
                    EditVerbose("Couldn't open %s\n",expdata);

                    /* skip over loop */
                    while(ep->code != EndLoop) {
                        ep = ep->next;
                    }
                    break;
                }

                EditVerbose("Starting ForEach loop with %s\n",expdata);
                continue;
            } else {
                if (!feof(loop_fp)) {
                    memset(g_editbuff,0,CF_BUFSIZE);

                    /* Like SetLine */
                    while (ReadLine(g_editbuff,CF_BUFSIZE,loop_fp)) {
                        if (strlen(g_editbuff) == 0) {
                            EditVerbose("ForEachLineIn skipping blank line");
                            continue;
                        }
                        break;
                    }

                    if (strlen(g_editbuff) == 0) {
                        EditVerbose("EndForEachLineIn\n");
                        fclose(loop_fp);
                        loopstart = NULL;

                        while(ep->code != EndLoop) {
                            ep = ep->next;
                        }
                        EditVerbose("EndForEachLineIn, set current line "
                                "to: %s\n",g_editbuff);
                    }

                    Debug("ForeachLine: %s\n",g_editbuff);
                } else {
                    EditVerbose("EndForEachLineIn");

                    fclose(loop_fp);
                    loopstart = NULL;

                    while(ep->code != EndLoop) {
                        ep = ep->next;
                    }
                }
            }
            break;

        case EndLoop:
            loopend = ep;
            ep = loopstart;
            continue;

        case ReplaceLinesMatchingField:
            ReplaceWithFieldMatch(&filestart, expdata, g_editbuff,
                    spliton, filename);
            break;

        case AppendToLineIfNotContains:
            AppendToLine(g_currentlineptr,expdata,filename);
            break;

        default:
            snprintf(g_output,CF_BUFSIZE*2,
                    "Unknown action in editing of file %s\n",filename);
            CfLog(cferror,g_output,"");
            break;
        }

        ep = ep->next;
    }

    abort :

    EditVerbose("End editing %s\n",filename);
    EditVerbose(".....................................................................\n");

    g_editverbose = false;
    g_edabortmode = false;

    if (g_dontdo || CompareToFile(filestart,filename)) {
        EditVerbose("Unchanged file: %s\n",filename);
        g_numberofedits = 0;
    }

    if ((! g_dontdo) && (g_numberofedits > 0)) {
        SaveItemList(filestart,filename,ptr->repository);
        AddEditfileClasses(ptr,true);
    } else {
        AddEditfileClasses(ptr,false);
    }

    ResetOutputRoute('d','d');
    ReleaseCurrentLock();

    DeleteItemList(filestart);

    if (!potentially_outstanding) {
        ptr->done = 'y';
    }
}

/* ----------------------------------------------------------------- */

int
IncrementEditPointer(char *str, struct Item *liststart)
{
    int i,n = 0;
    struct Item *ip;

    sscanf(str,"%d", &n);

    if (n == 0) {
        printf("Illegal increment value: %s\n",str);
        return false;
    }

    Debug("IncrementEditPointer(%d)\n",n);

    /* is prev undefined, set to line 1 */
    if (g_currentlineptr == NULL) {
        if (liststart == NULL) {
            EditVerbose("cannot increment line pointer in empty file\n");
            return true;
        } else {
            g_currentlineptr=liststart;
            g_currentlinenumber=1;
        }
    }

    if (n < 0) {
        if (g_currentlinenumber + n < 1) {
            EditVerbose("pointer decrements to before start of file!\n");
            EditVerbose("pointer stopped at start of file!\n");
            g_currentlineptr=liststart;
            g_currentlinenumber=1;
            return true;
        }

        i = 1;

        for (ip = liststart; ip != g_currentlineptr; ip=ip->next, i++) {
            if (i == g_currentlinenumber + n) {
                g_currentlinenumber += n;
                g_currentlineptr = ip;
                Debug2("Current line (%d) starts: %20.20s ...\n",
                        g_currentlinenumber,g_currentlineptr->name);

                return true;
            }
        }
    }

    for (i = 0; i < n; i++) {
        if (g_currentlineptr->next != NULL) {
            g_currentlineptr = g_currentlineptr->next;
            g_currentlinenumber++;

            EditVerbose("incrementing line pointer to line %d\n",
                    g_currentlinenumber);
        } else {
            EditVerbose("inc pointer failed, still at %d\n",
                    g_currentlinenumber);
        }
    }

    Debug2("Current line starts: %20s ...\n",g_currentlineptr->name);

    return true;
}

/* ----------------------------------------------------------------- */

int
ResetEditSearch (char *str, struct Item *list)
{
    int i = 1 ,n = -1;
    struct Item *ip;

    sscanf(str,"%d", &n);

    if (n < 1) {
        printf("Illegal reset value: %s\n",str);
        return false;
    }

    for (ip = list; (i < n) && (ip != NULL); ip=ip->next, i++) { }

    if (i < n || ip == NULL) {
        printf("Search for (%s) begins after end of file!!\n",str);
        return false;
    }

    EditVerbose("resetting pointers to line %d\n",n);

    g_currentlinenumber = n;
    g_currentlineptr = ip;

    return true;
}

/* ----------------------------------------------------------------- */

int
ReplaceEditLineWith(char *string)
{
    char *sp;

    if (strcmp(string,g_currentlineptr->name) == 0) {
        EditVerbose("ReplaceLineWith - line does not need correction.\n");
        return true;
    }

    if ((sp = malloc(strlen(string)+1)) == NULL) {
        printf("Memory allocation failed in ReplaceEditLineWith, "
                "aborting edit.\n");
        return false;
    }

    EditVerbose("Replacing line %d with %10s...\n",g_currentlinenumber,string);
    strcpy(sp,string);
    free (g_currentlineptr->name);
    g_currentlineptr->name = sp;
    g_numberofedits++;
    return true;
}

/* ----------------------------------------------------------------- */

int
RunEditScript(char *script, char *fname, struct Item **filestart,
        struct Edit *ptr)
{
    FILE *pp;
    char buffer[CF_BUFSIZE];

    if (script == NULL) {
        printf("No script defined for with SetScript\n");
        return false;
    }

    if (g_dontdo) {
        return true;
    }

    if (g_numberofedits > 0) {
        SaveItemList(*filestart,fname,ptr->repository);
        AddEditfileClasses(ptr,true);
    } else {
        AddEditfileClasses(ptr,false);
    }

    DeleteItemList(*filestart);

    snprintf(buffer,CF_BUFSIZE,"%s %s %s  2>&1",script,fname,
            g_classtext[g_vsystemhardclass]);

    EditVerbose("Running command: %s\n",buffer);


    switch (ptr->useshell) {
    case 'y':
        pp = cfpopen_sh(buffer,"r");
        break;
    default:
        pp = cfpopen(buffer,"r");
        break;
    }

    if (pp == NULL) {
        printf("Edit script %s failed to open.\n",fname);
        return false;
    }

    while (!feof(pp)) {
        ReadLine(g_currentitem,CF_BUFSIZE,pp);

        if (!feof(pp)) {
            EditVerbose("%s\n",g_currentitem);
        }
    }

    cfpclose(pp);

    *filestart = 0;

    if (! LoadItemList(filestart,fname)) {
        EditVerbose("File was marked for editing\n");
        return false;
    }

    g_numberofedits = 0;
    g_currentlinenumber = 1;
    g_currentlineptr = *filestart;
    return true;
}

/* ----------------------------------------------------------------- */

/* fix end of line char format */

/* 
 * Assumes that CF_EXTRA_SPACE macro allows enough space in the
 * allocated strings to add a few characters
 */

void
DoFixEndOfLine(struct Item *list, char *type)
{
    struct Item *ip;
    char *sp;
    int gotCR;

    EditVerbose("Checking end of line conventions: type = %s\n",type);

    if (strcmp("unix",type) == 0 || strcmp("UNIX",type) == 0) {
        for (ip = list; ip != NULL; ip=ip->next) {
            for (sp = ip->name; *sp != '\0'; sp++) {
                if (*sp == (char)13) {
                    *sp = '\0';
                    g_numberofedits++;
                }
            }
        }
        return;
    }

    if (strcmp("dos",type) == 0 || strcmp("DOS",type) == 0) {
        for (ip = list; ip != NULL; ip = ip->next) {
            gotCR = false;

            for (sp = ip->name; *sp !='\0'; sp++) {
                if (*sp == (char)13) {
                    gotCR = true;
                }
            }

            if (!gotCR) {
                *sp = (char)13;
                *(sp+1) = '\0';
                g_numberofedits++;
            }
        }
        return;
    }
    printf("Unknown file format: %s\n",type);
}

/* ----------------------------------------------------------------- */

void
HandleAutomountResources(struct Item **filestart, char *opts)
{
    struct Mountables *mp;
    char buffer[CF_BUFSIZE];
    char *sp;

    for (mp = g_vmountables; mp != NULL; mp=mp->next) {
        for (sp = mp->filesystem; *sp != ':'; sp++) { }
        sp++;
        snprintf(buffer,CF_BUFSIZE,"%s\t%s\t%s",sp,opts,mp->filesystem);

        if (LocateNextItemContaining(*filestart,sp) == NULL) {
            AppendItem(filestart,buffer,"");
            g_numberofedits++;
        } else {
            EditVerbose("have a server for %s\n",sp);
        }
    }
}

/* ----------------------------------------------------------------- */

void
CheckEditSwitches(char *filename, struct Edit *ptr)
{
    struct stat statbuf;
    struct Edlist *ep;
    char inform = 'd', log = 'd';
    char expdata[CF_EXPANDSIZE];
    int fd;
    struct Edlist *actions = ptr->actions;

    g_parsing = true;

    for (ep = actions; ep != NULL; ep=ep->next) {
        if (IsExcluded(ep->classes)) {
            continue;
        }

        ExpandVarstring(ep->data,expdata,NULL);

        switch(ep->code) {
        case AutoCreate:
            if (!g_dontdo) {
                mode_t mask;

                if (stat(filename,&statbuf) == -1) {
                    Debug("Setting umask to %o\n",ptr->umask);
                    mask=umask(ptr->umask);

                    if ((fd = creat(filename,0644)) == -1) {
                        snprintf(g_output,CF_BUFSIZE*2,
                                "Unable to create file %s\n",filename);
                        CfLog(cfinform,g_output,"creat");
                    } else {
                        g_autocreated = true;
                        close(fd);
                    }

                    snprintf(g_output,CF_BUFSIZE*2,
                            "Creating file %s, mode %o\n",
                            filename,(0644 & ~ptr->umask));
                    CfLog(cfinform,g_output,"");
                    umask(mask);
                    return;
                }
            }
            break;

        case EditBackup:
            if (strcmp("false",ToLowerStr(expdata)) == 0 ||
                    strcmp("off",ToLowerStr(expdata)) == 0) {
                g_imagebackup = 'n';
            }

            if (strcmp("single",ToLowerStr(expdata)) == 0 ||
                    strcmp("one",ToLowerStr(expdata)) == 0) {
                g_imagebackup = 'n';
            }

            if (strcmp("timestamp",ToLowerStr(expdata)) == 0 ||
                    strcmp("stamp",ToLowerStr(expdata)) == 0) {
                g_imagebackup = 's';
            }
            break;

        case EditLog:
            if (strcmp(ToLowerStr(expdata),"true") == 0 ||
                    strcmp(ToLowerStr(expdata),"on") == 0) {
                log = 'y';
                break;
            }

            if (strcmp(ToLowerStr(expdata),"false") == 0 ||
                    strcmp(ToLowerStr(expdata),"off") == 0) {
                log = 'n';
                break;
            }

        case EditInform:
            if (strcmp(ToLowerStr(expdata),"true") == 0 ||
                    strcmp(ToLowerStr(expdata),"on") == 0) {
                inform = 'y';
                break;
            }

            if (strcmp(ToLowerStr(expdata),"false") == 0 ||
                    strcmp(ToLowerStr(expdata),"off") == 0) {
                inform = 'n';
                break;
            }
        }
    }

    g_parsing = false;
    ResetOutputRoute(log,inform);
}


/*
 * --------------------------------------------------------------------
 * Level 3
 * --------------------------------------------------------------------
 */

void
AddEditfileClasses(struct Edit *list,int editsdone)
{
    char *sp, currentitem[CF_MAXVARSIZE];
    struct Edlist *ep;

    if (editsdone) {
        for (ep = list->actions; ep != NULL; ep=ep->next) {
            if (IsExcluded(ep->classes)) {
                continue;
            }

            if (ep->code == DefineClasses) {
                Debug("AddEditfileClasses(%s)\n",ep->data);

                for (sp = ep->data; *sp != '\0'; sp++) {
                    currentitem[0] = '\0';

                    sscanf(sp,"%[^,:.]",currentitem);

                    sp += strlen(currentitem);

                    AddClassToHeap(currentitem);
                }
            }
        }
    } else {
        for (ep = list->actions; ep != NULL; ep=ep->next) {
            if (IsExcluded(ep->classes)) {
                continue;
            }

            if (ep->code == ElseDefineClasses) {
                Debug("Entering AddEditfileClasses(%s)\n",ep->data);

                for (sp = ep->data; *sp != '\0'; sp++) {

                    currentitem[0] = '\0';

                    sscanf(sp,"%[^,:.]",currentitem);

                    sp += strlen(currentitem);

                    AddClassToHeap(currentitem);
                }
            }
        }
    }

    if (ep == NULL) {
        return;
    }

}

/* ----------------------------------------------------------------- */

int
DeleteLinesWithFileItems(struct Item **filestart, char *infile,
        enum editnames code)
{
    struct Item *ip,*ipc,*infilelist = NULL;
    char currentitem[CF_BUFSIZE];
    int slen, matches=0;
    int positive=false;

    if (!LoadItemList(&infilelist,infile)) {
        snprintf(g_output,CF_BUFSIZE,
                "Cannot open file iterator %s in editfiles - aborting editing",
                infile);
        CfLog(cferror,g_output,"");
        return false;
    }

    for (ip = *filestart; ip != NULL; ip = ip->next) {
        Verbose("Looking at line with %s\n",ip->name);
        positive = false;

        switch (code) {
        case DeleteLinesStartingFileItems:
            positive = true;

        case DeleteLinesNotStartingFileItems:
            matches = 0;

            for (ipc = infilelist; ipc != NULL; ipc=ipc->next) {
                Chop(ipc->name);
                slen = IntMin(strlen(ipc->name),strlen(ip->name));

                if (strncmp(ipc->name,ip->name,slen) == 0) {
                    Debug("Matched with %s\n",ipc->name);
                    matches++;
                }
            }

            if (positive && (matches > 0)) {
                Debug("%s POS matched %s\n",g_veditnames[code],ip->name);
                DeleteItem(filestart,ip);
            } else if (!positive && matches == 0) {
                Debug("%s NEG matched %s\n",g_veditnames[code],ip->name);
                DeleteItem(filestart,ip);
            }

            break;

        case DeleteLinesContainingFileItems:
        positive = true;
        case DeleteLinesNotContainingFileItems:
        matches = 0;

            for (ipc = infilelist; ipc != NULL; ipc=ipc->next) {
                Chop(ipc->name);

                if (strstr(ipc->name,ip->name) == 0) {
                    matches++;
                } else if(strstr(ip->name,ipc->name) == 0) {
                    matches++;
                }
            }

            if (positive && (matches > 0)) {
                Debug("%s matched %s\n",g_veditnames[code],ip->name);
                DeleteItem(filestart,ip);
            } else if (!positive &&matches == 0) {
                Debug("%s matched %s\n",g_veditnames[code],ip->name);
                DeleteItem(filestart,ip);
            }
            break;

        case DeleteLinesMatchingFileItems:
            positive = true;

        case DeleteLinesNotMatchingFileItems:
            Verbose("%s not implemented (yet)\n",g_veditnames[code]);
            break;

        }
    }
    DeleteItemList(infilelist);
    return true;
}

/* ----------------------------------------------------------------- */

int
AppendLinesFromFile(struct Item **filestart, char *filename)
{
    FILE *fp;
    char buffer[CF_BUFSIZE];

    if ((fp=fopen(filename,"r")) == NULL) {
        snprintf(g_output,CF_BUFSIZE,"Editfiles could not read file %s\n",filename);
        CfLog(cferror,g_output,"fopen");
        return false;
    }

    while (!feof(fp)) {
        buffer[0] = '\0';
        fgets(buffer,CF_BUFSIZE-1,fp);
        Chop(buffer);

        if (!IsItemIn(*filestart,buffer)) {
            AppendItem(filestart,buffer,NULL);
        }
    }

    fclose(fp);
    return true;
}

/* ----------------------------------------------------------------- */

struct Edlist *
ThrowAbort(struct Edlist *from)
{
    struct Edlist *ep, *last = NULL;

    for (ep = from; ep != NULL; ep=ep->next) {
        if (ep->code == CatchAbort) {
            return ep;
        }

        last = ep;
    }

    return last;
}

/* ----------------------------------------------------------------- */

struct Edlist *
SkipToEndGroup(struct Edlist *ep, char *filename)
{
    int level = -1;

    while(ep != NULL) {
        switch (ep->code) {
        case BeginGroupIfNoMatch:
        case BeginGroupIfNoLineMatching:
        case BeginGroupIfNoSuchLine:
        case BeginGroupIfFileIsNewer:
        case BeginGroupIfFileExists:
        case BeginGroupIfNoLineContaining:
        case BeginGroupIfDefined:
        case BeginGroupIfNotDefined:
            level ++;
        }

        Debug("   skip: %s (%d)\n",g_veditnames[ep->code],level);

        if (ep->code == EndGroup) {
            if (level == 0) {
                return ep;
            } level--;
        }

        if (ep->next == NULL) {
            snprintf(g_output,CF_BUFSIZE*2,"Missing EndGroup in %s",filename);
            CfLog(cferror,g_output,"");
            break;
        }

        ep=ep->next;
    }

    return ep;
}

/* ----------------------------------------------------------------- */

int
BinaryEditFile(struct Edit *ptr, char *filename)
{
    char expdata[CF_EXPANDSIZE],search[CF_BUFSIZE];
    struct Edlist *ep;
    struct stat statbuf;
    void *memseg;

    EditVerbose("Begin (binary) editing %s\n",filename);

    g_numberofedits = 0;
    search[0] = '\0';

    if (stat(filename,&statbuf) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"Couldn't stat %s\n",filename);
        CfLog(cfverbose,g_output,"stat");
        return false;
    }

    if ((g_editbinfilesize != 0) &&(statbuf.st_size > g_editbinfilesize)) {
        snprintf(g_output,CF_BUFSIZE*2,
                "File %s is bigger than the limit <editbinaryfilesize>\n",
                filename);
        CfLog(cfinform,g_output,"");
        return(false);
    }

    if (! S_ISREG(statbuf.st_mode)) {
        snprintf(g_output,CF_BUFSIZE*2,"%s is not a plain file\n",filename);
        CfLog(cfinform,g_output,"");
        return false;
    }

    if ((memseg = malloc(statbuf.st_size + BUFFER_MARGIN)) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Unable to load file %s into memory",filename);
        CfLog(cferror, g_output, "malloc");
        return false;
    }

    LoadBinaryFile(filename,statbuf.st_size,memseg);

    ep = ptr->actions;

    while (ep != NULL) {
        if (IsExcluded(ep->classes)) {
            ep = ep->next;
            continue;
        }

        ExpandVarstring(ep->data,expdata,NULL);

        Debug2("Edit action: %s\n",g_veditnames[ep->code]);

        switch(ep->code) {
        case WarnIfContainsString:
            WarnIfContainsRegex(memseg,statbuf.st_size,expdata,filename);
            break;

        case WarnIfContainsFile:
            WarnIfContainsFilePattern(memseg,statbuf.st_size,expdata,filename);
            break;

        case EditMode:
            break;

        case ReplaceAll:
            Debug("Replace %s\n", expdata);
            strncpy(search, expdata, CF_BUFSIZE);
            break;

        case With:
            if (strcmp(expdata,search) == 0) {
                Verbose("Search and replace patterns are identical "
                        "in binary edit %s\n",filename);
                continue;
            }

            if (BinaryReplaceRegex(memseg,statbuf.st_size,search,
                        expdata,filename)) {
                g_numberofedits++;
            }
            break;

        default:
            snprintf(g_output, CF_BUFSIZE*2,
                    "Cannot use %s in a binary edit (%s)",
                    g_veditnames[ep->code],filename);
            CfLog(cferror,g_output,"");
        }

        ep=ep->next;
    }

    if ((! g_dontdo) && (g_numberofedits > 0)) {
        SaveBinaryFile(filename,statbuf.st_size,memseg,ptr->repository);
        AddEditfileClasses(ptr,true);
    } else {
        AddEditfileClasses(ptr,false);
    }

    free(memseg);
    EditVerbose("End editing %s\n",filename);
    EditVerbose(".....................................................................\n");
    return true;
}

/*
 * --------------------------------------------------------------------
 * Level 4
 * --------------------------------------------------------------------
 */

int
LoadBinaryFile(char *source, off_t size, void *memseg)
{
    int sd,n_read;
    char buf[CF_BUFSIZE];
    off_t n_read_total = 0;
    char *ptr;

    Debug("LoadBinaryFile(%s,%d)\n",source,size);

    if ((sd = open(source,O_RDONLY)) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"Can't copy %s!\n",source);
        CfLog(cfinform,g_output,"open");
        return false;
    }

    ptr = memseg;

    while (true) {
        if ((n_read = read (sd,buf,CF_BUFSIZE)) == -1) {
            if (errno == EINTR) {
                continue;
            }

            close(sd);
            return false;
        }

        bcopy(buf,ptr,n_read);
        ptr += n_read;

        if (n_read < CF_BUFSIZE) {
            break;
        }

        n_read_total += n_read;
    }

    close(sd);
    return true;
}

/* ----------------------------------------------------------------- */

/* 
 * If we do this, we screw up checksums anyway, so no need to preserve
 * unix holes here ...
 */

int
SaveBinaryFile(char *file, off_t size, void *memseg, char *repository)
{
    int dd;
    char new[CF_BUFSIZE],backup[CF_BUFSIZE];

    Debug("SaveBinaryFile(%s,%d)\n",file,size);
    Verbose("Saving %s\n",file);

    strcpy(new,file);
    strcat(new,CF_NEW);
    strcpy(backup,file);
    strcat(backup,CF_EDITED);

    unlink(new);  /* To avoid link attacks */

    if ((dd = open(new,O_WRONLY|O_CREAT|O_TRUNC|O_EXCL, 0600)) == -1) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Copy %s security - failed attempt to exploit a race? "
                "(Not copied)\n", file);
        CfLog(cfinform,g_output,"open");
        return false;
    }

    cf_full_write (dd,(char *)memseg,size);

    close(dd);

    if (! IsItemIn(g_vreposlist,new)) {
        if (rename(file,backup) == -1) {
            snprintf(g_output, CF_BUFSIZE*2,
                    "Error while renaming backup %s\n",file);
            CfLog(cferror,g_output,"rename ");
            unlink(new);
            return false;
        } else if(Repository(backup,repository)) {
            if (rename(new,file) == -1) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Error while renaming %s\n",file);
                CfLog(cferror,g_output,"rename");
                return false;
            }
            unlink(backup);
            return true;
        }
    }

    if (rename(new,file) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"Error while renaming %s\n",file);
        CfLog(cferror,g_output,"rename");
        return false;
    }

    return true;
}

/* ----------------------------------------------------------------- */

void
WarnIfContainsRegex(void *memseg, off_t size, char *data, char *filename)
{
    off_t sp;
    regex_t rx,rxcache;
    regmatch_t pmatch;

    Debug("WarnIfContainsRegex(%s)\n",data);

    for (sp = 0; sp < (off_t)(size-strlen(data)); sp++) {
        if (bcmp((char *) memseg+sp,data,strlen(data)) == 0) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "WARNING! File %s contains literal string %.255s",
                    filename,data);
            CfLog(cferror,g_output,"");
            return;
        }
    }

    if (CfRegcomp(&rxcache,data,REG_EXTENDED) != 0) {
        return;
    }

    for (sp = 0; sp < (off_t)(size-strlen(data)); sp++) {
        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache,&rx,sizeof(rx));

        if (regexec(&rx,(char *)memseg+sp,1,&pmatch,0) == 0) {
            snprintf(g_output,CF_BUFSIZE*2,
                    "WARNING! File %s contains regular expression %.255s",
                    filename,data);
            CfLog(cferror,g_output,"");
            regfree(&rx);
            return;
        }
    }
}

/* ----------------------------------------------------------------- */

void
WarnIfContainsFilePattern(void *memseg, off_t size, char *data, char *filename)
{
    off_t sp;
    struct stat statbuf;
    char *pattern;

    Debug("WarnIfContainsFile(%s)\n", data);

    if (stat(data,&statbuf) == -1) {
        snprintf(g_output, CF_BUFSIZE*2, "File %s cannot be opened", data);
        CfLog(cferror, g_output, "stat");
        return;
    }

    if (! S_ISREG(statbuf.st_mode)) {
        snprintf(g_output, CF_BUFSIZE*2,
                "File %s cannot be used as a binary pattern", data);
        CfLog(cferror, g_output,"");
        return;
    }

    if (statbuf.st_size > size) {
        snprintf(g_output, CF_BUFSIZE*2,
                "File %s is larger than the search file, ignoring",data);
        CfLog(cfinform, g_output, "");
        return;
    }

    if ((pattern = malloc(statbuf.st_size + BUFFER_MARGIN)) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, "File %s cannot be loaded", data);
        CfLog(cferror, g_output,"");
        return;
    }

    if (!LoadBinaryFile(data, statbuf.st_size,pattern)) {
        snprintf(g_output, CF_BUFSIZE*2, "File %s cannot be opened", data);
        CfLog(cferror, g_output, "stat");
        return;
   }

    for (sp = 0; sp < (off_t)(size-statbuf.st_size); sp++) {
        if (bcmp((char *) memseg+sp,pattern,statbuf.st_size-1) == 0) {
            snprintf(g_output, CF_BUFSIZE*2, "WARNING! File %s contains "
                    "the contents of reference file %s", filename, data);
            CfLog(cferror, g_output, "");
            free(pattern);
            return;
        }
   }

    free(pattern);
}


/* ----------------------------------------------------------------- */

int
BinaryReplaceRegex(void *memseg, off_t size, char *search,
        char *replace, char *filename)
{
    off_t sp,spr;
    regex_t rx,rxcache;
    regmatch_t pmatch;
    int match = false;

    Debug("BinaryReplaceRegex(%s,%s,%s)\n",search,replace,filename);

    if (CfRegcomp(&rxcache,search,REG_EXTENDED) != 0) {
        return false;
    }

    for (sp = 0; sp < (off_t)(size-strlen(replace)); sp++) {

        /* To fix a bug on some implementations where rx gets emptied */
        bcopy(&rxcache,&rx,sizeof(rx));

        if (regexec(&rx,(char *)(sp+(char *)memseg),1,&pmatch,0) == 0) {
            if (pmatch.rm_eo-pmatch.rm_so < strlen(replace)) {
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Cannot perform binary replacement: string "
                        "doesn't fit in %s", filename);
                CfLog(cfverbose,g_output,"");
            } else {
                Verbose("Replacing [%s] with [%s] at %d\n",search,replace,sp);
                match = true;

                strncpy((char *)memseg+sp+(off_t)pmatch.rm_so,
                        replace,strlen(replace));

                Verbose("Padding character is %c\n",g_padchar);

                for (spr = (pmatch.rm_so+strlen(replace));
                        spr < pmatch.rm_eo; spr++) {

                    *((char *)memseg+spr+sp) = g_padchar; /* default space */
                }

                sp += pmatch.rm_eo - pmatch.rm_so - 1;
            }
        } else {
            sp += strlen(replace);
        }
    }

    regfree(&rx);
    return match;
}
