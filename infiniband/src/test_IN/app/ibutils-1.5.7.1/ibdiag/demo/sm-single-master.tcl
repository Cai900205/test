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

# This is the checker for Multiple SMs  Flow

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

	puts "---------------------------------------------------------------------\n"
	puts " Running OpenSM:"
   set osmCmd "$osmPath -D 0x43 -d2 -f $osmLog -g $osmPortGuid"
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
	global osmPid

   # wait for the SM up or dead
   set osmLog [file join $simDir osm.log]
   if {[osmWaitForUpOrDead $osmLog]} {
      return 1
   }

	set PORT_GUID(H-2/P1) [makeProcFSForNode $simDir H-2 1 1]
	set PORT_GUID(H-3/P1) [makeProcFSForNode $simDir H-3 1 1]

	# make sure /proc is updated ...
	puts "---------------------------------------------------------------------\n"
	puts " Creating /proc file system for H-1,H-2,H-3:"
	puts $simCtrlSock "updateProcFSForNode \$fabric $simDir H-1/U1 H-1/U1 1"
   set res [gets $simCtrlSock]
   puts "SIM: Updated H-1 proc file:$res"
	puts $simCtrlSock "updateProcFSForNode \$fabric $simDir H-2/U1 H-2/U1 1"
   set res [gets $simCtrlSock]
   puts "SIM: Updated H-2 proc file:$res"
	puts $simCtrlSock "updateProcFSForNode \$fabric $simDir H-3/U3 H-3/U1 1"
   set res [gets $simCtrlSock]
   puts "SIM: Updated H-3 proc file:$res"


	puts "---------------------------------------------------------------------\n"
	puts " THIS FLOW REQUIRES MANUALy RUNNING OpenSM.\n"
	puts " 1. GOOD CASE:"
	puts "   a. Use the following commands on a new window:"
	puts "     	cd $simDir"
	puts "      setenv IBMGTSIM_DIR  $simDir"
	puts "      setenv OSM_CACHE_DIR $simDir"
	puts "      setenv OSM_TMP_DIR   $simDir"
	puts "      setenv IBMGTSIM_NODE H-1"
	puts "   b. Run ibdiagnet for the first time to see all is OK."
	puts " press Enter when done"
	gets stdin

	puts "---------------------------------------------------------------------\n"
	puts " 2. Run with no SM (I will be killing OpenSM now)...."
	exec kill $osmPid
	puts "    OpenSM killed - now run ibdiagnet and see the error:"
	puts "    -E- Missing master SM in the discover fabric\n"
	puts " press Enter when done"
	gets stdin

	puts "---------------------------------------------------------------------\n"
	puts " 3. TWO MASTERs :"
	puts "   a. Start OpenSM on H-1 using the command:"
	puts "      $osmPath -s 1000 -f $osmLog.1 -g $osmPortGuid"
	puts "   b. Wait for 'SUBNET UP' message and then Ctrl-Z it"
	puts " press Enter when done"
	gets stdin
	puts "   c. Start OpenSM on H-2 using the commands:"
	puts "      setenv IBMGTSIM_NODE H-2"
	puts "      $osmPath -s 1000 -f $osmLog.2 -g $PORT_GUID(H-2/P1)"
	puts "   d. Wait for 'SUBNET UP' message (~2min - due to timeout other SM queries)"
	puts "   e. Now put the SM on H-2 to background by: 'Ctrl-Z' and 'bg' it"
	puts "   f. Put the first SM into action: bg %1"
	puts "   g. Run ibdiagnet again to see the error about two masters"
	puts "      See the error message:"
	puts "      -E- Found more then one master SM in the discover fabric\n"
	puts " press Enter when done"
	gets stdin
	puts "   h. Make one of teh SMs sweep: kill -HUP %2."
   puts "      Now one of the SMs gets to standby mode"
	puts "   i. Run ibdiagnet again to see the info about the SMs in /tmp/ibdiagnet.sm"
	puts " "
	puts " press Enter when done"
	gets stdin
	puts " DO NOT FORGET TO KILL THE SMs..."
   return 0
}
