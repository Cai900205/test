
dnl ibdm.m4: an autoconf for IBDM reference
dnl
dnl
dnl To use this macro, just do OPENIB_APP_IBDM.  It outputs
dnl with-ibdm
dnl If not successful, with_ibdm has the value "no".

AC_DEFUN([OPENIB_APP_IBDM], [
# --- BEGIN OPENIB_APP_IBDM ---
dnl To link against IBDM, configure does several things to make my life
dnl "easier".
dnl
dnl * if the user did define ibdm is look for it in "standard" places
dnl * if can not be found - ask the user for --with-ibdm
dnl

dnl Define a way for the user to provide path to ibdm
AC_ARG_WITH(ibdm,
[  --with-ibdm=<dir> define where to find IBDM],
AC_MSG_NOTICE(Using IBDM from:$with_ibdm),
with_ibdm="none")

dnl if the user did not provide --with-ibdm look for it in reasonable places
if test "x$with_ibdm" = xnone; then
   if test -d /usr/include/ibdm; then
      with_ibdm=/usr
   elif test -d [`pwd`]/ibdm; then
      with_ibdm=[`pwd`]/ibdm
   elif test -d [`pwd`]/../ibdm; then
      with_ibdm=[`pwd`]/../ibdm
   else
      AC_MSG_ERROR([--with-ibdm must be provided - failed to find standard IBDM installation])
   fi
fi

dnl validate the defined path
if test -f $with_ibdm/include/ibdm/Fabric.h; then
   AC_MSG_NOTICE([IBDM was installed in $with_ibdm])
   ibdm_ref_is_used=0
elif test -f $with_ibdm/ibdm/Fabric.h; then
   AC_MSG_NOTICE([IBDM building from sources: $with_ibdm])
   if test ! -d ../ibdm; then
     AC_MSG_ERROR([IBDM sources provided - but ibdm was not built in ../ibdm])
   fi
   dnl we actually want to create a link to the ibdm sources
   ln -s $with_ibdm/ibdm src/ibdm
   ibdm_ref_is_used=1
else
   AC_MSG_ERROR([ IBDM Fabric.h not found under $with_ibdm/include/ibdm/])
fi

AM_CONDITIONAL([IBDM_REF_IS_USED], test $ibdm_ref_is_used = 1)

AC_SUBST(with_ibdm)

# --- OPENIB_APP_IBDM ---
]) dnl OPENIB_APP_IBDM

