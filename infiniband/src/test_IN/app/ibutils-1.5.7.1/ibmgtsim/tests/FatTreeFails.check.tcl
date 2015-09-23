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

# This is the checker for for a fat-tree routing check

##############################################################################
#
# Start up the test applications
# This is the default flow that will start OpenSM only in 0x43 verbosity
# Return a list of process ids it started (to be killed on exit)
#
proc runner {simDir osmPath osmPortGuid} {
   set osmStdOutLog [file join $simDir osm.stdout.log]
   set osmLog [file join $simDir osm.log]
   puts "-I- Starting: $osmPath -R ftree -d2 -V -g $osmPortGuid ..."
   #set osmPid [exec $osmPath -f $osmLog -V -g $osmPortGuid  > $osmStdOutLog &]
   set osmPid [exec $osmPath -R ftree -f $osmLog -V -g $osmPortGuid  > $osmStdOutLog &]
   #set osmPid [exec valgrind --tool=memcheck -v --log-file-exactly=/tmp/kliteyn/osm.valgrind.log $osmPath -R ftree -f $osmLog -V -g $osmPortGuid  > $osmStdOutLog &]

   # start a tracker on the log file and process:
   startOsmLogAnalyzer $osmLog

   return $osmPid
}

##############################################################################
#
# Check for the test results
# 1. Make sure we got a "SUBNET UP"
# 2. Run ibdiagnet to check routing
# 3. Check that fat-tree routing has FAILED
# 4. At each step, return the exit code in case of any failure
#
proc checker {simDir osmPath osmPortGuid} {
   global env
   set osmLog [file join $simDir osm.log]

   puts "-I- Waiting max time of 100sec...."

   if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000]} {
      return 1
   }

   after 5000

   set ibdiagnetLog [file join $simDir ibdiagnet.log]
   set cmd "ibdiagnet -o $simDir"

   puts "-I- Invoking $cmd "
   if {[catch {set res [eval "exec $cmd > $ibdiagnetLog"]} e]} {
      puts "-E- ibdiagnet failed with status:$e"
      return 1
   }

   after 5000


   # Check that the fat-tree routing has run to completion.
   # If it has, then opensm-ftree-ca-order.dump file should exist
   # in the simulation directory.
   set osmFtreeCAOrderDump [file join $simDir opensm-ftree-ca-order.dump]
   if {[file exists $osmFtreeCAOrderDump]} {
      puts "-E- Fat-tree CA ordering file exists"
      puts "-E- Fat-tree routing was expected to fail, but it hasn't"
      return 1
   } else {
      puts "-I- Fat-tree CA ordering file doesn't exist"
      puts "-I- Fat-tree routing has failes as expected"
   }

   return 0
}

