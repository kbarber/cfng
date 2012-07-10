/*
 * $Id: checksums.c 682 2004-05-20 22:46:52Z skaar $
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
 * Checksums
 */


#include "cf.defs.h"
#include "cf.extern.h"

/* ----------------------------------------------------------------- */

int
CompareCheckSums(char *file1, char *file2, struct Image *ip,
        struct stat *sstat, struct stat *dstat)
{
    static unsigned char digest1[EVP_MAX_MD_SIZE+1], digest2[EVP_MAX_MD_SIZE+1];
    int i;

    Debug("CompareCheckSums(%s,%s)\n",file1,file2);

    if (sstat->st_size != dstat->st_size) {
        Debug("File sizes differ, no need to compute checksum\n");
        return true;
    }

    Debug2("Compare checksums on %s:%s & %s\n",ip->server,file1,file2);

    if (strcmp(ip->server,"localhost") == 0) {
        ChecksumFile(file1,digest1,'m');
        ChecksumFile(file2,digest2,'m');

        for (i = 0; i < EVP_MAX_MD_SIZE; i++) {
            if (digest1[i] != digest2[i]) {
                Verbose("Checksum mismatch...\n");
                return true;
            }
        }

        Debug("Files were identical\n");
        return false;  /* only if files are identical */
    } else {
        return CompareMD5Net(file1,file2,ip); /* client.c */
    }
}

/* ----------------------------------------------------------------- */

/* See the md5 algorithms in pub-lib/md5.c */
/* file 1 is source                        */

int CompareBinarySums(char *file1, char *file2, struct Image *ip,
        struct stat *sstat, struct stat *dstat)
{
    int fd1, fd2,bytes1,bytes2;
    char buff1[BUFSIZ],buff2[BUFSIZ];

    Debug("CompareBinarySums(%s,%s)\n",file1,file2);

    if (sstat->st_size != dstat->st_size) {
        Debug("File sizes differ, no need to compute checksum\n");
        return true;
    }

    Debug2("Compare binary sums on %s:%s & %s\n",ip->server,file1,file2);

    if (strcmp(ip->server,"localhost") == 0) {
        fd1 = open(file1, O_RDONLY|O_BINARY, 0400);
        fd2 = open(file2, O_RDONLY|O_BINARY, 0400);

        do {
            bytes1 = read(fd1, buff1, BUFSIZ);
            bytes2 = read(fd2, buff2, BUFSIZ);

            if ((bytes1 != bytes2) || (memcmp(buff1, buff2, bytes1) != 0)) {
                Verbose("Binary Comparison mismatch...\n");
                close(fd2);
                close(fd1);
                return true;
            }
        }
        while (bytes1 > 0);

        close(fd2);
        close(fd1);

        return false;  /* only if files are identical */

    } else {
        Debug("Using network md5 checksum instead\n");
        return CompareMD5Net(file1,file2,ip); /* client.c */
    }
}

/* ----------------------------------------------------------------- */

void
ChecksumFile(char *filename, unsigned char digest[EVP_MAX_MD_SIZE+1],
        char type)
{
    FILE *file;
    EVP_MD_CTX context;
    int len, md_len;
    unsigned char buffer[1024];
    const EVP_MD *md = NULL;

    Debug2("ChecksumFile(%c,%s)\n",type,filename);

    if ((file = fopen (filename, "rb")) == NULL) {
        printf ("%s can't be opened\n", filename);
    } else {
        switch (type) {
        case 's':
            md = EVP_get_digestbyname("sha");
            break;
        case 'm':
            md = EVP_get_digestbyname("md5");
            break;
        default:
            FatalError("Software failure in ChecksumFile");
        }

        EVP_DigestInit(&context,md);

        while (len = fread(buffer,1,1024,file)) {
            EVP_DigestUpdate(&context,buffer,len);
        }

        EVP_DigestFinal(&context,digest,&md_len);

        /* Digest length stored in md_len */
        fclose (file);
    }
}

/* ----------------------------------------------------------------- */

void
ChecksumList(struct Item *list, unsigned char digest[EVP_MAX_MD_SIZE+1],
        char type)
{
    struct Item *ip;
    EVP_MD_CTX context;
    int md_len;
    const EVP_MD *md = NULL;

    Debug2("ChecksumList(%c)\n",type);

    switch (type) {
    case 's':
        md = EVP_get_digestbyname("sha");
        break;
    case 'm':
        md = EVP_get_digestbyname("md5");
        break;
    default:
        FatalError("Software failure in ChecksumList");
    }

    EVP_DigestInit(&context,md);

    for (ip = list; ip != NULL; ip=ip->next) {
        Debug(" digesting %s\n",ip->name);
        EVP_DigestUpdate(&context,ip->name,strlen(ip->name));
    }

    EVP_DigestFinal(&context,digest,&md_len);

}

/* ----------------------------------------------------------------- */

void
ChecksumString(char *buffer, int len, unsigned char digest[EVP_MAX_MD_SIZE+1],
        char type)
{
    EVP_MD_CTX context;
    const EVP_MD *md = NULL;
    int md_len;

    Debug2("ChecksumString(%c)\n",type);

    switch (type) {
    case 's':
        md = EVP_get_digestbyname("sha");
        break;
    case 'm':
        md = EVP_get_digestbyname("md5");
        break;
    default:
        FatalError("Software failure in ChecksumFile");
    }

    EVP_DigestInit(&context,md);
    EVP_DigestUpdate(&context,(unsigned char*)buffer,len);
    EVP_DigestFinal(&context,digest,&md_len);
}

/* ----------------------------------------------------------------- */

int
ChecksumsMatch(unsigned char digest1[EVP_MAX_MD_SIZE+1],
        unsigned char digest2[EVP_MAX_MD_SIZE+1], char type)
{
    int i,size = EVP_MAX_MD_SIZE;

    /* Size of message digests */
    switch(type) {
    case 'm':
        size = CF_MD5_LEN;
        break;
    case 's':
        size = CF_SHA_LEN;
        break;
    }

    for (i = 0; i < size; i++) {
        if (digest1[i] != digest2[i]) {
            return false;
        }
    }

    return true;
}
