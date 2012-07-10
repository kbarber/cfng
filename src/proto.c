/*
 * $Id: proto.c 682 2004-05-20 22:46:52Z skaar $
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
 * File Image copying
 * client part for remote copying
 */

#include "cf.defs.h"
#include "cf.extern.h"

/* ----------------------------------------------------------------- */

int
BadProtoReply(char *buf)
{
    return (strncmp(buf,"BAD:",4) == 0);
}

/* ----------------------------------------------------------------- */

int
OKProtoReply(char *buf)
{
    return(strncmp(buf,"OK:",3) == 0);
}

/* ----------------------------------------------------------------- */

int FailedProtoReply(char *buf)
{
    return(strncmp(buf,CF_FAILEDSTR,strlen(CF_FAILEDSTR)) == 0);
}

/* ----------------------------------------------------------------- */

int
IdentifyForVerification(int sd,char *localip,int family)
{
    char sendbuff[CF_BUFSIZE],dnsname[CF_BUFSIZE];
    struct sockaddr_in myaddr;
    struct in_addr *iaddr;
    struct hostent *hp;
    int len,err;
    struct passwd *user_ptr;
    char *uname;

    memset(sendbuff,0,CF_BUFSIZE);
    memset(dnsname,0,CF_BUFSIZE);

    if (strcmp(g_vdomain,CF_START_DOMAIN) == 0) {
        CfLog(cferror,"Undefined domain name","");
        return false;
    }

    /* 
     * First we need to find out the IP address and DNS name of the
     * socket we are sending from. This is not necessarily the same as
     * g_vfqname if the machine has a different uname from its IP name
     * (!) This can happen on stupidly set up machines or on hosts with
     * multiple interfaces, with different names on each interface ...
     */

    switch (family) {
    case AF_INET:
        len = sizeof(struct sockaddr_in);
        break;
#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    case AF_INET6:
        len = sizeof(struct sockaddr_in6);
        break;
#endif
    default:
        CfLog(cferror,"Software error in IdentifyForVerification","");
    }

    if (getsockname(sd,(struct sockaddr *)&myaddr,&len) == -1) {
        CfLog(cferror,"Couldn't get socket address\n","getsockname");
        return false;
    }

    snprintf(localip,CF_MAX_IP_LEN-1,"%s",
            sockaddr_ntop((struct sockaddr *)&myaddr));

    Debug("Identifying this agent as %s i.e. %s, with signature %d\n",
            localip,g_vfqname,g_cfsignature);

#if defined(HAVE_GETADDRINFO) && !defined(DARWIN)
    if ((err=getnameinfo((struct sockaddr *)&myaddr,len,dnsname,
                    CF_MAXVARSIZE,NULL,0,0)) != 0) {
        snprintf(g_output,CF_BUFSIZE,"Couldn't look up address v6 for %s: %s\n",
                dnsname,gai_strerror(err));
        CfLog(cferror,g_output,"");
        return false;
    }
#else

    iaddr = &(myaddr.sin_addr);
    hp = gethostbyaddr((void *)iaddr,sizeof(myaddr.sin_addr),family);

    if ((hp == NULL) || (hp->h_name == NULL)) {
        CfLog(cferror,"Couldn't lookup IP address\n","gethostbyaddr");
        return false;
    }

    strncpy(dnsname,hp->h_name,CF_MAXVARSIZE);

    if ((strstr(hp->h_name,".") == 0) && (strlen(g_vdomain) > 0)) {
        strcat(dnsname,".");
        strcat(dnsname,g_vdomain);
    }
#endif

    user_ptr = getpwuid(getuid());
    uname = user_ptr ? user_ptr->pw_name : "UNKNOWN";

    /* 
     * Some resolvers will not return FQNAME and missing PTR will give
     * numerical result. 
     */

    if ((strlen(g_vdomain) > 0) && !IsIPV6Address(dnsname) &&
            !strchr(dnsname,'.')) {
        Debug("Appending domain %s to %s\n",g_vdomain,dnsname);
        strcat(dnsname,".");
        strncat(dnsname,g_vdomain,CF_MAXVARSIZE/2);
    }

    /* 
     * Seems to be a bug in some resolvers that adds garbage, when it
     * just returns the input.
     */
    if (strncmp(dnsname,localip,strlen(localip)) == 0) {
        strcpy(dnsname,localip);
    }

    snprintf(sendbuff,CF_BUFSIZE-1,"CAUTH %s %s %s %d",
            localip,dnsname,uname,g_cfsignature);

    Debug("SENT:::%s\n",sendbuff);

    SendTransaction(sd,sendbuff,0,CF_DONE);
    return true;
}

/* ----------------------------------------------------------------- */

int
KeyAuthentication(struct Image *ip)
{
    char sendbuffer[CF_BUFSIZE],in[CF_BUFSIZE],*out,*decrypted_cchall;
    BIGNUM *nonce_challenge, *bn = NULL;
    unsigned long err;
    unsigned char digest[EVP_MAX_MD_SIZE];
    int encrypted_len,nonce_len = 0,len;
    char cant_trust_server, keyname[CF_BUFSIZE];
    RSA *server_pubkey = NULL;

    if (g_compatibility_mode) {
        return true;
    }

    if (PUBKEY == NULL || PRIVKEY == NULL) {
        CfLog(cferror,"No public/private key pair found\n","");
        return false;
    }

    Debug("KeyAuthentication()\n");

    /* Generate a random challenge to authenticate the server */

    nonce_challenge = BN_new();
    BN_rand(nonce_challenge,256,0,0);

    nonce_len = BN_bn2mpi(nonce_challenge,in);
    ChecksumString(in,nonce_len,digest,'m');

    /* 
     * We assume that the server bound to the remote socket is the
     * official one i.e. = root's 
     */ 

    if (OptionIs(g_contextid,"HostnameKeys",true)) {
        snprintf(keyname,CF_BUFSIZE,"root-%s",ip->server);
    } else {
        snprintf(keyname,CF_BUFSIZE,"root-%s",g_conn->remoteip);
    }

    if (server_pubkey = HavePublicKey(keyname)) {
        cant_trust_server = 'y';
        /* encrypted_len = BN_num_bytes(server_pubkey->n);*/
        encrypted_len = RSA_size(server_pubkey);
    } else {

        /* have to trust server, since we can't verify id */
        cant_trust_server = 'n';
        encrypted_len = nonce_len;
    }

    snprintf(sendbuffer,CF_BUFSIZE,"SAUTH %c %d %d",
            cant_trust_server,encrypted_len,nonce_len);

    if ((out = malloc(encrypted_len)) == NULL) {
        FatalError("memory failure");
    }

    if (server_pubkey != NULL) {
        if (RSA_public_encrypt(nonce_len,in,out,
                    server_pubkey,RSA_PKCS1_PADDING) <= 0) {
            err = ERR_get_error();
            snprintf(g_output,CF_BUFSIZE,"Public encryption failed = %s\n",
                    ERR_reason_error_string(err));
            CfLog(cferror,g_output,"");
            return false;
        }

        bcopy(out,sendbuffer+CF_RSA_PROTO_OFFSET,encrypted_len);
    } else {
        bcopy(in,sendbuffer+CF_RSA_PROTO_OFFSET,nonce_len);
    }

    /* proposition C1 - Send challenge / nonce */

    SendTransaction(g_conn->sd,sendbuffer,
            CF_RSA_PROTO_OFFSET+encrypted_len,CF_DONE);

    BN_free(bn);
    BN_free(nonce_challenge);
    free(out);

    if (g_debug||g_d2) {
        RSA_print_fp(stdout,PUBKEY,0);
    }

    /*Send the public key - we don't know if server has it */
    /* proposition C2 */

    memset(sendbuffer,0,CF_BUFSIZE);
    len = BN_bn2mpi(PUBKEY->n,sendbuffer);

    /* No need to encrypt the public key ... */
    SendTransaction(g_conn->sd,sendbuffer,len,CF_DONE);

    /* proposition C3 */
    memset(sendbuffer,0,CF_BUFSIZE);
    len = BN_bn2mpi(PUBKEY->e,sendbuffer);
    SendTransaction(g_conn->sd,sendbuffer,len,CF_DONE);

    /* check reply about public key - server can break connection here */

    /* proposition S1 */
    memset(in,0,CF_BUFSIZE);
    ReceiveTransaction(g_conn->sd,in,NULL);

    if (BadProtoReply(in)) {
        CfLog(cferror,in,"");
        return false;
    }

    /* Get challenge response - should be md5 of challenge */

    /* proposition S2 */
    memset(in,0,CF_BUFSIZE);
    ReceiveTransaction(g_conn->sd,in,NULL);

    if (!ChecksumsMatch(digest,in,'m')) {

        snprintf(g_output,CF_BUFSIZE,
                "Challenge response from server %s/%s was incorrect!",
                ip->server,g_conn->remoteip);

        CfLog(cferror,g_output,"");
        return false;
    } else {
        /* challenge reply was correct */
        if (cant_trust_server == 'y')  {

            snprintf(g_output,CF_BUFSIZE,
                    "Strong authentication of server=%s connection confirmed\n",
                    ip->server);

            CfLog(cfverbose,g_output,"");
        } else {
            if (ip->trustkey == 'y') {

                snprintf(g_output, CF_BUFSIZE, 
                        "Trusting server identity and willing "
                        "to accept key from %s=%s",
                        ip->server,g_conn->remoteip);

                CfLog(cferror,g_output,"");
            } else {

                snprintf(g_output, CF_BUFSIZE, 
                        "Not authorized to trust the server=%s's "
                        "public key (trustkey=false)\n", ip->server);

                CfLog(cferror,g_output,"");
                return false;
            }
        }
    }

    /* Receive counter challenge from server */

    Debug("Receive counter challenge from server\n");
    /* proposition S3 */
    memset(in,0,CF_BUFSIZE);
    encrypted_len = ReceiveTransaction(g_conn->sd,in,NULL);


    if ((decrypted_cchall = malloc(encrypted_len)) == NULL) {
        FatalError("memory failure");
    }

    if (RSA_private_decrypt(encrypted_len,in,decrypted_cchall,
                PRIVKEY,RSA_PKCS1_PADDING) <= 0) {
        err = ERR_get_error();
        snprintf(g_output,CF_BUFSIZE,
                "Private decrypt failed = %s, abandoning\n",
                ERR_reason_error_string(err));
        CfLog(cferror,g_output,"");
        return false;
   }

    /* proposition C4 */
    ChecksumString(decrypted_cchall,nonce_len,digest,'m');
    Debug("Replying to counter challenge with md5\n");
    SendTransaction(g_conn->sd,digest,16,CF_DONE);
    free(decrypted_cchall);

    /* If we don't have the server's public key, it will be sent */

    if (server_pubkey == NULL) {
        RSA *newkey = RSA_new();

        Debug("Collecting public key from server!\n");

        /* proposition S4 - conditional */
        if ((len = ReceiveTransaction(g_conn->sd,in,NULL)) == 0) {
            CfLog(cferror,"Protocol error in RSA authentation from IP %s\n",
                    ip->server);
            return false;
        }

        if ((newkey->n = BN_mpi2bn(in,len,NULL)) == NULL) {
            err = ERR_get_error();
            snprintf(g_output,CF_BUFSIZE,"Private decrypt failed = %s\n",
                    ERR_reason_error_string(err));
            CfLog(cferror,g_output,"");
            return false;
        }

        /* proposition S5 - conditional */
        if ((len=ReceiveTransaction(g_conn->sd,in,NULL)) == 0) {
            CfLog(cfinform,"Protocol error in RSA authentation from IP %s\n",
                    ip->server);
            return false;
        }

        if ((newkey->e = BN_mpi2bn(in,len,NULL)) == NULL) {
            err = ERR_get_error();
            snprintf(g_output,CF_BUFSIZE,"Private decrypt failed = %s\n",
                    ERR_reason_error_string(err));
            CfLog(cferror,g_output,"");
            return false;
        }

        SavePublicKey(keyname,newkey);
        RSA_free(newkey);
    } else {
        RSA_free(server_pubkey);
    }

    /* proposition C5 */
    GenerateRandomSessionKey();

    if (g_conn->session_key == NULL) {
        CfLog(cferror,"A random session key could not be established","");
        return false;
    }

    SendTransaction(g_conn->sd,g_conn->session_key,16,CF_DONE);
    return true;
}

