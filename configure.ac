dnl ##########################################################################
dnl #
dnl # Process this file with autoconf then autoheader to build a configure script
dnl # automake -a -c; autoconf
dnl #
dnl ##########################################################################

AC_PREREQ(2.52)

AC_INIT(src/cfagent.c)
AC_CANONICAL_TARGET

dnl
dnl The version in the next line is the only one to set
dnl

AM_INIT_AUTOMAKE(cfng, 0.1.1)   dnl remember to set version

AM_CONFIG_HEADER(src/conf.h)

#
# Add to the default list of places in LDFLAGS to compensate for
# ... the configure default value of LIBS on some systems
#
for x in /usr/local/gnu/lib /usr/local/gnulib /opt/dce/lib /sw/lib
do
  if test -d "$x"; then
    y=`expr " $LDFLAGS " : ".* -L$x "`
    if test $y -eq 0; then
      LDFLAGS="$LDFLAGS -L$x"
    fi
  fi
done

#
# Add to the default list of places in CPPFLAGS to match LDFLAGS above
# 
for x in /usr/local/gnu/include /opt/dce/include /sw/include
do 
  if test -d "$x"; then
    y=`expr " $CPPFLAGS " : ".* -I$x "`
    if test $y -eq 0; then
      CPPFLAGS="$CPPFLAGS -I$x"
    fi
  fi
done

ACX_PTHREAD
CC=$PTHREAD_CC
CFLAGS="$PTHREAD_CFLAGS $CFLAGS"
CPPFLAGS="$PTHREAD_CPPFLAGS $CPPFLAGS"
LDFLAGS="$PTHREAD_LDFLAGS $LDFLAGS"

dnl ######################################################################
dnl Checks for programs.
dnl ######################################################################

AC_PROG_CC
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_RANLIB
AC_PROG_INSTALL

AC_CONFIG_LIBOBJ_DIR(pub)
AC_FUNC_GETLOADAVG 


AC_PATH_PROG(LATEX, latex, false, $PATH:$prefix/bin:/usr/bin:/usr/local/bin)
AC_PATH_PROG(TEXI2DVI, texi2dvi, false, $PATH:$prefix/bin:/usr/bin:/usr/local/bin)
AC_PATH_PROG(DVIPS, dvips, false, $PATH:$prefix/bin:/usr/bin:/usr/local/bin)
AC_PATH_PROG(MAKEINFO, makeinfo, false, $PATH:$prefix/bin:/usr/bin:/usr/local/bin)
AC_PATH_PROG(PS2PDF, ps2pdf, false, $PATH:$prefix/bin:/usr/bin:/usr/local/bin)

dnl ######################################################################
dnl Checks for libraries.
dnl ######################################################################

AC_CHECK_LIB(m,main)
AC_CHECK_FUNC(door_create,door)
AC_CHECK_FUNC(setsockopt, , AC_CHECK_LIB(socket, setsockopt))
AC_CHECK_FUNC(gethostent, , AC_CHECK_LIB(nsl, gethostent))
AC_CHECK_FUNC(getaddrinfo, , AC_CHECK_LIB(socket, getaddrinfo))

dnl AC_CHECK_FUNC(yp_get_default_domain, , AC_CHECK_LIB(nsl, yp_get_default_domain))

AC_SEARCH_LIBS(socket, socket, ,
    [AC_CHECK_LIB(nsl, socket, LIBS="$LIBS -lsocket -lnsl", , -lsocket)])


dnl
dnl  Look for Berkeley DB.
dnl 

AC_ARG_WITH(berkeleydb,
    [  --with-berkeleydb[=PATH] directory where BerkeleyDB exists],
    BERKELEY_DB_DIR=$with_berkeleydb,
    BERKELEY_DB_DIR=yes)
AC_MSG_CHECKING(for BerkeleyDB location)
if test "x$BERKELEY_DB_DIR" = "xno" ; then
    AC_MSG_RESULT( )
    AC_MSG_ERROR(This release of cfng requires BerkeleyDB 3.2 or later)
else
    if test "x$BERKELEY_DB_DIR" = "xyes" ; then
        for v in BerkeleyDB.3.2 BerkeleyDB.3.3 BerkeleyDB.4.0  BerkeleyDB.4.1  BerkeleyDB.4.2 BerkeleyDB.4.3 BerkeleyDB.4.4; do
            for d in /opt /usr/local /usr; do
                test -d "$d/$v" && BERKELEY_DB_DIR="$d/$v"
            done
        done
    fi
    if test "x$BERKELEY_DB_DIR" = "xyes" ; then
        for d in /opt /usr/local /usr; do
            for v in db-4 db4 db3 db; do
                if test -f "$d/include/$v/db.h" ; then
                    test "x$d" != "x/usr" && BERKELEY_DB_LDFLAGS="-L$d/lib"
                    BERKELEY_DB_CFLAGS="-I$d/include/$v"
                    late_LIBS=$LIBS
		    # In RedHat 8, for instance, we have /usr/include/db4
		    # and libdb-4.0.a.  Debian has /usr/lib/libdb-4.1.a, for
		    # instance.  Look for the appropriate library.
                    if test $v = db4; then
			AC_SEARCH_LIBS(db_create, 
				[db-4 db4 db-4.5 db-4.4 db-4.3 db-4.2 db-4.1 db-4.0], 
				[BERKELEY_DB_LIB=$ac_cv_search_db_create])
		    else
                    	BERKELEY_DB_LIB="-l$v"
                    fi
		    LIBS=$late_LIBS
                    AC_MSG_RESULT($d)
                    break
                fi
            done
	    test "x$BERKELEY_DB_LIB" != "x" && break
            if test -f "$d/include/db.h"; then
                if test "x$d" != "x/usr"; then
                    BERKELEY_DB_LDFLAGS="-L$d/lib"
                    BERKELEY_DB_CFLAGS="-I$d/include"
                fi
                BERKELEY_DB_LIB="-ldb"
                AC_MSG_RESULT($d)
                break
            fi
	done
        if test "x$BERKELEY_DB_LIB" = "x" ; then
            AC_MSG_ERROR(Cannot find BerkeleyDB)
        fi
    elif test -f "$BERKELEY_DB_DIR/include/db.h";then
        case "$target_os" in
        solaris*)  BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib -R$BERKELEY_DB_DIR/lib";;
        *)         BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib";;
        esac
  
        BERKELEY_DB_CFLAGS="-I$BERKELEY_DB_DIR/include"
        BERKELEY_DB_LIB="-ldb"
        AC_MSG_RESULT($BERKELEY_DB_DIR)
    elif test -d "$BERKELEY_DB_DIR"; then

        BERKELEY_DB_CFLAGS="-I$BERKELEY_DB_DIR/include"
        for v in . db44 db43 db42 db41 db40 db4 db33 db32 db3 db; do
            if test -f "$BERKELEY_DB_DIR/include/$v/db.h"; then
                BERKELEY_DB_CFLAGS="-I$BERKELEY_DB_DIR/include/$v"
            fi
        done

        BERKELEY_DB_LIB="-ldb"
        for v in db41 db40 db4 db33 db32 db3 db; do
            if test -f "$BERKELEY_DB_DIR/lib/lib$v.so"; then
                BERKELEY_DB_LIB="-l$v"
            fi
        done

        case "$target_os" in
        solaris*)  BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib -R$BERKELEY_DB_DIR/lib";;
        *)         BERKELEY_DB_LDFLAGS="-L$BERKELEY_DB_DIR/lib";;
        esac
  
        AC_MSG_RESULT($BERKELEY_DB_DIR)
    else
        AC_MSG_ERROR(Cannot find BerkeleyDB)
    fi
    AC_DEFINE(USE_BERKELEY_DB, 1, [Define if BerkeleyDB is available.])

    BERKELEY_DB_SAVE_LDFLAGS=$LDFLAGS
    BERKELEY_DB_SAVE_CPPFLAGS=$CPPFLAGS
    BERKELEY_DB_SAVE_LIBS=$LIBS
    LDFLAGS="$LDFLAGS $BERKELEY_DB_LDFLAGS"
    CFLAGS="$CFLAGS $BERKELEY_DB_CFLAGS"
    LIBS="$LIBS $BERKELEY_DB_LIB"

    AC_MSG_CHECKING(Berkeley DB Version)

    AC_TRY_RUN(
       [
#include <db.h>
#include <stdio.h>
int main(void)
{
    printf("%d.%d.%d ",DB_VERSION_MAJOR,DB_VERSION_MINOR,DB_VERSION_PATCH);
    if (DB_VERSION_MAJOR < 3 || (DB_VERSION_MAJOR == 3 && DB_VERSION_MINOR < 2))
        exit(1);
    exit(0);
}
       ],
       [AC_MSG_RESULT(OK)],
       [AC_MSG_ERROR(This release of cfng requires BerkeleyDB 3.2 or later)]
    )

    AC_MSG_CHECKING(Berkeley DB API)

    AC_TRY_RUN(
       [
#include <db.h>
#include <stdio.h>
int main(void)
{
    printf("%d.%d.%d ",DB_VERSION_MAJOR,DB_VERSION_MINOR,DB_VERSION_PATCH);
    if (DB_VERSION_MAJOR < 4 || (DB_VERSION_MAJOR == 4 && DB_VERSION_MINOR < 1))
        exit(1);
    exit(0);
}
       ],
       [AC_MSG_RESULT(OK)],
       [AC_DEFINE(CF_OLD_DB,1,Define if old Berkeley API)]
    )

    AC_CHECK_LIB(db, db_create)

    LDFLAGS=$BERKELEY_DB_SAVE_LDFLAGS
    CPPFLAGS=$BERKELEY_DB_SAVE_CPPFLAGS
    LIBS=$BERKELEY_DB_SAVE_LIBS

fi
AC_SUBST(BERKELEY_DB_LDFLAGS)
AC_SUBST(BERKELEY_DB_CFLAGS)
AC_SUBST(BERKELEY_DB_LIB)


###
### Search for openssl
###


AC_ARG_WITH(openssl,
    [  --with-openssl[=PATH] directory where OpenSSL exists],
    OPENSSL_LIB_DIR=$with_openssl,
    OPENSSL_LIB_DIR=yes)
AC_MSG_CHECKING(for OpenSSL location)
if test x"$OPENSSL_LIB_DIR" = xno ; then
    AC_MSG_RESULT(no)
    AC_MSG_ERROR(This release of cfng requires OpenSSL 0.9.6b or later)
else
    if test x"$OPENSSL_LIB_DIR" = xyes ; then
        for d in /opt/ssl /usr/local/ssl /usr/local /usr; do
            if test -f "$d/include/openssl/opensslv.h"; then
                OPENSSL_LIB_LDFLAGS="-L$d/lib"
                OPENSSL_LIB_CPPFLAGS="-I$d/include"
                OPENSSL_LIB_LIB="-lcrypto"
                AC_MSG_RESULT($d)
                break
            fi
        done
        if test x"$OPENSSL_LIB_LIB" = x ; then
            AC_MSG_ERROR(Cannot find OpenSSL)
        fi
    elif test -f "$OPENSSL_LIB_DIR/include/openssl/ssl.h";then
        case "$target_os" in
        solaris*)  OPENSSL_LIB_LDFLAGS="-L$OPENSSL_LIB_DIR/lib -R$OPENSSL_LIB_DIR/lib";;
        *)         OPENSSL_LIB_LDFLAGS="-L$OPENSSL_LIB_DIR/lib";;
        esac
  
        OPENSSL_LIB_CPPFLAGS="-I$OPENSSL_LIB_DIR/include"
        OPENSSL_LIB_LIB="-lcrypto"
        AC_MSG_RESULT($OPENSSL_LIB_DIR)
    else
        AC_MSG_ERROR(Cannot find OpenSSL)
    fi
    AC_DEFINE(USE_OPENSSL_LIB, 1, [Define if OpenSSL is available.])

    OPENSSL_SAVE_LDFLAGS=$LDFLAGS
    OPENSSL_SAVE_CPPFLAGS=$CPPFLAGS
    OPENSSL_SAVE_LIBS=$LIBS

    LDFLAGS="$LDFLAGS $OPENSSL_LIB_LDFLAGS"
    CPPFLAGS="$CPPFLAGS $OPENSSL_LIB_CPPFLAGS"
    LIBS="$LIBS $OPENSSL_LIB_LIB"

    AC_MSG_CHECKING(OpenSSL Version)

    AC_TRY_RUN(
       [
#include <openssl/opensslv.h>
int main(void)
{
    if (OPENSSL_VERSION_NUMBER < 0x0090602fL)
        exit(1);
    exit(0);
}
       ],
       [AC_MSG_RESULT(OK)],
       [AC_MSG_ERROR(This release of cfng requires openssl 0.9.6b or later)]
    )

    dnl ######################################################################
    dnl Checks for OpenSSL crypto lib
    dnl ######################################################################

    AC_CHECK_LIB(crypto,main)


    LDFLAGS=$OPENSSL_SAVE_LDFLAGS
    CPPFLAGS=$OPENSSL_SAVE_CPPFLAGS
    LIBS=$OPENSSL_SAVE_LIBS

fi
AC_SUBST(OPENSSL_LIB_LDFLAGS)
AC_SUBST(OPENSSL_LIB_CPPFLAGS)
AC_SUBST(OPENSSL_LIB_LIB)



dnl ######################################################################
dnl Checks for regular expressions
dnl GNU regex lib if you have regcomp in libc you should NOT have this lib too
dnl ######################################################################

AC_CHECK_LIB(rx, main)

dnl ######################################################################
dnl Checks for header files.
dnl ######################################################################

AC_CHECK_HEADERS(unistd.h stdlib.h sys/loadavg.h)
AC_CHECK_HEADERS(sys/mount.h)
AC_CHECK_HEADERS(utime.h)
AC_CHECK_HEADERS(time.h)
AC_CHECK_HEADERS(sys/time.h)
AC_CHECK_HEADERS(malloc.h)
AC_CHECK_HEADERS(vfs.h)
AC_CHECK_HEADERS(sys/vfs.h)
AC_CHECK_HEADERS(sys/sockio.h)
AC_CHECK_HEADERS(sys/statvfs.h)
AC_CHECK_HEADERS(sys/statfs.h)
AC_CHECK_HEADERS(sys/param.h)
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(sys/filesys.h)
AC_CHECK_HEADERS(dustat.h)
AC_CHECK_HEADERS(regex.h rxposix.h)
AC_CHECK_HEADERS(sys/systeminfo.h)
AC_CHECK_HEADERS(sys/acl.h)

AC_HEADER_STDC
AC_HEADER_TIME
AC_HEADER_SYS_WAIT
AC_HEADER_DIRENT


dnl ######################################################################
dnl Checks for data types
dnl ######################################################################

AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T
AC_TYPE_PID_T

dnl ######################################################################
dnl Checks for typedefs, structures, and compiler characteristics.
dnl ######################################################################

AC_C_CONST

dnl ######################################################################
dnl Check for special functions
dnl ######################################################################

AC_CHECK_FUNCS(getcwd getnetgrent waitpid seteuid setegid setreuid setregid)
AC_CHECK_FUNCS(uname gethostname realpath regcomp chflags)
AC_CHECK_FUNCS(strstr putenv drand48 srand48 getaddrinfo)
AC_CHECK_FUNCS(bcopy mkfifo statfs statvfs door)
AC_CHECK_FUNCS(sysinfo setsid strdup strrchr strerror snprintf sysconf)

dnl #######################################################################
dnl Newer BSD systems don't have a compatible rtentry - use ortentry
dnl #######################################################################
 
rtry=none
AC_MSG_CHECKING(for either struct rtentry or struct ortentry)
AC_EGREP_HEADER(rtentry, net/route.h, rtry=rtentry)
if test "$rtry" = rtentry; then
 AC_DEFINE(HAVE_RTENTRY)
fi
AC_EGREP_HEADER(ortentry, net/route.h, rtry=ortentry)
if test "$rtry" = ortentry; then
 AC_DEFINE(HAVE_ORTENTRY)
fi
AC_MSG_RESULT([$rtry])

dnl #######################################################################
dnl Handle DCE + threading stuff
dnl #######################################################################

AC_CHECK_LIB(pthread,main)         dnl POSIX threads

if test "$ac_cv_lib_pthread_main" = "yes"; then
	AC_CHECK_HEADERS(pthread.h)
	AC_CHECK_HEADERS(sched.h)
fi

AC_CHECK_FUNC(lchown, AC_DEFINE(HAVE_LCHOWN))

AC_CHECK_FUNC(pthread_attr_setstacksize, AC_DEFINE(HAVE_PTHREAD_ATTR_SETSTACKSIZE), AC_CHECK_LIB(pthread, pthread_create))
AC_CHECK_FUNC(pthread_sigmask, AC_DEFINE(HAVE_PTHREAD_SIGMASK), AC_CHECK_LIB(pthread, pthread_create))

AC_ARG_ENABLE(DCE,[  --enable-DCE            support for DCE and ACLs],[
  case "$enableval" in
    no) 
        ;;
    *)
	AC_CHECK_LIB(thread, main) 
	AC_CHECK_LIB(dce, main)            dnl DFS
	AC_CHECK_HEADERS(dce/daclif.h)
	AC_CHECK_HEADERS(pthread.h)
	AC_CHECK_HEADERS(sched.h)
	;;
  esac
])

dnl #####################################################################
dnl Configure WORKDIR
dnl #####################################################################

AC_ARG_WITH(workdir,
	[  --with-workdir=WORKDIR  default internal for trusted cache ],
	[
		if test x$withval != x ; then
			WORKDIR=$withval
		else
			WORKDIR=/var/cfng
		fi
		AC_DEFINE_UNQUOTED(WORKDIR, "${WORKDIR}")
	],
	[
		WORKDIR=/var/cfng

		AC_DEFINE_UNQUOTED(WORKDIR, "${WORKDIR}")
	]
)


dnl ######################################################################
dnl OS specific stuff
dnl ######################################################################

dnl To ensure conf.h is picked up via VPATH
if test "$srcdir" != "."; then
  CPPFLAGS="$CPPFLAGS -I`pwd`/src"
fi

case "$target_os" in

   sunos3*)
	AC_DEFINE(SUN3)
        ;;
   sunos4*)
	AC_DEFINE(SUN4)
        ;;
   solaris2.4*)
	AC_DEFINE(SOLARIS)
        AC_DEFINE(HAVE_SYS_ACL_H,)
        CPPFLAGS="-w $CPPFLAGS"
        LIBS="$LIBS -lelf"
        ;;
   solaris2.5*)
	AC_DEFINE(SOLARIS)
        AC_DEFINE(_POSIX_C_SOURCE)
        AC_DEFINE(__EXTENSIONS__)
        LIBS="$LIBS -lelf -lsec"
        ;;
   solaris2*)
	AC_DEFINE(SOLARIS)
        AC_DEFINE(__BIT_TYPES_DEFINED__) # avoid conflict with db.h
        AC_DEFINE(_POSIX_C_SOURCE)
        AC_DEFINE(__EXTENSIONS__)
        LIBS="$LIBS -lelf -lsec"
        RUNTIME_LDFLAGS="-R${WORKDIR}/lib"
        ;;
   ultrix*)
	AC_DEFINE(ULTRIX)
        ;;
   hpux*|hp-ux*) 
	AC_DEFINE(HPuUX)
        if test "$GCC" != "yes"; then
	  AC_DEFINE(REGEX_MALLOC)
        fi

        LIBS="$LIBS -lc"
	AC_CHECK_LIB(PW, main)
        ;;
   aix*)
	AC_DEFINE(AIX)
        CPPFLAGS="$CPPFLAGS -w"
	AC_CHECK_LIB(pthreads, main)
        AC_CHECK_HEADER(pthreads.h)
        ;;
   osf*)
	AC_DEFINE(OSF)
	if test "$GCC" = yes; then
	   AC_MSG_WARN([pthreads may not work with GCC under Tru64])
	   AC_MSG_WARN([If you get build errors mentioning PTHREADDEFAULTS etc.,])
	   AC_MSG_WARN([re-configure with CC=cc.])
	fi
        ;;
   irix6*)
	# rtentry is detected OK on a 6.5.19 system.
        AC_DEFINE(HAVE_ORTENTRY) # Have to hack this for 6.* owing to bug
	AC_DEFINE(IRIX)
        CFLAGS="$CFLAGS -w"
        ;;
   irix4*)
	AC_DEFINE(IRIX)
        CFLAGS="$CFLAGS -w"
        LIBS="$LIBS -lsun"
        ;;
   irix*)
	AC_DEFINE(IRIX)
        CFLAGS="$CFLAGS -w"
        ;;
   linux*)
	AC_DEFINE(LINUX)
	AC_CHECK_LIB(nss_nis, yp_get_default_domain)
    RUNTIME_LDFLAGS="-Xlinker -rpath -Xlinker ${WORKDIR}/lib"
        ;;
   freebsd*)
	AC_DEFINE(FREEBSD)
        CFLAGS="$CFLAGS -pthread -D_THREAD_SAFE -DBUILDTIN_GCC_THREAD"
        AC_CHECK_HEADERS(pthread.h)
        ;;
   netbsd*)
	AC_DEFINE(NETBSD)
        ;;
   newsos*)
	AC_DEFINE(NEWS_OS)
        ;;
   bsd/os*)
	AC_DEFINE(BSDOS)
        ;;
   bsd*)
	AC_DEFINE(BSD43)
        ;;
   aos*)
	AC_DEFINE(AOS)
        ;;
   nextstep*)
	AC_DEFINE(NEXTSTEP)
        ;;

   unicos*)
	AC_DEFINE(CFCRAY)
        ;;

   cray*)
	AC_DEFINE(CFCRAY)
        ;;

   openbsd*|obsd*)
	AC_DEFINE(OPENBSD)
    RUNTIME_LDFLAGS="-Xlinker -rpath -Xlinker ${WORKDIR}/lib"
        ;;

   gnu*)
	AC_DEFINE(CFGNU)
        ;;

   sysv4.2MP|unix_sv*)
	AC_DEFINE(UNIXWARE)
        AC_CHECK_LIB(thread,main)         dnl POSIX threads, weird setup
        if test "$ac_cv_lib_thread_main" = "yes"; then
	   AC_CHECK_HEADERS(thread.h)
        fi
        ;;

   cygwin*)
        AC_DEFINE(NT)
        ;;

   sco*)
        AC_DEFINE(SCO)
        ;;

   darwin*)
        AC_DEFINE(DARWIN)
        LDFLAGS="-Xlinker -m $LDFLAGS"
        ;;

   *)
	AC_MSG_ERROR(Unknown system type $target_os)
        ;;
esac

AC_SUBST(RUNTIME_LDFLAGS)

dnl LIBS="-L../pub -lpub $LIBS"


dnl #####################################################################
dnl Fix for lex/flex
dnl #####################################################################

AC_MSG_CHECKING(8-bit support in Flex)

if test "$LEX" = "flex"; then
   EIGHTBIT="\200-\377"
   AC_SUBST(EIGHTBIT)
   AC_MSG_RESULT(8-bit support added)
   NOWRAP="%option noyywrap"
else
   AC_MSG_RESULT(no 8-bit support)
   NOWRAP=""
fi

AC_SUBST(NOWRAP)

LIBS="-L../pub -lpub $LIBS"

dnl ####################################################################
dnl Set GCC CFLAGS only if using GCC.
dnl ####################################################################

AC_MSG_CHECKING(Checking for GCC Specific compile flags)
if test x"$GCC" = "xyes"; then
	dnl This is a really bad default
	GCC_CFLAG="-g -O2 -Wreturn-type -Wmissing-prototypes -Wuninitialized"		
	AC_MSG_RESULT(yes)
	AC_SUBST(GCC_CFLAG)
else 
	GCC_CFLAG=""
	AC_MSG_RESULT(no)
	AC_SUBST(GCC_CFLAG)
fi


dnl #####################################################################
dnl Hostname and Version stuff
dnl #####################################################################

AC_PATH_PROG(HOSTNAME, hostname, "", $PATH)

AC_DEFINE_UNQUOTED(AUTOCONF_HOSTNAME, "`$HOSTNAME`")
AC_DEFINE_UNQUOTED(AUTOCONF_SYSNAME, "$target_os")


dnl ######################################################################
dnl Now make the Makefiles
dnl ######################################################################

AC_OUTPUT(pub/Makefile src/Makefile doc/Makefile misc/Makefile Makefile \
	src/cflex.l misc/cfengine/Makefile misc/cfengine/inputs/Makefile \
	misc/cfengine/contrib/Makefile misc/cfengine/contrib/vicf)

rm -f src/cfparse.c src/cfparse.h

AC_MSG_RESULT(DONE: Configuration done. Run make/gmake to build cfng.)

