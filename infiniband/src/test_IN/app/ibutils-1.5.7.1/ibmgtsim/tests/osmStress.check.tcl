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

   set lmc 0
   fconfigure $simCtrlSock -blocking 1 -buffering line

   # randomize lids
   puts $simCtrlSock "assignLegalLids \$fabric $lmc"
   puts "SIM: [gets $simCtrlSock]"

   # Disconnect ports
   puts $simCtrlSock "setPortsDisconnected  \$fabric $lmc"
   puts "SIM: [gets $simCtrlSock]"

   # randomize guid2lid file:
   set env(OSM_CACHE_DIR) $simDir/
   puts $simCtrlSock "writeGuid2LidFile $simDir/guid2lid $lmc"
   puts "SIM: [gets $simCtrlSock]"

   file copy $simDir/guid2lid $simDir/guid2lid.orig

   set osmCmd "$osmPath -d2 -l $lmc -V -f $osmLog -g $osmPortGuid"
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
   global topologyFile
   set osmLog [file join $simDir osm.log]

   puts "-I- Waiting max time of 100sec...."

   if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000]} {
      return 1
   }

   # update node proc file
   puts $simCtrlSock "updateProcFSForNode \$fabric $simDir $env(IBMGTSIM_NODE) $env(IBMGTSIM_NODE) 1"
   set res [gets $simCtrlSock]
   if {$res == 1} {return 1}
   puts "SIM: Updated H-1 proc file:$res"

   # check for lid validity:
   puts $simCtrlSock "checkLidValues \$fabric $lmc"
   set res [gets $simCtrlSock]
   puts "SIM: Number of LID check errors:$res"
   if {$res != 0} {
      return $res
   }

   # we try several iterations of changes:
   for {set i 1} {$i < 2} {incr i} {
      # connect the disconnected
      puts $simCtrlSock "connectAllDisconnected \$fabric 1"
      puts "SIM: [gets $simCtrlSock]"

      # refresh the lid database and start the POST_SUBNET_UP mode
      puts $simCtrlSock "updateAssignedLids \$fabric"
      puts "SIM: [gets $simCtrlSock]"

      for {set j 1} {$j < 10} {incr j} {
         # Disconnect ports
         puts $simCtrlSock "setPortsDisconnected \$fabric $lmc"
         puts "SIM: [gets $simCtrlSock]"
         # connect the disconnected
         puts $simCtrlSock "connectAllDisconnected \$fabric 1"
         puts "SIM: [gets $simCtrlSock]"
      }

      # wait for sweep to end or exit
      if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000]} {
         return 1
      }
      puts $simCtrlSock "updateProcFSForNode \$fabric $simDir $env(IBMGTSIM_NODE) $env(IBMGTSIM_NODE) 1"
      set res [gets $simCtrlSock]
      if {$res == 1} {return 1}
      puts "SIM: Updated H-1 proc file:$res"
      set env(IBMGTSIM_NODE) $res

      # wait 3 seconds
      after 3000

      # check for lid validity:
      puts $simCtrlSock "checkLidValues \$fabric $lmc"
      set res [gets $simCtrlSock]
      puts "SIM: Number of LID check errors:$res"
      if {$res != 0} {
         return $res
      }

		# sending event forwarding notification requests...
      puts "-I- Sending event forwarding notification requests"
      puts $simCtrlSock "randomRegisterFormInformInfo fabric:1"
      set  returnVal [gets $simCtrlSock]
      puts "SIM: -I- $returnVal"

      # start Random Flow:
      set iterations 240
      puts "-I- Starting the random stress flow with $iterations..."
      puts $simCtrlSock "RunRandomStressFlow fabric:1 $iterations"
      set  returnVal [gets $simCtrlSock]
      puts "SIM: -I- $returnVal"

      # At the end, connect all the ports back
      puts "-I- Connecting all disconnected ..."
      puts $simCtrlSock "connectAllDisconnected \$fabric 1"
      set  returnVal [gets $simCtrlSock]
      puts "SIM: $returnVal"

      # wait for sweep to end or exit
      puts "-I- if we did connect some we need to wait for them"
      if {"-I- Reconnected 0 nodes" != $returnVal} {
         if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000]} {
            return 1
         }
      }

      # and yet another light sweep
      after 20000

      #At the end, join all to the multicast group
      puts "-I- Joining all Ports ..."
      set joinAllHCAs 1
      set interJoinDelay_ms 1
      puts $simCtrlSock "randomJoinAllHCAPorts fabric:1 $interJoinDelay_ms $joinAllHCAs"
      set  numHcasJoined [gets $simCtrlSock]
      puts "SIM: -I- Joined $numHcasJoined HCAs"

      # force a sweep:
      puts "-I- Forcing a sweep..."
      puts $simCtrlSock "setOneSwitchChangeBit \$fabric"
      set  returnVal [gets $simCtrlSock]
      puts "SIM: $returnVal"

      # wait for sweep to end or exit
      if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000]} {
         return 1
      }

      # wait ~1 sec per joining port - to enable the SM to complete connecting them
      after [expr $numHcasJoined * 1000]

      # use ibdiagnet instead of relying on opensm reports...
      if {0} {
         set ibdmchkLog [file join $simDir ibdmchk.log]
         set subnetFile [file join $simDir opensm-subnet.lst]
         set fdbsFile [file join $simDir opensm.fdbs]
         set mcfdbsFile [file join $simDir opensm.mcfdbs]
         set cmd "ibdmchk -s $subnetFile -f $fdbsFile -m $mcfdbsFile"

         puts "-I- Invoking $cmd "
         if {[catch {set res [eval "exec $cmd > $ibdmchkLog"]} e]} {
            puts "-E- ibdmchk failed"
            puts "-I- Result value $res"
            puts "-I- Error: $e"
            return 1
         }
      }

      set cmd "ibdiagnet -v -r -t $topologyFile -o $simDir -s $env(IBMGTSIM_NODE)"
      set ibdiagnetLog [file join $simDir ibdiagnet.stdout.log]
      puts "-I- Invoking $cmd "
      if {[catch {set res [eval "exec $cmd >& $ibdiagnetLog"]} e]} {
         puts "-E- ibdiagnet failed"
         puts "-I- Result value $res"
         puts "-I- Error: $e"
			return 1
      }

      # make sure all HCAs are now joined:
      set res [exec grep "Multicast Group:0xC000 has:" $ibdiagnetLog]
      if {![regexp {Multicast Group:0xC000 has:[0-9]+ switches and:([0-9]+) HCAs} $res d1 hcas]} {
         puts "-E- Fail to parse the Multicast registration ports:$res"
         return 1
      }

      if {$numHcasJoined != $hcas} {
         puts "-E- Not all HCAs are registered. Expected:$numHcasJoined got:$hcas"
         return 1
      }
   }

   return 0
}
