%{
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

/*
 * Parser for cfng
 */

#include <stdio.h>
#include "cf.defs.h"
#include "cf.extern.h"

extern char *yytext;

%}

%token LVALUE ID VAROBJ LBRACK RBRACK CONTROL GROUPS
%token ARROW EQUALS EDITFILES QSTRING RVALUE BCLASS
%token LBRACE RBRACE PARSECLASS LARROW OPTION FUNCTION
%token ACL ADMIT DENY FILTERS STRATEGIES ACTIONTYPE ACCESSOBJ



%%

specification:       { yyerror("Warning: invalid statement"); }
                     | statements;

statements:            statement
                     | statements statement;

statement:             CONTROL controllist
                     | CONTROL
                     | GROUPS controllist
                     | GROUPS
                     | ACTIONTYPE classlist
                     | ACTIONTYPE
                     | EDITFILES
                     | EDITFILES objects
                     | ACL objects
                     | ACL
                     | FILTERS objects
                     | FILTERS
                     | STRATEGIES objects
                     | STRATEGIES
                     | ADMIT classaccesslist
                     | ADMIT
                     | DENY classaccesslist
                     | DENY;

controllist:           declarations
                     | PARSECLASS declarations
                     | PARSECLASS
                     | controllist PARSECLASS
                     | controllist PARSECLASS declarations; 

declarations:          declaration
                     | declarations declaration;

classlist:             entries
                     | PARSECLASS entries
                     | PARSECLASS
                     | classlist PARSECLASS
                     | classlist PARSECLASS entries;

classaccesslist:       accessentries
                     | PARSECLASS accessentries
                     | PARSECLASS
                     | classaccesslist PARSECLASS
                     | classaccesslist PARSECLASS accessentries;

declaration:           LVALUE EQUALS bracketlist;

bracketlist:           LBRACK rvalues RBRACK;

rvalues:               RVALUE
                     | FUNCTION
                     | rvalues FUNCTION
                     | rvalues RVALUE;

entries:               entry
                     | entries entry;

accessentries:         accessentry
                     | accessentries accessentry;

entry:                 FUNCTION
                     | FUNCTION options
                     | VAROBJ
                     | VAROBJ options
                     | VAROBJ ARROW VAROBJ options
                     | VAROBJ ARROW VAROBJ
                     | VAROBJ LARROW VAROBJ options
                     | VAROBJ LARROW VAROBJ
                     | QSTRING
                     | QSTRING options;

accessentry:           ACCESSOBJ
                     | ACCESSOBJ options;

options:               options OPTION
                     | OPTION;

objects:               objectbrackets
                     | PARSECLASS
                     | PARSECLASS objectbrackets
                     | objects PARSECLASS
                     | objects PARSECLASS objectbrackets;

objectbrackets:        objectbracket
                     | objectbrackets objectbracket;

objectbracket:         LBRACE VAROBJ objlist RBRACE
                     | LBRACE ID objlist RBRACE;

objlist:               obj
                     | objlist obj;

obj:                   BCLASS QSTRING
                     | ID QSTRING
                     | ID 
                     | VAROBJ;

%%

/* ----------------------------------------------------------------- */ 

void 
yyerror(char *s) {
    fprintf (stderr, "cf:%s:%s:%d: %s \n",
        g_vprefix,g_vcurrentfile,g_linenumber,s);

    g_errorcount++;

    if (g_errorcount > 10) {
        FatalError("Too many errors");
    }
}

/* ----------------------------------------------------------------- */
