dnl This macro checks for the existance of ibdm library and defines the
dnl corresponding path variable
dnl
dnl Synopsys:
dnl    CHECK_IBDM_TCLLIB()
dnl
dnl Result:
dnl    IBDM_TCLLIB - points to the directory holding the ibdmX.Y library
dnl
AC_DEFUN([CHECK_IBDM_TCLLIB],[

dnl Define a way for the user to provide the path
AC_ARG_WITH(ibdm-lib,
[  --with-ibdm-lib=<dir> define where to find IBDM TCL library],
AC_MSG_NOTICE(IBDM: given path:$with_ibdm_lib),
with_ibdm_lib="none")

dnl if we were not given a path - try finding one:
if test "x$with_ibdm_lib" = xnone; then
   dirs="/usr/lib /usr/local/lib /usr/local/ibgd/lib /usr/local/ibg2/lib /usr/local/ibed/lib /usr/local/ofed/lib"
   for d in $dirs; do
     if test -d $d/ibdm1.0; then
        with_ibdm_lib=$d
        AC_MSG_NOTICE(IBDM: found in:$with_ibdm_lib)
     fi
   done
   for d in $dirs; do
     if test -d ${d}64/ibdm1.0; then
        with_ibdm_lib=${d}64
        AC_MSG_NOTICE(IBDM: found in:$with_ibdm_lib)
     fi
   done
fi

AC_MSG_NOTICE(IBDM: using TCL lib from:$with_ibdm_lib)
AC_SUBST(with_ibdm_lib)
])
