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

# This is the checker for the semi static lid assignment feature:

# A. the sim code should generate the cache file once the simulator is up.
# it should randomize:
# 1. some guids should not have a lid
# 2. some guids should share a lid
# 3. some extra guids should be there

# B. Wait for OpenSM SUBNET UP
#
# C. The simulator code should randomally do the following (several of each)
# 1. Zero some port lids
# 2. Copy some port lids to other ports
# 3. Invent some new lids to some ports
# 4. Turn some node ports down - disconect (all ports of the node)
#
# D. The simulator shoudl send a trap or set a switch change bit
#
# E. Wait for heavy sweep.
#
# F. The simulator code should verify that the lids match what it expects:
#    Note that the nodes that did have a non overlapping lid in the file
#    must have it. The rest of the ports should have valid lid values.
#

##############################################################################
#
# Start up the test applications
# This is the default flow that will start OpenSM only in 0x43 verbosity
# Return a list of process ids it started (to be killed on exit)
#
proc runner {simDir osmPath osmPortGuid} {
   global simCtrlSock
   global env
   global lmc

   set osmStdOutLog [file join $simDir osm.stdout.log]
   set osmLog [file join $simDir osm.log]

   set lmc 1
   fconfigure $simCtrlSock -blocking 1 -buffering line

   # randomize lids
   puts $simCtrlSock "assignLegalLids \$fabric $lmc"
   puts "SIM: [gets $simCtrlSock]"

   # randomize lid failures:
   puts $simCtrlSock "setLidAssignmentErrors  \$fabric $lmc"
   puts "SIM: [gets $simCtrlSock]"

   # randomize guid2lid file:
   set env(OSM_CACHE_DIR) $simDir/
   puts $simCtrlSock "writeGuid2LidFile $simDir/guid2lid $lmc"
   puts "SIM: [gets $simCtrlSock]"

   file copy $simDir/guid2lid $simDir/guid2lid.orig

   set osmCmd "$osmPath -l $lmc -d2 -V -f $osmLog -g $osmPortGuid"
   puts "-I- Starting: $osmCmd"
   set osmPid [eval "exec $osmCmd > $osmStdOutLog &"]

   # start a tracker on the log file and process:
   startOsmLogAnalyzer $osmLog

   return $osmPid
}

##############################################################################
#
# Check for the test results: make sure we got a "SUBNET UP"
# Return the exit code
proc checker {simDir osmPath osmPortGuid} {
   global env
   global simCtrlSock
   global lmc
   set osmLog [file join $simDir osm.log]

   puts "-I- Waiting max time of 100sec...."

   if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000]} {
      return 1
   }

   # check for lid validity:
   puts $simCtrlSock "checkLidValues \$fabric $lmc"
   set res [gets $simCtrlSock]
   puts "SIM: Number of check errors:$res"
   if {$res != 0} {
      return $res
   }

   # we try several iterations of changes:
   for {set i 1} {$i < 5} {incr i} {
      # connect the disconnected
      puts $simCtrlSock "connectAllDisconnected \$fabric"
      puts "SIM: [gets $simCtrlSock]"

      # refresh the lid database and start the POST_SUBNET_UP mode
      puts $simCtrlSock "updateAssignedLids \$fabric"
      puts "SIM: [gets $simCtrlSock]"

      # randomize lid failures:
      puts $simCtrlSock "setLidAssignmentErrors \$fabric $lmc"
      puts "SIM: [gets $simCtrlSock]"

      # inject a change bit
      puts "-I- Injecting change bit"
      puts $simCtrlSock "setOneSwitchChangeBit \$fabric"
      puts "SIM: [gets $simCtrlSock]"

      # wait for sweep to end or exit
      puts "-I- Waiting for subnet up"
      set ignorePrev 1
      if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000 $ignorePrev]} {
         return 1
      }

      # wait 30 seconds
      after 30000

      # inject a change bit
      puts "-I- Injecting change bit"
      puts $simCtrlSock "setOneSwitchChangeBit \$fabric"
      puts "SIM: [gets $simCtrlSock]"

      # wait for sweep to end or exit
      puts "-I- Waiting for subnet up"
      set ignorePrev 1
      if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000 $ignorePrev]} {
         return 1
      }

      # wait 3 seconds
      after 3000

      # check for lid validity:
      puts "-I- Checking LID Validity"
      puts $simCtrlSock "checkLidValues \$fabric $lmc"
      set res [gets $simCtrlSock]
      puts "SIM: Number of check errors:$res"
      if {$res != 0} {
         return $res
      }
   }

   return 0
}
