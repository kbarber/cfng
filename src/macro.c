/*
 * $Id: macro.c 748 2004-05-25 14:47:10Z skaar $
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
 * Macro substitution based on g_hash-table
 */

/* ----------------------------------------------------------------- */

void
SetContext(char *id)
{
    InstallObject(id);
    Verbose("\n$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n");
    Verbose(" * (Changing context state to: %s) *", id);
    Verbose("\n$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$\n\n");
    strncpy(g_contextid, id, 31);
}

/* ----------------------------------------------------------------- */

int
ScopeIsMethod(void)
{
    if (strcmp(g_contextid, "private-method") == 0) {
        return true;
    } else {
        return false;
    }
}

/* ----------------------------------------------------------------- */

void
InitHashTable(char **table)
{
    int i;

    for (i = 0; i < CF_HASHTABLESIZE; i++) {
        table[i] = NULL;
    }
}

/* ----------------------------------------------------------------- */

void PrintHashTable(char **table)
{
    int i;

    for (i = 0; i < CF_HASHTABLESIZE; i++) {
        if (table[i] != NULL) {
            printf ("   %d : %s\n", i, table[i]);
        }
    }
}

/* ----------------------------------------------------------------- */

int
Hash(char *name)
{
    int i, slot = 0;

    for (i = 0; name[i] != '\0'; i++) {
        slot = (CF_MACROALPHABET * slot + name[i]) % CF_HASHTABLESIZE;
    }

    return slot;
}

/* ----------------------------------------------------------------- */

int
ElfHash(char *key)
{
    unsigned int h = 0;
    unsigned int g;

    while (*key) {
        h = (h << 4) + *key++;

        g = h & 0xF0000000;         /* Assumes int is 32 bit */

        if (g) {
            h ^= g >> 24;
        }

        h &= ~g;
    }

    return (h % CF_HASHTABLESIZE);
}

/* ----------------------------------------------------------------- */

void
AddMacroValue(char *scope, char *name, char *value)
{
    char *sp; 
    char buffer[CF_BUFSIZE],exp[CF_EXPANDSIZE];
    struct cfObject *ptr;
    int slot;

    Debug("AddMacroValue(%s.%s=%s)\n", scope, name, value);

    if (name == NULL || value == NULL || scope == NULL) {
        yyerror("Bad macro or scope");
        CfLog(cferror, "Empty macro", "");
    }

    if (strlen(name) > CF_MAXVARSIZE) {
        yyerror("macro name too long");
        return;
    }

    ExpandVarstring(value, exp, NULL);

    ptr = ObjectContext(scope);

    snprintf(buffer, CF_BUFSIZE, "%s=%s", name, exp);

    if ((sp = malloc(strlen(buffer)+1)) == NULL) {
        perror("malloc");
        FatalError("aborting");
    }

    strcpy(sp, buffer);

    slot = Hash(name);

    if (ptr->hashtable[slot] != 0) {
        Debug("Macro Collision!\n");
        if (CompareMacro(name, ptr->hashtable[slot]) == 0) {
            if (g_parsing && !IsItemIn(g_vredefines, name)) {

                snprintf(g_vbuff, CF_BUFSIZE, 
                        "Redefinition of macro %s=%s "
                        "(or perhaps missing quote)", name, exp);

                Warning(g_vbuff);
            }
            free(ptr->hashtable[slot]);
            ptr->hashtable[slot] = sp;
            Debug("Stored %s in context %s\n", sp, name);
            return;
        }

        while ((ptr->hashtable[++slot % CF_HASHTABLESIZE] != 0)) {
            if (slot == CF_HASHTABLESIZE-1) {
                slot = 0;
            }
            if (slot == Hash(name)) {
                FatalError("AddMacroValue - internal error #1");
            }

            if (CompareMacro(name,ptr->hashtable[slot]) == 0) {
                snprintf(g_vbuff, CF_BUFSIZE,
                        "Redefinition of macro %s=%s", name, exp);
                Warning(g_vbuff);
                free(ptr->hashtable[slot]);
                ptr->hashtable[slot] = sp;
                return;
            }
        }
    }

    ptr->hashtable[slot] = sp;

    Debug("Added Macro at hash address %d to object %s with "
            "value %s\n", slot, scope, sp);
}

/* ----------------------------------------------------------------- */

/*
 * HvB: Bas van der Vlies
 *  This function checks if the given name has the requested value:
 *    1 --> check for values on or true
 *    0 --> check for values off or false
 *  Return true if the name has the requested value.
 */
int
OptionIs(char *scope, char *name, short on)
{
    char *result;

    result = GetMacroValue(scope, name);
    if ( result == NULL) {
        return(false);
    }

    if (on) {
        if ( (strcmp(result, "on") == 0) 
                || ( strcmp(result, "true") == 0) ) {
            return(true);
        }
    } else {
        if ( (strcmp(result, "off") == 0) 
                || (strcmp(result, "false") == 0) ) {
            return(true);
        }
    }

    return(false);
}

/* ----------------------------------------------------------------- */

char *
GetMacroValue(char *scope, char *name)
{
    char *sp;
    int slot,i;
    struct cfObject *ptr = NULL;
    char objscope[CF_MAXVARSIZE], vname[CF_MAXVARSIZE];

    /* 
     * Check the class.id identity to see if this is private .. and replace
     *      g_hash[slot] by objectptr->hash[slot]
     */
    Debug("GetMacroValue(%s,%s)\n",scope,name);
    if (strstr(name, ".")) {
        objscope[0] = '\0';
        sscanf(name, "%[^.].%s", objscope, vname);
        Debug("Macro identifier %s is prefixed with an object %s\n",
                vname, objscope);
        ptr = ObjectContext(objscope);
    }

    if (ptr == NULL) {
        strcpy(vname, name);
        ptr = ObjectContext(scope);
    }

    i = slot = Hash(vname);

    if (CompareMacro(vname, ptr->hashtable[slot]) != 0) {
        while (true) {
            i++;

            if (i >= CF_HASHTABLESIZE-1) {
                i = 0;
            }

            if (CompareMacro(vname, ptr->hashtable[i]) == 0) {
                for(sp = ptr->hashtable[i]; *sp != '='; sp++) { }

                return(sp+1);
            }

            if (i == slot-1) {
                /* Look in environment if not found */
                return(getenv(vname));
            }
        }
    } else {
        for(sp = ptr->hashtable[slot]; *sp != '='; sp++) { }

        return(sp+1);
   }
}

/* ----------------------------------------------------------------- */

void
DeleteMacro(char *scope, char *id)
{
    int slot,i;
    struct cfObject *ptr;

    i = slot = Hash(id);
    ptr = ObjectContext(scope);

    if (CompareMacro(id, ptr->hashtable[slot]) != 0) {
        while (true) {
            i++;

            if (i == slot) {
                Debug("No macro matched\n");
                break;
            }

            if (i >= CF_HASHTABLESIZE-1) {
                i = 0;
            }

            if (CompareMacro(id,ptr->hashtable[i]) == 0) {
                free(ptr->hashtable[i]);
                ptr->hashtable[i] = NULL;
            }
        }
    } else {
        free(ptr->hashtable[i]);
        ptr->hashtable[i] = NULL;
    }
}

/* ----------------------------------------------------------------- */

int
CompareMacro(char *name, char *macro)
{
    char buffer[CF_BUFSIZE];

    if (macro == NULL || name == NULL) {
        return 1;
    }

    sscanf(macro, "%[^=]", buffer);
    return(strcmp(buffer, name));
}

/* ----------------------------------------------------------------- */

void
DeleteMacros(char *scope)
{
    int i;
    struct cfObject *ptr;

    ptr = ObjectContext(scope);

    for (i = 0; i < CF_HASHTABLESIZE; i++) {
        if (ptr->hashtable[i] != NULL) {
            free(ptr->hashtable[i]);
            ptr->hashtable[i] = NULL;
        }
    }
}

/* ----------------------------------------------------------------- */

struct cfObject *
ObjectContext(char *scope)
{
    struct cfObject *cp = NULL;

    for (cp = g_vobj; cp != NULL; cp=cp->next) {
        if (strcmp(cp->scope,scope) == 0) {
            break;
        }
    }

    return cp;
}
