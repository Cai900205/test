
dnl tcl.m4: an autoconf Tcl locator
dnl
dnl
dnl BUGS
dnl   The command-line arguments are overcomplicated.
dnl   There are doubtlessly others...

dnl To use this macro, just do MLX_LANG_TCL.  It outputs
dnl TCL_LIBS, TCL_CPPFLAGS, and TCL_DEFS and SUBSTs them.
dnl If successful, these have stuff in them.  If not, they're empty.
dnl If not successful, with_tcl has the value "no".

AC_DEFUN([MLX_LANG_TCL], [
# --- BEGIN MLX_LANG_TCL ---
dnl To link against Tcl, configure does several things to make my life
dnl "easier".
dnl
dnl * maybe ask the user where they think Tcl lives, and try to find it
dnl * maybe ask the user what "tclsh" is called this week (i.e., "tclsh8.0")
dnl * run tclsh, ask it for a path, then run that path through sed
dnl * sanity check its result (many installs are a little broken)
dnl * try to figure out where Tcl is based on this result
dnl * try to guess where the Tcl include files are
dnl
dnl Notes from previous incarnations:
dnl > XXX MUST CHECK FOR TCL BEFORE KERBEROS V4 XXX
dnl > This is because some genius at MIT named one of the Kerberos v4
dnl > library functions log().  This of course conflicts with the
dnl > logarithm function in the standard math library, used by Tcl.
dnl
dnl > Checking for Tcl first puts -lm before -lkrb on the library list.
dnl

dnl do not do anything if we are in disabled libcheck mode...
if test "x$libcheck" = "xtrue"; then

   dnl Check for some information from the user on what the world looks like
   AC_ARG_WITH(tclconfig,[  --with-tclconfig=PATH   use tclConfig.sh from PATH
                             (configure gets Tcl configuration from here)],
           dnl trim tclConfig.sh off the end so we can add it back on later.
   	TclLibBase=`echo ${withval} | sed s/tclConfig.sh\$//`)
   AC_ARG_WITH(tcl,      [  --with-tcl=PATH         use Tcl from PATH],
   	TclLibBase="${withval}/lib")
   AC_ARG_WITH(tclsh,    [  --with-tclsh=TCLSH      use TCLSH as the tclsh program
                             (let configure find Tcl using this program)],
   	TCLSH="${withval}")

   if test "x$TCLSH" = "xno" -o "x$with_tclconfig" = "xno" ; then
     AC_MSG_WARN([Tcl disabled because tclsh or tclconfig specified as "no"])
     with_tcl=no
   fi

   if test "x$with_tcl" != xno; then
     if test \! -z "$with_tclconfig" -a \! -d "$with_tclconfig" ; then
       AC_MSG_ERROR([--with-tclconfig requires a directory argument.])
     fi

     if test \! -z "$TCLSH" -a \! -x "$TCLSH" ; then
       AC_MSG_ERROR([--with-tclsh must specify an executable file.])
     fi

     if test X"$TclLibBase" = X; then # do we already know?
       # No? Run tclsh and ask it where it lives.

       # Do we know where a tclsh lives?
       if test X"$TCLSH" = X; then
         # Try and find tclsh.  Any tclsh.
         # If a new version of tcl comes out and unfortunately adds another
         # filename, it should be safe to add it (to the front of the line --
         # somef vendors have older, badly installed tclshs that we want to avoid
         # if we can)
         AC_PATH_PROGS(TCLSH, [tclsh tclsh8.3 tclsh8.4], "unknown")
       fi

       # Do we know where to get a tclsh?
       if test "X${TCLSH}" != "Xunknown"; then
         AC_MSG_CHECKING([where Tcl says it lives])
         dnl to avoid .tclshrc issues use from a file...
         echo "puts \$tcl_library" > /tmp/tcl.conf.$$
         TclLibBase=`${TCLSH} /tmp/tcl.conf.$$ | sed -e 's,[^/]*$,,'`
         rm /tmp/tcl.conf.$$
          AC_MSG_RESULT($TclLibBase)
       fi
     fi

     if test -z "$TclLibBase" ; then
       AC_MSG_RESULT([can't find tclsh])
       AC_MSG_WARN([can't find Tcl installtion; use of Tcl disabled.])
       with_tcl=no
     else
       AC_MSG_CHECKING([for tclConfig.sh])
       # Check a list of places where the tclConfig.sh file might be.
       # Note we prefer the 64 bit version if exists
       tclCondifSearchPath="$tclCondifSearchPath ${TclLibBase}"
       tclCondifSearchPath="$tclCondifSearchPath ${TclLibBase}/.."
       tclCondifSearchPath="$tclCondifSearchPath `echo ${TCLSH} | sed s/sh//`"
       tclCondifSearchPath="$tclCondifSearchPath /usr/lib64"
       tclCondifSearchPath="$tclCondifSearchPath /usr/lib"
       tclCondifSearchPath="$tclCondifSearchPath /usr/local/lib"
       for tcldir in $tclCondifSearchPath; do
         if test -f "${tcldir}/tclConfig.sh"; then
           TclLibBase="${tcldir}"
           break
         fi
       done

       if test -z "${TclLibBase}" ; then
         AC_MSG_RESULT("unknown")
         AC_MSG_WARN([can't find Tcl configuration; use of Tcl disabled.])
         with_tcl=no
       else
         AC_MSG_RESULT(${TclLibBase}/)
       fi

       if test "X${with_tcl}" != Xno ; then
         AC_MSG_CHECKING([Tcl configuration on what Tcl needs to compile])
         . ${TclLibBase}/tclConfig.sh
         AC_MSG_RESULT(ok)
         dnl no TK stuff for us.
         dnl . ${TclLibBase}/tkConfig.sh
       fi

       dnl We hack the provided TCL_LIB_SPEC since it is using the /usr/lib even
       dnl if the build was using lib64
       if test -d /usr/lib64 ; then
          TCL_LIB_SPEC=`echo ${TCL_LIB_SPEC} | sed 's=/usr/lib =/usr/lib64 =g'`
       fi

       if test "X${with_tcl}" != Xno ; then
         dnl Now, hunt for the Tcl include files, since we don't strictly
         dnl know where they are; some folks put them (properly) in the
         dnl default include path, or maybe in /usr/local; the *BSD folks
         dnl put them in other places.
         AC_MSG_CHECKING([where Tcl includes are])
         for tclinclude in "${TCL_PREFIX}/include/tcl${TCL_VERSION}" \
                           "${TCL_PREFIX}/include/tcl" \
                           "${TCL_PREFIX}/include" ; do
           if test -r "${tclinclude}/tcl.h" ; then
             TCL_CPPFLAGS="-I${tclinclude}"
             break
           fi
         done
         if test X"${TCL_CPPFLAGS}" = X ; then
           AC_MSG_WARN(can't find Tcl includes; use of Tcl disabled.)
           with_tcl=no
         fi
         AC_MSG_RESULT(${TCL_CPPFLAGS})
       fi

       # Finally, pick up the Tcl configuration if we haven't found an
       # excuse not to.
       if test "X${with_tcl}" != Xno; then
         dnl TCL_LIBS="${TK_LIB_SPEC} ${TK_XLIBSW} ${TCL_LD_SEARCH_FLAGS} ${TCL_LIB_SPEC}"
         dnl we are using libtool so need to convert to -rpath if at all
         TCL_SEARCH=`echo ${TCL_LD_SEARCH_FLAGS} | sed 's/-Wl,-rpath,/-rpath/'`

         dnl sometimes we got empty libs: use TCL_LIB_FILE
         if test X"$TCL_LIBS" = X; then
           dnl extract the lib style name...
           TCL_LIBS=`echo ${TCL_LIB_FILE} | sed 's/lib\([[^ \t]]*\)\.\(so\|a\)/-l\1/'`
         fi

         dnl sometimes we got empty libs: use TCL_LIB_SPEC
         if test X"$TCL_LIB_SPEC" = X; then
           dnl extract the lib style name...
           TCL_LIB_SPEC='-L/usr/lib'
         fi

         TCL_LIBS1="${TCL_LIB_SPEC} ${TCL_LIBS}"
         dnl Filter out the ieee - I do not see a shared version for it.
         TCL_LIBS=`echo ${TCL_LIBS1} | sed 's/-lieee//'`
       fi
     fi
   fi
else
dnl disbled libcheck mode - we do not need anything...
   TCL_DEFS=disabled
   TCL_LIBS=disabled
   TCL_CPPFLAGS=disabled
   TCL_PREFIX=disabled
fi

AC_SUBST(TCL_DEFS)
AC_SUBST(TCL_LIBS)
AC_SUBST(TCL_CPPFLAGS)
AC_SUBST(TCL_PREFIX)

# --- END MLX_LANG_TCL ---
]) dnl MLX_LANG_TCL

