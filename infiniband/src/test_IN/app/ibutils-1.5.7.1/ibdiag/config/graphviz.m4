dnl This macro checks for the existance of Graphviz libs
dnl
dnl Synopsys:
dnl    SET_GRAPHVIZ_LIBS()
dnl
dnl Result:
dnl    graphviz_lib - points to the directory holding the lib
dnl
AC_DEFUN([SET_GRAPHVIZ_LIBS],[

dnl Define a way for the user to provide the path
AC_ARG_WITH(graphviz-lib,
[  --with-graphviz-lib=<dir> define directory which holds the graphviz lib installation],
AC_MSG_NOTICE(GRAPHVIZ: given path:$with_graphviz_lib),
with_graphviz_lib="none")

dnl if we were not given a path - try finding one:
if test "x$with_graphviz_lib" = "xnone"; then
   dirs="/usr /usr/local /usr/local/ibgd /usr/local/ibg2 /usr/local/ibed /usr/local/ofed"
   for d in $dirs; do
     if test -e $d/lib/graphviz/tcl/libtcldot.so; then
        with_graphviz_lib=$d/lib
        AC_MSG_NOTICE(GRAPHVIZ: found in:$with_graphviz_lib)
     fi
     if test -e $d/lib64/graphviz/tcl/libtcldot.so; then
        with_graphviz_lib=$d/lib64
        AC_MSG_NOTICE(GRAPHVIZ: found in:$with_graphviz_lib)
     fi
   done
fi

if test "x$with_graphviz_lib" = "xnone"; then
	AC_MSG_WARN(GRAPHVIZ: failed to find graphviz/tcl/libtcldot.so lib. Please use --with-graphviz-lib or else ibdiagui will not run properly)
fi

AC_SUBST(with_graphviz_lib)
])
