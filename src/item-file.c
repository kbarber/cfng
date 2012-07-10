/*
 * $Id: item-file.c 682 2004-05-20 22:46:52Z skaar $
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
 * Toolkit: the "item file extension" object for cfng
 */


#include "cf.defs.h"
#include "cf.extern.h"

/* ----------------------------------------------------------------- */

int
LoadItemList(struct Item **liststart,char *file)
{
    FILE *fp;
    struct stat statbuf;

    if (stat(file,&statbuf) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"Couldn't stat %s\n",file);
        CfLog(cfverbose,g_output,"stat");
        return false;
    }

    if ((g_editfilesize != 0) &&(statbuf.st_size > g_editfilesize)) {
        snprintf(g_output,CF_BUFSIZE*2,
                "File %s is bigger than the limit <editfilesize>\n",file);
        CfLog(cfinform,g_output,"");
        return(false);
    }

    if (! S_ISREG(statbuf.st_mode)) {
        snprintf(g_output,CF_BUFSIZE*2,"%s is not a plain file\n",file);
        CfLog(cfinform,g_output,"");
        return false;
    }

    if ((fp = fopen(file,"r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't read file %s for editing\n", file);
        CfLog(cferror,g_output,"fopen");
        return false;
    }


    memset(g_vbuff,0,CF_BUFSIZE);

    while(!feof(fp)) {
        ReadLine(g_vbuff,CF_BUFSIZE,fp);

        if (!feof(fp) || (strlen(g_vbuff) != 0)) {
            AppendItem(liststart,g_vbuff,NULL);
        }
        g_vbuff[0] = '\0';
    }

    fclose(fp);
    return (true);
}

/* ----------------------------------------------------------------- */

int
SaveItemList(struct Item *liststart, char *file, char *repository)
{
    struct Item *ip;
    struct stat statbuf;
    char new[CF_BUFSIZE],backup[CF_BUFSIZE];
    FILE *fp;
    mode_t mask;
    char stamp[CF_BUFSIZE];
    time_t STAMPNOW;
    STAMPNOW = time((time_t *)NULL);

    if (stat(file,&statbuf) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,
                "Couldn't stat %s, which needed editing!\n",file);
        CfLog(cferror,g_output,"");
        CfLog(cferror,
                "Check definition in program - is file NFS mounted?\n\n","");
        return false;
    }

    strcpy(new,file);
    strcat(new,CF_EDITED);

    strcpy(backup,file);

    sprintf(stamp, "_%d_%s", g_cfstarttime, CanonifyName(ctime(&STAMPNOW)));

    if (g_imagebackup == 's') {
        strcat(backup,stamp);
    }

    strcat(backup,CF_SAVED);

    unlink(new); /* Just in case of races */

    if ((fp = fopen(new,"w")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't write file %s after editing\n", new);
        CfLog(cferror,g_output,"");
        return false;
    }

    for (ip = liststart; ip != NULL; ip=ip->next) {
        fprintf(fp,"%s\n",ip->name);
    }

    if (fclose(fp) == -1) {
        CfLog(cferror, "Unable to close file while writing", "fclose");
        return false;
    }

    if (g_iscfengine && (g_action == editfiles)) {
        snprintf(g_output,CF_BUFSIZE*2,"Edited file %s \n",file);
        CfLog(cfinform,g_output,"");
    }

    if (g_imagebackup != 'n') {
        if (! IsItemIn(g_vreposlist,new)) {
            if (rename(file,backup) == -1) {
                snprintf(g_output,CF_BUFSIZE*2,
                        "Error while renaming backup %s\n",file);
                CfLog(cferror,g_output,"rename ");
                unlink(new);
                return false;
            } else if (Repository(backup,repository)) {
                unlink(backup);
            }
        }
    }

    if (rename(new,file) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,"Error while renaming %s\n",file);
        CfLog(cferror,g_output,"rename");
        return false;
    }

    mask = umask(0);

    /* Restore file permissions etc */
    chmod(file,statbuf.st_mode);
    chown(file,statbuf.st_uid,statbuf.st_gid);
    umask(mask);
    return true;
}

/* ----------------------------------------------------------------- */

/* returns true if file on disk is identical to file in memory */
int
CompareToFile(struct Item *liststart, char *file)
{
    FILE *fp;
    struct stat statbuf;
    struct Item *ip = liststart;

    Debug("CompareToFile(%s)\n",file);

    if (stat(file,&statbuf) == -1) {
        return false;
    }

    if (liststart == NULL) {
        return false;
    }

    if ((fp = fopen(file,"r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Couldn't read file %s for editing\n", file);
        CfLog(cferror,g_output,"fopen");
        return false;
    }

    memset(g_vbuff,0,CF_BUFSIZE);

    for (ip = liststart; ip != NULL; ip=ip->next) {
        ReadLine(g_vbuff,CF_BUFSIZE,fp);

        if (feof(fp) && (ip->next != NULL)) {
            fclose(fp);
            return false;
        }

        if ((ip->name == NULL) && (strlen(g_vbuff) == 0)) {
            continue;
        }

        if (ip->name == NULL) {
            fclose(fp);
            return false;
        }

        if (strcmp(ip->name,g_vbuff) != 0) {
            fclose(fp);
            return false;
        }

        g_vbuff[0] = '\0';
    }

    if (!feof(fp) && (ReadLine(g_vbuff,CF_BUFSIZE,fp) != false)) {
        fclose(fp);
        return false;
    }

    fclose(fp);
    return (true);
}

/* ----------------------------------------------------------------- */

void
InsertFileAfter(struct Item **filestart, struct Item *ptr, char *string)
{
    struct Item *ip, *old;
    char *sp;
    FILE *fp;
    char linebuf[CF_BUFSIZE];

    EditVerbose("Edit: Inserting file %s \n",string);

    if ((fp=fopen(string,"r")) == NULL) {
        Verbose("Could not open file %s\n",string);
        return;
    }

    while(!feof(fp) && ReadLine(linebuf,CF_BUFSIZE,fp)) {
        if ((ip = (struct Item *)malloc(sizeof(struct Item))) == NULL) {
            CfLog(cferror,"","Can't allocate memory in InsertItemAfter()");
            FatalError("");
        }

        if ((sp = malloc(strlen(linebuf)+1)) == NULL) {
            CfLog(cferror,"","Can't allocate memory in InsertItemAfter()");
            FatalError("");
        }

        if (g_currentlineptr == NULL) {
            if (*filestart == NULL) {
                *filestart = ip;
                ip->next = NULL;
            } else {
                ip->next = (*filestart)->next;
                (*filestart)->next = ip;
            }

            strcpy(sp,linebuf);
            ip->name = sp;
            ip->classes = NULL;
            g_currentlineptr = ip;
            g_currentlinenumber = 1;
        } else {
            ip->next = g_currentlineptr->next;
            g_currentlineptr->next = ip;
            g_currentlineptr=ip;
            g_currentlinenumber++;
            strcpy(sp,linebuf);
            ip->name = sp;
            ip->classes = NULL;
        }
    }

    g_numberofedits++;

    fclose(fp);

    return;
}
