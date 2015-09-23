dnl This macro checks for the existance of swig and defines the
dnl corresponding SWIG variable.
dnl
dnl Synopsys:
dnl    MLX_PROG_SWIG(maj.min.patch, eq|lt)
dnl The second parameter defines the required relation of the found version
dnl to the requested version:
dnl lt - the found version required to be newer or equal to the maj.min.patch
dnl eq - the found version required to be equal to the maj.min.patch
dnl
dnl Result:
dnl    HAS_SWIG conditional - set to 1 if swig was found or 0 if not
dnl    SWIG - the executable name
dnl
AC_DEFUN([MLX_PROG_SWIG],[
   AC_PATH_PROG([SWIG],[swig])
   # we use this to track the existance of swig
   has_swig=0
   if test -z "$SWIG" ; then
      AC_MSG_WARN([cannot find 'swig' program. You should look at http://www.swig.org])
      SWIG='echo "Error: SWIG is not installed. You should look at http://www.swig.org" ; false'
   elif test -n "$1" ; then
      AC_MSG_CHECKING([for SWIG version])
      [swig_version=`$SWIG -version 2>&1 | grep 'SWIG Version' | sed 's/.*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/g'`]
      AC_MSG_RESULT([$swig_version])
      if test -n "$swig_version" ; then
         # Calculate the required version number components
         [required=$1]
         [required_major=`echo $required | sed 's/[^0-9].*//'`]
         if test -z "$required_major" ; then
            [required_major=0]
         fi
         [required=`echo $required | sed 's/[0-9]*[^0-9]//'`]
         [required_minor=`echo $required | sed 's/[^0-9].*//'`]
         if test -z "$required_minor" ; then
            [required_minor=0]
         fi
         [required=`echo $required | sed 's/[0-9]*[^0-9]//'`]
         [required_patch=`echo $required | sed 's/[^0-9].*//'`]
         if test -z "$required_patch" ; then
            [required_patch=0]
         fi
         # Calculate the available version number components
         [available=`echo $swig_version | sed 's/[^0-9]*//'`]
         [available_major=`echo $available | sed 's/[^0-9].*//'`]
         if test -z "$available_major" ; then
            [available_major=0]
         fi
         [available=`echo $available | sed 's/[0-9]*[^0-9]//'`]
         [available_minor=`echo $available | sed 's/[^0-9].*//'`]
         if test -z "$available_minor" ; then
            [available_minor=0]
         fi
         [available=`echo $available | sed 's/[0-9]*[^0-9]//'`]
         [available_patch=`echo $available | sed -e 's/.*Patch[^0-9]*//' -e 's/[^0-9]*//g' `]
         if test -z "$available_patch" ; then
            [available_patch=0]
         fi
         # we have two modes of comparison...
         if test x"$2" == xeq; then
            if test $available_major -ne $required_major \
               -o $available_minor -ne $required_minor \
               -o $available_patch -ne $required_patch ; then
               AC_MSG_WARN([SWIG version == $1 is required.  You have $available_major.$available_minor.$available_patch. You should look at http://www.swig.org ])
               SWIG='echo "Error: SWIG version == $1 is required.  You have '"$swig_version"'.  You should look at http://www.swig.org" ; false'
            else
               has_swig=1
            fi
         else
            if test $available_major -ne $required_major \
               -o $available_minor -ne $required_minor \
               -o $available_patch -lt $required_patch ; then
               AC_MSG_WARN([SWIG version >= $1 is required.  You have $swig_version.  You should look at http://www.swig.org])
               SWIG='echo "Error: SWIG version >= $1 is required.  You have '"$swig_version"'.  You should look at http://www.swig.org" ; false'
            else
               has_swig=1
            fi
        fi
      else
         AC_MSG_WARN([cannot determine SWIG version])
         SWIG='echo "Error: Cannot determine SWIG version.  You should look at http://www.swig.org" ; false'
      fi
   fi
   if test ! -z "$has_swig"; then
        dnl AC_MSG_INFO([SWIG executable is '$SWIG'])
        SWIG_LIB=`$SWIG -swiglib`
        dnl AC_MSG_INFO([SWIG runtime library directory is '$SWIG_LIB'])
        AM_CONDITIONAL(HAS_SWIG,[test 1])
   else
        AM_CONDITIONAL(HAS_SWIG,[test 0])
   fi
   AC_SUBST([SWIG_LIB])
])
