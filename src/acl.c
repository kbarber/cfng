/*
 * $Id: acl.c 743 2004-05-23 07:27:32Z skaar $
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
 * --------------------------------------------------------------------
 * Author: Mark / Demosthenes / Patch by Goran Oberg
 * NT code by Bjoern Gustafson and Joergen Kjensli
 * --------------------------------------------------------------------
 */

#include <stdio.h>

#include "cf.defs.h"
#include "cf.extern.h"

#ifdef HAVE_SYS_ACL_H
# include <sys/acl.h>
#endif


#ifndef HAVE_DCE_DACLIF_H

int 
CheckDFSACE(struct CFACE *aces, char method, char *filename, 
        enum fileactions action)
{
    CfLog(cferror,"DCE operation attempted, without DCE support","");
    return false;
}

#endif

/* ----------------------------------------------------------------- */

char *CFFSTYPES[] = {
    "posix",
    "solaris",
    "dfs",
    "afs",
    "hpux",
    "nt",
    NULL
};


/* ----------------------------------------------------------------- */

#if defined SOLARIS && defined HAVE_SYS_ACL_H
void
aclsortperror(int error)
{
    switch (error) {
    case GRP_ERROR:
        CfLog(cferror, "acl: There is more than one group_obj ACL entry.\n","");
        break;
    case USER_ERROR:
        CfLog(cferror, "acl: There is more than one user_obj ACL entry.\n","");
        break;
    case CLASS_ERROR:
        CfLog(cferror, "acl: There is more than one "
                "class_obj ACL entry.\n", "");
        break;
    case OTHER_ERROR:
        CfLog(cferror, "acl: There is more than one "
                "other_obj ACL entry.\n", "");
        break;
    case DUPLICATE_ERROR:
        CfLog(cferror, "acl: Duplicate entries of user or group.\n", "");
        break;
    case ENTRY_ERROR:
        CfLog(cferror, "acl: The entry type is invalid.\n", "");
        break;
    case MISS_ERROR:
        CfLog(cferror, "acl: Missing group_obj, user_obj, " 
                "class_obj, or other_obj entries.\n", "");
        break;
    case MEM_ERROR:
        CfLog(cferror, "acl: The system can't allocate any memory.\n", "");
        break;
    default:
        snprintf(g_output, CF_BUFSIZE*2, "acl: Unknown ACL error code: %d !\n", 
                error);
        CfLog(cferror, g_output,"");
        break;
        }
    return;
}
#endif


/* ----------------------------------------------------------------- */

#if 0 /* XXX alb - moved to dce_acl.c */

#ifdef HAVE_DCE
#endif

#endif /* XXX alb */

/* ----------------------------------------------------------------- */

#if defined SOLARIS && defined HAVE_SYS_ACL_H

int
ParseSolarisMode(char* mode, mode_t oldmode)
{
    char *a;
    mode_t perm;
    enum {add, del} o;

    perm = oldmode;
    if (strcmp(mode, "noaccess") == 0) {
        return 0; /* No access for this user/group */
    }
    
    o = add;
    for (a = mode; *a != '\0' ; *a++) {
        if (*a == '+' || *a == ',') {
            o = add;
        } else if (*a == '-') {
            o = del;
        } else if (*a == '=') {
            o = add;
            perm = 0;
        } else {
            switch (*a) {
            case 'r':
                if (o == add) {
                    perm |= 04;
                } else {
                    perm &= ~04;
                }
                break;
            case 'w': 
                if (o == add) {
                    perm |= 02;
                } else {
                    perm &= ~02;
                }
                break;
            case 'x':
                if (o == add) {
                    perm |= 01;
                } else {
                    perm &= ~01;
                }
                break;
            default:  
                snprintf(g_output,CF_BUFSIZE*2,
                        "Invalid mode string in solaris acl: %s\n", mode);
                CfLog(cferror,g_output,"");
            }
        }
    }
    return perm;
}

/* ----------------------------------------------------------------- */

int
BuildAclEntry(struct stat *sb, char *acltype, char *name, 
        struct acl *newaclbufp)
{
    struct passwd *pw;
    struct group *gr;
 
    if (strcmp(acltype, "user") == 0) {
        if (*name == 0) {
        newaclbufp->a_type = USER_OBJ;
        newaclbufp->a_id = sb->st_uid;
        } else {
            newaclbufp->a_type = USER;
            if ((pw = getpwnam(name)) != NULL) {
                newaclbufp->a_id = pw->pw_uid;
            } else {
                snprintf(g_output, CF_BUFSIZE*2,
                        "acl: no such user: %s\n", name);
                CfLog(cferror,g_output,"");
                return -1;
            }
        }
    } else if (strcmp(acltype, "group") == 0) {
        if (*name == 0) {
            newaclbufp->a_type = GROUP_OBJ;
            newaclbufp->a_id = sb->st_gid;
        } else {
            newaclbufp->a_type = GROUP;
            if ((gr = getgrnam(name)) != NULL) {
                newaclbufp->a_id = gr->gr_gid;
            } else {
                snprintf(g_output,CF_BUFSIZE*2,
                        "acl: no such group: %s\n",name);
                CfLog(cferror,g_output,"");
                return -1;
            }
        }
    } else if (strcmp(acltype, "mask") == 0) {
        newaclbufp->a_type = CLASS_OBJ;
        newaclbufp->a_id = 0;
    } else if (strcmp(acltype, "other") == 0) {
        newaclbufp->a_type = OTHER_OBJ;
        newaclbufp->a_id = 0;
    } else if (strcmp(acltype, "default_user") == 0) {
        if (*name == 0) {
            newaclbufp->a_type = DEF_USER_OBJ;
            newaclbufp->a_id = 0;
        } else {
            newaclbufp->a_type = DEF_USER;
            if ((pw = getpwnam(name)) != NULL) {
                newaclbufp->a_id = pw->pw_uid;
            } else {
                snprintf(g_output,CF_BUFSIZE*2,
                        "%s: acl: no such user: %s\n", g_vprefix, name);
                CfLog(cferror,g_output,"");
                return -1;
            }
        }
    } else if (strcmp(acltype, "default_group") == 0) {
        if (*name == 0) {
            newaclbufp->a_type = DEF_GROUP_OBJ;
            newaclbufp->a_id = 0;
        } else {
            newaclbufp->a_type = DEF_GROUP;
            if ((gr = getgrnam(name)) != NULL) {
                newaclbufp->a_id = gr->gr_gid;
            } else {
                snprintf(g_output,CF_BUFSIZE*2,
                        "acl: no such group: %s\n",name);
                CfLog(cferror,g_output,"");
                return -1;
            } 
        }
    } else if (strcmp(acltype, "default_mask") == 0) {
        newaclbufp->a_type = DEF_CLASS_OBJ;
        newaclbufp->a_id = 0;
    } else if (strcmp(acltype, "default_other") == 0) {
        newaclbufp->a_type = DEF_OTHER_OBJ;
        newaclbufp->a_id = 0;
    }

    newaclbufp->a_perm = 0;

    return 0;
}
#endif

/* ----------------------------------------------------------------- */

void InstallACL(char *alias, char *classes)
{ 
    struct CFACL *ptr;
    char ebuff[CF_EXPANDSIZE];
 
    Debug1("InstallACL(%s,%s)\n",alias,classes);
 
    if (! IsInstallable(classes)) {
        Debug1("Not installing ACL no match\n");
        return;
    }
 
    ExpandVarstring(alias,ebuff,"");
 
    if ((ptr = (struct CFACL *)malloc(sizeof(struct CFACL))) == NULL) {
        FatalError("Memory Allocation failed for InstallACL() #1");
    }
 
    if ((ptr->acl_alias = strdup(ebuff)) == NULL) {
        FatalError("Memory Allocation failed for InstallEditFile() #2");
    }
    
        /* First element in the list */
    if (g_vacllisttop == NULL) {
        g_vacllist = ptr;
    } else {
        g_vacllisttop->next = ptr;
    }
 
    ptr->aces = NULL;
    ptr->next = NULL;
    ptr->method = 'o';
    ptr->type = StringToFstype("none"); 
    g_vacllisttop = ptr;
}

/****** Code added for NT by J�rgen Kjensli & Bj�rn Gustafson, May 1999 *****/

#ifdef NT

#include <windows.h>


DWORD getNTModeMask(char *new_mode, DWORD old_mode);
DWORD getNTACEs_Size(struct CFACE *);
SECURITY_DESCRIPTOR* getNTACLInformation(char *filename, PACL *old_pacl, 
        BOOL *haveACL, ACL_SIZE_INFORMATION *oldACLSize);
void attachToNTACEs(struct CFACE *new_aces, char *user, char *mode, 
        char *access, char *classes, DWORD NTMask);
void appendNTACEs(struct CFACE *aces, struct CFACE *new_aces,
        ACL_SIZE_INFORMATION oldACLSize, PACL old_pacl);
void AddNTACEs(char *accessType, struct CFACE *new_aces, PACL *new_pacl);
void createNTACL(struct CFACE *aces, char method, char *filename, 
        enum fileactions action);
#endif

int CheckNTACE(struct CFACE *aces, char method, char *filename, 
        enum fileactions action);

#ifdef NT

DWORD
getNTModeMask(char *new_mode, DWORD old_mode)
{
	/* Declare and initialize variables */
	char *a;			   /* Character to iterate the mode with */
	enum {add, del} o;     /* Flag saying whether to add or remove rights */
	DWORD accessMask;      /* Variable to hold the final mask */
	o = add;			   /* Start function with raised flag */
	accessMask = old_mode; /* Initilaize the mask to the original/old mask */

	/* modetype: noaccess, default, all, change, read, rwxdop */

	/* No access. Should never get here (noaccess => all and denied ACE).
	   If it does get here, return the old mask (nothing is done) */
	if (strcmp(new_mode, "noaccess") == 0) {
		Debug2("Setting ACE-mode to old_mode: %x\n",old_mode);
		return (old_mode);
	}

	/* Default - remove ACE. Set mask to 0 */
	if (strcmp(new_mode, "default") == 0) {
		Verbose("Mode was default.\n");
		Debug2("Setting ACE-mode to 0\n");
		return (0);
	}

	/* All. Return the NT's predefined bit mask for all rights */
	if (strcmp(new_mode, "all") == 0) {
		Debug2("Setting ACE-mode to GENERIC_ALL: %d\n",GENERIC_ALL);
		return (GENERIC_ALL);
	}

	/* Change. Return the "rwxd" rights ORed with the existing mask */
	if (strcmp(new_mode, "change") == 0) {
		Debug2("Setting ACE-mode to CHANGE: %d\n",
                (old_mode | GENERIC_READ | GENERIC_WRITE 
                 | GENERIC_EXECUTE | DELETE));
		return (old_mode | GENERIC_READ | GENERIC_WRITE 
                | GENERIC_EXECUTE | DELETE);
	}

	/* Read. Return the "rx" rights ORed with the existing mask */
	if (strcmp(new_mode, "read") == 0) {
		Debug2("Setting ACE-mode to READ: %d\n",
                (old_mode | GENERIC_READ | GENERIC_EXECUTE));
		return (old_mode | GENERIC_READ | GENERIC_EXECUTE);
	}

	/* If the code gets here we must treat the string character by character
	   We cannot remove rights if NT's predefined bit mask for all rights is
	   used. Translate this mask to a mask containing all rights (rwxdop) */
	if (accessMask == GENERIC_ALL) {
		accessMask = (0x120089 | 0x120116 | 0x1200A0 | DELETE 
                | WRITE_OWNER | WRITE_DAC);
	}

	/* Iterate string with desired mode. Check each character */
	for (a = new_mode; *a != '\0' ; *a++) {
		/* Decide wheteher to add or subtract permission bits */
		if (*a == '+' || *a == ',') {
			o = add;
		}
		/* If equal sign, throw away old mask. Get ready to add permissions */
		else if (*a == '=') {
			o = add;
			accessMask = 0;
		}
		else if (*a == '-') {
			/* Prevent subtracting rights from user with no rights */
			if (accessMask == 0) {
				/* Return the old, original mask. Nothing is changed */
				return old_mode;
			}
			o = del;
		}

		/* Character was a 'permission' character. Check which permission
		   and manipulate the mask */
		else {
			/* Check permission bits and set accessMask */
			switch (*a) {
            case 'r':
                if (o == add) {
                    /* Adding read rights */
                    Debug2("Setting ACE-mode to READ.\n");
                    accessMask |= 0x120089;
                } else {
                    /* Removing read rights */
                    Debug2("Removing READ from ACE-mode\n");

                    /* 
                     * Execute allready set in mask. Remove relevant
                     * read-bits. 
                     */
                    if ((accessMask & 0x1200A0) == 0x1200A0) {
                        accessMask &= 0xFFFFF6;
                    }
                    /* 
                     * Write is allready set in mask. Remove relevant
                     * read-bits. 
                     */
                    else if ((accessMask & 0x120116) == 0x120116) {
                        accessMask &= 0xFFFF76;
                    }
                    /* 
                     * Neither write nor execute is set. Remove relevant
                     * read-bits.
                     */
                    else {
                        accessMask &= 0xEDFF76;
                    }
                }
                break;
            case 'w':
                if (o == add) {
                    /* Adding write rights */
                    Debug2("Setting ACE-mode to WRITE.\n");
                    accessMask |= 0x120116;
                } else {
                    /* Removing write rights */
                    Debug2("Removing WRITE from ACE-mode\n");

                    Verbose("Old mask: %x\n",accessMask);

                    /* 
                     * Execute or read is already set in mask.  Remove
                     * relevant write-bits.
                     */ 
                    if (((accessMask & 0x120089) == 0x120089) 
                            || ((accessMask & 0x1200A0) == 0x1200A0)) {
                        accessMask &= 0xFFFEE9;
                    }
                    /* 
                     * Neither read nor execute is set. Remove relevant
                     * write-bits. 
                     */
                    else {
							accessMask &= 0xEDFEE9;
                    }
                }
                break;
            case 'x':
                if (o == add) {
                    /* Adding execute rights */
                    Debug2("Setting ACE-mode to EXECUTE.\n");
                    accessMask |= 0x1200A0;
                } else {
                    /* Removing execute rights */
                    Debug2("Removing EXECUTE from ACE-mode\n");

                    /* 
                     * Read allready set in mask. Remove relevant
                     * execute-bits.
                     */ 
                    if ((accessMask & 0x120089) == 0x120089) {
                        accessMask &= 0xFFFFDF;
                    }
                    /* 
                     * Write is allready set in mask. Remove relevant
                     * execute-bits.
                     */
                    else if ((accessMask & 0x120116) == 0x120116) {
                        accessMask &= 0xFFFF5F;
                    }
                    /* 
                     * Neither write nor read is set. Remove relevant
                     * execute-bits.
                     */
                    else {
                        accessMask &= 0xEDFF5F;
                    }
                }
                break;
            case 'd':
                if (o == add) {
                    /* Adding delete rights */
                    Debug2("Setting ACE-mode to DELETE: %d\n",DELETE);
                    accessMask |= DELETE;
                } else {
                    /* Removing delete rights */
                    Debug2("Setting ACE-mode to ~DELETE: %d\n",~DELETE);
                    accessMask &= ~DELETE;
                }
                break;
            case 'o':
                if (o == add) {
                    /* Adding owner rights */
                    Debug2("Setting ACE-mode to WRITE_OWNER: %d\n",
                                WRITE_OWNER);
                    accessMask |= WRITE_OWNER;
                } else {
                    /* Removing owner rights */
                    Debug2("Setting ACE-mode to ~WRITE_OWNER: %d\n",
                                ~WRITE_OWNER);
                    accessMask &= ~WRITE_OWNER;
                }
                break;
            case 'p':
                if (o == add) {
                    /* Adding "change permission" rights */
                    Debug2("Setting ACE-mode to WRITE_DAC: %d\n",WRITE_DAC);
                    accessMask |= WRITE_DAC;
                } else {
                    /* Removing "change permission" rights */
                    Debug2("Setting ACE-mode to ~WRITE_DAC: %d\n",
                                ~WRITE_DAC);
                    accessMask &= ~WRITE_DAC;
                }
                break;
            default:
                /* The character was not a valid permission */
                snprintf(g_output, CF_BUFSIZE*2, 
                        "Invalid mode string in NT acl: %s. "
                        "No changes made.\n", new_mode);
                CfLog(cferror,g_output,"");
                return old_mode;
            }
		}
	}

	/* Make sure the standard "ALL" is returned instead of RWXDOP. This is
	   done so that "All" is showed in NT's GUI */
	if (accessMask == (0x120089 | 0x120116 | 0x1200A0 | DELETE 
                | WRITE_OWNER | WRITE_DAC)) {
		return (GENERIC_ALL);
	}

	/* Return the new mask */
	return (accessMask);
}

#endif

/*************************** END NT Addition *******************************/

void 
AddACE(char *acl, char *string, char *classes)
{ 
    struct CFACL *ptr;
    struct CFACE *new, *top;
    char varbuff[CF_BUFSIZE], *cp, ebuff[CF_EXPANDSIZE];
    char comm[CF_MAXVARSIZE],data1[CF_MAXVARSIZE],data2[CF_MAXVARSIZE];
/****** Code added for NT by J�rgen Kjensli & Bj�rn Gustafson, May 1999 *****/

    char data3[CF_MAXVARSIZE];     /* To hold the extra variable accesstype */

/*************************** END NT Addition *******************************/
    int i;

    Debug1("AddACE(%s,%s,[%s])\n",acl,string,classes);

    if ( ! IsInstallable(g_classbuff)) {
        Debug1("Not installing ACE no match\n");
        return;
    }
    
    for (ptr = g_vacllist; ptr != NULL; ptr=ptr->next) {
        varbuff[0] = '\0';
        ExpandVarstring(acl,varbuff,"");
        
        if (strcmp(ptr->acl_alias,varbuff) == 0) {
            comm[0] = data1[0] = data2[0] = '\0';
       
/****** Code added for NT by J�rgen Kjensli & Bj�rn Gustafson, May 1999 *****/
       
            data3[0] = '\0';      /* Initialize variable */
       
/*************************** END NT Addition *******************************/
       
            if ((new = (struct CFACE *)calloc(1,sizeof(struct CFACE))) == NULL) {
                FatalError("Memory Allocation failed for AddEditAction() #1");
            }
       
            if (ptr->aces == NULL) {
                ptr->aces = new;
            } else {
                for (top = ptr->aces; top->next != NULL; top=top->next) { }
                top->next = new;
                new->next = NULL;
            }
       
            if (string == NULL) {
                new->name = NULL;
                new->next = NULL;
            } else {
                memset(ebuff,0, sizeof(ebuff));
                ebuff[0]='\0';                         
                /* Expand any variables */
                ExpandVarstring(string,ebuff,"");
            
                cp = ebuff;
                i = 0;
                while (*cp != ':' && *cp != '\0') {
                    comm[i++] = *cp++;
                }
                comm[i] = 0;
                *cp++;

                i = 0;
                while (*cp != ':' && *cp != '\0') {
                    data1[i++] = *cp++;
                }
                data1[i] = 0;
                *cp++;
                
                i = 0;
                while (*cp != ':' && *cp != '\0') {
                    data2[i++] = *cp++;
                }
                data2[i] = 0;

/****** Code added for NT by J�rgen Kjensli & Bj�rn Gustafson, May 1999 *****/

 /* Parse the string to get the access type */
#ifdef NT
                *cp++;
                i = 0;
                while (*cp != ':' && *cp != '\0') {
                    data3[i++] = *cp++;
                }
                data3[i] = 0;
#endif

/*************************** END NT Addition *******************************/

                if (strncmp("fstype",comm,strlen(comm)) == 0) {
                    ptr->type = StringToFstype(data1);
                    new->next = NULL;
                    return;
                }
                
                if (strncmp("method",comm,strlen(comm)) == 0) {
                    ptr->method = ToLower(*data1);
                    new->next = NULL;
                    return;
                }

                if ((new->acltype = strdup(comm)) == NULL) {
                    FatalError("Memory Allocation failed for AddACE() #1");
                }
                
                if ((new->name = strdup(data1)) == NULL) {
                    FatalError("Memory Allocation failed for AddACE() #2");
                }

/****** Code added for NT by J�rgen Kjensli & Bj�rn Gustafson, May 1999 *****/
#ifdef NT

                /* Give all users defined in config file a default mask of 0 */
                new->NTMode = 0;
                
                /* 
                 * If "noaccess" is the specified permission, store the
                 * permission "all" on an access denied ACE to get the
                 * wanted effect of "No Access" showed in NT's GUI
                 */
                if (strcmp(data2,"noaccess") == 0) {
                    if ((new->mode = strdup("all")) == NULL) {
                        FatalError("Memory Allocation failed for AddACE() #2");
                    }
                    if ((new->access = strdup("denied")) == NULL) {
                        FatalError("Memory Allocation failed for AddACE() #3");
                    }
                }
                /* 
                 * Unless "noaccess" is specified, store the specified
                 * values as usual
                 */
                else {
                    if ((new->mode = strdup(data2)) == NULL) {
                        FatalError("Memory Allocation failed for AddACE() #2");
                    } 
                    if ((new->access = strdup(data3)) == NULL) {
                        FatalError("Memory Allocation failed for AddACE() #3");
                    }
                }

#else

/*************************** END NT Addition *******************************/
                if ((new->mode = strdup(data2)) == NULL) {
                    FatalError("Memory Allocation failed for AddACE() #2");
                }
#endif
            }
       
            if ((new->classes = strdup(classes)) == NULL) {
                FatalError("Memory Allocation failed for InstallEditFile() #3");
            }
            return;
        }
    }
    printf("cfng: software error - no ACL matched installing %s \n",acl);
}

/* ----------------------------------------------------------------- */

int CheckACLs(char *filename, enum fileactions action, struct Item *acl_aliases)
{ 
    struct Item *ip;
    struct CFACL *ap;
    int status = false;
 
    Debug1("CheckACLs(%s)\n",filename);

    for (ip = acl_aliases; ip != NULL; ip = ip->next) {
        if ((ap = GetACL(ip->name)) != NULL) {
            Verbose(" ACL method (overwrite/append) = %c on %s\n",
                    ap->method,filename);

            switch(ap->type) {
            case solarisfs:
            case posixfs:
                status = CheckPosixACE(ap->aces,ap->method,filename,action);
                break;
            case dfsfs:
                status = CheckDFSACE(ap->aces,ap->method,filename,action);
                break;
            case ntfs:
                /* The nt filesystem is defined. Call our function to implement
                    the ACLs on NT. */
                status = CheckNTACE(ap->aces,ap->method,filename,action);
                break;
            default:
                /* XXX alb - fix, was missing g_output initial parameter */
                snprintf(g_output,CF_BUFSIZE*2, 
                        "Unknown filesystem type in ACL %s\n",ip->name);
                CfLog(cferror,g_output,"");
            }
        } else {
        Verbose("ACL %s does not exist (file=%s)\n",ip->name,filename);
        }
    }
    return status;
}


/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

enum cffstype 
StringToFstype(char *string)
{ 
    int i;

    for (i = 0; CFFSTYPES[i] != NULL; i++) {
        if (strcmp(string,CFFSTYPES[i]) == 0) {
            return (enum cffstype) i;
        }
    }
    
    return (enum cffstype) i;
}

/* ----------------------------------------------------------------- */

struct CFACL *
GetACL(char *acl_alias)
{ 
    struct CFACL *ap;

    for (ap = g_vacllist; ap != NULL; ap = ap->next) {
        if (strcmp(acl_alias,ap->acl_alias) == 0) {
            return ap;
        }
    }

    return NULL;
}

/****** Code added for NT by J�rgen Kjensli & Bj�rn Gustafson, May 1999 *****/

#ifdef NT

DWORD
getNTACEs_Size(aces)
struct CFACE *aces; /* List of CFACEs */
{
	/* Declarations */
	struct CFACE *ep;       /* Pointer to a CFACE struct. To iterate through
							   the existing list (received as parameter)  */
	DWORD sidSize = 256;    /* The maximum size of the users SID          */
	DWORD domainSize = 256; /* The maximum size of the name of the domain */
	char domain[256];		/* Allocate max memory for the domain name    */
	PSID psid;				/* A pointer to a security identifier (SID)   */
	SID_NAME_USE snu;       /* Enumerated type indicating type of account */
	DWORD totalACESize = 0; /* Total accumulated size of all ACEs in list */
	DWORD accessSize = 0;   /* Variable to hold the size of the ACE       */
	DWORD psidSize = 0;     /* Variable to hold the size of the SID       */

	/* Iterate through every CFACE in the list sent to the function */
	for (ep = aces; ep != NULL; ep=ep->next)
	{
		/* Do not count ACE if name is null or if class is excluded or
		   if mode is "default"*/
		if ((ep->name == NULL) || (IsExcluded(ep->classes)) 
                || (strcmp(ep->mode,"default") == 0)) {
			continue;
		}

		/* Finding size of SID */
		sidSize = 0;
		domainSize = 256;
		LookupAccountName(NULL, ep->name, NULL, &sidSize, 
                domain, &domainSize, &snu);

		/* If SID doesn't exist, continue loop */
		if (!sidSize) {
			continue;
		}

		/* Reset accessSize and get the size of the ACE */
		accessSize = 0;
		if ((strcmp(ep->access,"allowed") == 0) || (ep->access == '\0')) {
			accessSize = sizeof(ACCESS_ALLOWED_ACE);
		} else if (strcmp(ep->access,"denied") == 0) {
			accessSize = sizeof(ACCESS_DENIED_ACE);
		}

        /* 
         * Finally add up the size of the sid, the accesstype, and
         * subtract a DWORD. We subtract a DWORD because the ACE size by
         * default contains one DWORD while we know the exact size of it
         * (sidBufferSize)
         */
		totalACESize += (sidSize + accessSize - sizeof(DWORD));
	}

	/* Return the total size of all ACEs */
	return totalACESize;
}

#endif

/* ----------------------------------------------------------------- */

#ifdef NT

SECURITY_DESCRIPTOR*
getNTACLInformation(filename, old_pacl, haveACL, oldACLSize)
char *filename;  /* The name of the file */
PACL *old_pacl;  /* Pointer to an ACL */
BOOL *haveACL;   /* Flag to say wheteher ACL exists or not */
ACL_SIZE_INFORMATION *oldACLSize; /*Pointer to a struct with info about ACL*/
{
	DWORD sizeRqd;		/* Size needed to hold the file's SD */
	DWORD old_sdSize;   /* Size needed to hold the SD */
	SECURITY_DESCRIPTOR *old_sd; /* Pointer to a SD */
	BOOL byDef;			/* Flag needed for a function call */

	/* Find the size of the old security descriptor */
	sizeRqd = 0;
	GetFileSecurity(filename, DACL_SECURITY_INFORMATION, NULL, 0, &sizeRqd);

	if (sizeRqd == 0) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Unable to get size of old SD\n", g_vprefix);
		CfLog(cferror,g_output,"");
		return;
	}

	/* Allocate memory for the old security descriptor */
	if ((old_sd = (SECURITY_DESCRIPTOR *) malloc(sizeRqd)) == NULL) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Unable to allocate memory for the old SD\n", 
                g_vprefix);
		CfLog(cferror,g_output,"");
		return;
	}

	/* Retrieve the old security descriptor */
	old_sdSize = sizeRqd;
	if (!GetFileSecurity(filename, DACL_SECURITY_INFORMATION, 
                old_sd, old_sdSize, &sizeRqd)) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Unable to get the old SD\n", g_vprefix);
		CfLog(cferror,g_output,"");
		return;
	}

	/* Retrieve information about the old security descriptor */
	if (!GetSecurityDescriptorDacl(old_sd, haveACL, old_pacl, &byDef)) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Unable to get information about the old SD\n", 
                g_vprefix);
		CfLog(cferror,g_output,"");
		return;
	}

	/* File did have an ACL */
	if (*haveACL) {
        /* 
         * Retrieve size information about the old ACL and add it to the
         * size of the new
         */
		if (!GetAclInformation(*old_pacl, oldACLSize, 
                    sizeof(ACL_SIZE_INFORMATION), AclSizeInformation)) {
			snprintf(g_output,CF_BUFSIZE*2,
                    "%s: acl: Unable to get information about the old ACL\n", 
                    g_vprefix);
			CfLog(cferror,g_output,"");
			return;
		}
	} else {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: The old security descriptor does not contain "
                "an ACL\n", g_vprefix);
		CfLog(cferror,g_output,"");
		return;
	}

	return old_sd;
}

#endif

/* ----------------------------------------------------------------- */

#ifdef NT

void attachToNTACEs(new_aces, user, mode, access, classes, NTMask)
struct CFACE *new_aces; /* Pointer to an empty list of CFACEs */
char *user;			    /* Name of user */
char *mode;				/* User's mode */
char *access;           /* User's type of access */
char *classes;			/* Class */
DWORD NTMask;			/* User's accessmask */
{
	/* Declaration of variables */
	struct CFACE *newACE; /* Pointer to a CFACE to hold new information */
	struct CFACE *ep;	  /* Pointer to a CFACE. To iterate through list */

	/* Check if mode was default i.e. NTMask = 0 */
	if (NTMask == 0) {
		return;
	}

	/* Allocate memory for new ACE */
	if ((newACE = (struct CFACE *)malloc(sizeof(struct CFACE))) == NULL) {
		FatalError("Memory Allocation failed for AddEditAction() #1");
	}

	/* Hook the new ACE to the end list */
	if (new_aces == NULL) {
		new_aces = newACE;
	} else {
		for (ep = new_aces; ep->next != NULL; ep=ep->next) { }
		ep->next = newACE;
	}
	newACE->next = NULL;

	/* Set the values of the new ACE */
	if ((newACE->name = strdup(user)) == NULL) {
		FatalError("Memory Allocation failed for name-member.");
	}
	if ((newACE->mode = strdup(mode)) == NULL) {
		FatalError("Memory Allocation failed for mode-member.");
	}
	if ((newACE->access = strdup(access)) == NULL) {
		FatalError("Memory Allocation failed for access-member.");
	}
	if ((newACE->classes = strdup(classes)) == NULL) {
		FatalError("Memory Allocation failed for classes-member.");
	}
	newACE->NTMode = NTMask;
}

#endif

/* ----------------------------------------------------------------- */

#ifdef NT

void appendNTACEs(aces, new_aces, oldACLSize, old_pacl)
struct CFACE *aces;     		/* List of CFACEs */
struct CFACE *new_aces; 		/* List of CFACEs */
ACL_SIZE_INFORMATION oldACLSize;/* Will hold info about the file's ACL */
PACL old_pacl;                  /* Pointer to an ACL */
{
	int x;						/* Counter */
	struct CFACE *ep;           /* Pointer to CFACE. For iteration purposes */
	ACCESS_ALLOWED_ACE *aceType;/* A Pointer to an ACE */
	SID_NAME_USE sidType;       /* Indicating type of account*/
	char user[256];             /* Buffer to hold username */
	char domain[256];			/* Buffer to hold domain name */
	DWORD uLength = 256;		/* Maximum length of username */
	DWORD dLength = 256;        /* Maximum length of domain name */
	DWORD NTMode = 0;

	Verbose("Existing users: %d\n",oldACLSize.AceCount);

	/* Iterate through all existing ACEs */
	for (x = 0; x < oldACLSize.AceCount; x++) {
		/* Get ace information */
		if (GetAce(old_pacl, x, (LPVOID *) &aceType)) {
			/* Reset maximum lengths and check that the user exists */
			uLength = dLength = 256;
			if (!LookupAccountSid(NULL, (PSID)(&aceType->SidStart), 
                        user, &uLength, domain, &dLength, &sidType)) {
				snprintf(g_output,CF_BUFSIZE*2,
                        "%s: acl: Unable to get allowed ACE's username\n", 
                        g_vprefix);
				CfLog(cferror,g_output,"");
				continue;
			}

			/* Remember the existing ACEs mask */
			NTMode = aceType->Mask;

            /* 
             * If user allready exists in our old CFACE list, give him a
             * new mask 
             */
			for (ep = aces; ep != NULL; ep=ep->next) {
                /* 
                 * Skip ACE if name is null or if class is excluded or
                 * if mode is "default"
                 */
				if ((ep->name == NULL) || (IsExcluded(ep->classes))) {
					continue;
				}

                /* 
                 * If user is listed in config file with access allowed,
                 * and existing ACE-type is also allowed, or user is
                 * listed in config file with access denied, and
                 * existing ACE-type is also denied, then find new
                 * updated and proper NTModeMask for that user
                 */
				if (strcmp(ep->name,user) == 0) {
					if (((strcmp(ep->access,"allowed") == 0) 
                                && ((aceType->Header.AceType) == 
                                    ACCESS_ALLOWED_ACE_TYPE)) 
                                || ((strcmp(ep->access,"denied") == 0) 
                                && ((aceType->Header.AceType) == 
                                    ACCESS_DENIED_ACE_TYPE))) {
                        /* 
                         * Get new mask (combination of old mask and new
                         * mode)
                         */
						ep->NTMode = NTMode = getNTModeMask(ep->mode,
                                aceType->Mask);
						break;
					}
				}
			}

			/* User didn't exist in config file. NTMode has the ACEs mask then.
			   Attach the user to our new list */
			if ((aceType->Header.AceType) == ACCESS_ALLOWED_ACE_TYPE) {
				Debug1("Attaching allowed ace for %s with mask: %x\n",
                        user,NTMode);
				attachToNTACEs(new_aces, user, "", "allowed", "any", NTMode);
			} else if ((aceType->Header.AceType) == ACCESS_DENIED_ACE_TYPE) {
				Debug1("Attaching denied ace for %s with mask: %x\n",
                        user,NTMode);
				attachToNTACEs(new_aces, user, "", "denied", "any", NTMode);
			} else {
				snprintf(g_output,CF_BUFSIZE*2,
                        "%s: acl: Existing ACE lost due to it's unfamiliar "
                        "ACE type(#%x) (allowed=0, denied=1)\n", 
                        g_vprefix, (aceType->Header.AceType));
				CfLog(cferror,g_output,"");
			}
		}

		/* Unable to get old ACE */
		else {
			printf("Unable to get ACE.\n");
			continue;
		}
	}
}

#endif

/* ----------------------------------------------------------------- */

#ifdef NT

void AddNTACEs(accessType,new_aces,new_pacl)
char *accessType;       /* type of ACEs to add (allowed/denied) */
struct CFACE *new_aces; /* List of ACEs to check for the accesstype*/
PACL *new_pacl;			/* Pointer to the ACL that shoul be altered */
{
	/* Declarations of variables */
	struct CFACE *ep;   /* To iterate through list of CFACEs */
	PSID psid;			/* Pointer to a SID */
	DWORD sidSize;      /* To hold size of SID */
	char domain[256];	/* Buffer to hold domain name */
	DWORD domainSize=256;/*Maximum size of domain */
	SID_NAME_USE snu;	/* Indicating type of account*/
	int counter = -1;   /* Counter for verbose reasons */

	Verbose("---------------------- ACE info-------------------------------\n");

	/* Iterate through list of CFACEs */
	for (ep = new_aces; ep != NULL; ep=ep->next) {
		counter++;

        /* 
         * Do not create ACE if name is null or if class is excluded or
         * if mode is "default"
         */
		if ((ep->name == NULL) || (IsExcluded(ep->classes)) 
                || (ep->NTMode == 0) || (strcmp(ep->mode,"default") == 0)) {
			continue;
		}

		/* Finding size of SID */
		sidSize = 0;
		domainSize = 256;
		LookupAccountName(NULL, ep->name, NULL, &sidSize, 
                domain, &domainSize, &snu);

		/* If SID doesn't exist, continue loop */
		if (!sidSize) {
			snprintf(g_output, CF_BUFSIZE*2,
                    "%s: acl: User %s doesn't exist. ACE not created.\n", 
                    g_vprefix, ep->name);
			CfLog(cferror,g_output,"");
			continue;
		}

		/* Allocate memory for list with new ACEs */
		if ((psid = (PSID)malloc(sidSize)) == NULL) {
			FatalError("Memory Allocation failed for SID\n");
		}

        /* 
         * Reset buffersize, get the SID and create ACE if user or group
         * exists 
         */
		domainSize = 256;
		if (!LookupAccountName(NULL, ep->name, psid, &sidSize, 
                    domain, &domainSize, &snu)) {
			snprintf(g_output,CF_BUFSIZE*2,
                    "%s: acl: User doesn't exist. Will not create "
                    "ACE for: %s\n", g_vprefix, ep->name);
			CfLog(cferror,g_output,"");
			free(psid);
			continue;
		}

		/* Check accesstype and add either an Access Denied or Allowed ACE. */
		if ((strcmp(accessType,"denied") == 0) 
                && (strcmp(ep->access,"denied") == 0)) {
			if (!AddAccessDeniedAce(*new_pacl, ACL_REVISION, 
                        ep->NTMode, psid)) {
				snprintf(g_output,CF_BUFSIZE*2,
                        "%s: acl: Unable to add new denied ACE for user %s\n",
                        g_vprefix, ep->name);
				CfLog(cferror,g_output,"");
				free(psid);
				return;
			}
			Debug2("- Ace #%d info:\n",counter);
			Debug2("          mode (permission flags) is    %x\n",ep->NTMode);
			Debug2("          name (id name) is             %s\n",ep->name);
			Debug2("          access type (allow/deny) is   %s\n",ep->access);
			Debug2("          classes is                    %s\n",ep->classes);
			Debug2("          ----------------------------------\n");
			Debug2("Adding access denied for user: %s\n\n",ep->name);
		}
		else if ((strcmp(accessType,"allowed") == 0) 
                && ((strcmp(ep->access,"allowed") == 0) 
                || (ep->access == '\0'))) {
			if (!AddAccessAllowedAce(*new_pacl, ACL_REVISION, 
                        ep->NTMode, psid)) {
				snprintf(g_output,CF_BUFSIZE*2,
                        "%s: acl: Unable to add new allowed ACE for user %s\n", 
                        g_vprefix, ep->name);
				CfLog(cferror,g_output,"");
				free(psid);
				return;
			}
			Debug2("- Ace #%d info:\n",counter);
			Debug2("          mode (permission flags) is    %x\n",ep->NTMode);
			Debug2("          name (id name) is             %s\n",ep->name);
			Debug2("          access type (allow/deny) is   %s\n",ep->access);
			Debug2("          classes is                    %s\n",ep->classes);
			Debug2("          ----------------------------------\n");
			Debug2("Adding access allowed for user: %s\n",ep->name);
		}

		/* Free memory */
		free(psid);
	}
	Verbose("----------------------end-------------------------------\n");
}

#endif

/* ----------------------------------------------------------------- */

#ifdef NT

/* 
 * Extra buffer size (beyond length of original string) needed to convert
 * file names, worst case should be "c:\" -> "/cygdrive/c/" (plus one for
 * null terminatory) - so 9 should be sufficient.
 */

#define CONVERT_PAD 12

/* Write error message to log from last failed windows API function. */

void
LogWinError(void)
{
LPVOID lpMsgBuf;
FormatMessage( 
    FORMAT_MESSAGE_ALLOCATE_BUFFER | 
    FORMAT_MESSAGE_FROM_SYSTEM | 
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    GetLastError(),
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), /* Default language */
    (LPTSTR) &lpMsgBuf,
    0,
    NULL 
);

CfLog(cferror,(char *) lpMsgBuf,"");
LocalFree( lpMsgBuf );
}

void
createNTACL(aces, method, filename, action)
struct CFACE *aces;     /* List built up during parsing of config file */
char method;			/* ACL operation method (o/a) */
char *filename;			/* The filename */
enum fileactions action;/* The action to be performed */
{
	/* Declarations */
	struct CFACE *ep;		/* Pointer for iteration purposes */
	struct CFACE *new_aces = 0;	/* Pointer to a new list of CFACEs */
	struct CFACE *temp_ace; /* Temporary pointer to iterate a list later */
	SECURITY_DESCRIPTOR *old_sd = 0;/* Pointer to the file's old SD */
	SECURITY_ATTRIBUTES sa;		/* To hold security info */
	SECURITY_DESCRIPTOR sd;		/* Pointer to a new SD */
	PACL new_pacl = 0;	 		/* Pointer to a new ACL */
	PACL old_pacl = 0;			/* Pointer to the old ACL */
	DWORD newACLSize;		/* To hold the size of all the ACEs */
	BOOL haveACL;			/* Flag to say whether old ACL existed */
	ACL_SIZE_INFORMATION oldACLSize;	/* To hold inof about old ACL */
	ACL_SIZE_INFORMATION newACLSizeInfo;/* To hold inof about new ACL */
	char *win_path = 0;

	/* Convert cygnus-style file path to native win32 one. */

	win_path = malloc(strlen(filename) + CONVERT_PAD);
	if (!win_path) {
        FatalError("Memory Allocation failed");
	}
	cygwin_conv_to_win32_path(filename, win_path);


	/* Allocate memory for list with new ACEs */
	if ((new_aces = (struct CFACE *)calloc(1,sizeof(struct CFACE))) == NULL) {
		FatalError("Memory Allocation failed for new_aces\n");
	}
	new_aces->next = NULL;

	/* Get information about existing ACL if append method */
	if (method == 'a') {
		/* Get information about existing ACL */
		old_sd = getNTACLInformation(win_path, &old_pacl, 
                &haveACL, &oldACLSize);
		if (haveACL) {
			/* The file had an ACL. Append existing ACEs to our new list */
			appendNTACEs(aces, new_aces, oldACLSize, old_pacl);
		} else {
			Verbose("File: %s has no security descriptor (Unrestricted access)!!\n");
		}
	}

    /* 
     * All users from old ACL are added to our list with correct mask
     * (if append). Users in our CFACE list with NTModeMask=0 are users
     * that didn't exist before.  Give them a proper mask.
     */
	for (ep = aces; ep != NULL; ep=ep->next) {
        /* 
         * Skip ACE if name is null or if class is excluded or if mode
         * is "default"
         */
		if ((ep->name == NULL) || (IsExcluded(ep->classes)) 
                || (strcmp(ep->mode,"default") == 0)) {
			continue;
		}

        /* 
         * User has noNTModeMask. Give him one and attach him to our new
         * list. 
         */
		if (ep->NTMode == 0) {
			attachToNTACEs(new_aces, ep->name, ep->mode, 
                    ep->access, ep->classes, 
                    getNTModeMask(ep->mode,ep->NTMode));
		}
	}

	/* Get the size of all ACE's that will be inserted in the new ACL */
	if ((newACLSize = getNTACEs_Size(new_aces)) == 0) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Size of ACEs is zero.\n", g_vprefix);
		CfLog(cferror,g_output,"");
		goto done;
	}

	/* Add the size of an empty ACL */
	newACLSize += sizeof(ACL);

	/* Initialize a Security Descriptor for the new ACL*/
	if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
		snprintf(g_output,CF_BUFSIZE*2,"%s: acl: Unable to init new SD\n",
                g_vprefix);
		CfLog(cferror,g_output,"");
		goto done;
	}

	/* Allocate memory for the new ACL */
	if ((new_pacl = (PACL)calloc(1,newACLSize)) == NULL) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Unable to allocate memory for new ACL\n", g_vprefix);
		CfLog(cferror,g_output,"");
		goto done;
	}

	/* Initialize the new ACL */
	if (!InitializeAcl(new_pacl, newACLSize, ACL_REVISION)) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Unable to initialize the new ACL\n", g_vprefix);
		CfLog(cferror,g_output,"");
		goto done;
	}

	/* Adding all denied aces first, then all allowed aces */
	AddNTACEs("denied",new_aces,&new_pacl);
	AddNTACEs("allowed",new_aces,&new_pacl);

	/* Set the new ACL into the Security Descriptor */
	if (!SetSecurityDescriptorDacl(&sd, TRUE, new_pacl, FALSE)) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Unable to install new ACL\n", g_vprefix);
		CfLog(cferror,g_output,"");
		goto done;
	}

	/* Check the new Security Descriptor */
	if (!IsValidSecurityDescriptor(&sd)) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: New Security Descriptor is invalid\n", g_vprefix);
		CfLog(cferror,g_output,"");
		goto done;
	}

	/* Install new ACL and new Security Descriptor */
	if (!SetFileSecurity(win_path, DACL_SECURITY_INFORMATION, &sd)) {
		snprintf(g_output,CF_BUFSIZE*2,
                "%s: acl: Unable to set file security on file: %s\n", 
                g_vprefix, filename);
		CfLog(cferror,g_output,"");
		LogWinError();
		goto done;
	}

done:
	/* Release memory */
	if (win_path) free(win_path);
	if (new_pacl) free(new_pacl);
	if (old_sd) free(old_sd);
	ep = new_aces;
	while (ep) {
		temp_ace = ep;
		ep = ep->next;
		free(temp_ace);
	}

	Verbose("ACL installed successfully for file: %s\n",filename);
}

#endif

/* ----------------------------------------------------------------- */

int CheckNTACE(aces,method,filename,action)
struct CFACE *aces;     /* List built up during parsing of config file */
char method;			/* ACL operation method (o/a) */
char *filename;			/* The filename */
enum fileactions action;/* The action to be performed */
{
#ifdef NT
 struct CFACE *ep;
 Debug2("\n------------------------ ACL info ---------------------------------\n");
 Debug2("- Filesystem is ntfs.\n");
 Debug2("- Filename is %s\n",filename);
 Debug2("- Method is %c\n",method);
 Debug2("- Action is element # %d of enum fileactions\n",action);

 /* Create the new ACL */
 createNTACL(aces, method, filename, action);

 return true;
#else
 Verbose("This system is not NT. Can't create ACLs on this system...\n");
 return false;
#endif
}

/*************************** END NT Addition *******************************/

int 
CheckPosixACE(struct CFACE *aces, char method, char *filename, 
        enum fileactions action)
{
#if defined(HAVE_SYS_ACL_H) && defined(SOLARIS)
    struct CFACE *ep;
    aclent_t aclbufp[MAX_ACL_ENTRIES], newaclbufp[MAX_ACL_ENTRIES], tmpacl;
    int nacl = 0, newacls = 0, status = 0, update = 0, i, j;
    struct stat sb;
    uid_t myuid;
    
    memset(aclbufp,0, sizeof(aclbufp));
    memset(newaclbufp,0, sizeof(newaclbufp));
    nacl = acl (filename, GETACLCNT, 0, NULL);
    nacl = acl (filename, GETACL, nacl, aclbufp);
    
    if (stat(filename, &sb) != 0) {
        CfLog(cferror,"","stat");
        return -1;
    }

    Verbose(" Old acl has %d entries and is:\n", nacl);
 
    for (i = 0; i < nacl; i++) {
        Debug1(" a_type = %x, \ta_id = %d, \ta_perm = %o\n",
        aclbufp[i].a_type, aclbufp[i].a_id, aclbufp[i].a_perm);
    }

    Debug1("method = %c\n", method);
    
    if (method == 'a') {
        newacls = nacl;
        for (i = 0; i < nacl; i++) {
            newaclbufp[i].a_id = aclbufp[i].a_id;
            newaclbufp[i].a_type = aclbufp[i].a_type;
            newaclbufp[i].a_perm = aclbufp[i].a_perm;
        }
    }   

    for (ep = aces; ep !=NULL; ep = ep->next) {
        if (ep->name == NULL) {
            continue;
        }
        if(IsExcluded(ep->classes)) {
            continue;
        }

        Verbose("%s: Mode =%s, name=%s, type=%s\n",
                g_vprefix,ep->mode,ep->name,ep->acltype);
        if (strcmp(ep->name, "*") == 0) {
            memset(ep->name,0, sizeof(ep->name));
        }
        
        if (BuildAclEntry(&sb,ep->acltype,ep->name,&tmpacl) == 0) {
            status = 0;
            for (i = 0; i < newacls; i++) {
                if (newaclbufp[i].a_id == tmpacl.a_id && 
                        newaclbufp[i].a_type == tmpacl.a_type) {
                    status = 1;
                    if (strcmp(ep->mode, "default") == 0) {
                        snprintf(g_output, CF_BUFSIZE*2,
                            "Deleting ACL entry %d: "
                            "type = %x,\tid = %d,\tperm = %o\n",
                            i, newaclbufp[i].a_type, newaclbufp[i].a_id, 
                            newaclbufp[i].a_perm);

                        CfLog(cfverbose,g_output,"");
                        newacls--;
                        newaclbufp[i].a_id = newaclbufp[newacls].a_id;
                        newaclbufp[i].a_type = newaclbufp[newacls].a_type;
                        newaclbufp[i].a_perm = newaclbufp[newacls].a_perm;
                    } else {
                        tmpacl.a_perm = 
                            ParseSolarisMode(ep->mode, newaclbufp[i].a_perm);

                        if (tmpacl.a_perm != newaclbufp[i].a_perm) {
                            newaclbufp[i].a_id = tmpacl.a_id;
                            newaclbufp[i].a_type = tmpacl.a_type;
                            newaclbufp[i].a_perm = tmpacl.a_perm;
                            
                            snprintf(g_output,CF_BUFSIZE*2,
                                "Replaced ACL entry %d: "
                                "type = %x,\tid = %d,\tperm = %o\n",
                                i, newaclbufp[i].a_type, newaclbufp[i].a_id, 
                                newaclbufp[i].a_perm);

                            CfLog(cfverbose,g_output,"");
                        }
                    }
                }
            }
        }
        if (status == 0 && (strcmp(ep->mode, "default") != 0)) {
            newaclbufp[newacls].a_id = tmpacl.a_id;
            newaclbufp[newacls].a_type = tmpacl.a_type;
            newaclbufp[newacls].a_perm = ParseSolarisMode(ep->mode, 0);
            newacls++;
       
            snprintf(g_output,CF_BUFSIZE*2,
                "Added ACL entry %d: type = %x,\tid = %d,\tperm = %o\n",
                i, newaclbufp[i].a_type, newaclbufp[i].a_id, 
                newaclbufp[i].a_perm);

            CfLog(cfverbose,g_output,"");
        }
    }

    if (newacls != nacl) {
        update = 1;
    } else {
        for (i = 0 ; i < nacl ; i++) {
            status = 0;
            for (j = 0 ; j < newacls ; j++) {
                if (aclbufp[i].a_id == newaclbufp[j].a_id && 
                        aclbufp[i].a_type == newaclbufp[j].a_type && 
                        aclbufp[i].a_perm == newaclbufp[j].a_perm) {
                    status = 1;
                }
            }
            if (status == 0)
                update = 1;
        }
    }

    if (update == 0) {
        Verbose("no update necessary\n");
        return false;
    }
    
    if (action == warnall || action == warnplain || action == warndirs) {
        snprintf(g_output,CF_BUFSIZE*2,"File %s needs ACL update\n",filename);
        CfLog(cfinform,g_output,"");
        return false;
    }
    
    if ((status = aclcheck(newaclbufp, newacls, &i)) != 0) {
        printf("aclcheck failed\n");
        aclsortperror(status);
        return false;
    }
    
    if (aclsort(newacls, 0, (aclent_t *) newaclbufp) != 0) {
        printf("%s: aclsort failed\n", g_vprefix);
        return false;
    }
 
    Debug1("new acl has %d entries and is:\n", newacls);
    for (i = 0; i < newacls; i++) {
        Debug1 ("a_type = %x,\ta_id = %d,\ta_perm = %o\n",
        newaclbufp[i].a_type, newaclbufp[i].a_id, newaclbufp[i].a_perm);
    }

    Debug1("setting acl of %s with %d acl-entries\n", filename, newacls);

    myuid = getuid();
    if (sb.st_uid != myuid) {
        if (myuid == 0) {
            Verbose("Changing effective uid to %ld\n", (long)sb.st_uid);
            if (seteuid(sb.st_uid) == -1) {
                
                snprintf(g_output,CF_BUFSIZE*2,
                        "Couldn't set effective uid from %ld to %ld\n",
                        (long)myuid,(long)sb.st_uid);

                CfLog(cferror,g_output,"seteuid");
                return false;
            }
            Debug1("effective uid now %ld\n", (long)sb.st_uid);
        } else {

            snprintf(g_output,CF_BUFSIZE*2,
                "Can't set effective uid from %ld to %ld, not super-user!\n",
                (long)myuid, (long)sb.st_uid);

            CfLog(cferror,g_output,"seteuid");
            return false;
        }
        Debug1("now the correct uid to manage acl of %s\n", filename, newacls);
        
        
        if (acl (filename, SETACL, newacls, newaclbufp) != newacls) {
            CfLog(cferror,"","acl");
            return false;
        }
        
        Debug1("setting acl of %s resulted in %d acl-entries\n",
            filename, newacls);
        
        if (seteuid(myuid) == -1) {

            snprintf(g_output,CF_BUFSIZE*2,
                    "Unable to regain privileges of user %ld\n",
                    (long)myuid);

            CfLog(cferror,g_output,"seteuid");
            FatalError("Aborting cfng");
        }
    } else {
        if (acl(filename, SETACL, newacls, newaclbufp) != newacls) {
            CfLog(cferror,"","acl");
            return false;
        }
        Debug1("setting acl of %s resulted in %d acl-entries\n", 
                filename, newacls);
    }
 
    snprintf(g_output,CF_BUFSIZE*2,"ACL for file %s updated\n",filename);
    CfLog(cfinform,g_output,"");
    
    return true;
#else
    Verbose("Can't do ACLs on this system...\n");
    return false;
#endif
}
