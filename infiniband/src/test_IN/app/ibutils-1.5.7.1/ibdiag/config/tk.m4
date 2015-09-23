dnl This macro checks for the existance of tk8.4 or tk8.5 libs
dnl
dnl Synopsys:
dnl    SET_TK_LIBS()
dnl
dnl Result:
dnl    tk_lib - points to the directory holding the tk lib
dnl
AC_DEFUN([SET_TK_LIBS],[

dnl Define a way for the user to provide the path
AC_ARG_WITH(tk-lib,
[  --with-tk-lib=<dir> define directory which holds the tk lib installation],
AC_MSG_NOTICE(TK: given path:$with_tk_lib),
with_tk_lib="none")

dnl if we were not given a path - try finding one:
if test "x$with_tk_lib" = "xnone"; then
   dirs="/usr /usr/local /usr/local/ibgd /usr/local/ibg2 /usr/local/ibed /usr/local/ofed"
   for d in $dirs; do
     if test -e $d/lib/libtk8.4.so -o -e $d/lib/libtk8.5.so; then
        with_tk_lib=$d/lib
        AC_MSG_NOTICE(TK: found in:$with_tk_lib)
     fi
     if test -e $d/lib64/libtk8.4.so -o -e $d/lib64/libtk8.5.so; then
        with_tk_lib=$d/lib64
        AC_MSG_NOTICE(TK: found in:$with_tk_lib)
     fi
   done
fi

if test "x$with_tk_lib" = "xnone"; then
	AC_MSG_ERROR(TK: failed to find tk8.4 or tk8.5 lib. Please use --with-tk-lib)
fi

AC_SUBST(with_tk_lib)
])
