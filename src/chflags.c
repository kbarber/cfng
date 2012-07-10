/*
 * $Id: chflags.c 682 2004-05-20 22:46:52Z skaar $
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

/*
 * Toolkit: Flagstring
 * Contributed to cfengine by Andreas Klussman 
 * adapted from FreeBSD /usr/src/bin/ls/stat_flags.c
 */


#define	TEST(a, b, f) {							\
	if (!memcmp(a, b, sizeof(b))) {					\
		if (clear) {						\
			if (minusmask)					\
				*minusmask |= (f);			\
		} else if (plusmask)					\
			*plusmask |= (f);				\
		break;							\
	}								\
}

/* ----------------------------------------------------------------- */

void
ParseFlagString(char *flagstring, u_long *plusmask, u_long *minusmask)
{
    char *sp, *next;
    int clear;
    u_long value;

    value = 0;

    if (flagstring == NULL) {
        return;
    }

    Debug1("ParseFlagString(%s)\n",flagstring);

    for ( sp = flagstring; sp && *sp; sp = next ) {
        if ( next = strchr( sp, ',' )) {
            *next ++ = '\0';
        }

        clear = 0;

        if ( sp[0] == 'n' && sp[1] == 'o' ) {
            clear = 1;
            sp += 2;
        }

        switch ( *sp ) {
        case 'a':
            TEST(sp, "arch", SF_ARCHIVED);
            TEST(sp, "archived", SF_ARCHIVED);
            goto Error;
        case 'd':
            clear = !clear;
            TEST(sp, "dump", UF_NODUMP);
            goto Error;
        case 'o':
            TEST(sp, "opaque", UF_OPAQUE);
            goto Error;
        case 's':
            TEST(sp, "sappnd", SF_APPEND);
            TEST(sp, "sappend", SF_APPEND);
            TEST(sp, "schg", SF_IMMUTABLE);
            TEST(sp, "schange", SF_IMMUTABLE);
            TEST(sp, "simmutable", SF_IMMUTABLE);
            TEST(sp, "sunlnk", SF_NOUNLINK);
            TEST(sp, "sunlink", SF_NOUNLINK);
            goto Error;
        case 'u':
            TEST(sp, "uappnd", UF_APPEND);
            TEST(sp, "uappend", UF_APPEND);
            TEST(sp, "uchg", UF_IMMUTABLE);
            TEST(sp, "uchange", UF_IMMUTABLE);
            TEST(sp, "uimmutable", UF_IMMUTABLE);
            TEST(sp, "uunlnk", UF_NOUNLINK);
            TEST(sp, "uunlink", UF_NOUNLINK);
            goto Error;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
            sscanf(sp,"%o",&value);
            *plusmask = value;
            *minusmask = ~value & CHFLAGS_MASK;
            break;

        case '\0':
            break;

        Error:
        default:
            printf( "Invalid flag string '%s'\n", sp );
            yyerror ("Invalid flag string");
            return;
        }
    }
    Debug1("ParseFlagString:[PLUS=%o][MINUS=%o]\n",*plusmask,*minusmask);
}


/*
CHFLAGS(1)              FreeBSD General Commands Manual             CHFLAGS(1)

NAME
     chflags - change file flags

SYNOPSIS
     chflags [-R [-H | -L | -P]] flags file ...

DESCRIPTION
     The chflags utility modifies the file flags of the listed files as speci-
     fied by the flags operand.

     The options are as follows:

     -H      If the -R option is specified, symbolic links on the command line
             are followed.  (Symbolic links encountered in the tree traversal
             are not followed.)

     -L      If the -R option is specified, all symbolic links are followed.

     -P      If the -R option is specified, no symbolic links are followed.

     -R      Change the file flags for the file hierarchies rooted in the
             files instead of just the files themselves.

     Flags are a comma separated list of keywords.  The following keywords are
     currently defined:

           arch    set the archived flag (super-user only)
           dump    set the dump flag
           sappnd  set the system append-only flag (super-user only)
           schg    set the system immutable flag (super-user only)
           sunlnk  set the system undeletable flag (super-user only)
           uappnd  set the user append-only flag (owner or super-user only)
           uchg    set the user immutable flag (owner or super-user only)
           uunlnk  set the user undeletable flag (owner or super-user only)
           archived, sappend, schange, simmutable, uappend, uchange, uimmutable,
           sunlink, uunlink
                   aliases for the above

     Putting the letters ``no'' before an option causes the flag to be turned
     off.  For example:

           nodump  the file should never be dumped

     Symbolic links do not have flags, so unless the -H or -L option is set,
     chflags on a symbolic link always succeeds and has no effect.  The -H, -L
     and -P options are ignored unless the -R option is specified.  In addi-
     tion, these options override each other and the command's actions are de-
     termined by the last one specified.

     You can use "ls -lo" to see the flags of existing files.

     The chflags utility exits 0 on success, and >0 if an error occurs.

SEE ALSO
     ls(1),  chflags(2),  stat(2),  fts(3),  symlink(7)

HISTORY
     The chflags command first appeared in 4.4BSD.

BSD                             March 31, 1994                               1
*/
