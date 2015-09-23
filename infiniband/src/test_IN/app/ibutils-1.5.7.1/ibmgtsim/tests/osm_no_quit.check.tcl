# This is the checker for for running opensm and do not exit ever

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
   set osmPid [exec $osmPath -d2 -f $osmLog -g $osmPortGuid > $osmStdOutLog &]

   # start a tracker on the log file and process:
   startOsmLogAnalyzer $osmLog

   return $osmPid
}

##############################################################################
#
# Check for the test results
# Return the exit code
proc checker {simDir osmPath osmPortGuid} {
   set osmTestPath      [file join [file dirname $osmPath] osmtest]
   set osmTestLog       [file join $simDir osmtest.log]
   set osmTestStdOutLog [file join $simDir osmtest.stdout.log]
   set osmTestInventory [file join $simDir osmtest.dat]

   # wait for the SM up or dead
   set osmLog [file join $simDir osm.log]
   if {[osmWaitForUpOrDead $osmLog]} {
      return 1
   }

   puts "-I- Looping forever ..."
   while {1} {
      after 10000
   }
}
