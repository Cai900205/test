
dnl osm.m4: an autoconf for OpenSM (vendor and complib) reference
dnl
dnl
dnl To use this macro, just do OPENIB_APP_OSM.
dnl The following variables are defined:
dnl with-osm - the osm installation prefix
dnl OSM_CFLAGS - CFLAGS additions required (-I and debug )
dnl OSM_LDFLAGS - a variable holding link directives
dnl OSM_VENDOR - The type of vendor library available (ts, sim)
dnl OSM_BUILD - The type of build used for buikding OpenSM either gen1 or openib
dnl
dnl Several conditionals are also defined:
dnl OSM_BUILD_OPENIB - set when the build type is openib (gen2)
dnl OSM_VENDOR_TS - should use gen1/gen2 API
dnl OSM_VENDOR_SIM - interface a simulator vendor
dnl If successful, these have stuff in them.  If not, they're empty.
dnl If not successful, with_osm has the value "no".

AC_DEFUN([OPENIB_APP_OSM], [
# --- BEGIN OPENIB_APP_OSM ---
dnl To link against OpenSM Vendor or Complib, configure does several
dnl things to make my life "easier".
dnl
dnl * if the user did define where opensm is look for it in "standard" places
dnl * if can not be found - ask the user for --with-osm
dnl * figure out if OpenSM was compiles in debug mode or not
dnl
dnl As we might have an OpenSM installation of IBGD or OpenIB and
dnl different vendors too we need to use some heuristics for
dnl figuring out the type of both STACK and VENDOR.
dnl Heuristics is:
dnl
dnl If with_osm is not provided look fo it under:
dnl   /usr/local/ibgd/apps/osm
dnl   /usr/mellanox/osm
dnl   /usr/mellanox/osm-sim
dnl   /usr/local/lib/libopensm.a
dnl   /usr/lib/libopensm.a
dnl
dnl Figure out what kind of BUILD it is gen1 or gen2:
dnl if the with_osm/include/infiniband exists we are on gen2 stack
dnl
dnl Now decide what vendor was built:
dnl if gen2 build
dnl  if $with_osm/lib/osmvendor_gen1.so -> ts
dnl  if $with_osm/lib/osmvendor_mtl.so -> mtl
dnl  if $with_osm/lib/osmvendor_sim.so -> sim
dnl  if $with_osm/lib/osmvendor.so -> openib
dnl if gen1 build
dnl  if $with_osm/lib/osmsvc_ts.so -> ts
dnl  if $with_osm/lib/osmsvc_mtl.so -> mtl
dnl  if $with_osm/lib/osmsvc_sim.so -> sim
dnl ----------------------------------------------------------------

dnl Define a way for the user to provide path to OpenSM
AC_ARG_WITH(osm,
[  --with-osm=<dir> define OSM prefix],
AC_MSG_NOTICE(Using OSM from:$with_osm),
with_osm="none")

dnl Define a way for the user to provide path to OpenSM libs
AC_ARG_WITH(osm-libs,
[  --with-osm-libs=<dir> define where to find OSM libs],
AC_MSG_NOTICE(Using OSM libs from:$with_osm_libs),
with_osm_libs="none")

osm_lib_dir="none"

if test "x$libcheck" = "xtrue"; then
   dnl if the user did not provide --with-osm look for it in reasonable places
   if test "x$with_osm" = xnone; then
      if test -d /usr/local/ibgd/apps/osm; then
         with_osm=/usr/local/ibgd/apps/osm
      elif test -d /usr/mellanox/osm; then
         with_osm=/usr/mellanox
      dnl if it is an ofed or gen2 install, we can use /etc/infiniband/info for searching opensm prefix
      elif test -f /etc/infiniband/info; then
         ofed_dir=`/etc/infiniband/info | grep prefix | cut -d = -f 2`
         if (test -f $ofed_dir/lib/libopensm.so ||
				test -f $ofed_dir/lib64/libopensm.so); then
            with_osm=$ofed_dir
         fi
      elif (test -f /usr/local/lib64/libopensm.so ||
				test -f /usr/local/lib/libopensm.so); then
         with_osm=/usr/local
      elif (test -f /usr/lib64/libopensm.so ||
				test -f /usr/lib/libopensm.so); then
         with_osm=/usr
      else
         AC_MSG_ERROR([OSM: --with-osm must be provided - fail to find standard OpenSM installation])
      fi
   fi
   AC_MSG_NOTICE(OSM: used from $with_osm)

   if test "x$with_osm_libs" = "xnone"; then
   dnl if the user did not provide --with-osm-libs then look for it based on $with_osm
      if (test -f $with_osm/lib64/libosmvendor_gen1.so ||
			 test -f $with_osm/lib64/libosmvendor_vapi.so ||
       	 test -f $with_osm/lib64/libopensm.so         ||
			 test -f $with_osm/lib64/libosmvendor_sim.so); then
         osm_lib_dir=lib64
      else
         osm_lib_dir=lib
      fi
   fi
   with_osm_libs=$with_osm/$osm_lib_dir

   dnl check what build we have gen1 or gen2
   if test -d $with_osm/include/infiniband; then
      OSM_BUILD=openib
   else
      OSM_BUILD=gen1
   fi
   AC_MSG_NOTICE(OSM: build type $OSM_BUILD)

   OSM_LDFLAGS="-Wl,-rpath -Wl,$with_osm_libs -L$with_osm_libs"
   dnl based on the with_osm dir and the libs available
   dnl we can try and decide what vendor was used:
   if test $OSM_BUILD = openib; then
      dnl it is an OpenIB based build but can be any vendor too.
      osm_include_dir="$with_osm/include/infiniband"
      osm_extra_includes="-I$with_osm/include"

      if test -L $with_osm_libs/libosmvendor_gen1.so; then
         OSM_VENDOR=ts
         osm_vendor_sel="-DOSM_VENDOR_INTF_TS"
         OSM_LDFLAGS="$OSM_LDFLAGS -lopensm -losmvendor -losmcomp"
      elif test -L $with_osm_libs/libosmvendor_vapi.so; then
         OSM_VENDOR=mtl
         osm_vendor_sel="-DOSM_VENDOR_INTF_MTL"
         OSM_LDFLAGS="$OSM_LDFLAGS -lopensm -losmvendor -losmcomp -lvapi -lib_mgt"
      elif test -L $with_osm_libs/libosmvendor_sim.so; then
         OSM_VENDOR=sim
         osm_vendor_sel="-DOSM_VENDOR_INTF_SIM"
         OSM_LDFLAGS="$OSM_LDFLAGS -lopensm -losmvendor -libmscli -losmcomp"
      elif test -L $with_osm_libs/libopensm.so; then
         OSM_VENDOR=openib
         osm_vendor_sel="-DOSM_VENDOR_INTF_OPENIB "
         OSM_LDFLAGS="$OSM_LDFLAGS -lopensm -losmvendor -losmcomp -libumad"
      else
         AC_MSG_ERROR([OSM: Fail to recognize vendor type])
      fi
      osm_vendor_sel="$osm_vendor_sel -DOSM_BUILD_OPENIB"
   else
      # we are in gen1 build
      osm_include_dir="$with_osm/include"

      if test -L $with_osm_libs/libosmsvc_ts.so; then
         OSM_VENDOR=ts
         OSM_LDFLAGS="$OSM_LDFLAGS -losmsvc_ts -lcomplib"
         osm_vendor_sel="-DOSM_VENDOR_INTF_TS"
      elif test -L $with_osm_libs/libosmsvc_mtl.so; then
         OSM_VENDOR=mtl
         OSM_LDFLAGS="$OSM_LDFLAGS -losmsvc_mtl -lcomplib -lvapi -lib_mgt"
         osm_vendor_sel="-DOSM_VENDOR_INTF_MTL"
      elif test -L $with_osm_libs/libosmsvc_sim.so; then
         OSM_VENDOR=sim
         OSM_LDFLAGS="$OSM_LDFLAGS -losmsvc_sim -lcomplib"
         osm_vendor_sel="-DOSM_VENDOR_INTF_SIM"
      else
         AC_MSG_ERROR([OSM: Fail to recognize vendor type])
      fi
   fi
   AC_MSG_NOTICE(OSM: vendor type $OSM_VENDOR)


   dnl validate the defined path - so the build id header is there
   AC_CHECK_FILE($osm_include_dir/opensm/osm_config.h,,
      AC_MSG_ERROR([OSM: could not find $with_osm/include/opensm/osm_config.h]))

   dnl now figure out somehow if the build was for debug or not
   grep '#define OSM_DEBUG 1' $osm_include_dir/opensm/osm_config.h > /dev/null
   if test $? -eq 0 ; then
      dnl why did they need so many ???
      osm_debug_flags='-DDEBUG -D_DEBUG -D_DEBUG_ -DDBG'
      AC_MSG_NOTICE(OSM: compiled in DEBUG mode)
   else
      osm_debug_flags=
   fi

   OSM_CFLAGS="-I$osm_include_dir $osm_extra_includes $osm_debug_flags $osm_vendor_sel -D_XOPEN_SOURCE=600 -D_BSD_SOURCE=1"
else
   dnl dummy values
   with_osm=disabled
   OSM_CLFAGS=disabled
   OSM_LDFLAGS=disabled
   OSM_VEDNOR=disabled
   OSM_BUILD=disabled
fi

AM_CONDITIONAL(OSM_VENDOR_TS, test "x$OSM_VENDOR" = xts)
AM_CONDITIONAL(OSM_VENDOR_MTL, test "x$OSM_VENDOR" = xmtl)
AM_CONDITIONAL(OSM_VENDOR_SIM, test "x$OSM_VENDOR" = xsim)
AM_CONDITIONAL(OSM_BUILD_OPENIB, test "x$OSM_BUILD" = xopenib)

AC_SUBST(with_osm)
AC_SUBST(OSM_CFLAGS)
AC_SUBST(OSM_LDFLAGS)
AC_SUBST(OSM_VENDOR)
AC_SUBST(OSM_BUILD)

# --- OPENIB_APP_OSM ---
]) dnl OPENIB_APP_OSM

