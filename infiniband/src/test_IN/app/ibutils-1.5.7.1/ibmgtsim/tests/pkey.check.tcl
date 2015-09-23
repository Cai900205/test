# This is the checker for for a simple 16 node test with opensm and osmtest

proc parseNodePortGroup {simDir} {
   set f [open [file join $simDir "port_pkey_groups.txt"] r]
   set res {}
   while {[gets $f sLine] >= 0} {
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

   # loop on the tree groups collecting their member guids and printing them out
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
            puts -nonewline $f "   $g"
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

# validate osmtest.dat versus the list of node port group
# group 1 must only have info regarding nodes of group1
# group 2 is similar
# group 3 see all others
#
# HACK: we currently only count the number of nodes/ports/path records
# and not use the full GUID based check.
proc validateInventoryVsGroup {simDir group nodePortGroupList} {

   # count the number of ports we have in each group:
   set GUIDS(1) {}
   set GUIDS(2) {}
   set GUIDS(3) {}

   foreach npg $nodePortGroupList {
      set g [lindex $npg 2]
      set guid [lindex $npg 3]
      lappend GUIDS($g) $guid
      set GUID_GRP($guid) $g
   }

   set cnt1 [llength $GUIDS(1)]
   set cnt2 [llength $GUIDS(2)]
   set cnt3 [llength $GUIDS(3)]
   switch $group {
      1 {
         foreach g $GUIDS(1) {set REQUIRED($g) 0}
         foreach g $GUIDS(3) {set REQUIRED($g) 0}
         foreach g $GUIDS(2) {set DISALLOWED($g) 2}
			set expected [expr ($cnt1 + $cnt3)*($cnt1 + $cnt3)]
      }
      2 {
         foreach g $GUIDS(2) {set REQUIRED($g) 0}
         foreach g $GUIDS(3) {set REQUIRED($g) 0}
         foreach g $GUIDS(1) {set DISALLOWED($g) 1}
			set expected [expr ($cnt2 + $cnt3)*($cnt2 + $cnt3)]
      }
      3 {
         foreach g $GUIDS(1) {set REQUIRED($g) 0}
         foreach g $GUIDS(2) {set REQUIRED($g) 0}
         foreach g $GUIDS(3) {set REQUIRED($g) 0}
			set expected [expr \
								  $cnt3*($cnt1 + $cnt2 + $cnt3) + \
								  $cnt1*($cnt1 + $cnt3) + \
								  $cnt2*($cnt2 + $cnt3) ]
      }
   }

   puts "-I- Expecting:$expected paths (membership is: 1=$cnt1 2=$cnt2 3=$cnt3 )"

   # parse the inventory:
   set fn [file join $simDir osmtest.dat]
   set f [open $fn r]
   set lineNum 0
   set errCnt 0
   set state none
   while {[gets $f sLine] >= 0} {
      incr lineNum
      if {$state == "none"} {
         if {[lindex $sLine 0] == "DEFINE_NODE"} {
            set state node
         } elseif {[lindex $sLine 0] == "DEFINE_PORT"} {
            set state port
         } elseif {[lindex $sLine 0] == "DEFINE_PATH"} {
            set state path
         }
      } elseif {$state == "node"} {
         set field [lindex $sLine 0]
         # we only care about guid line and lid
         if {$field == "port_guid"} {
            set guid [lindex $sLine 1]
            set GUID_BY_LID($lid) $guid

            # now we can check if the guid is expected or not
            if {[info exist DISALLOWED($guid)]} {
               puts "-E- Got disallowed guid:$guid of group $DISALLOWED($guid)"
               incr errCnt
            } else {
               # we might require it so mark ...
               if {[info exist REQUIRED($guid)]} {
                  incr REQUIRED($guid)
               }
            }
         } elseif {$field == "lid"} {
            set lid [lindex $sLine 1]
         } elseif {$field == "END"} {
            set state none
         }
      } elseif {$state == "port"} {
         # we only care about lid line
         set field [lindex $sLine 0]
         if {$field == "base_lid"} {
            set lid [lindex $sLine 1]
            # ignore lid 0x0 on physp of switches...
            if {$lid != "0x0"} {
               set guid $GUID_BY_LID($lid)

               # now we can check if the guid is expected or not
               if {[info exist DISALLOWED($guid)]} {
                  puts "-E- Got disallowed guid:$guid of group $DISALLOWED($guid)"
                  incr errCnt
               } else {
                  # we might require it so mark ...
                  if {[info exist REQUIRED($guid)]} {
                     incr REQUIRED($guid)
                  }
               }
            }
         } elseif {$field == "END"} {
            set state none
         }
      } elseif {$state == "path"} {
         # we need to check both sides of the path
         set field [lindex $sLine 0]
         if {$field == "sgid"} {
            set sguid [lindex $sLine 2]
            if {[info exist DISALLOWED($sguid)]} {
               puts "-E- Got disallowed path from guid:$sguid of group $DISALLOWED($sguid)"
               incr errCnt
            } else {

               # the path is allowed only if the ends are allowed to
               # see wach other - catch cases where they are not:
               if {[info exist GUID_GRP($sguid)] &&
                   [info exist GUID_GRP($dguid)] &&
                   ((($GUID_GRP($sguid) == 1) && ($GUID_GRP($dguid) == 2)) ||
                    (($GUID_GRP($sguid) == 2) && ($GUID_GRP($dguid) == 1)))} {
                  incr errCnt
                  puts "-E- Got path from guid:$sguid to guid:$dguid"
                  incr errCnt
               } else {
                  # track paths:
                  set k "$sguid $dguid"
                  set PATHS($k) 1
               }
            }
         } elseif {$field == "dgid"} {
            set dguid [lindex $sLine 2]
            if {[info exist DISALLOWED($dguid)]} {
               puts "-E- Got disallowed path to guid:$dguid of group $DISALLOWED($dguid)"
               incr errCnt
            }
         } elseif {$field == "END"} {
            set state none
         }
      }
   }
   close $f
   foreach sguid [array names REQUIRED] {
      if {$REQUIRED($sguid) != 2} {
         puts "-E- Missing port or node for guid $sguid"
         incr errCnt
      }

      foreach dguid [array names REQUIRED] {
         # it is not enough for the ports to be visible - they need
         # to see each other so can not be one from 1 and other from 2...
         if {($GUID_GRP($sguid) == 1) && ($GUID_GRP($dguid) == 2)} continue
         if {($GUID_GRP($sguid) == 2) && ($GUID_GRP($dguid) == 1)} continue

         set k "$sguid $dguid"
         if {![info exist PATHS($k)]} {
            puts "-E- Missing path $k"
            incr errCnt
         }
      }
   }

   puts "-I- Obtained: [llength [array names PATHS]] paths for group:$group"
   return $errCnt
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
   set partitionPolicyFile  [file join $simDir partitions.policy]
   setupPartitionPolicyFile $partitionPolicyFile

   # start the SM
   set valgrind "/usr/bin/valgrind --tool=memcheck"
   set osmCmd "$osmPath -P$partitionPolicyFile -D 0x3 -d2 -t 8000 -f $osmLog -g $osmPortGuid"
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
   global simCtrlSock osmPid
   global nodePortGroupList

   set osmTestPath      [file join [file dirname $osmPath] osmtest]
   set osmTestLog       [file join $simDir osmtest.log]
   set osmTestStdOutLog [file join $simDir osmtest.stdout.log]
   set osmTestInventory [file join $simDir osmtest.dat]

   # wait for the SM up or dead
   set osmLog [file join $simDir osm.log]
   if {[osmWaitForUpOrDead $osmLog]} {
      return 1
   }

   # randomly sellect several nodes and create inventory by running osmtest
   # on them - then check only valid entries were reported
   for {set i 0 } {$i < 5} {incr i} {

      # decide which will be the node name we will use
      set r [expr int([rmRand]*[llength $nodePortGroupList])]
      set nodeNPortNGroup [lindex $nodePortGroupList $r]
      set nodeName [lindex $nodeNPortNGroup 0]
      set portNum  [lindex $nodeNPortNGroup 1]
      set group    [lindex $nodeNPortNGroup 2]
      set portGuid [makeProcFSForNode $simDir $nodeName/U1 $portNum 1]
      set env(IBMGTSIM_NODE) $nodeName/U1

      puts "-I- Invoking osmtest from node:$nodeName port:$portNum"

      set osmTestCmd1 "$osmTestPath -t 8000 -g $portGuid -l $osmTestLog -f c -i $osmTestInventory"
      puts "-I- Invoking: $osmTestCmd1 ..."

      # HACK: we currently ignore osmtest craches on exit flow:
      catch {set res [eval "exec $osmTestCmd1 >& $osmTestStdOutLog"]}

      if {[catch {exec grep "OSMTEST: TEST \"Create Inventory\" PASS" $osmTestStdOutLog}]} {
         puts "-E- osmtest Create Inventory failed"
         return 1
      }

      if {[validateInventoryVsGroup $simDir $group $nodePortGroupList]} {
			puts $simCtrlSock "dumpPKeyTables \$fabric"
			puts "SIM: [gets $simCtrlSock]"
         return 1
      }
   }

   ###### Verifing the pkey manager behaviour ################

   # Remove the default pkey from the HCA ports (except the SM)
	# HACK: for now the SM does not refresh PKey tables no matter what...
	if {0} {
		puts $simCtrlSock "removeDefaultPKeyFromTableForHcaPorts \$fabric"
		puts "SIM: [gets $simCtrlSock]"
	}

	# Verify all pkeys are in correct place:
   puts "-I- Calling simulator to verify all defined indexie are correct"
   puts $simCtrlSock "verifyCorrectPKeyIndexForAllHcaPorts \$fabric"
   set res [gets $simCtrlSock]
   puts "SIM: $res"

   if {$res != 0} {
      puts "-E- $res ports have miss-placed pkeys"
		return 1
   }

   #Inject a changebit - to force heavy sweep
   puts $simCtrlSock "setOneSwitchChangeBit \$fabric"
   puts "SIM: [gets $simCtrlSock]"

   # wait for sweep to end or exit
   if {[osmWaitForUpOrDead $osmLog 1]} {
      return 1
   }

   # wait 3 seconds
   after 1000

   #Verify that the default port is in the PKey table of all ports
   puts "-I- Calling simulator to verify all HCA ports have either 0x7fff or 0xffff"
   puts $simCtrlSock "verifyDefaultPKeyForAllHcaPorts \$fabric"
   set res [gets $simCtrlSock]
   puts "SIM: $res"

   if {$res == 0} {
      puts "-I- Pkey check flow completed successfuly"
   } else {
      puts "-E- Pkey check flow failed"
   }

   return $res
}
