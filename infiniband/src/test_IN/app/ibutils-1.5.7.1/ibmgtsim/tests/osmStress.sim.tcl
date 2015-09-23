# TEST FLOW FOR OPENSM STRESS TESTING
#
#
########################################################################
#
# IMPLEMENTATION:
#
#
########################################################################

# Returns randomly 0 or 1
proc notToDo {} {
    return [expr [rmRand] > 0.5]
}

# set the change bit on one of the switches:
# TEMPORARY - need to add traps instead
proc setOneSwitchChangeBit {fabric} {
   global IB_SW_NODE

   set allNodes [IBFabric_NodeByName_get $fabric]
   foreach nameNNode $allNodes {
      set node [lindex $nameNNode 1]
      #if Switch
      if {[IBNode_type_get $node] == $IB_SW_NODE} {
         set swi [IBMSNode_getSwitchInfo sim$node]
         set lifeState [ib_switch_info_t_life_state_get $swi]
         set lifeState [expr ($lifeState & 0xf8) | 4 ]
         ib_switch_info_t_life_state_set $swi $lifeState
         puts "-I- Set change bit on switch:$node"
         return "-I- Set change bit on switch:$node"
      }
   }
   return "-E- Fail to set any change bit. Could not find a switch"
}

# obtain the list of addressible ports of the fabric:
# since the IBDM does not support port 0 (all port are really physp)
# we need to provide back the list of node/portNum pairs
proc getAddressiblePorts {fabric} {
   global IB_SW_NODE

   set nodePortNumPairs {}

   # go over all nodes
   foreach nodeNameNId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameNId 1]

      # switches has only one port - port 0
      if {[IBNode_type_get $node] == $IB_SW_NODE} {
         lappend nodePortNumPairs [list $node 0]
      } else {
         set pMin 1
         set pMax [IBNode_numPorts_get $node]
         for {set pn $pMin} {$pn <= $pMax} {incr pn} {
            set port [IBNode_getPort $node $pn]
            if {$port == ""} {continue}

            # if the port is not connected ignore it:
            if {[IBPort_p_remotePort_get $port] != ""} {
               lappend nodePortNumPairs [list $node $pn]
            }
         }
      }
   }
   return $nodePortNumPairs
}

# assign a specify port lid:
proc assignPortLid {node portNum lid} {
   global PORT_LID LID_PORTS BAD_LIDS
   global POST_SUBNET_UP

   # HACK we can not trust the Fabric PortByLid anymore...

   # first we set the IBDM port lid
   if {$portNum == 0} {
      set pMin 1
      set pMax [IBNode_numPorts_get $node]
      for {set pn $pMin} {$pn <= $pMax} {incr pn} {
         set port [IBNode_getPort $node $pn]
         if {$port != ""} {
            IBPort_base_lid_set $port $lid
         }
      }
   } else {
      set port [IBNode_getPort $node $portNum]
      if {$port != ""} {
         IBPort_base_lid_set $port $lid
      }
   }
   set pi [IBMSNode_getPortInfo sim$node $portNum]
   set prevLid [ib_port_info_t_base_lid_get $pi]

   if {$lid == $prevLid} {return 0}

   ib_port_info_t_base_lid_set $pi $lid

   # if case the SM was previously run we do not want any
   # effect of the PORT_LID.. tables
   if {$POST_SUBNET_UP} {return}

   # track it:
   set key "$node $portNum"
   if {[info exists LID_PORTS($prevLid)]} {
      set idx [lsearch $LID_PORTS($prevLid) $key]
      if {$idx >= 0} {
         set LID_PORTS($prevLid) [lreplace $LID_PORTS($prevLid) $idx $idx]
         if {[llength $LID_PORTS($prevLid)] < 2} {
            if {[info exists BAD_LIDS($prevLid)]} {
               unset BAD_LIDS($prevLid)
            }
         }
      }
   }

   set PORT_LID($key) $lid
   if {![info exists LID_PORTS($lid)]} {
      set LID_PORTS($lid) "{$key}"
   } else {
      if {[lsearch $LID_PORTS($lid) $key] < 0} {
         lappend LID_PORTS($lid) $key
         if {[llength $LID_PORTS($lid)] > 1} {
            set BAD_LIDS($lid) 1
         }
      }
   }
}

# Assign legal lids
proc assignLegalLids {fabric lmc} {
   global POST_SUBNET_UP

   # mark that we are only in initial lid assignment
   # the SM was not run yet.
   set  POST_SUBNET_UP 0

   set addrNodePortNumPairs [getAddressiblePorts $fabric]
   set lidStep [expr 1 << $lmc]

   # init all port to contigious lids
   # every 15 lids skip some.
   set lid $lidStep

   # go over al the ports in the fabric and set their lids
   foreach nodePortNum $addrNodePortNumPairs {
      set node [lindex $nodePortNum 0]
      set pn   [lindex $nodePortNum 1]
      assignPortLid $node $pn $lid

      if {$lid % ($lidStep *15) == 0} {
         set newLid [expr $lid + $lidStep * 5]
         puts [format "-I- Skipping some lids 0x%04x -> 0x%04x" $lid $newLid]
         set lid $newLid
      } else {
         incr lid $lidStep
      }

   }
   return "-I- Set all port lids"
}

# after first OpenSM run we need to be able to get the assigned
# lids
proc updateAssignedLids {fabric} {
   global PORT_LID LID_PORTS BAD_LIDS POST_SUBNET_UP

   set addrNodePortNumPairs [getAddressiblePorts $fabric]
   foreach nodePortNum $addrNodePortNumPairs {

      set node [lindex $nodePortNum 0]
      set pn   [lindex $nodePortNum 1]
      set pi [IBMSNode_getPortInfo sim$node $pn]
      set lid [ib_port_info_t_base_lid_get $pi]

      assignPortLid $node $pn $lid
   }

   # flag the fact that from now any change in lids
   # is not going to affect the PORT_LID...
   set POST_SUBNET_UP 1

   set numLids [llength [array names LID_PORTS]]
   set numBad  [llength [array names BAD_LIDS]]
   return "-I- Updated $numLids lids $numBad are bad"
}

# randomaly select one of the numbers provided in the given sequence
proc getRandomNumOfSequence {seq} {
   set idx [expr int([rmRand]*[llength $seq])]
   return [lindex $seq $idx]
}

# get a free lid value by randomizing it and avoiding used ones
proc getFreeLid {lmc} {
   global LID_PORTS
   set lid 0
   while {$lid == 0} {
      set lid [expr int( [array size LID_PORTS] * 3 * [rmRand])]
      set lid [expr ($lid >> $lmc) << $lmc]
      if {[info exists LID_PORTS($lid)]} {set lid 0}
   }
   return $lid
}

# get a used lid
proc getUsedLid {} {
   global LID_PORTS
   return [getRandomNumOfSequence [array names LID_PORTS]]
}

proc setNodePortsState {node state} {
   global DISCONNECTED_NODES
   global IB_SW_NODE

   set name [IBNode_name_get $node]
   # simply go over all ports of the node excluding port 0 and
   # set the link logic state on the port info to $state
   for {set pn 1} {$pn <= [IBNode_numPorts_get $node]} {incr pn} {

      set port [IBNode_getPort $node $pn]
      if {$port != ""} {
         set guid [IBPort_guid_get $port]
         set remPort [IBPort_p_remotePort_get $port]
         if {$remPort != ""} {

				# Make sure the remote port is not the SM port
				# HACK: rely on SM to be running on H-1/U1 ..
				if {($pn == 1) && ([IBNode_name_get [IBPort_p_node_get $remPort]] == "H-1/U1")} {
					puts "-I- Disconnecting node:$name guid:$guid SKIPPING port:$pn connected to the SM"
					continue
				}

            # handle the local port
            if {$state == 1} {
               # disconnected
               puts "-I- Disconnecting node:$name guid:$guid port:$pn"
               set DISCONNECTED_NODES($node) 1
            } else {
               # connected
               puts "-I- Connecting node:$name guid:$guid port:$pn"
               catch {unset DISCONNECTED_NODES($node)}
            }
            # do the actual job (including traps and change bits...)
            IBMSNode_setLinkStatus sim$node $pn $state


            set remPn [IBPort_num_get $remPort]
            set remNode [IBPort_p_node_get $remPort]
            set remPortGuid [IBPort_guid_get $remPort]
            set remName [IBNode_name_get $remNode]
            # if the remote port is of an HCA we need to mark it too as
            # BAD or clean it out:
            if {[IBNode_type_get $remNode] != $IB_SW_NODE} {
               if {$state == 1} {
                  # disconnected
                  puts "-I- Disconnecting node:$remName guid:$remPortGuid port:$remPn"
                  set DISCONNECTED_NODES($remNode) 1
               } else {
                  # connected
                  puts "-I- Connecting node:$remName guid:$remPortGuid port:$remPn"
                  catch {unset DISCONNECTED_NODES($remNode)}
               }
            }
            # always take the action to the remote port too:
            IBMSNode_setLinkStatus sim$remNode $remPn $state
         }
      }
   }
}

# disconnect a node from the fabric by setting physical port
# state to DOWN
proc disconnectNode {node} {
   global DISCONNECTED_NODES
   setNodePortsState $node 1
   set DISCONNECTED_NODES($node) 1
}

# Connect a node of the fabric by setting physical port
# state to INIT
proc connectNode {node} {
   global DISCONNECTED_NODES
   setNodePortsState $node 2
   if {[info exists DISCONNECTED_NODES($node)]} {
      unset DISCONNECTED_NODES($node)
   }
}

proc setPortsDisconnected {fabric lmc} {
   global PORT_LID BAD_LIDS
   global DISCONNECTED_NODES

   set addrNodePortNumPairs [getAddressiblePorts $fabric]

   # never do more then 1/5 of the HCAs disconnects to avoid disconnecting
   # half subnet ...
   set maxDisconnects [expr int([llength $addrNodePortNumPairs] / 5.0)]

   set numDisc 0
   # go over all the ports in the fabric and set their lids
   foreach nodePortNum $addrNodePortNumPairs {
      set node [lindex $nodePortNum 0]
      set pn   [lindex $nodePortNum 1]

      if {$pn != 0} {
         set port [IBNode_getPort $node $pn]
         if {$port == ""} {continue}
         set guid [IBPort_guid_get $port]
      } else {
         set guid [IBNode_guid_get $node]
      }

      #Randomly do the disconnection
      if {[notToDo]} {
          continue
      }

      if {[array size DISCONNECTED_NODES] > $maxDisconnects} {
         continue
      }

      # avoid disconnect of the SM node.
      set smNode [IBFabric_getNode $fabric "H-1/U1"]
      set smPort [IBNode_getPort $smNode 1]
      set smRemPort [IBPort_p_remotePort_get $smPort]
      set smRemNode [IBPort_p_node_get $smRemPort]

      if {($smNode != $node) && ($smRemNode != $node)} {
          puts "-I- Disconnecting node:$node guid:$guid"
          disconnectNode $node
          incr numDisc
      }
   }
   # every node/port
   set res "-I- Disconnected $numDisc nodes (total [array size DISCONNECTED_NODES])"
   puts "$res"
   return $res
}

proc writeGuid2LidFile {fileName lmc} {
   global PORT_LID BAD_LID BAD_GUIDS
   global SM_UNKNOWN_LIDS DISCONNECTED_NODES

   if {[catch {set f [open $fileName w]} e]} {
      puts "-E- $e"
      exit 1
   }

   set randProfile {
      OK OK OK OK OK OK OK OK OK
      OK OK OK OK ExtraGuid
      OK OK OK OK Skip
      OK OK OK OK ExtraGuidCollision
      OK OK OK OK LidZero
      OK OK OK OK LidOutOfRange
      OK OK OK OK NoLid
      OK OK OK OK GarbadgeGuid
      OK OK OK OK OK OK OK OK OK
   }

   set numMods 0

   # go over all PORT_LID
   foreach nodeNPort [array names PORT_LID] {
      set node [lindex $nodeNPort 0]
      set pn   [lindex $nodeNPort 1]

      if {$pn != 0} {
         set port [IBNode_getPort $node $pn]
         if {$port == ""} {continue}
         set guid [IBPort_guid_get $port]
         set maxLidOffset [expr (1 << $lmc) - 1]
      } else {
         set guid [IBNode_guid_get $node]
         set maxLidOffset 0
      }

      set pi [IBMSNode_getPortInfo sim$node $pn]
      set lid [ib_port_info_t_base_lid_get $pi]
      set lidMax [expr $lid + $maxLidOffset]

      # randomize what we want to do with it:
      set hackCode [getRandomNumOfSequence $randProfile]
      switch $hackCode {
         OK {
            puts $f "$guid $lid $lidMax\n"
         }
         ExtraGuid {
            puts $f "$guid $lid $lidMax\n"
            set extraGuid [string replace $guid 4 5 "8"]
            set extraLid [getFreeLid $lmc]
            set extraLidMax  [expr $extraLid + $maxLidOffset]
            puts $f "$extraGuid $extraLid \n"
            puts [format "-I- Added extra guid:$extraGuid lid:0x%04x,0x%04x" \
                     $extraLid $extraLidMax]
            incr numMods
         }
         ExtraGuidCollision {
            puts $f "$guid $lid $lidMax\n"
            set extraGuid [string replace $guid 4 5 "8"]
            puts $f "$extraGuid $lid $lidMax\n"
            puts [format "-I- Added colliding guid:$extraGuid lid:0x%04x,0x%04x" \
                     $lid $lidMax]
            set BAD_LIDS($lid) 1
            set BAD_GUIDS($guid) 1
            # if the node is disconnected and we create invalid guid2lid
            # the SM will never know about that lid
            if {[info exists DISCONNECTED_NODES($node)]} {
               set SM_UNKNOWN_LIDS($lid) 1
            }
            incr numMods
         }
         Skip {
            puts [format "-I- Skipped guid:$guid lid:0x%04x,0x%04x" \
                     $lid $lidMax]
            # if the node is disconnected and we create invalid guid2lid
            # the SM will never know about that lid
            if {[info exists DISCONNECTED_NODES($node)]} {
               set SM_UNKNOWN_LIDS($lid) 1
            }
            incr numMods
         }
         LidZero {
            puts $f "$guid 0 0\n"
            puts [format "-I- LidZero guid:$guid lid:0x%04x,0x%04x" \
                     $lid $lidMax]
            # if the node is disconnected and we create invalid guid2lid
            # the SM will never know about that lid
            if {[info exists DISCONNECTED_NODES($node)]} {
               set SM_UNKNOWN_LIDS($lid) 1
            }
            incr numMods
         }
         LidOutOfRange {
            puts $f "$guid 0xc001 0xc002\n"
            puts [format "-I- OutOfRange guid:$guid lid:0x%04x,0x%04x" \
                     $lid $lidMax]
            # if the node is disconnected and we create invalid guid2lid
            # the SM will never know about that lid
            if {[info exists DISCONNECTED_NODES($node)]} {
               set SM_UNKNOWN_LIDS($lid) 1
            }
            incr numMods
         }
         NoLid {
            puts $f "$guid \n"
            puts [format "-I- NoLids guid:$guid lid:0x%04x,0x%04x" \
                     $lid $lidMax]
            # if the node is disconnected and we create invalid guid2lid
            # the SM will never know about that lid
            if {[info exists DISCONNECTED_NODES($node)]} {
               set SM_UNKNOWN_LIDS($lid) 1
            }
            incr numMods
         }
         GarbadgeGuid {
            puts $f "blablablabla $lid $lidMax\n"
            puts [format "-I- GarbadgeGuid guid:$guid lid:0x%04x,0x%04x" \
                     $lid $lidMax]
            # if the node is disconnected and we create invalid guid2lid
            # the SM will never know about that lid
            if {[info exists DISCONNECTED_NODES($node)]} {
               set SM_UNKNOWN_LIDS($lid) 1
            }
            incr numMods
         }
      }
   }
   close $f
   puts "-I- injected $numMods modifications to guid2lid file"
   puts "-I- total num lids which will be unknown to the SM:[llength [array names SM_UNKNOWN_LIDS]]"
   return "-I- injected $numMods modifications to guid2lid file"
}


# connect back all disconnected
# Only if isAll==1 connect all back, else connect randomly
proc connectAllDisconnected {fabric isAll} {
   global DISCONNECTED_NODES

   set numConn 0
   foreach node [array names DISCONNECTED_NODES] {
      if { ($isAll == 0) && [notToDo]} { continue }

      puts "-I- Re-Conneting $node"
      connectNode $node
      incr numConn
   }
   return "-I- Reconnected $numConn nodes"
}

# check that all the ports that have a valid lid
# still holds this value
proc checkLidValues {fabric lmc} {
   global PORT_LID LID_PORTS BAD_LIDS BAD_GUIDS
   global DISCONNECTED_NODES SM_UNKNOWN_LIDS

   # get all addressable nodes
   set addrNodePortNumPairs [getAddressiblePorts $fabric]

   set numPorts 0
   set numErrs 0
   foreach nodePortNumPair $addrNodePortNumPairs {
      set node [lindex $nodePortNumPair 0]
      set pn   [lindex $nodePortNumPair 1]
      set key "$node $pn"

      if {$pn == 0} {
         set guid [IBNode_guid_get $node]
      } else {
         set port [IBNode_getPort $node $pn]
         set guid [IBPort_guid_get $port]
      }

      # ignore nodes that are disconnected:
      if {[info exists DISCONNECTED_NODES($node)]} {continue}

      # ignore marked bad guids:
      if {[info exists BAD_GUIDS($guid)]} {continue}

      # check what was the target lid:
      if {![info exists PORT_LID($key)]} {
         puts "-W- Somehow we do not have a lid assigned to $key"
         continue
      }

      set lid $PORT_LID($key)

      # ignore lids that are unknown to the SM
      if {[info exists SM_UNKNOWN_LIDS($lid)]} {continue}

      # check if this lid is not marked "bad" - i.e. distorted somehow:
      if {[info exists BAD_LIDS($lid)]} {continue}

      # now go get the actual lid:
      set pi [IBMSNode_getPortInfo sim$node $pn]
      set actLid [ib_port_info_t_base_lid_get $pi]

      incr numPorts

      if {($lid != 0) && ($lid != $actLid)} {
         puts [format "-E- On Port:$key Guid:$guid Expected lid:0x%04x != 0x%04x" \
                  $lid $actLid]
         incr numErrs
      }
   }
   if {$numErrs} {
      puts "-E- Got $numErrs missmatches in lid assignment out of $numPorts ports"
   } else {
      puts "-I- scanned $numPorts ports with no error"
   }
   return $numErrs
}

# set the change bit on one of the switches:
proc setOneSwitchChangeBit {fabric} {
   global DISCONNECTED_NODES
   global IB_SW_NODE

   set allNodes [IBFabric_NodeByName_get $fabric]

   foreach nameNNode $allNodes {
      set node [lindex $nameNNode 1]
      if {[IBNode_type_get $node] == $IB_SW_NODE} {
         if {![info exists DISCONNECTED_NODES($node)]} {
            set swi [IBMSNode_getSwitchInfo sim$node]
            set lifeState [ib_switch_info_t_life_state_get $swi]
            set lifeState [expr ($lifeState & 0xf8) | 4 ]
            ib_switch_info_t_life_state_set $swi $lifeState
            puts "-I- Set change bit on switch:$node"
            return "-I- Set change bit on switch:$node"
         }
      }
   }
   return "-E- Fail to set any change bit. Could not find a switch"
}

# send a single port join request
proc sendJoinLeaveForPort {fabric port isLeave} {

   # allocate a new mc member record:
   set mcm [new_madMcMemberRec]

   # join the IPoIB broadcast gid:
   madMcMemberRec_mgid_set $mcm 0xff12401bffff0000:00000000ffffffff

   # we must provide our own port gid
   set portGuid [IBPort_guid_get $port]
   madMcMemberRec_port_gid_set $mcm \
      "0xfe80000000000000:[string range $portGuid 2 end]"

   # must require full membership:
   madMcMemberRec_scope_state_set $mcm 0x1

   # we need port number and sim node for the mad send:
   set portNum [IBPort_num_get $port]
   set node [IBPort_p_node_get $port]

   # we need the comp_mask to include the mgid, port gid and join state:
   set compMask [format "0x%X" [expr (1<<16) | 3]]


   #getting the SM lid
   #From ibmgtsim.guids.txt get the GUID of H-1/P1
   #From guid2lid file get the lid of the GUID
   set n [IBFabric_getNode $fabric "H-1/U1"]
   set p [IBNode_getPort $n 1]
   set guid [IBPort_guid_get $p]
   set pi [IBMSNode_getPortInfo sim$n 1]
   set lid [ib_port_info_t_base_lid_get $pi]

   # send it to the SM_LID:
   if {$isLeave} {
      puts "-I- Leaving port $node $portNum $portGuid"
      madMcMemberRec_send_del $mcm sim$node $portNum $lid $compMask
   } else {
      puts "-I- Joining port $node $portNum $portGuid"
      madMcMemberRec_send_set $mcm sim$node $portNum $lid $compMask
   }


   # deallocate
   delete_madMcMemberRec $mcm

   return 0
}

# find all active HCA ports
proc getAllActiveHCAPorts {fabric} {
   global IB_SW_NODE

   set hcaPorts {}

   # go over all nodes:
   foreach nodeNameId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameId 1]

      # we do care about non switches only
      if {[IBNode_type_get $node] != $IB_SW_NODE} {
         # go over all ports:
         for {set pn 1} {$pn <= [IBNode_numPorts_get $node]} {incr pn} {
            set port [IBNode_getPort $node $pn]
            if {($port != "") && ([IBPort_p_remotePort_get $port] != "")} {
               lappend hcaPorts $port
            }
         }
      }
   }
   return $hcaPorts
}

# randomize join for all of the fabric HCA ports:
# if isAll==1 join all, else join randomly
proc randomJoinAllHCAPorts {fabric maxDelay_ms isAll} {
   # get all HCA ports:
   set hcaPorts [getAllActiveHCAPorts $fabric]
   puts "-I- Total of [llength $hcaPorts] HCA ports"

   # set a random order:
   set orederedPorts {}
   foreach port $hcaPorts {
      lappend orederedPorts [list $port [rmRand]]
   }

   # sort:
   set orederedPorts [lsort -index 1 -real $orederedPorts]
   set numHcasJoined 0

   # Now do the joins - waiting random time between them:
   foreach portNOrder $orederedPorts {
      if {($isAll == 0) && [notToDo]} {continue}

      set port [lindex $portNOrder 0]

      if {![sendJoinLeaveForPort $fabric $port 0]} {
         incr numHcasJoined
      }

      after [expr int([rmRand]*$maxDelay_ms)]
   }
   return $numHcasJoined
}

# randomize join for all of the fabric HCA ports:
# Please note that NOT all the ports are leaving -
# each port is by random leaving or staying the same
proc randomLeaveAllHCAPorts {fabric maxDelay_ms} {
   # get all HCA ports:
   set hcaPorts [getAllActiveHCAPorts $fabric]

   # set a random order:
   set orederedPorts {}
   foreach port $hcaPorts {
      lappend orederedPorts [list $port [rmRand]]
   }

   # sort:
   set orederedPorts [lsort -index 1 -real $orederedPorts]
   set numHcasLeft 0

   # Now do the joins - waiting random time between them:
   foreach portNOrder $orederedPorts {

      if {[notToDo]} {
          continue
      }


      set port [lindex $portNOrder 0]

      if {![sendJoinLeaveForPort $fabric $port 1]} {
         incr numHcasLeft
      }

      after [expr int([rmRand]*$maxDelay_ms)]
   }
   return $numHcasLeft
}

# register some of the nodes to receive some
# reports.
proc randomRegisterFormInformInfo {fabric} {
	global IB_INFORM_INFO_COMP_TRAP_NUM IB_INFORM_INFO_COMP_GID
	global IB_INFORM_INFO_COMP_LID_END
	global IB_INFORM_INFO_COMP_LID_BEGIN

	set joinTypes {ByTrap ByGid ByLid ByLidRange All All All All All}

   # get all HCA ports:
   set hcaPorts [getAllActiveHCAPorts $fabric]

	# HACK assume SM is H-1/U1
   set smNode [IBFabric_getNode $fabric "H-1/U1"]
   set smPort [IBNode_getPort $smNode 1]
   set smPortInfo [IBMSNode_getPortInfo sim$smNode 1]
   set smLid [ib_port_info_t_base_lid_get $smPortInfo]

	foreach port $hcaPorts {
		# we skip most of them
		if {[rmRand] > 0.66} {continue}

		madGenericInform i

		if {[rmRand] > 0.5} {
			i configure -subscribe 1
			set mode subscribing
		} else {
			i configure -subscribe 0
			set mode clearing
		}

		set portName [IBPort_getName $port]
		set node [IBPort_p_node_get $port]
		set portNum [IBPort_num_get $port]

		set joinType [getRandomNumOfSequence $joinTypes]

		switch $joinType {
			ByTrap {
				set trapNum [getRandomNumOfSequence {128 144 64 65 66 67}]
				i configure -trap_num $trapNum
				set compMask $IB_INFORM_INFO_COMP_TRAP_NUM
				puts "-I- $mode InformInfo for $portName by trap num:$trapNum"
			}
			ByGid {
				set otherPort [getRandomNumOfSequence $hcaPorts]
				set gid "0xfe80000000000000:[string range [IBPort_guid_get $otherPort] 2 end]"
				i configure -gid $gid
				set compMask $IB_INFORM_INFO_COMP_GID
				puts "-I- $mode InformInfo for $portName by GID:$gid"
			}
			ByLid {
				set otherPort [getRandomNumOfSequence $hcaPorts]
				set fromLid [IBPort_base_lid_get $otherPort]
				set compMask $IB_INFORM_INFO_COMP_LID_BEGIN
				puts "-I- $mode InformInfo for $portName by LID:$fromLid"
			}
			ByLidRange {
				set otherPort [getRandomNumOfSequence $hcaPorts]
				set fromLid [IBPort_base_lid_get $otherPort]
				set toLid [expr $fromLid + int([rmRand]*10)]
				set compMask [expr $IB_INFORM_INFO_COMP_LID_BEGIN | $IB_INFORM_INFO_COMP_LID_END]
				puts "-I- $mode InformInfo for $portName by LID Range:$fromLid - $toLid"
			}
			All {
				puts "-I- $mode InformInfo for $portName to match all events"
				set compMask 0
			}
		}

		i send_set sim$node $portNum $smLid $compMask

		rename i ""
	}
	return 0
}

# send a path record request
# port1 - source
# port2 - destination
# port3 - requestor
proc sendPathRecordRequest {fabric port1 port2 port3} {
   puts "-I- Sending Path Record SRC=$port1 DST=$port2 Requstor:$port3"
   # allocate a new path record:
   set pam [new_madPathRec]

   #getting the SM lid
   #From ibmgtsim.guids.txt get the GUID of H-1/P1
   #From guid2lid file get the lid of the GUID
   set n [IBFabric_getNode $fabric "H-1/U1"]
   set p [IBNode_getPort $n 1]
   set guidSM \
      "0xfe80000000000000:[string range [IBPort_guid_get $p] 2 end]"
   set pi [IBMSNode_getPortInfo sim$n 1]
   set lid [ib_port_info_t_base_lid_get $pi]

   # update the path record mad:
   # provide our own port gid as the source gid
   madPathRec_dgid_set $pam \
      "0xfe80000000000000:[string range [IBPort_guid_get $port1] 2 end]"
   madPathRec_sgid_set $pam \
      "0xfe80000000000000:[string range [IBPort_guid_get $port2] 2 end]"
   madPathRec_num_path_set $pam 1
   madPathRec_qos_class_sl_set $pam 0x8
   madPathRec_mtu_set $pam 4
   madPathRec_rate_set $pam 2

   # we need port number and sim node for the mad send:
   set portNum [IBPort_num_get $port3]
   set node [IBPort_p_node_get $port3]

   # we need the comp_mask to include all the above (see 15.2.5.16)
   # 3 SGID
   # 12 NumbPath
   # 15 SL
   # 16,17 MTU
   # 18,19 RATE
   set compMask \
     [format "0x%X" [expr (1<<19) | (1<<18) | (1<<17) | (1<<16) | (1<<15) | (1<<12) | (1<<3) | (1<<2)]]

   # send it to the SM_LID:
   madPathRec_send_get $pam sim$node $portNum $lid $compMask

   # deallocate
   delete_madPathRec $pam

   return 0
}

# send a service record request
proc sendServiceRecordRequest {fabric port} {
   puts "-I- Sending Service Record request"
   # allocate a new path record:
   set pam [new_madServiceRec]

   # update the path record mad:
   # provide our own port gid as the source gid
   madServiceRec_sgid_set $pam \
      "0xfe80000000000000:[string range [IBPort_guid_get $port] 2 end]"
   madServiceRec_num_path_set $pam 1
   madServiceRec_sl_set $pam 0x8
   madServiceRec_mtu_set $pam 4
   madServiceRec_rate_set $pam 2

   # we need port number and sim node for the mad send:
   set portNum [IBPort_num_get $port]
   set node [IBPort_p_node_get $port]

   # we need the comp_mask to include all the above (see 15.2.5.16)
   # 3 SGID
   # 12 NumbPath
   # 15 SL
   # 16,17 MTU
   # 18,19 RATE
   set compMask \
     [format "0x%X" [expr (1<<19) | (1<<18) | (1<<17) | (1<<16) | (1<<15) | (1<<12) | (1<<3)]]


   #getting the SM lid
   #From ibmgtsim.guids.txt get the GUID of H-1/P1
   #From guid2lid file get the lid of the GUID
   set n [IBFabric_getNode $fabric "H-1/U1"]
   set p [IBNode_getPort $n 1]
   set guid [IBPort_guid_get $p]
   set pi [IBMSNode_getPortInfo sim$n 1]
   set lid [ib_port_info_t_base_lid_get $pi]

   # send it to the SM_LID:
   madServiceRec_send_set $pam sim$node $portNum $lid $compMask

   # deallocate
   delete_madServiceRec $pam

   return 0
}

# the returned list is used to select random actions to perform
proc InitRandomActionsList {} {
   set actList {
      CONNECT
      DISCONNECT
      JOIN
      LEAVE
      PATH-QUERY
		INFORM
   }
   return $actList
}

proc DisconnectRandomPorts {fabric} {
    setPortsDisconnected $fabric 0
}

proc ConnectRandomPorts {fabric} {
    connectAllDisconnected $fabric 0
}

# send a random number of patch record requests from any HCA to any other
proc SendRandomPathRecordRequests {fabric} {

   # get all HCA ports:
   set hcaPorts [getAllActiveHCAPorts $fabric]

   # set a random order:
   set orederedPorts {}
   foreach port $hcaPorts {
      lappend orederedPorts [list $port [rmRand]]
   }

   set numQueries [expr int([rmRand]*[llength $hcaPorts])]
   puts "-I- SendRandomPathRecordRequests randomizing $numQueries requests"

   # sort:
   set orederedPorts [lsort -index 1 -real $orederedPorts]

   for {set i 0} {$i < $numQueries} {incr i} {
      set portNOrder1 [getRandomNumOfSequence $orederedPorts]
      set portNOrder2 [getRandomNumOfSequence $orederedPorts]
      set portNOrder3 [getRandomNumOfSequence $orederedPorts]
      set port1 [lindex $portNOrder1 0]
      set port2 [lindex $portNOrder2 0]
      set port3 [lindex $portNOrder2 0]

      # Now send the request - waiting random time between them:
      after [expr int([rmRand]*10)]
      set retval [sendPathRecordRequest $fabric $port1 $port2 $port3]
   }
   puts "-I- SendRandomPathRecordRequests finished"
   return $numQueries
}

proc SendMulticastJoinMad {fabric} {
    randomJoinAllHCAPorts $fabric 10 0
}

proc SendMulticastLeaveMad {fabric} {
    randomLeaveAllHCAPorts $fabric 10
}

proc RunRandomStressFlow {fabric iterations} {

    puts "-I- INVOKING RANDOM FLOW for $iterations iterations"
    set actionsList [InitRandomActionsList]

    foreach action $actionsList {
       set RUN_ACTIONS($action) 0
    }

    #Do random steps
    for {set i 0} {$i < $iterations} {incr i} {

        set action [getRandomNumOfSequence $actionsList]
        incr RUN_ACTIONS($action)
        puts "-I- Performing Action: $action"

        #Disconnect ports
        switch $action {
           DISCONNECT { DisconnectRandomPorts $fabric  }
           CONNECT    { ConnectRandomPorts $fabric     }
           JOIN       { SendMulticastJoinMad $fabric   }
           LEAVE      { SendMulticastLeaveMad $fabric  }
           PATH-QUERY { SendRandomPathRecordRequests $fabric }
			  INFORM     { randomRegisterFormInformInfo $fabric }
           default    {
              puts "-E- Ignroing unknown action $action"
           }
        }
    }

    set actionsRun ""
    foreach action [array names RUN_ACTIONS] {
       append actionsRun "$action:$RUN_ACTIONS($action) "
    }
    if {$actionsRun == ""} {
       set actionsRun "No random actions run!!!"
    }

    return $actionsRun
}

set fabric [IBMgtSimulator getFabric]
set ACTION_ARRAY {}

# IBMgtSimulator init /home/eitan/CLUSTERS/Galactic.topo 42514 1
# source /home/eitan/SW/SVN/osm/branches/main2_0/osm/test/osmLidAssignment.sim.tcl
# assignPortLid node:1:SWL2/spine1/U3 0 99
#  checkLidValues $fabric 1
# set swi [IBMSNode_getSwitchInfo sim$node]
# set n simnode:1:SWL2/spine1/U3
# set x [IBMSNode_getPKeyTblB $n 0 0]
#

