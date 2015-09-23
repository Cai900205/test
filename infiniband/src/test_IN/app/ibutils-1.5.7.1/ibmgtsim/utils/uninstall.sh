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

function cleanup_ibmgtsim_files() {
    local PREFIX=$1
    # Clean old distribution
    binApps="IBMgtSim  ibmsquit  ibmssh  mkSimNodeDir  RunSimTest"

    echo "Removing Executables from : .......... $PREFIX/bin."
    for f in $binApps; do
        rm -f ${PREFIX}/bin/$f 2&>1 > /dev/null;
        if [ $? == 0 ]; then
            echo " Removed : ${PREFIX}/bin/$f"
        fi
    done

    echo "Removing Include Files from : ........ $PREFIX/include."
    rm -rf ${PREFIX}/include/ibmgtsim 2&>1 > /dev/null
    if [ $? == 0 ]; then
        echo " Removed : ${PREFIX}/include/ibmgtsim"
    fi

    echo "Removing Libs from : ................. $PREFIX/lib."
    libs="libibmscli.a libibmscli.la libibmscli.so libibmscli.so.1
          libibmscli.so.1.0.0"
    for f in $libs; do
        rm -rf ${PREFIX}/lib/$f 2&>1 > /dev/null;
        if [ $? == 0 ]; then
            echo " Removed : ${PREFIX}/lib/$f"
        fi
    done
}

NO_BAR=0

# parse parameters
while [ "$1" ]; do
#  echo "Current parsed param is : $1"
  case $1 in
    "--prefix")
          PREFIX=$2
          cleanup_ibmgtsim_files $2
          shift
          ;;
    *)
     echo "Usage: $0 [--prefix <install-dir>]"
     echo ""
     echo "Options:"
     echo "   --prefix <dir> : the prefix used for the IBMgtSim instalaltion"
     exit 1
  esac
  shift
done

if test -f /usr/bin/ibmssh || test -L /usr/bin/ibmssh; then
   cleanup_ibmgtsim_files /usr
fi

if test -f /usr/local/bin/ibmssh || test -L /usr/local/bin/ibmssh; then
   cleanup_ibmgtsim_files /usr/local
fi
