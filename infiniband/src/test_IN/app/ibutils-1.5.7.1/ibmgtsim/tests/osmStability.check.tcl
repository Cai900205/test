# This is the checker for for a simple 16 node test with opensm and osmtest

##############################################################################
#
# Start up the test applications
# This is the default flow that will start OpenSM only in 0x43 verbosity
# Return a list of process ids it started (to be killed on exit)
#
proc runner {simDir osmPath osmPortGuid} {
   set osmStdOutLog [file join $simDir osm.stdout.log]
   set osmLog [file join $simDir osm.log]
   puts "-I- Starting: $osmPath -R updn -V -g $osmPortGuid ..."
   set osmPid [exec $osmPath -R updn -d2 -V -f $osmLog -g $osmPortGuid > $osmStdOutLog &]

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
   set osmLog [file join $simDir osm.log]

   puts "-I- Waiting max time of 1000sec...."

   if {[osmWaitForUpOrDeadWithTimeout $osmLog 1000000]} {
      return 1
   }

   after 5000

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
   # make sure directory is not remoevd
   return 0
}
