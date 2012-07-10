/*
 * $Id: locks.c 748 2004-05-25 14:47:10Z skaar $
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
 * Toolkit: Locks and signals.
 */

/* 
 * A log file of the run times is kept for each host separately.  This
 * records each atomic lock and the time at which it completed, for use
 * in computing the elapsed time. The file format is:
 *
 *      %s time:operation:operand
 *
 * Each operation (independently of operand) has a "last" inode which
 * keeps the time at which is last completed, for use in calculating
 * IfElapsed. The idea here is that the elapsed time is from the time at
 * which the last operation of this type FINISHED. This is different
 * from a lock (which is used to allow several sub operations to
 * coexist). Here we are limiting activity in general to avoid
 * "spamming".
 *
 * Each atomic operation (including operand) has a lock. The removal of
 * this lock causes the "last" file to be updated.  This is used to
 * actually prevent execution of an atom which is already being
 * executed. If this lock has existed for longer than the ExpireAfter
 * time, the process owning the lock is killed and the lock is
 * re-established. The lock file contains the pid.
 *
 * This is robust to hanging locks and can be thought of as a garbage
 * collection mechanism for these locks.
 *
 * Last files are just inodes (empty files) so they use no disk.  The
 * locks (which never exceed the no of running processes) contain the
 * pid.
 */

#include "cf.defs.h"
#include "cf.extern.h"

# include <db.h>

DB *DBP;

struct LockData {
   pid_t pid;
   time_t time;
};

/* ----------------------------------------------------------------- */

void
PreLockState(void)
{
    strcpy(g_cflock, "pre-lock-state");
}

/* ----------------------------------------------------------------- */

void
SaveExecLock(void)
{
    strcpy(g_savelock, g_cflock);
}


/* ----------------------------------------------------------------- */

void
RestoreExecLock(void)
{
    strcpy(g_cflock, g_savelock);
}

/* ----------------------------------------------------------------- */

void
HandleSignal(int signum)
{
    snprintf(g_output, CF_BUFSIZE*2,
            "Received signal %d (%s) while doing [%s]",
            signum, g_signals[signum], g_cflock);

    Chop(g_output);
    CfLog(cferror, g_output, "");
    snprintf(g_output, CF_BUFSIZE*2, "Logical start time %s ",
            ctime(&g_cfstarttime));
    Chop(g_output);
    CfLog(cferror, g_output, "");

    snprintf(g_output, CF_BUFSIZE*2,
            "This sub-task started really at %s\n",
            ctime(&g_cfinitstarttime));

    CfLog(cferror, g_output, "");
    fflush(stdout);

    if (signum == SIGTERM || signum == SIGINT || signum == SIGHUP 
            || signum == SIGSEGV || signum == SIGKILL) {
        ReleaseCurrentLock();
        closelog();
        exit(0);
    }
}

/* ----------------------------------------------------------------- */

void
InitializeLocks(void)
{
    int errno;

    if (g_ignorelock) {
        return;
    }

    snprintf(g_lockdb, CF_BUFSIZE, "%s/cfng_lock.db", g_vlockdir);

    if ((errno = db_create(&DBP, NULL,0)) != 0) {
        snprintf(g_output, CF_BUFSIZE*2,
                "Couldn't open lock database %s\n", g_lockdb);
        CfLog(cferror, g_output, "db_open");
        g_ignorelock = true;
        return;
    }

#ifdef CF_OLD_DB
    if ((errno = DBP->open(DBP, g_lockdb, NULL, DB_BTREE,
                    DB_CREATE, 0644)) != 0)
#else
    if ((errno = DBP->open(DBP, NULL, g_lockdb, NULL, DB_BTREE,
                    DB_CREATE, 0644)) != 0)
#endif
    {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Couldn't open lock database %s\n", g_lockdb);
        CfLog(cferror, g_output, "db_open");
        g_ignorelock = true;
        return;
    }

}

/* ----------------------------------------------------------------- */

void
CloseLocks(void)
{
    if (g_ignorelock) {
        return;
    }

    DBP->close(DBP,0);
}

/* ----------------------------------------------------------------- */

int
GetLock(char *operator,char *operand,int ifelapsed,int expireafter,
        char *host,time_t now)
{
    unsigned int pid;
    int err;
    time_t lastcompleted = 0, elapsedtime;

    if (g_ignorelock) {
        return true;
    }

    if (now == 0) {
        if ((now = time((time_t *)NULL)) == -1) {
            printf("Couldn't read system clock\n");
        }
        return true;
    }

    Debug("GetLock(%s,%s,time=%d), ExpireAfter=%d, IfElapsed=%d\n",
            operator, operand, now, expireafter, ifelapsed);

    memset(g_cflock, 0, CF_BUFSIZE);
    memset(g_cflast, 0, CF_BUFSIZE);

    snprintf(g_cflog, CF_BUFSIZE, "%s/cfng_lock.log", g_vlogdir);

    snprintf(g_cflock, CF_BUFSIZE, "lock.%.100s.%.40s.%s.%.100s",
            g_vcanonicalfile, host, operator, operand);

    snprintf(g_cflast, CF_BUFSIZE, "last.%s.100.%.40s.%s.%.100s",
            g_vcanonicalfile, host, operator, operand);

    if (strlen(g_cflock) > MAX_FILENAME) {
        /* most nodenames are 255 chars or less */
        g_cflock[MAX_FILENAME] = '\0';
    }

    if (strlen(g_cflast) > MAX_FILENAME) {
        /* most nodenames are 255 chars or less */
        g_cflast[MAX_FILENAME] = '\0';
    }

    /* Look for non-existent (old) processes */

    lastcompleted = GetLastLock();
    elapsedtime = (time_t)(now-lastcompleted) / 60;

    if (elapsedtime < 0) {

        snprintf(g_output, CF_BUFSIZE*2,
                "Another cfagent seems to have done %s.%s since I started\n",
                operator, operand);

        CfLog(cfverbose, g_output,"");
        return false;
    }

    if (elapsedtime < ifelapsed) {

        snprintf(g_output, CF_BUFSIZE*2,
                "Nothing scheduled for %s.%s (%u/%u minutes elapsed)\n",
                operator, operand, elapsedtime, ifelapsed);

        CfLog(cfverbose, g_output,"");
        return false;
    }

    /* Look for existing (current) processes */

    lastcompleted = CheckOldLock();
    elapsedtime = (time_t)(now-lastcompleted) / 60;

    if (lastcompleted != 0) {
        if (elapsedtime >= expireafter) {

            snprintf(g_output, CF_BUFSIZE*2,
                    "Lock %s expired...(after %u/%u minutes)\n",
                    g_cflock, elapsedtime, expireafter);

            CfLog(cfinform, g_output, "");

            pid = GetLockPid(g_cflock);

            if (pid == -1) {

                snprintf(g_output, CF_BUFSIZE*2,
                        "Illegal pid in corrupt lock %s - ignoring lock\n",
                        g_cflock);

                CfLog(cferror, g_output,"");
            } else {
                Verbose("Trying to kill expired process, pid %d\n", pid);

                err = 0;

                /* if (((err = kill(pid,SIGCONT)) == -1) && (errno != ESRCH))
                        sleep(3);
                        err=0;

                Does anyone remember who put this in or why it was here?
                */

                if ((err = kill(pid,SIGINT)) == -1) {
                    sleep(1);
                    err=0;

                    if ((err = kill(pid,SIGTERM)) == -1) {
                        sleep(5);
                        err=0;

                        if ((err = kill(pid,SIGKILL)) == -1) {
                            sleep(1);
                        }
                    }
                }

                if (err == 0 || errno == ESRCH) {
                    LockLog(pid, "Lock expired, process killed",
                            operator, operand);
                    unlink(g_cflock);
                } else {

                    snprintf(g_output, CF_BUFSIZE*2, 
                            "Unable to kill expired cfagent process %d "
                            "from lock %s, exiting this time..\n",
                            pid, g_cflock);

                    CfLog(cferror, g_output, "kill");

                    FatalError("");
                }
            }
        } else {
            Verbose("Couldn't obtain lock for %s "
                    "(already running!)\n", g_cflock);
            return false;
        }
    }

    SetLock();
    return true;
}

/* ----------------------------------------------------------------- */

void
ReleaseCurrentLock(void)
{
    if (g_ignorelock) {
        return;
    }

    Debug("ReleaseCurrentLock(%s)\n", g_cflock);

    if (strlen(g_cflast) == 0) {
        return;
    }

    if (DeleteLock(g_cflock) == -1) {
        Debug("Unable to remove lock %s\n", g_cflock);
        return;
    }

    if (PutLock(g_cflast) == -1) {
        snprintf(g_output, CF_BUFSIZE*2, "Unable to create %s\n", g_cflast);
        CfLog(cferror, g_output, "creat");
        return;
    }

    LockLog(getpid(), "Lock removed normally ", g_cflock,"");
    strcpy(g_cflock, "no_active_lock");
}


/* ----------------------------------------------------------------- */

/* Count the number of active locks == number of cfengines running */
int
CountActiveLocks(void)
{
    int count = 0;
    DBT key,value;
    DBC *dbcp;
    struct LockData entry;
    time_t elapsedtime;

    Debug("CountActiveLocks()\n");

    if (g_ignorelock) {
        return 1;
    }

    memset(&value, 0, sizeof(value));
    memset(&key, 0, sizeof(key));

    InitializeLocks();

    if ((errno = DBP->cursor(DBP, NULL, &dbcp, 0)) != 0) {
        CfLog(cfverbose, "Couldn't dump lock db", "");
        return -1;
    }

    while ((errno = dbcp->c_get(dbcp, &key, &value, DB_NEXT)) == 0) {
        if (value.data != NULL) {
            bcopy(value.data, &entry, sizeof(entry));

            elapsedtime = (time_t)(g_cfstarttime-entry.time) / 60;

            if (elapsedtime >= g_vexpireafter) {

                Debug("LOCK-DB-EXPIRED: %s %s\n",
                        ctime(&(entry.time)), key.data);

                continue;
            }

            Debug("LOCK-DB-DUMP   : %s %s\n", ctime(&(entry.time)), key.data);

            if (strncmp(key.data,"lock",4) == 0) {
                count++;
            }
        }
    }

    CloseLocks();
    Debug("Found %d active/concurrent cfagents", count);
    return count;
}

/*
 * --------------------------------------------------------------------
 * Level 2
 * --------------------------------------------------------------------
 */

time_t
GetLastLock(void)
{
    time_t mtime;

    Debug("GetLastLock()\n");

    /* 
     * Do this to prevent deadlock loops from surviving if IfElapsed >
     * T_sched 
     */

    if ((mtime = GetLockTime(g_cflast)) == -1) {

        if (PutLock(g_cflast) == -1) {
            snprintf(g_output, CF_BUFSIZE*2, "Unable to lock %s\n", g_cflast);
            CfLog(cferror, g_output, "");
            return 0;
        }
        return 0;
    } else {
        return mtime;
    }
}

/* ----------------------------------------------------------------- */

time_t
CheckOldLock(void)
{
    time_t mtime;

    Debug("CheckOldLock(%s)\n", g_cflock);

    if ((mtime = GetLockTime(g_cflock)) == -1) {
        Debug("Unable to find lock data %s\n", g_cflock);
        return 0;
    } else {
        Debug("Lock %s last ran at %s\n", g_cflock, ctime(&mtime));
        return mtime;
    }
}

/* ----------------------------------------------------------------- */

void
SetLock(void)
{
    Debug("SetLock(%s)\n", g_cflock);

    if (PutLock(g_cflock) == -1) {
        snprintf(g_output, CF_BUFSIZE*2,
                "GetLock: can't open new lock file %s\n", g_cflock);
        CfLog(cferror, g_output, "fopen");
        FatalError("");;
    }
}


/*
 * --------------------------------------------------------------------
 * Level 3
 * --------------------------------------------------------------------
 */

int
PutLock(char *name)
{
    DBT key,value;
    struct LockData entry;

    Debug("PutLock(%s)\n", name);

    memset(&value, 0, sizeof(value));
    memset(&key, 0, sizeof(key));

    key.data = name;
    key.size = strlen(name)+1;

    InitializeLocks();

    if (g_ignorelock) {
        return 0;
    }

    if ((errno = DBP->del(DBP, NULL, &key, 0)) != 0) {
        Debug("Unable to delete lock [%s]: %s\n", name, db_strerror(errno));
    }

    key.data = name;
    key.size = strlen(name)+1;
    entry.pid = getpid();
    entry.time = time((time_t *)NULL);
    value.data = &entry;
    value.size = sizeof(entry);

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD

    if (pthread_mutex_lock(&MUTEX_LOCK) != 0) {
        CfLog(cferror, "pthread_mutex_lock failed", "pthread_mutex_lock");
    }

#endif

    if ((errno = DBP->put(DBP, NULL, &key, &value, 0)) != 0) {
        CfLog(cferror, "put failed", "db->put");
        CloseLocks();
        return -1;
    }

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD

    if (pthread_mutex_unlock(&MUTEX_LOCK) != 0) {
        CfLog(cferror, "pthread_mutex_unlock failed", "unlock");
    }

#endif

    CloseLocks();
    return 0;
}

/* ----------------------------------------------------------------- */

int
DeleteLock(char *name)
{
    DBT key;

    if (strcmp(name, "pre-lock-state") == 0) {
        return 0;
    }

    memset(&key, 0, sizeof(key));

    key.data = name;
    key.size = strlen(name)+1;

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD
    if (pthread_mutex_lock(&MUTEX_LOCK) != 0) {
        CfLog(cferror, "pthread_mutex_lock failed", "pthread_mutex_lock");
    }
#endif

    InitializeLocks();

    if ((errno = DBP->del(DBP, NULL, &key,0)) != 0) {
        Debug("Unable to delete lock [%s]: %s\n", name, db_strerror(errno));
    }

#if defined HAVE_LIBPTHREAD || defined BUILDTIN_GCC_THREAD

if (pthread_mutex_unlock(&MUTEX_LOCK) != 0)
   {
   CfLog(cferror, "pthread_mutex_unlock failed", "unlock");
   }

#endif

    CloseLocks();
    return 0;
}

/* ----------------------------------------------------------------- */

time_t
GetLockTime(char *name)
{
    DBT key,value;
    struct LockData entry;

    memset(&key, 0, sizeof(key));
    memset(&value, 0, sizeof(value));
    memset(&entry, 0, sizeof(entry));

    key.data = name;
    key.size = strlen(name)+1;

    InitializeLocks();

    if ((errno = DBP->get(DBP, NULL, &key, &value, 0)) != 0) {
        CloseLocks();
        return -1;
    }

    if (value.data != NULL) {
        bcopy(value.data, &entry, sizeof(entry));
        CloseLocks();
        return entry.time;
    }

    CloseLocks();
    return -1;
}

/* ----------------------------------------------------------------- */

pid_t
GetLockPid(char *name)
{
    DBT key,value;
    struct LockData entry;

    memset(&value, 0, sizeof(value));
    memset(&key, 0, sizeof(key));

    key.data = name;
    key.size = strlen(name)+1;

    InitializeLocks();

    if ((errno = DBP->get(DBP, NULL, &key, &value, 0)) != 0) {
        CloseLocks();
        return -1;
    }

    if (value.data != NULL) {
        bcopy(value.data, &entry, sizeof(entry));
        CloseLocks();
        return entry.pid;
    }

    CloseLocks();
    return -1;
}

/* ----------------------------------------------------------------- */

void
LockLog(int pid, char *str, char *operator, char *operand)
{
    FILE *fp;
    char buffer[CF_MAXVARSIZE];
    struct stat statbuf;
    time_t tim;

    Debug("LockLog(%s)\n", str);

    if ((fp = fopen(g_cflog, "a")) == NULL) {
        snprintf(g_output, CF_BUFSIZE*2, 
                "Can't open lock-log file %s\n", g_cflog);
        CfLog(cferror, g_output, "fopen");
        FatalError("");
    }

    if ((tim = time((time_t *)NULL)) == -1) {
        Debug("Cfengine: couldn't read system clock\n");
    }

    sprintf(buffer, "%s", ctime(&tim));

    Chop(buffer);

    fprintf(fp, "%s:%s:pid=%d:%s:%s\n", buffer, str, pid, operator, operand);

    fclose(fp);

    if (stat(g_cflog, &statbuf) != -1) {
        if (statbuf.st_size > CFLOGSIZE) {
            Verbose("Rotating lock-runlog file\n");
            RotateFiles(g_cflog, 2);
        }
    }
}
