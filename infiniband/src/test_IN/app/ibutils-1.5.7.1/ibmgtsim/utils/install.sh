#!/bin/bash

#--
# Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
#
# This software is available to you under a choice of one of two
# licenses.  You may choose to be licensed under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree, or the
# OpenIB.org BSD license below:
#
#     Redistribution and use in source and binary forms, with or
#     without modification, are permitted provided that the following
#     conditions are met:
#
#      - Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      - Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#--

# pointing to the dir where this install script resides
PACKAGE_ORIG_DIR=`cd ${0%*/*};pwd`

# we modify the srcs so we need to
PACKAGE_DIR=/tmp/ibmgtsim_install_tmp
if [ -d $PACKAGE_DIR ]; then
    rm -rf $PACKAGE_DIR
fi
mkdir -p $PACKAGE_DIR

# copy over the original dir
cp -ap $PACKAGE_ORIG_DIR/* $PACKAGE_DIR/

# go there
cd $PACKAGE_DIR

########################################################################
#
# FUNCTIONS:
#

#Report: if config failed
function config_failed() {
	echo "Config failed ! (see $CFGLOG for details)"
	exit 1
}

#Report: if make failed
function make_failed() {
	echo "Make failed ! (see $MAKELOG for details)"
	exit 1
}

########################################################################

# Initialize default before parsing parameters
PREFIX=/usr
NO_BAR=0

# parse parameters
while [ "$1" ]; do
#  echo "Current parsed param is : $1"
  case $1 in
    "--prefix")
          PREFIX=$2
          shift
          ;;
    "--with-ibdm")
          IBDM_PREFIX=$2
          shift
          ;;
    "--with-tclsh")
          TCLSH=$2
          shift
          ;;
    "--with-osm")
          OSM_PREFIX=$2
          shift
          ;;
    "--batch")
          NO_BAR=1
          ;;
    *)
     echo "Usage: $0 [--prefix <dir>][--with-osm <dir>][--with-ibdm <dir>][--with-tclsh <path>][--batch]"
     echo ""
     echo "Options:"
     echo "   --prefix <dir> : place bin/lib/include into <dir>"
     echo "   --with-osm <dir> : use this OpenSM installation dir"
     echo "   --with-ibdm <dir>: use this IBDM installation dir"
     echo "   --with-tclsh <path>: use this tclsh executable (should be linked with g++)"
     echo "   --batch option : do not use progress bar"
     exit 1
  esac
  shift
done

tar_file=ibmgtsim-1.0.tar.gz

cd $PACKAGE_DIR
if test ! -f $tar_file; then
    echo " Fail to find matching tar file for the current platform:$tar_file"
    exit 1
fi

tar -zxf $tar_file
find . -exec touch {} \;

CFGLOG=/tmp/IBMGTSIM.config.log.$$
rm -f /tmp/IBMGTSIM.config.log.* 2&>1 > /dev/null
echo "configuration as `date`." >| $CFGLOG
echo "configuration log file:" >| $CFGLOG

MAKELOG=/tmp/IBMGTSIM.make.log.$$
rm -f /tmp/IBMGTSIM.make.log.* 2&>1 > /dev/null
echo "IBMGTSIM installation at `date`." >| $MAKELOG
echo "make log file:" >| $MAKELOG

echo IBMGTSIM installation script
echo Copyright \(C\) June 2005, Mellanox Technologies Ltd. ALL RIGHTS RESERVED.
echo Use of software subject to the terms and conditions detailed in the
echo file \"LICENSE.txt\".
echo " "

IBMGTSIMHOME=/usr/mellanox/ibmgtsim
mkdir -p $IBMGTSIMHOME
cp ${PACKAGE_DIR}/BUILD_ID ${IBMGTSIMHOME}/BUILD_ID
cp ${PACKAGE_DIR}/uninstall.sh ${IBMGTSIMHOME}/
cp ${PACKAGE_DIR}/LICENSE.txt ${IBMGTSIMHOME}/

echo ""
echo ""
echo "This installation installs the IBMgtSim components into $PREFIX"

echo " Removing possible previous installations ..."
$PACKAGE_ORIG_DIR/uninstall.sh 2>&1 > /dev/null

cd $IBMGTSIMHOME
export PATH=$PATH:${PACKAGE_DIR}
echo " Configuring $IBMGTSIMHOME directory ..."
cfg="${PACKAGE_DIR}/ibmgtsim-1.0/configure --prefix=${PREFIX} --enable-debug"
if test ! -z $OSM_PREFIX; then
    cfg="$cfg --with-osm=$OSM_PREFIX"
fi
if test ! -z $IBDM_PREFIX; then
    cfg="$cfg --with-ibdm=$IBDM_PREFIX"
fi

if test -z $TCLSH; then
 # prefer the local tcl installation if available:
 local_tclsh=/mswg/projects/ibadm/BINS/`uname -m`/bin/tclsh8.4
 if test -f $local_tclsh; then
     echo " Using tclsh from:$local_tclsh"
     cfg="$cfg --with-tclsh=$local_tclsh"
 fi
else
 echo " Using tclsh from:$TCLSH"
 cfg="$cfg --with-tclsh=$TCLSH"
fi

eval $cfg >> $CFGLOG 2>&1 &
config_pid=$!
wait $config_pid >/dev/null 2>&1
if [ $? != 0 ]; then config_failed ; fi

echo " Building all packages (this will take a few minutes) ... "
make install>> $MAKELOG 2>&1 &
make_pid=$!

EXPECTED_MAKEINSTALLLOG_SIZE=18562
if [ "$NO_BAR" == "0" ] ; then
	$PACKAGE_DIR/fprogbar.sh $MAKELOG $EXPECTED_MAKEINSTALLLOG_SIZE $make_pid
fi

wait $make_pid >/dev/null 2>&1
if [ $? != 0 ]; then make_failed ; fi
echo " Executables were placed in : $PREFIX/bin"

# cleanup
rm -rf $PACKAGE_DIR

echo IBMgtSim installation done.
echo "  "
exit 0
