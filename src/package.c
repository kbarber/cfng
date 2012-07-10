/*
 * $Id: package.c 744 2004-05-23 07:53:59Z skaar $
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

/* Local prototypes that nobody else should care about... */

void ParseEVR(char * evr, const char **ep, const char **vp, const char **rp);
int rpmvercmp(const char *a, const char *b);
int xislower(int c);
int xisupper(int c);
int xisalpha(int c);
int xisalnum(int c);
int xisdigit(int c);


/* 
 * Make sure we are insensitive to the locale so everyone gets the same
 * experience from these. 
 */

int
xislower(int c)
{
    return (c >= 'a' && c <= 'z');
}

int
xisupper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

int
xisalpha(int c)
{
    return (xislower(c) || xisupper(c));
}

int
xisdigit(int c)
{
    return (c >= '0' && c <= '9');
}

int
xisalnum(int c)
{
    return (xisalpha(c) || xisdigit(c));
}

/* 
 * RPMPackageCheck: 
 *  returns: 1 - found a match
 *           0 - found no match
 */
int
RPMPackageCheck(char *package,char *version,enum cmpsense cmp)
{
    FILE *pp;
    struct Item *evrlist = NULL;
    struct Item *evr;
    char *sepA, *sepB; /* Locations of the separators in the EVR string */
    int epochA = 0; /* What is installed.  Assume 0 if we don't get one. */
    int epochB = 0; /* What we are looking for.  Assume 0 if we don't get one. */
    const char *eA = NULL; /* What is installed */
    const char *eB = NULL; /* What we are looking for */
    const char *vA = NULL; /* What is installed */
    const char *vB = NULL; /* What we are looking for */
    const char *rA = NULL; /* What is installed */
    const char *rB = NULL; /* What we are looking for */
    enum cmpsense result;
    int match = 0;

    if (GetMacroValue(g_contextid,"RPMcommand")) {

        snprintf(g_vbuff,CF_BUFSIZE,
                "%s -q --queryformat %%{EPOCH}:%%{VERSION}-%%{RELEASE}\\n %s",
                GetMacroValue(g_contextid,"RPMcommand"),package);

    } else {

        snprintf(g_vbuff, CF_BUFSIZE,
                "/bin/rpm -q --queryformat "
                "%%{EPOCH}:%%{VERSION}-%%{RELEASE}\\n %s", package);

    }

    if ((pp = cfpopen(g_vbuff, "r")) == NULL) {

        Verbose("Could not execute the RPM command.  "
                "Assuming package not installed.\n");

        return 0;
    }

    while(!feof(pp)) {
        *g_vbuff = '\0';

        ReadLine(g_vbuff,CF_BUFSIZE,pp);

        if (*g_vbuff != '\0') {
            AppendItem(&evrlist,g_vbuff,"");
        }
    }

    /* 
     * Non-zero exit status means that we could not find the package, so
     * Zero the list and bail.
     */ 

    if (cfpclose(pp) != 0) {
        DeleteItemList(evrlist);
        evrlist = NULL;
    }

    if (evrlist == NULL) {
        Verbose("RPM Package %s not installed.\n", package);
        return 0;
    }


    Verbose("RPMCheckPackage(): Requested %s %s %s\n",
            package, g_cmpsensetext[cmp],(version[0] ? version : "ANY"));

    /* 
     * If no version was specified, just return 1, because if we got
     * this far some package by that name exists. 
     */

    if (!version[0]) {
        DeleteItemList(evrlist);
        return 1;
    }

    /* Parse the EVR we are looking for once at the start */

    ParseEVR(version, &eB, &vB, &rB);

    /* The rule here will be: if any package in the list matches, then
     * the first one to match wins, and we bail out.
     */

    for (evr = evrlist; evr != NULL; evr=evr->next) {
        char *evrstart;
        evrstart = evr->name;

        /* Start out assuming the comparison will be equal. */
        result = cmpsense_eq;

        /* 
         * RPM returns the string "(none)" for the epoch if there is
         * none instead of the number 0.  This will cause ParseEVR() to
         * misinterpret it as part of the version component, since
         * epochs must be numeric.  If we get "(none)" at the start of
         * the EVR string, we must be careful to replace it with a 0 and
         * reset evrstart to where the 0 is.  Ugh.
         */

        if (!strncmp(evrstart, "(none)", strlen("(none)"))) {

            /* We have no EVR in the installed package.  Fudge it. */
            evrstart = strchr(evrstart, ':') - 1;
            *evrstart = '0';
        }

        Verbose("RPMCheckPackage(): Trying installed version %s\n", evrstart);
        ParseEVR(evrstart, &eA, &vA, &rA);

        /* 
         * Get the epochs at ints 
         * The above code makes sure we always have this.
         */ 
        epochA = atol(eA);   

        /* the B side is what the user entered.  Better check. */
        if (eB && *eB) {
            epochB = atol(eB);
        }

        /* First try the epoch. */
        if (epochA > epochB) {
            result = cmpsense_gt;
        }
        if (epochA < epochB) {
            result = cmpsense_lt;
        }

        /* 
         * If that did not decide it, try version.  We must *always* *
         * have a version string.  That's just the way it is.
         */
        switch (rpmvercmp(vA, vB)) {
        case 1:
            result = cmpsense_gt;
            break;
        case -1:
            result = cmpsense_lt;
            break;
        }

        /* 
         * If we wind up here, everything rides on the release if both
         * have it.  RPM always stores a release internally in the
         * database, so the A side will have it.  It's just a matter of
         * whether or not the user cares about it at this point.
         */

        if (rB && *rB) {
            switch (rpmvercmp(rA, rB)) {
            case 1:
                result = cmpsense_gt;
                break;
            case -1:
                result = cmpsense_lt;
                break;
            }
        }

        Verbose("Comparison result: %s\n",g_cmpsensetext[result]);

        switch(cmp) {
        case cmpsense_gt:
            match = (result == cmpsense_gt);
            break;
        case cmpsense_ge:
            match = (result == cmpsense_gt || result == cmpsense_eq);
            break;
        case cmpsense_lt:
            match = (result == cmpsense_lt);
            break;
        case cmpsense_le:
            match = (result == cmpsense_lt || result == cmpsense_eq);
            break;
        case cmpsense_eq:
            match = (result == cmpsense_eq);
            break;
        case cmpsense_ne:
            match = (result != cmpsense_eq);
            break;
        }

        /* 
         * If we find a match, just return it now, and don't bother
         * checking anything else RPM returned, if it returns multiple
         * packages
         */

        if (match) {
            DeleteItemList(evrlist);
            return 1;
        }
    }

    /* If we manage to make it out of the loop, we did not find a match. */

    DeleteItemList(evrlist);
    return 0;
}


/* ----------------------------------------------------------------- */
/* Debian */

int
DPKGPackageCheck(char *package,char *version,enum cmpsense cmp)
{
  FILE *pp;
  struct Item *evrlist = NULL;
  struct Item *evr;
  char *evrstart;
  enum cmpsense result;
  int match = 0;
  char tmpBUFF[CF_BUFSIZE];

  Verbose ("Package: ");
  Verbose (package);
  Verbose ("\n");

  /* check that the package exists in the package database */
  snprintf (g_vbuff, CF_BUFSIZE, 
          "/usr/bin/apt-cache policy %s 2>&1 | grep -v " \
	    "\"W: Unable to locate package \"", package);
  if ((pp = cfpopen (g_vbuff, "r")) == NULL) {
    Verbose ("Could not execute APT-command (apt-cache).\n");
    return 0;
  }
  while (!feof (pp)) {
    *g_vbuff = '\0';
    ReadLine (g_vbuff, CF_BUFSIZE, pp);
  }
  if (cfpclose (pp) != 0) {
    Verbose ("The package %s did not exist in the package database.\n", \
	     package);
    return 0;
  }

  /* check what version is installed on the system (if any) */
  snprintf (g_vbuff, CF_BUFSIZE, "/usr/bin/apt-cache policy %s", package);

  if ((pp = cfpopen (g_vbuff, "r")) == NULL) {
    Verbose ("Could not execute APT-command (apt-cache policy).\n");
    return 0;
  }
  while (!feof (pp)) {
    *g_vbuff = '\0';
    ReadLine (g_vbuff, CF_BUFSIZE, pp);
    if (*g_vbuff != '\0') {
      if (sscanf (g_vbuff, "  Installed: %s", tmpBUFF) > 0) {
	AppendItem (&evrlist, tmpBUFF, "");
      }
    }
  }
  if (cfpclose (pp) != 0) {
    Verbose ("Something impossible happened... ('grep' exited abnormally).\n");
    DeleteItemList (evrlist);
    return 0;
  }

  /* Assume that there is only one package in the list */
  evr = evrlist;

  if (evr == NULL) {
    /* We did not find a match, and returns */
    DeleteItemList (evrlist);
    return 0;
  }

  evrstart = evr->name;

  /* 
   * If version value is "(null)", the packages was not installed -> the
   * package has no version and dpkg --compare-versions will treat "" as
   * "no version". 
   */
  
  if (strncmp (evrstart, "(none)", strlen ("(none)")) == 0) {
    sprintf (evrstart, "\"\"");
  }

  if (strncmp (version, "(none)", strlen ("(none)")) == 0) {
    sprintf (version, "\"\"");
  }

  /* 
   * the evrstart shall be a version number which we will compare to
   * version using '/usr/bin/dpkg --compare-versions' 
   */
  
  /* start with assuming that the versions are equal */
  result = cmpsense_eq;
  
  /* check if installed version is gt version */
  snprintf (g_vbuff, CF_BUFSIZE, "/usr/bin/dpkg --compare-versions %s gt " \
	    "%s", evrstart, version);
  
  if ((pp = cfpopen (g_vbuff, "r")) == NULL) {
    Verbose ("Could not execute DPKG-command.\n");
    return 0;
  }
  while (!feof (pp)) {
    *g_vbuff = '\0';
    ReadLine (g_vbuff, CF_BUFSIZE, pp);
  }
  /* 
   * if dpkg --compare-versions exits with zero result the condition was
   * satisfied, else not satisfied 
   */
  if (cfpclose (pp) == 0) {
    result = cmpsense_gt;
  }    
  
  /* check if installed version is lt version */
  snprintf (g_vbuff, CF_BUFSIZE, "/usr/bin/dpkg --compare-versions %s lt " \
	    "%s", evrstart, version);

  if ((pp = cfpopen (g_vbuff, "r")) == NULL) {
    Verbose ("Could not execute DPKG-command.\n");
    return 0;
  }
  while (!feof (pp)) {
    *g_vbuff = '\0';
    ReadLine (g_vbuff, CF_BUFSIZE, pp);
  }
  /* 
   * if dpkg --compare-versions exits with zero result the condition was
   * satisfied, else not satisfied 
   */
  if (cfpclose (pp) == 0) {
    result = cmpsense_lt;
  }    
  
  Verbose ("Comparison result: %s\n", g_cmpsensetext[result]);
  
  switch (cmp) {
    case cmpsense_gt:
      match = (result == cmpsense_gt);
      break;
    case cmpsense_ge:
      match = (result == cmpsense_gt || result == cmpsense_eq);
      break;
    case cmpsense_lt:
      match = (result == cmpsense_lt);
      break;
    case cmpsense_le:
      match = (result == cmpsense_lt || result == cmpsense_eq);
      break;
    case cmpsense_eq:
      match = (result == cmpsense_eq);
      break;
    case cmpsense_ne:
      match = (result != cmpsense_eq);
      break;
  }
  
  if (match) {
    DeleteItemList (evrlist);
    return 1;
  }

  Verbose("\n");

  /* if we manage to make it here, we did not find a match */
  DeleteItemList (evrlist);
  return 0;
}

/* ----------------------------------------------------------------- */
/* RPM Version string comparison logic
 *
 * ParseEVR and rpmvercmp are taken directly from the rpm 4.1 sources.
 * ParseEVR is taken from the parseEVR routine in lib/rpmds.c and rpmvercmp
 * is taken from llib/rpmvercmp.c
 */

/* 
 * compare alpha and numeric segments of two versions 
 * return 1: a is newer than b 
 *        0: a and b are the same version 
 *       -1: b is newer than a 
 */

int
rpmvercmp(const char * a, const char * b)
{
    char oldch1, oldch2;
    char * str1, * str2;
    char * one, * two;
    char *s1,*s2;
    int rc;
    int isnum;

    /* easy comparison to see if versions are identical */
    if (!strcmp(a, b)) return 0;

    one = str1 = s1 = strdup(a);
    two = str2 = s2 = strdup(b);

    /* loop through each version segment of str1 and str2 and compare them */

    while (*one && *two) {
        while (*one && !xisalnum(*one)) one++;
        while (*two && !xisalnum(*two)) two++;

        str1 = one;
        str2 = two;

       /* 
        * Grab first completely alpha or completely numeric segment
        * leave one and two pointing to the start of the alpha or
        * numeric segment and walk str1 and str2 to end of segment 
        */
        if (xisdigit(*str1)) {
            while (*str1 && xisdigit(*str1)) str1++;
            while (*str2 && xisdigit(*str2)) str2++;
            isnum = 1;
        } else {
            while (*str1 && xisalpha(*str1)) str1++;
            while (*str2 && xisalpha(*str2)) str2++;
            isnum = 0;
        }

       /* 
        * save character at the end of the alpha or numeric segment so
        * that they can be restored after the comparison.
        */

        oldch1 = *str1;
        *str1 = '\0';
        oldch2 = *str2;
        *str2 = '\0';

       /* 
        * take care of the case where the two version segments are
        * different types: one numeric, the other alpha (i.e. empty) 
        */
        if (one == str1) {
            free(s1);
            free(s2);
            return -1; /* arbitrary */
        }

        if (two == str2) {
            free(s1);
            free(s2);
            return -1;
        }

        /* 
         * this used to be done by converting the digit segments to ints
         * using atoi() - it's changed because long  digit segments can
         * overflow an int - this should fix that.
         */
        if (isnum) {

            /* throw away any leading zeros - it's a number, right? */
            while (*one == '0') one++;
            while (*two == '0') two++;

            /* whichever number has more digits wins */
            if (strlen(one) > strlen(two)) {
                free(s1);
                free(s2);
                return 1;
            }

            if (strlen(two) > strlen(one)) {
                free(s1);
                free(s2);
                return -1;
            }
        }

        /* 
         * strcmp will return which one is greater - even if the two
         * segments are alpha or if they are numeric.  don't return  if
         * they are equal because there might be more segments to
         * compare 
         */
        rc = strcmp(one, two);
        if (rc) {
            free(s1);
            free(s2);
            return rc;
        }

       /* restore character that was replaced by null above */

        *str1 = oldch1;
        one = str1;
        *str2 = oldch2;
        two = str2;
    }

    /* 
     * this catches the case where all numeric and alpha segments have
     * compared identically but the segment sepparating characters were
     * different .
     */

    if ((!*one) && (!*two)) {
        free(s1);
        free(s2);
        return 0;
    }

    /* whichever version still has characters left over wins */
    if (!*one) {
        free(s1);
        free(s2);
        return -1;
    } else {
        free(s1);
        free(s2);
        return 1;
    }
}

/*
 * Split EVR into epoch, version, and release components.
 * @param evr [epoch:]version[-release] string
 * @retval *ep pointer to epoch
 * @retval *vp pointer to version
 * @retval *rp pointer to release
 */


void
ParseEVR(char * evr,const char ** ep,const char ** vp,const char ** rp)
{
    const char *epoch;
    const char *version; /* assume only version is present */
    const char *release;
    char *s, *se;

    s = evr;

    while (*s && xisdigit(*s)) s++; /* s points to epoch terminator */

    se = strrchr(s, '-'); /* se points to version terminator */

    if (*s == ':') {
        epoch = evr;
        *s++ = '\0';
        version = s;
        if (*epoch == '\0') epoch = "0";
    } else {
        epoch = NULL; /* XXX disable epoch compare if missing */
        version = evr;
    }

    if (se) {
        *se++ = '\0';
        release = se;
    } else {
        release = NULL;
    }

    if (ep) *ep = epoch;
    if (vp) *vp = version;
    if (rp) *rp = release;
}
