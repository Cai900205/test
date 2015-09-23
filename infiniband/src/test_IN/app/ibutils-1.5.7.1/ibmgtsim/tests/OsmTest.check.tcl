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

   # prevent long transactions return BUSY signal
   exec echo max_msg_fifo_timeout 0 > $simDir/opensm.opts

   puts "-I- Starting: $osmPath -g $osmPortGuid  ..."
   set osmPid [exec $osmPath -d2 -s 0 -t 1000 -f $osmLog -g $osmPortGuid > $osmStdOutLog &]

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
   global env

   set osmTestPath      [file join [file dirname $osmPath] osmtest]
   set osmTestLog       [file join $simDir osmtest.log]
   set osmTestStdOutLog [file join $simDir osmtest.stdout.log]
   set osmTestInventory [file join $simDir osmtest.dat]

   # wait for the SM up or dead
   set osmLog [file join $simDir osm.log]
   if {[osmWaitForUpOrDead $osmLog]} {
      return 1
   }

   # update node proc file
   puts $simCtrlSock "updateProcFSForNode \$fabric $simDir $env(IBMGTSIM_NODE) $env(IBMGTSIM_NODE) 1"
   set res [gets $simCtrlSock]
   if {$res == 1} {return 1}
   puts "SIM: Updated H-1 proc file:$res"
   set env(IBMGTSIM_NODE) $res

   # if we did get a subnet up:
   set osmTestCmd1 "$osmTestPath -v -t 1000 -g $osmPortGuid -l $osmTestLog -f c -i $osmTestInventory"
   puts "-I- Invoking: $osmTestCmd1 ..."
   # HACK: we currently ignore osmtest craches on exit flow:
   catch {set res [eval "exec $osmTestCmd1 >& $osmTestStdOutLog"]}

   if {[catch {exec grep "OSMTEST: TEST \"Create Inventory\" PASS" $osmTestStdOutLog}]} {
      puts "-E- osmtest Create Inventory failed"
      return 1
   }

   after 2000

   # now do the actual test...
   set osmTestCmd2 "$osmTestPath -v -t 1000 -g $osmPortGuid -l $osmTestLog -f a -i $osmTestInventory"
   puts "-I- Invoking: $osmTestCmd2 ..."
   # HACK: we currently ignore osmtest craches on exit flow:
   catch {set res [eval "exec $osmTestCmd2 >& $osmTestStdOutLog"]}
   if {[catch {exec grep "OSMTEST: TEST \"All Validations\" PASS" $osmTestStdOutLog}]} {
      puts "-E- osmtest All Validations failed"
      return 1
   }

   puts "-I- osmtest completed successfuly"
   return 0
}
