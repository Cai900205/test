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

# This is the checker for QoS Flow

proc setupOpensmOptionsFile {simDir} {
	# by default we set all switches to have
	# VL = SL % 8
	# VLArb High = VLArb Low = 0.1,1.1,2.1,3.1,4.1,5.1,6.1,7.1
	set f [open $simDir/opensm.conf w]
	puts $f {

# Disable QoS setup
no_qos FALSE

# QoS default options
#qos_max_vls    8
#qos_high_limit 3
#qos_vlarb_high 0:1,1:1,2:1,3:1,4:1,5:1,6:1,7:1
#qos_vlarb_low  0:1,1:1,2:1,3:1,4:1,5:1,6:1,7:1
#qos_sl2vl      0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7

# QoS CA options
qos_ca_max_vls    8
qos_ca_high_limit 4
qos_ca_vlarb_high 0:1,1:1,2:1,3:1,4:1,5:1,6:1,7:1
qos_ca_vlarb_low  0:1,1:1,2:1,3:1,4:1,5:1,6:1,7:1
qos_ca_sl2vl      0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7

# QoS Switch Port 0 options
qos_sw0_max_vls    8
qos_sw0_high_limit 5
qos_sw0_vlarb_high 0:1,1:1,2:1,3:1,4:1,5:1,6:1,7:1
qos_sw0_vlarb_low  0:1,1:1,2:1,3:1,4:1,5:1,6:1,7:1
qos_sw0_sl2vl      0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7

# QoS Switch external ports options
qos_swe_max_vls     8
qos_swe_high_limit  6
qos_swe_vlarb_high  0:1,1:1,2:1,3:1,4:1,5:1,6:1,7:1
qos_swe_vlarb_low   0:1,1:1,2:1,3:1,4:1,5:1,6:1,7:1
qos_swe_sl2vl       0,1,2,3,4,5,6,7,0,1,2,3,4,5,6,7

	}
	close $f

}

##############################################################################
#
# Start up the test applications
# This is the default flow that will start OpenSM only in 0x43 verbosity
# Return a list of process ids it started (to be killed on exit)
#
proc runner {simDir osmPath osmPortGuid} {
   global simCtrlSock
   global env

   set osmStdOutLog [file join $simDir osm.stdout.log]
   set osmLog [file join $simDir osm.log]

   fconfigure $simCtrlSock -blocking 1 -buffering line

   # Prepare the OpenSM config options file
	setupOpensmOptionsFile $simDir

   # start the SM
	puts "---------------------------------------------------------------------"
	puts " Starting the SM\n"
   set valgrind "/usr/bin/valgrind --tool=memcheck"
   set osmCmd "$osmPath -F $simDir/opensm.conf -Q -D 0x43 -d2 -t 4000 -f $osmLog -g $osmPortGuid"
   puts "-I- Starting: $osmCmd"
   set osmPid [eval "exec $osmCmd > $osmStdOutLog &"]

   # start a tracker on the log file and process:
   startOsmLogAnalyzer $osmLog

   return $osmPid
}

##############################################################################
#
# Check for the test results
# Return the exit code
proc checker {simDir osmPath osmPortGuid} {
   global env
   global simCtrlSock

   # wait for the SM up or dead
   set osmLog [file join $simDir osm.log]
   if {[osmWaitForUpOrDead $osmLog]} {
      return 1
   }

	# make sure /proc is updated ...
	puts $simCtrlSock "updateProcFSForNode \$fabric $simDir H-1/U1 H-1/U1 1"
   set res [gets $simCtrlSock]
   puts "SIM: Updated H-1 proc file:$res"

	puts "---------------------------------------------------------------------"
	puts " OpemSM brought up the network"
	puts "---------------------------------------------------------------------"
	puts "-I- Default SL2VL is: {0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7}"
	puts "-I- Default VLArb is: {{0 1} {1 1} {2 1} {3 1} {4 1} {5 1} {6 1} {7 1}}"
	puts "\nModify SL2VL and VLArb accross specific hosts"
	puts $simCtrlSock "setVlArbAccross \$fabric H-2/U1 1 {{8 1} {9 1} {10 1} {11 1} {12 1} {13 1} {14 1} {15 1}}"
   puts "SIM: [gets $simCtrlSock]"
	puts $simCtrlSock "setVlArbAccross \$fabric H-3/U1 1 {{0 0} {1 0} {2 0} {3 1} {4 1} {5 1} {6 1} {7 1}}"
   puts "SIM: [gets $simCtrlSock]"
	puts $simCtrlSock "setSl2VlTableToPortAccross \$fabric H-4/U1 1 {15 15 2 3 4 15 15 15 15 15 15 15 15 15 15 15}"
   puts "SIM: [gets $simCtrlSock]"
	puts $simCtrlSock "setSl2VlTableToPortAccross \$fabric H-5/U1 1 {8 9 10 11 12 13 14 15 8 9 10 11 12 13 14 15}"
   puts "SIM: [gets $simCtrlSock]"
	puts "---------------------------------------------------------------------"
	puts " SUBNET READY FOR DIAGNOSTICS"
	puts "\nCut and paste the following in a new window then run ibdiagnet:"
	puts "cd $simDir"
	puts "setenv IBMGTSIM_DIR  $simDir"
	puts "setenv OSM_CACHE_DIR $simDir"
	puts "setenv OSM_TMP_DIR   $simDir"
	puts " "
	puts " press Enter when done"
	gets stdin
   return 0
}
