# This checker will wait for OpenSM subnet up and then Try to Join from all
# ports randomally. Then it will wait a few seconds and call ibdmchk. The ibdmchk
# log is then parsed to make sure all ports are part of the multicast group C000

##############################################################################
#
# Start up the test applications
# This is the default flow that will start OpenSM only in 0x43 verbosity
# Return a list of process ids it started (to be killed on exit)
#
proc runner {simDir osmPath osmPortGuid} {
   set osmStdOutLog [file join $simDir osm.stdout.log]
   set osmLog [file join $simDir osm.log]
   puts "-I- Starting: $osmPath -g $osmPortGuid  ..."
   set osmPid [exec $osmPath -d2 -V -f $osmLog -g $osmPortGuid -R updn > $osmStdOutLog &]

   # start a tracker on the log file and process:
   startOsmLogAnalyzer $osmLog

   return $osmPid
}

##############################################################################
#
# Check for the test results
# Return the exit code
proc checker {simDir osmPath osmPortGuid} {
   global simCtrlSock

   # wait for the SM up or dead
   set osmLog [file join $simDir osm.log]
   if {[osmWaitForUpOrDead $osmLog]} {
      return 1
   }

   # start the join requests:
   puts $simCtrlSock "randomJoinAllHCAPorts fabric:1 10"
   set  numHcasJoined [gets $simCtrlSock]
   puts "-I- Joined $numHcasJoined HCAs"

   # wait for a while :
   after 10000

   set ibdmchkLog [file join $simDir ibdmchk.log]
   set subnetFile [file join $simDir opensm-subnet.lst]
   set fdbsFile [file join $simDir opensm.fdbs]
   set mcfdbsFile [file join $simDir opensm.mcfdbs]
   set cmd "ibdmchk -s $subnetFile -f $fdbsFile -m $mcfdbsFile"

   puts "-I- Invoking $cmd "
   if {[catch {set res [eval "exec $cmd > $ibdmchkLog"]} e]} {
      puts "-E- ibdmchk failed"
      return 1
   }

   # make sure all HCAs are now joined:
   set res [exec grep "Multicast Group:0xC000 has:" $ibdmchkLog]
   if {![regexp {Multicast Group:0xC000 has:[0-9]+ switches and:([0-9]+) HCAs} $res d1 hcas]} {
      puts "-E- Fail to parse the Multicast registration ports:$res"
      return 1
   }

   if {$numHcasJoined != $hcas} {
      puts "-E- Not all HCAs are registered. Expected:$numHcasJoined got:$hcas"
      return 1
   }

   return 0
}
