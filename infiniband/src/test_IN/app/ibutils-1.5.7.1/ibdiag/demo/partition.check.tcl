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

# This is the checker for Partitions flow

proc parseNodePortGroup {simDir} {
   set f [open [file join $simDir "port_pkey_groups.txt"] r]
   set res {}
   while {[gets $f sLine] >= 0} {
		if {[regexp {^\#} $sLine]} {continue}
      lappend res $sLine
   }
   close $f
   puts "-I- Defined [llength $res] ports"
   return $res
}

# given the node port group defined by the sim flow
# setup the partitions policy file for the SM
proc setupPartitionPolicyFile {fileName} {
   global nodePortGroupList
	for {set g 1} {$g <= 3} {incr g} {
		set GROUP_PKEYS($g) ""
	}
   set f [open $fileName w]

   # no need for default partition
   # puts $f "Default=0x7fff ,ipoib : ALL, SELF=full ;"

   # loop on the tree groups collecting their member guids and printing
	# them out
   foreach p {1 2 3} {
      set guids {}
      foreach png $nodePortGroupList {
         # png = { name num grp guid pkey }
         set grp [lindex $png 2]
         if {$grp == $p} {
            lappend guids [lindex $png 3]
            set GROUP_PKEYS($grp) [lindex $png 4]
         } elseif {$grp == 3} {
            # group 3 ports are members of both other groups
            lappend guids [lindex $png 3]
         }
      }

      puts $f "G$p=$GROUP_PKEYS($p) :"
      set lastGuid [lindex $guids end]
      foreach g $guids {
         if {$p != 3} {
            puts -nonewline $f "   $g=full"
         } else {
            puts -nonewline $f "   $g=full"
         }
         if {$lastGuid == $g} {
            puts $f ";"
         } else {
            puts $f ","
         }
      }
      puts $f " "
   }

   close $f
}

# obtain the list of hosts per each group and dump first few of them
proc getFirstHostOfEachGroup {} {
   global nodePortGroupList
	global GROUP_HOSTS
   foreach p {1 2 3} {
      set hosts {}
      foreach png $nodePortGroupList {
         # png = { name num grp guid pkey }
         set grp [lindex $png 2]
         if {$grp == $p} {
            lappend hosts [lindex $png 0]
         }
      }

		set GROUP_HOSTS($p) $hosts
		puts "Group \#$p has [llength $hosts] hosts: [lrange $hosts 0 2] ..."
	}
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
   global nodePortGroupList

   set osmStdOutLog [file join $simDir osm.stdout.log]
   set osmLog [file join $simDir osm.log]

   fconfigure $simCtrlSock -blocking 1 -buffering line

   # randomize pkey tables
   puts $simCtrlSock "setAllHcaPortsPKeyTable \$fabric"
   puts "SIM: [gets $simCtrlSock]"
   puts $simCtrlSock "dumpHcaPKeyGroupFile $simDir"
   puts "SIM: [gets $simCtrlSock]"

   # parse the node/port/pkey_group file from the sim dir:
   set nodePortGroupList [parseNodePortGroup $simDir]

   # Prepare the nodes partitions data
   set partitionPolicyFile  [file join $simDir opensm-partitions.policy]
   setupPartitionPolicyFile $partitionPolicyFile

   # start the SM
   set osmCmd "$osmPath -P$partitionPolicyFile -D 0x3 -d2 -f $osmLog -g $osmPortGuid"
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
   global nodePortGroupList
	global GROUP_HOSTS

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
	   puts $simCtrlSock "dumpPKeyTables \$fabric"
	   puts "SIM:[gets $simCtrlSock]"
	   puts "---------------------------------------------------------------------"
	   puts " Listing of 3 Nodes of each group:"
	   getFirstHostOfEachGroup
	   puts "---------------------------------------------------------------------"
	   puts " Drop some pkeys from switch ports"
	   foreach g {1 2 3} {
		set hostNode "[lindex $GROUP_HOSTS($g) 2]/U1"
		puts $simCtrlSock "removeGroupPKeyAccrosForHcaPort \$fabric $hostNode 1 $g"
		set res [gets $simCtrlSock]
		if {[regexp {^ERR} $res]} {
			puts "$res"
				return 1
		}
		puts "$res"
	}
	puts "---------------------------------------------------------------------"
		puts " SUBNET READY FOR DIAGNOSTICS"
		puts " press ^C when done"
		puts " "
		puts "cd $simDir"
		puts "setenv IBMGTSIM_DIR  $simDir"
		puts "setenv OSM_CACHE_DIR $simDir"
		puts "setenv OSM_TMP_DIR   $simDir"
		puts "---------------------------------------------------------------------"
		puts ""
		puts "1) try: ibdiagnet -o $simDir -t  network.topo"
		puts "2) try: ibdiagpath -t network.topo -n H-9,H-1"
		puts " press Enter when done"

		puts " press Enter when done"
		gets stdin
		return 0
}
