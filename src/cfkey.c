/*
 * $Id: cfkey.c 743 2004-05-23 07:27:32Z skaar $
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

char g_cflock[CF_BUFSIZE];

void Initialize(void);
int RecursiveTidySpecialArea(char *name, struct Tidy *tp, 
        int maxrecurse, struct stat *sb);

/* ----------------------------------------------------------------- */

int
main(void)
{
    unsigned long err;
    RSA *pair;
    FILE *fp;
    struct stat statbuf;
    int fd;
    static char *passphrase = "Cfengine passphrase";
    EVP_CIPHER *cipher = EVP_des_ede3_cbc();

    Initialize();

    if (stat(g_cfprivkeyfile,&statbuf) != -1) {
    printf("A key file already exists at %s.\n",g_cfprivkeyfile);
    return 1;
    }

    if (stat(g_cfpubkeyfile,&statbuf) != -1) {
        printf("A key file already exists at %s.\n",g_cfpubkeyfile);
        return 1;
    }

    printf("Making a key pair for cfng, please wait, "
            "this could take a minute...\n");

    pair = RSA_generate_key(2048,35,NULL,NULL);

    if (pair == NULL) {
        err = ERR_get_error();
        printf("Error = %s\n",ERR_reason_error_string(err));
        return 1;
    }

    if (g_verbose) {
        RSA_print_fp(stdout,pair,0);
    }

    fd = open(g_cfprivkeyfile,O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0) {
        printf("Ppen %s failed: %s.",g_cfprivkeyfile,strerror(errno));
        return 0;
    }

    if ((fp = fdopen(fd, "w")) == NULL ) {
        printf("fdopen %s failed: %s.",g_cfprivkeyfile, strerror(errno));
        close(fd);
        return 0;
    }

    printf("Writing private key to %s\n",g_cfprivkeyfile);

    if (!PEM_write_RSAPrivateKey(fp,pair,cipher,passphrase,
                strlen(passphrase),NULL,NULL)) {

        err = ERR_get_error();
        printf("Error = %s\n",ERR_reason_error_string(err));
        return 1;
    }

    fclose(fp);

    fd = open(g_cfpubkeyfile,O_WRONLY | O_CREAT | O_TRUNC, 0600);

    if (fd < 0) {
        printf("open %s failed: %s.",g_cfpubkeyfile,strerror(errno));
        return 0;
    }

    if ((fp = fdopen(fd, "w")) == NULL ) {
        printf("fdopen %s failed: %s.",g_cfpubkeyfile, strerror(errno));
        close(fd);
        return 0;
    }

    printf("Writing public key to %s\n",g_cfpubkeyfile);

    if(!PEM_write_RSAPublicKey(fp,pair)) {
        err = ERR_get_error();
        printf("Error = %s\n",ERR_reason_error_string(err));
        return 1;
    }

    fclose(fp);

    snprintf(g_vbuff,CF_BUFSIZE,"%s/state/randseed",WORKDIR);
    RAND_write_file(g_vbuff);
    chmod(g_vbuff,0644);
    return 0;
}


/*
 * --------------------------------------------------------------------
 * Level 1
 * --------------------------------------------------------------------
 */

void
Initialize(void)
{
    umask(077);

    sprintf(g_vbuff,"%s/logs",WORKDIR);
    strncpy(g_vlogdir,g_vbuff,CF_BUFSIZE-1);

    sprintf(g_vbuff,"%s/state",WORKDIR);
    strncpy(g_vlockdir,g_vbuff,CF_BUFSIZE-1);

    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();

    CheckWorkDirectories();
    RandomSeed();
}

/*
 * --------------------------------------------------------------------
 * linker triks
 * --------------------------------------------------------------------
 */

void
AddMultipleClasses(char *classlist)
{
}

void
ReleaseCurrentLock(void)
{
}

int
RecursiveTidySpecialArea(char *name, struct Tidy *tp,
        int maxrecurse, struct stat *sb)
{
    return true;
}

char *
GetMacroValue(char *s, char *sp)
{
    return NULL;
}
