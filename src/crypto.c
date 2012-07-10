/*
 * $Id: crypto.c 699 2004-05-21 18:39:16Z skaar $
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
#include "../pub/global.h"

/* ----------------------------------------------------------------- */

void
RandomSeed(void)
{
    static unsigned char digest[EVP_MAX_MD_SIZE+1];
    struct stat statbuf;

    /* Use the system database as the entropy source for random numbers */

    Debug("RandomSeed() work directory is %s\n", g_vlogdir);

    snprintf(g_vbuff, CF_BUFSIZE, "%s/state/randseed", WORKDIR);

    if (stat(g_vbuff, &statbuf) == -1) {
        snprintf(g_avdb, CF_BUFSIZE, "%s/state/%s", WORKDIR, CF_AVDB_FILE);
    } else {
        strcpy(g_avdb, g_vbuff);
    }

    Verbose("Looking for a source of entropy in %s\n", g_avdb);

    if (!RAND_load_file(g_avdb, -1)) {
        snprintf(g_output, CF_BUFSIZE,
                "Could not read sufficient randomness from %s\n", g_avdb);
        CfLog(cfverbose, g_output, "");
    }

    while (!RAND_status()) {
        MD5Random(digest);
        RAND_seed((void *)digest, 16);
    }
}

/* ----------------------------------------------------------------- */

void
LoadSecretKeys(void)
{
    FILE *fp;
    static char *passphrase = "Cfengine passphrase";
    unsigned long err;

    if ((fp = fopen(g_cfprivkeyfile, "r")) == NULL) {
        snprintf(g_output,CF_BUFSIZE,
                "Couldn't find a private key (%s) - use cfkey to get one",
                g_cfprivkeyfile);
        CfLog(cferror, g_output, "open");
        return;
    }

    if ((PRIVKEY = PEM_read_RSAPrivateKey(fp,(RSA **)NULL,
                    NULL, passphrase)) == NULL) {
        err = ERR_get_error();
        snprintf(g_output, CF_BUFSIZE,
                "Error reading Private Key = %s\n",
                ERR_reason_error_string(err));
        CfLog(cferror, g_output, "");
        PRIVKEY = NULL;
        fclose(fp);
        return;
    }

    fclose(fp);

    Verbose("Loaded %s\n", g_cfprivkeyfile);

    if ((fp = fopen(g_cfpubkeyfile, "r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE,
                "Couldn't find a public key (%s) - use cfkey to get one",
                g_cfpubkeyfile);
        CfLog(cferror, g_output, "fopen");
        return;
    }

    if ((PUBKEY = PEM_read_RSAPublicKey(fp, NULL, NULL, passphrase)) == NULL) {
        err = ERR_get_error();
        snprintf(g_output, CF_BUFSIZE, "Error reading Private Key = %s\n",
                ERR_reason_error_string(err));
        CfLog(cferror, g_output, "");
        PUBKEY = NULL;
        fclose(fp);
        return;
    }

    Verbose("Loaded %s\n", g_cfpubkeyfile);
    fclose(fp);

    if (BN_num_bits(PUBKEY->e) < 2 || !BN_is_odd(PUBKEY->e)) {
        FatalError("RSA Exponent too small or not odd");
    }

}

/* ----------------------------------------------------------------- */

RSA *
HavePublicKey(char *name)
{
    char *sp;
    char filename[CF_BUFSIZE];
    struct stat statbuf;
    static char *passphrase = "public";
    unsigned long err;
    FILE *fp;
    RSA *newkey = NULL;

    Debug("Havekey(%s)\n", name);

    if (!IsPrivileged()) {
        Verbose("\n(Non privileged user...)\n\n");

        if ((sp = getenv("HOME")) == NULL) {
            FatalError("You do not have a HOME variable pointing "
                    "to your home directory");
        }
        snprintf(filename, CF_BUFSIZE, "%s/.cfng/keys/%s.pub", sp, name);
    } else {
        snprintf(filename, CF_BUFSIZE, "%s/keys/%s.pub", WORKDIR, name);
    }

    if (stat(filename, &statbuf) == -1) {
        Debug("Did not have key %s\n", name);
        return NULL;
    } else {
        if ((fp = fopen(filename, "r")) == NULL) {
            snprintf(g_output, CF_BUFSIZE,
                    "Couldn't find a public key (%s) - use cfkey to get one",
                    filename);
            CfLog(cferror, g_output, "open");
            return NULL;
        }

        if ((newkey = PEM_read_RSAPublicKey(fp, 
                        NULL, NULL, passphrase)) == NULL) {
            err = ERR_get_error();
            snprintf(g_output, CF_BUFSIZE, "Error reading Private Key = %s\n",
                    ERR_reason_error_string(err));
            CfLog(cferror, g_output, "");
            fclose(fp);
            return NULL;
        }

        Verbose("Loaded %s\n", filename);
        fclose(fp);

        if (BN_num_bits(newkey->e) < 2 || !BN_is_odd(newkey->e)) {
            FatalError("RSA Exponent too small or not odd");
        }

        return newkey;
    }
}

/* ----------------------------------------------------------------- */

void
SavePublicKey(char *name, RSA *key)
{
    char *sp;
    char filename[CF_BUFSIZE];
    struct stat statbuf;
    FILE *fp;
    int err;

    if (!IsPrivileged()) {
        Verbose("\n(Non privileged user...)\n\n");

        if ((sp = getenv("HOME")) == NULL) {
            FatalError("You do not have a HOME variable pointing to "
                    "your home directory");
        }
        snprintf(filename, CF_BUFSIZE, "%s/.cfng/keys/%s.pub", sp, name);
    } else {
        snprintf(filename, CF_BUFSIZE, "%s/keys/%s.pub", WORKDIR, name);
    }

    if (stat(filename, &statbuf) != -1) {
        return;
    }

    Verbose("Saving public key %s\n", filename);

    if ((fp = fopen(filename, "w")) == NULL ) {
        snprintf(g_output, CF_BUFSIZE, "Unable to write a public key %s",
                filename);
        CfLog(cferror, g_output, "fopen");
        return;
    }

    if (!PEM_write_RSAPublicKey(fp, key)) {
        err = ERR_get_error();
        snprintf(g_output, CF_BUFSIZE, "Error saving public key %s = %s\n",
                filename, ERR_reason_error_string(err));
        CfLog(cferror, g_output, "");
    }

    fclose(fp);
}

/* ----------------------------------------------------------------- */

void
DeletePublicKey(char *name)
{
    char *sp;
    char filename[CF_BUFSIZE];
    int err;

    if (!IsPrivileged()) {
        Verbose("\n(Non privileged user...)\n\n");

        if ((sp = getenv("HOME")) == NULL) {
            FatalError("You do not have a HOME variable pointing "
                    "to your home directory");
        }
        snprintf(filename, CF_BUFSIZE, "%s/.cfng/keys/%s.pub", sp, name);
    } else {
        snprintf(filename, CF_BUFSIZE, "%s/keys/%s.pub", WORKDIR, name);
    }
    unlink(filename);
}

/* ----------------------------------------------------------------- */

/* 
 * Make a decent random number by crunching some system states & garbage
 * through MD5. We can use this as a seed for pseudo random generator
 */
void
MD5Random(unsigned char digest[EVP_MAX_MD_SIZE+1])
{
    unsigned char buffer[CF_BUFSIZE];
    char pscomm[CF_MAXLINKSIZE];
    char uninitbuffer[100];
    int md_len;
    const EVP_MD *md;
    EVP_MD_CTX context;
    FILE *pp;

    Verbose("Looking for a random number seed...\n");

    md = EVP_get_digestbyname("md5");
    EVP_DigestInit(&context, md);

    Verbose("...\n");

    snprintf(buffer, CF_BUFSIZE, "%d%d%25s", (int)g_cfstarttime,
            (int)digest, g_vfqname);

    EVP_DigestUpdate(&context, buffer, CF_BUFSIZE);

    snprintf(pscomm, CF_BUFSIZE, "%s %s",
            g_vpscomm[g_vsystemhardclass], g_vpsopts[g_vsystemhardclass]);

    if ((pp = cfpopen(pscomm, "r")) == NULL) {
        snprintf(g_output, CF_BUFSIZE,
                "Couldn't open the process list with command %s\n", pscomm);
        CfLog(cferror, g_output, "popen");
    }

    while (!feof(pp)) {
        ReadLine(buffer, CF_BUFSIZE, pp);
        EVP_DigestUpdate(&context, buffer, CF_BUFSIZE);
    }

    uninitbuffer[99] = '\0';
    snprintf(buffer, CF_BUFSIZE-1, "%ld %s", time(NULL), uninitbuffer);
    EVP_DigestUpdate(&context, buffer, CF_BUFSIZE);

    cfpclose(pp);

    EVP_DigestFinal(&context, digest, &md_len);
}

/* ----------------------------------------------------------------- */

void
GenerateRandomSessionKey(void)
{
    BIGNUM *bp;

    /* Hardcode blowfish for now - it's fast */

    bp = BN_new();
    BN_rand(bp, 16, 0, 0);
    g_conn->session_key = (unsigned char *)bp;
}

/* ----------------------------------------------------------------- */

int
EncryptString(char *in, char *out, unsigned char *key, int plainlen)
{
    int cipherlen, tmplen;
    unsigned char iv[8] = {1,2,3,4,5,6,7,8};
    EVP_CIPHER_CTX ctx;

    EVP_CIPHER_CTX_init(&ctx);
    EVP_EncryptInit(&ctx, EVP_bf_cbc(), key, iv);

    if (!EVP_EncryptUpdate(&ctx, out, &cipherlen, in, plainlen)) {
        return -1;
    }

    if (!EVP_EncryptFinal(&ctx, out+cipherlen, &tmplen)) {
        return -1;
    }

    cipherlen += tmplen;
    EVP_CIPHER_CTX_cleanup(&ctx);
    return cipherlen;
}

/* ----------------------------------------------------------------- */

int
DecryptString(char *in, char *out, unsigned char *key, int cipherlen)
{
    int plainlen, tmplen;
    unsigned char iv[8] = {1,2,3,4,5,6,7,8};
    EVP_CIPHER_CTX ctx;

    EVP_CIPHER_CTX_init(&ctx);
    EVP_DecryptInit(&ctx, EVP_bf_cbc(), key, iv);

    if (!EVP_DecryptUpdate(&ctx, out, &plainlen, in, cipherlen)) {
        return -1;
    }

    if (!EVP_DecryptFinal(&ctx, out+plainlen, &tmplen)) {
        return -1;
    }

    plainlen += tmplen;

    EVP_CIPHER_CTX_cleanup(&ctx);
    return plainlen;
}
