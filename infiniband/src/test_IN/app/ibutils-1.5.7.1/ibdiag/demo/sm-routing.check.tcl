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

# This is the checker for SM LFT/MFT assignment checks

# given the node port group defined by the sim flow
# setup the partitions policy file for the SM
proc setupPartitionPolicyFile {fileName} {
   set f [open $fileName w]

   # no need for default partition
   # puts $f "Default=0x7fff ,ipoib : ALL, SELF=full ;"

	puts $f "G1=0x8001, ipoib: ALL=full;"
	puts $f "G2=0x8002, ipoib: ALL=full;"
	puts $f "G2=0x8003, ipoib: ALL=full;"
   close $f
}


##############################################################################
#
# Start up the test applications
# This is the default flow that will start OpenSM only in 0x43 verbosity
# Return a list of process ids it started (to be killed on exit)
#
proc runner {simDir osmPath osmPortGuid} {
   global simCtrlSock osmPid
   global env

   set osmStdOutLog [file join $simDir osm.stdout.log]
   set osmLog [file join $simDir osm.log]

   # Prepare the nodes partitions data
   set partitionPolicyFile  [file join $simDir opensm-partitions.policy]
   setupPartitionPolicyFile $partitionPolicyFile

   fconfigure $simCtrlSock -blocking 1 -buffering line

	puts "---------------------------------------------------------------------"
	puts " Starting the SM\n"

   set osmCmd "$osmPath -P$partitionPolicyFile -d2 -f $osmLog -g $osmPortGuid"
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
	global topologyFile
	global osmPid

   set osmLog [file join $simDir osm.log]

   if {[osmWaitForUpOrDead $osmLog]} {
      return 1
   }

	puts "---------------------------------------------------------------------"
	puts " OpemSM brought up the network"
	puts $simCtrlSock "updateProcFSForNode \$fabric $simDir H-1/U1 H-1/U1 1"
   set res [gets $simCtrlSock]
   puts "SIM: Updated H-1 proc file:$res"

	puts " All hosts now joining their IPoIB Subnets"
	puts $simCtrlSock "joinPortsByPartition \$fabric {0x8001 0x8002 0x8003}"
   puts "SIM: [gets $simCtrlSock]"
	after 10000
	puts "---------------------------------------------------------------------"
	puts " SUBNET READY FOR DIAGNOSTICS"
	puts "\nCut and paste the following in a new window then run ibdiagnet:"
	puts "cd $simDir"
	puts "setenv IBMGTSIM_DIR  $simDir"
	puts "setenv OSM_CACHE_DIR $simDir"
	puts "setenv OSM_TMP_DIR   $simDir"
	puts " "
	puts "---------------------------------------------------------------------"
	puts " Forcing LFT/MFT changes ... "
	puts $simCtrlSock "causeDeadEndOnPath \$fabric H-2/U1 1 H-41/U1 1"
	set ret [gets $simCtrlSock]
   puts "SIM:$ret\n"
	if {![regexp {at switch:(\S+)} $ret d1 swName]} {
		return 1
	}
	puts "See what errors are reported using ibdiagnet and ibdiagpath"
	puts "1) try: ibdiagnet -r"
	puts "2) try: ibdiagpath -n H-2,H-41 -t $topologyFile"
	puts " press Enter when done"
	gets stdin
	puts "---------------------------------------------------------------------"
	puts " Let the SM fix the issue:"
	puts $simCtrlSock "setSwitchChangeBit \$fabric $swName"
	puts "SIM: [gets $simCtrlSock]"
	exec kill -HUP $osmPid
	# wait for sweep to end or exit
	puts "-I- Waiting for subnet up"
	set ignorePrev 1
	if {[osmWaitForUpOrDead $osmLog $ignorePrev]} {
		return 1
	}
	puts "Repeat the previous commands to see everything is back to normal"
	puts " press Enter when done"
	gets stdin
	puts "---------------------------------------------------------------------"
	puts $simCtrlSock "causeDeadEndOnPath \$fabric H-4/U1 1 H-23/U1 1"
	set ret [gets $simCtrlSock]
   puts "SIM:$ret\n"
	if {![regexp {at switch:(\S+)} $ret d1 swName]} {
		return 1
	}
	puts "See what errors are reported using ibdiagnet and ibdiagpath"
	puts "1) try: ibdiagnet -r"
	puts "2) try: ibdiagpath -n H-4,H-23 -t $topologyFile"
	puts " press Enter when done"
	gets stdin
	puts "---------------------------------------------------------------------"
	puts " Let the SM fix the issue:"
	puts $simCtrlSock "setSwitchChangeBit \$fabric $swName"
	puts "SIM: [gets $simCtrlSock]"
	exec kill -HUP $osmPid
	# wait for sweep to end or exit
	puts "-I- Waiting for subnet up"
	set ignorePrev 1
	if {[osmWaitForUpOrDead $osmLog $ignorePrev]} {
		return 1
	}
	puts "Repeat the previous commands to see everything is back to normal"
	puts " press Enter when done"
	gets stdin
	puts "---------------------------------------------------------------------"
	puts $simCtrlSock "causeLoopOnPath \$fabric H-8/U1 1 H-22/U1 1"
	set ret [gets $simCtrlSock]
   puts "SIM:$ret\n"
	if {![regexp {at switch:(\S+)} $ret d1 swName]} {
		return 1
	}
	puts "See what errors are reported using ibdiagnet and ibdiagpath"
	puts "1) try: ibdiagnet -r"
	puts "2) try: ibdiagpath -n H-8,H-22 -t $topologyFile"
	puts " press Enter when done"
	gets stdin
	puts "---------------------------------------------------------------------"
	puts " Let the SM fix the issue:"
	puts $simCtrlSock "setSwitchChangeBit \$fabric $swName"
	puts "SIM: [gets $simCtrlSock]"
	exec kill -HUP $osmPid
	# wait for sweep to end or exit
	puts "-I- Waiting for subnet up"
	set ignorePrev 1
	if {[osmWaitForUpOrDead $osmLog $ignorePrev]} {
		return 1
	}
	puts "Repeat the previous commands to see everything is back to normal"
	puts " press Enter when done"
	gets stdin
	puts "---------------------------------------------------------------------"
	puts $simCtrlSock "breakMCG \$fabric 0xc002"
   puts "SIM: [gets $simCtrlSock]\n"
	puts "See what errors are reported using ibdiagnet:"
	puts "1) try: ibdiagnet -r"
	puts " press Enter when done"
	gets stdin
   return 0
}
