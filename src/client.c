/*
 * $Id: client.c 733 2004-05-23 05:57:13Z skaar $
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
 * File Image copying: client part for remote copying
 */


#include "cf.defs.h"
#include "cf.extern.h"

/*
 * --------------------------------------------------------------------
 * Level 1
 * --------------------------------------------------------------------
 */

int
OpenServerConnection(struct Image *ip)
{
    char server[CF_EXPANDSIZE];

    if (strcmp(ip->server,"localhost") == 0) {
        g_authenticated = true;
        return true;
    }

    g_authenticated = false;
    ExpandVarstring(ip->server,server,NULL);

    if (g_conn->sd == CF_NOT_CONNECTED) {
        Debug("Opening server connnection to %s\n",ip->server);

        if (!RemoteConnect(server,ip->forceipv4)) {
            CfLog(cfinform,"Couldn't open a socket","socket");
            if (g_conn->sd != CF_NOT_CONNECTED) {
                CloseServerConnection();
            }
            g_authenticated = false;
            return false;
        }

        if (!IdentifyForVerification(g_conn->sd, g_conn->localip, 
                    g_conn->family)) {
            snprintf(g_output,CF_BUFSIZE,"Id-authentication for %s failed\n",
                    g_vfqname);
            CfLog(cferror,g_output,"");
            errno = EPERM;
            CloseServerConnection();
            g_authenticated = false;
            return false;
        }

        if (ip->compat == 'y') {
            CfLog(cfinform, "WARNING: the connection to %s is not "
                    "offering key authentication\n","");
            CfLog(cfinform, "WARNING: oldserver=true is a TEMPORARY "
                    "measure only\n","");
        }

        else if (!KeyAuthentication(ip)) {
            snprintf(g_output,CF_BUFSIZE,
                    "Authentication dialogue with %s failed\n",server);
            CfLog(cferror,g_output,"");
            errno = EPERM;
            CloseServerConnection();
            g_authenticated = false;
            return false;
        }

        g_authenticated = true;
        return true;
    } else {
        Debug("Server connection to %s already open on %d\n",
                server,g_conn->sd);
    }

    g_authenticated = true;
    return true;
}

/* ----------------------------------------------------------------- */

void
CloseServerConnection(void)
{
    Debug("Closing current connection\n");

    close(g_conn->sd);

    g_conn->sd = CF_NOT_CONNECTED;

    if (g_conn->session_key != NULL) {
        free(g_conn->session_key);
        g_conn->session_key = NULL;
    }
}

/* ----------------------------------------------------------------- */

/* 
 * If a link, this reads readlink and sends it back in the same package.
 * It then caches the value for each copy command 
 */

int cf_rstat(char *file, struct stat *buf, struct Image *ip, char *stattype)
{
    char sendbuffer[CF_BUFSIZE];
    char recvbuffer[CF_BUFSIZE];
    char in[CF_BUFSIZE],out[CF_BUFSIZE];
    struct cfstat cfst;
    int ret,tosend,cipherlen;
    time_t tloc;

    Debug("cf_rstat(%s)\n",file);
    memset(recvbuffer,0,CF_BUFSIZE);

    if (strlen(file) > CF_BUFSIZE-30) {
        CfLog(cferror,"Filename too long","");
        return -1;
    }

    ret = GetCachedStatData(file,buf,ip,stattype);

    if (ret != 0) {
        return ret;
    }

    if ((tloc = time((time_t *)NULL)) == -1) {
        CfLog(cferror,"Couldn't read system clock\n","");
    }

    sendbuffer[0] = '\0';

    if (ip->encrypt == 'y') {
        if (g_conn->session_key == NULL) {
            CfLog(cferror,
                    "Cannot do encrypted copy without keys (use cfkey)","");
            return -1;
        }

        snprintf(in,CF_BUFSIZE-1,"SYNCH %d STAT %s",tloc,file);
        cipherlen = EncryptString(in,out,g_conn->session_key,strlen(in)+1);
        snprintf(sendbuffer,CF_BUFSIZE-1,"SSYNCH %d",cipherlen);
        bcopy(out,sendbuffer+CF_PROTO_OFFSET,cipherlen);
        tosend = cipherlen+CF_PROTO_OFFSET;
    } else {
        snprintf(sendbuffer,CF_BUFSIZE,"SYNCH %d STAT %s",tloc,file);
        tosend = strlen(sendbuffer);
    }

    if (SendTransaction(g_conn->sd,sendbuffer,tosend,CF_DONE) == -1) {
        snprintf(g_output,CF_BUFSIZE*2,
                "Transmission failed/refused talking to %.255s:%.255s in stat",
                ip->server,file);
        CfLog(cfinform,g_output,"send");
        return -1;
    }

    if (ReceiveTransaction(g_conn->sd,recvbuffer,NULL) == -1) {
        return -1;
    }

    if (strstr(recvbuffer,"unsynchronized")) {
        CfLog(cferror,
                "Clocks differ too much to do copy by date (security)","");
        CfLog(cferror,recvbuffer+4,"");
        return -1;
    }

    if (BadProtoReply(recvbuffer)) {
        snprintf(g_output,CF_BUFSIZE*2,
                "Server returned error: %s\n",recvbuffer+4);
        CfLog(cfverbose,g_output,"");
        errno = EPERM;
        return -1;
    }

    if (OKProtoReply(recvbuffer)) {
        long d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12=0,d13=0;

        sscanf(recvbuffer,"OK: %1ld %5ld %14ld %14ld %14ld %14ld "
                "%14ld %14ld %14ld %14ld %14ld %14ld %14ld",
                &d1,&d2,&d3,&d4,&d5,&d6,&d7,&d8,&d9,&d10,&d11,&d12,&d13);

        cfst.cf_type = (enum cf_filetype) d1;
        cfst.cf_mode = (mode_t) d2;
        cfst.cf_lmode = (mode_t) d3;
        cfst.cf_uid = (uid_t) d4;
        cfst.cf_gid = (gid_t) d5;
        cfst.cf_size = (off_t) d6;
        cfst.cf_atime = (time_t) d7;
        cfst.cf_mtime = (time_t) d8;
        cfst.cf_ctime = (time_t) d9;
        cfst.cf_makeholes = (char) d10;
        ip->makeholes = (char) d10;
        cfst.cf_ino = d11;
        cfst.cf_nlink = d12;
        cfst.cf_dev = d13;

        /* Use %?d here to avoid memory overflow attacks */

        Debug("Mode = %d,%d\n",d2,d3);

        Debug("OK: type=%d\n mode=%o\n lmode=%o\n uid=%d\n gid=%d\n "
                "size=%ld\n atime=%d\n mtime=%d ino=%d nlnk=%d, dev=%d\n",
                cfst.cf_type, cfst.cf_mode, cfst.cf_lmode, cfst.cf_uid,
                cfst.cf_gid, (long)cfst.cf_size, cfst.cf_atime, cfst.cf_mtime,
                cfst.cf_ino, cfst.cf_nlink, cfst.cf_dev);

        memset(recvbuffer,0,CF_BUFSIZE);

        if (ReceiveTransaction(g_conn->sd,recvbuffer,NULL) == -1) {
            return -1;
        }

        Debug("Linkbuffer: %s\n",recvbuffer);

        if (strlen(recvbuffer) > 3) {
            cfst.cf_readlink = strdup(recvbuffer+3);
        } else {
            cfst.cf_readlink = NULL;
        }

        switch (cfst.cf_type) {
        case cf_reg:
            cfst.cf_mode |= (mode_t) S_IFREG;
            break;
        case cf_dir:
            cfst.cf_mode |= (mode_t) S_IFDIR;
            break;
        case cf_char:
            cfst.cf_mode |= (mode_t) S_IFCHR;
            break;
        case cf_fifo:
            cfst.cf_mode |= (mode_t) S_IFIFO;
            break;
        case cf_sock:
            cfst.cf_mode |= (mode_t) S_IFSOCK;
            break;
        case cf_block:
            cfst.cf_mode |= (mode_t) S_IFBLK;
            break;
        case cf_link:
            cfst.cf_mode |= (mode_t) S_IFLNK;
            break;
        }


        cfst.cf_filename = strdup(file);
        cfst.cf_server =  strdup(ip->server);

        if ((cfst.cf_filename == NULL) ||(cfst.cf_server) == NULL) {
            FatalError("Memory allocation in cf_rstat");
        }

        cfst.cf_failed = false;

        if (cfst.cf_lmode != 0) {
            cfst.cf_lmode |= (mode_t) S_IFLNK;
        }

        CacheData(&cfst,ip);

        if ((cfst.cf_lmode != 0) && (strcmp(stattype,"link") == 0)) {
            buf->st_mode = cfst.cf_lmode;
        } else {
            buf->st_mode = cfst.cf_mode;
        }

        buf->st_uid = cfst.cf_uid;
        buf->st_gid = cfst.cf_gid;
        buf->st_size = cfst.cf_size;
        buf->st_mtime = cfst.cf_mtime;
        buf->st_ctime = cfst.cf_ctime;
        buf->st_atime = cfst.cf_atime;
        buf->st_ino   = cfst.cf_ino;
        buf->st_dev   = cfst.cf_dev;
        buf->st_nlink = cfst.cf_nlink;

        return 0;
    }
    snprintf(g_output,CF_BUFSIZE*2,
            "Transmission refused or failed statting %s\nGot: %s\n",
            file,recvbuffer);

    CfLog(cferror,g_output,"");
    errno = EPERM;

    return -1;
}

/* ----------------------------------------------------------------- */

CFDIR *
cf_ropendir(char *dirname, struct Image *ip)
{
    char sendbuffer[CF_BUFSIZE];
    char recvbuffer[CF_BUFSIZE];
    int n, done=false;
    CFDIR *cfdirh;
    char *sp;

    Debug("CfOpenDir(%s:%s)\n",ip->server,dirname);

    if (strlen(dirname) > CF_BUFSIZE - 20) {
        CfLog(cferror,"Directory name too long","");
        return NULL;
    }

    if ((cfdirh = (CFDIR *)malloc(sizeof(CFDIR))) == NULL) {
        CfLog(cferror,"Couldn't allocate memory in cf_ropendir\n","");
        exit(1);
    }

    cfdirh->cf_list = NULL;
    cfdirh->cf_listpos = NULL;
    cfdirh->cf_dirh = NULL;

    snprintf(sendbuffer,CF_BUFSIZE,"OPENDIR %s",dirname);

    if (SendTransaction(g_conn->sd,sendbuffer,0,CF_DONE) == -1) {
        return NULL;
    }

    while (!done) {
        if ((n = ReceiveTransaction(g_conn->sd,recvbuffer,NULL)) == -1) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        if (n == 0) {
            break;
        }

        if (FailedProtoReply(recvbuffer)) {
            snprintf(g_output,CF_BUFSIZE*2,"Network access to %s:%s denied\n",
                    ip->server,dirname);
            CfLog(cfinform,g_output,"");
            return false;
        }

        if (BadProtoReply(recvbuffer)) {
            snprintf(g_output,CF_BUFSIZE*2,"%s\n",recvbuffer+4);
            CfLog(cfinform,g_output,"");
            return false;
        }

        for (sp = recvbuffer; *sp != '\0'; sp++) {

            /* End transmission */
            if (strncmp(sp,CFD_TERMINATOR,strlen(CFD_TERMINATOR)) == 0) {
                cfdirh->cf_listpos = cfdirh->cf_list;
                return cfdirh;
            }

            AppendItem(&(cfdirh->cf_list),sp,NULL);

            while(*sp != '\0') {
                sp++;
            }
        }
    }

    cfdirh->cf_listpos = cfdirh->cf_list;
    return cfdirh;
}

/* ----------------------------------------------------------------- */

void
FlushClientCache(struct Image *ip)
{
    if (ip->cache) {
        free(ip->cache);
        ip->cache = NULL;
    }
}

/* ----------------------------------------------------------------- */

int
CompareMD5Net(char *file1, char *file2, struct Image *ip)
{
    static unsigned char d[CF_MD5_LEN];
    char *sp,sendbuffer[CF_BUFSIZE],recvbuffer[CF_BUFSIZE],in[CF_BUFSIZE],out[CF_BUFSIZE];
    int i,tosend,cipherlen;


    ChecksumFile(file2,d,'m');   /* send md5 to the server for comparison */
    Debug("Send digest of %s to server, %s\n",file2,ChecksumPrint('m',d));

    memset(recvbuffer,0,CF_BUFSIZE);

    if (ip->encrypt == 'y') {
        snprintf(in,CF_BUFSIZE,"MD5 %s",file1);

        sp = in + strlen(in) + CF_SMALL_OFFSET;

        for (i = 0; i < CF_MD5_LEN; i++) {
            *sp++ = d[i];
        }

        cipherlen = EncryptString(in,out,g_conn->session_key,
                strlen(in)+CF_SMALL_OFFSET+CF_MD5_LEN);
        snprintf(sendbuffer,CF_BUFSIZE,"SMD5 %d",cipherlen);
        bcopy(out,sendbuffer+CF_PROTO_OFFSET,cipherlen);
        tosend = cipherlen + CF_PROTO_OFFSET;
    } else {
        snprintf(sendbuffer,CF_BUFSIZE,"MD5 %s",file1);
        sp = sendbuffer + strlen(sendbuffer) + CF_SMALL_OFFSET;

        for (i = 0; i < CF_MD5_LEN; i++) {
            *sp++ = d[i];
        }

        tosend = strlen(sendbuffer)+CF_SMALL_OFFSET+CF_MD5_LEN;
    }

    if (SendTransaction(g_conn->sd,sendbuffer,tosend,CF_DONE) == -1) {
        CfLog(cferror,"","send");
        return false;
    }

    if (ReceiveTransaction(g_conn->sd,recvbuffer,NULL) == -1) {
        Verbose("No answer from host, assuming checksum ok to avoid "
                "remote copy for now...\n");
        return false;
    }

    if (strcmp(CFD_TRUE,recvbuffer) == 0) {
        Debug("MD5 mismatch: (reply - %s)\n",recvbuffer);
        return true; /* mismatch */
    } else {
        Debug("MD5 matched ok: (reply - %s)\n",recvbuffer);
        return false;
    }

    /* Not reached */
}

/* ----------------------------------------------------------------- */

int
CopyRegNet(char *source, char *new, struct Image *ip, off_t size)
{
    int dd, buf_size,n_read = 0,toget,towrite,plainlen,more = true;
    int last_write_made_hole = 0, done = false,tosend,cipherlen=0;
    char *buf,in[CF_BUFSIZE],out[CF_BUFSIZE];
    char sendbuffer[CF_BUFSIZE],cfchangedstr[265];
    unsigned char iv[] = {1,2,3,4,5,6,7,8};
    long n_read_total = 0;
    EVP_CIPHER_CTX ctx;

    snprintf(cfchangedstr,255,"%s%s",CF_CHANGEDSTR1,CF_CHANGEDSTR2);

    EVP_CIPHER_CTX_init(&ctx);

    if ((strlen(new) > CF_BUFSIZE-20)) {
        CfLog(cferror,"Filename too long","");
        return false;
    }

    unlink(new);  /* To avoid link attacks */

    if ((dd = open(new,O_WRONLY|O_CREAT|O_TRUNC|O_EXCL|O_BINARY, 0600)) == -1) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Copy %s:%s security - failed attempt to exploit a race? "
                "(Not copied)\n",ip->server,new);
        CfLog(cferror,g_output,"open");
        unlink(new);
        return false;
    }

    sendbuffer[0] = '\0';

    buf_size = ST_BLKSIZE(dstat);

    if (buf_size < 2048) {
        buf_size = 2048;
    }

    if (ip->encrypt == 'y') {
        snprintf(in,CF_BUFSIZE-CF_PROTO_OFFSET,"GET dummykey %s",source);
        cipherlen = EncryptString(in,out,g_conn->session_key,strlen(in)+1);
        snprintf(sendbuffer,CF_BUFSIZE,"SGET %4d %4d",cipherlen,buf_size);
        bcopy(out,sendbuffer+CF_PROTO_OFFSET,cipherlen);
        tosend=cipherlen+CF_PROTO_OFFSET;
        EVP_DecryptInit(&ctx,EVP_bf_cbc(),g_conn->session_key,iv);
    } else {
        snprintf(sendbuffer,CF_BUFSIZE,"GET %d %s",buf_size,source);
        tosend=strlen(sendbuffer);
    }

    if (SendTransaction(g_conn->sd,sendbuffer,tosend,CF_DONE) == -1) {
        CfLog(cferror,"Couldn't send","send");
        close(dd);
        return false;
    }

    /* Note CF_BUFSIZE not buf_size !! */
    buf = (char *) malloc(CF_BUFSIZE + sizeof(int)); 
    n_read_total = 0;

    while (!done) {
        cipherlen = 0;

        if ((size - n_read_total)/buf_size > 0) {
            toget = towrite = buf_size;
        } else if (size != 0) {
            towrite = (size - n_read_total);

            if (ip->compat == 'y') {
                toget = buf_size;
            } else {
                toget = towrite;
            }
        } else {
            toget = towrite = 0;
        }

        if (ip->encrypt == 'y') {
            if (more) {
                cipherlen = ReceiveTransaction(g_conn->sd,buf,&more);
            } else {
                break;  /* Already written last encrypted buffer */
            }
        } else {
            if ((n_read = RecvSocketStream(g_conn->sd,buf,toget,0)) == -1) {
                if (errno == EINTR) {
                    continue;
                }

                CfLog(cferror,"Error in socket stream","recv");
                close(dd);
                free(buf);
                return false;
            }
        }


        /* If the first thing we get is an error message, break. */

        if (n_read_total == 0 && strncmp(buf,CF_FAILEDSTR,
                    strlen(CF_FAILEDSTR)) == 0) {

            snprintf(g_output,CF_BUFSIZE*2,"Network access to %s:%s denied\n",
                    ip->server,source);

            if (ip->encrypt != 'y') {
                /* flush rest of transaction */
                RecvSocketStream(g_conn->sd,buf,buf_size-n_read,0);
            }

            CfLog(cfinform,g_output,"");
            close(dd);
            free(buf);
            return false;
        }

        if (strncmp(buf,cfchangedstr,strlen(cfchangedstr)) == 0) {
            snprintf(g_output,CF_BUFSIZE*2,"File %s:%s changed while copying\n",
                    ip->server,source);

            /* flush rest of transaction */
            RecvSocketStream(g_conn->sd,buf,buf_size-n_read,0);

            CfLog(cfinform,g_output,"");
            close(dd);
            free(buf);
            return false;
        }

        if (ip->encrypt == 'y') {
            if (!EVP_DecryptUpdate(&ctx,sendbuffer,&plainlen,buf,cipherlen)) {
                Debug("Decryption failed\n");
                return false;
            }

            bcopy(sendbuffer,buf,plainlen);
            n_read = towrite = plainlen;
        }

        if (ip->encrypt != 'y') {
            if (n_read == 0) {
                break;
            }

            if (n_read == size) {
                if (n_read_total == 0 && strncmp(buf,CF_FAILEDSTR,size) == 0) {
                    snprintf(g_output,CF_BUFSIZE*2,
                            "Network access to %s:%s denied\n",
                            ip->server,source);
                    CfLog(cfinform,g_output,"");
                    close(dd);
                    free(buf);
                    return false;
                }
            }
        }

        /*   if (n_read < toget)
            {
            snprintf(g_output,CF_BUFSIZE*2,"Network error getting %s:%s\n",ip->server,source);
            CfLog(cfinform,g_output,"");
            close(dd);
            free(buf);
            return false;
            }
        */

        n_read_total += towrite; /* n_read; */

        if (ip->encrypt == 'n') {


            /* Handle EOF without closing socket */
            if (n_read_total >= (long)size) {
                done = true;
            }
        }

        if (!EmbeddedWrite(new,dd,buf,ip,towrite,
                    &last_write_made_hole,n_read)) {

            snprintf(g_output,CF_BUFSIZE,
                    "Local disk write failed copying %s:%s to %s\n",
                    ip->server,source,new);

            CfLog(cferror,g_output,"");
            free(buf);
            unlink(new);
            close(dd);
            FlushToEnd(g_conn->sd,size - n_read_total);
            EVP_CIPHER_CTX_cleanup(&ctx);
            return false;
        }
    }

    /* final crypto cleanup */
    if (ip->encrypt == 'y') {
        if (!EVP_DecryptFinal(&ctx,buf,&plainlen)) {
            Debug("Final decrypt failed\n");
            return false;
        }

        if (!EmbeddedWrite(new,dd,buf,ip,plainlen,
                    &last_write_made_hole,n_read)) {

            snprintf(g_output,CF_BUFSIZE,
                    "Local disk write failed copying %s:%s to %s\n",
                    ip->server,source,new);
            CfLog(cferror,g_output,"");
            free(buf);
            unlink(new);
            close(dd);
            FlushToEnd(g_conn->sd,size - n_read_total);
            EVP_CIPHER_CTX_cleanup(&ctx);
            return false;
        }
    }

    /* 
     * If the file ends with a `hole', something needs to be written at
     * the end.  Otherwise the kernel would truncate the file at the end
     * of the last write operation. Write a null character and truncate
     * it again.
     */

    if (last_write_made_hole)   {
        if (cf_full_write (dd,"",1) < 0 || ftruncate (dd,n_read_total) < 0) {

            CfLog(cferror,
                    "cfng: full_write or ftruncate error in CopyReg\n","");

            free(buf);
            unlink(new);
            close(dd);
            FlushToEnd(g_conn->sd,size - n_read_total);
            EVP_CIPHER_CTX_cleanup(&ctx);
            return false;
        }
    }

    Debug("End of CopyNetReg\n");
    close(dd);
    free(buf);
    EVP_CIPHER_CTX_cleanup(&ctx);
    return true;
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

int
GetCachedStatData(char *file, struct stat *statbuf,
        struct Image *ip, char *stattype)
{
    struct cfstat *sp;

    Debug("GetCachedStatData(%s)\n",file);

    for (sp = ip->cache; sp != NULL; sp=sp->next) {
        if ((strcmp(ip->server,sp->cf_server) == 0) &&
                (strcmp(file,sp->cf_filename) == 0)) {

            /* cached failure from cfopendir */
            if (sp->cf_failed) {
                errno = EPERM;
                Debug("Cached failure to stat\n");
                return -1;
            }

            if ((strcmp(stattype,"link") == 0) && (sp->cf_lmode != 0)) {
                statbuf->st_mode  = sp->cf_lmode;
            } else {
                statbuf->st_mode  = sp->cf_mode;
            }

            statbuf->st_uid   = sp->cf_uid;
            statbuf->st_gid   = sp->cf_gid;
            statbuf->st_size  = sp->cf_size;
            statbuf->st_atime = sp->cf_atime;
            statbuf->st_mtime = sp->cf_mtime;
            statbuf->st_ctime = sp->cf_ctime;
            statbuf->st_ino   = sp->cf_ino;
            statbuf->st_nlink = sp->cf_nlink;

            Debug("Found in cache\n");
            return true;
        }
    }

    Debug("Did not find in cache\n");
    return false;
}

/* ----------------------------------------------------------------- */

void
CacheData(struct cfstat *data, struct Image *ip)
{
    struct cfstat *sp;

    if ((sp = (struct cfstat *) malloc(sizeof(struct cfstat))) == NULL) {
        CfLog(cferror,"Memory allocation faliure in CacheData()\n","");
        return;
    }

    bcopy(data,sp,sizeof(struct cfstat));

    sp->next = ip->cache;
    ip->cache = sp;
}

/* ----------------------------------------------------------------- */

void
FlushToEnd(int sd, int toget)
{
    int i;
    char buffer[2];

    snprintf(g_output,CF_BUFSIZE*2,"Flushing rest of file...%d bytes\n",toget);
    CfLog(cfinform,g_output,"");

    for (i = 0; i < toget; i++) {
        recv(sd,buffer,1,0);  /* flush to end of current file */
    }
}

/* ----------------------------------------------------------------- */

struct cfagent_connection *
NewAgentConn(void)
{
    struct cfagent_connection *ap;

    if ((ap = (struct cfagent_connection *)malloc(sizeof(struct cfagent_connection))) == NULL) {
        return NULL;
    }

    Debug("New server connection...\n");
    ap->sd = CF_NOT_CONNECTED;
    ap->family = AF_INET;
    ap->trust = false;
    ap->localip[0] = '\0';
    ap->remoteip[0] = '\0';
    ap->session_key = NULL;
    ap->error = false;
    return ap;
};

/* ----------------------------------------------------------------- */

void
DeleteAgentConn(struct cfagent_connection *ap)
{
    if (ap->session_key != NULL) {
        free(ap->session_key);
    }

    free(ap);
    ap = NULL;
}
