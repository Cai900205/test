# TEST FLOW FOR STATIC LID ASSIGNMENT:
#
# 5. Generate file:
#    a. extra guids
#    b. missing guids
#    c. collision (2 guids same lid)
#    e. miss aligned
#    f. out of range lids
#    d. use 1 a,b,c
# 6. Build list of good mapping. keep extra guids used lids.
# 6.5 randomize fabric lids:
#    a. lid is pre-assigned correctly
#    b. lid is pre-assigned incorrectly
#       1. no other node use that lid
#       2. some other nodes use it
#       3. lid is un-assigned
#   c. for unknown guids:
#      1. collision
#      2. unassigned
#      3. ussigned unique
# 7. Run SM
# 8. Check :
#     a. all good assignments are met.
#     b. all others are valid (aligned, not overlapping, keep as much
#        as possible)
#     c. no extending of lid range if holes.
# 9. validate new file.
# 10. Randomize
#      a. nodes diassper appear
#      b. reset the lid
#      c. new lid uniq
#      d. new lid colide.
#
########################################################################
#
# IMPLEMENTATION:
#
# 0. Prepare the fabric:
#    a. assign random lids - not colliding LMC aligned.
#       Keep some empty ranges. - Init the "good" map lists.
#
# 1. randomize lid errors
#    * not assigned
#    * colide with other ports
#    * mis-aligned lids
#    * modify and single
#    * modify and collide
#
# 2. Disconnect some nodes (all ports that connects to the fabric)
#    when disconnecting leaf switches mark the HCA lids as BAD
#
# 3. Create guid2lid file:
#    a. invent extra guids
#    b. skip some existing guids
#    c. on top of collisions from the fabric add some extra collision
#    d. out of range lids
#    e. bad formatted guids
#    f. bad formatted lids
#
# 4. Generate lists of :
#    a. bad lids - those who have been hacked and are not expected to
#       be assigned by the SM.
#    b. unknown lids: those who are not accessible by the SM and also
#       do not have persistance data. To be ignored by the following sweeps.
#
# 5. Run the SM
#
# 6. Check:
#    a. all lids that should retain their value did so
#    b. all other lids are assigned correctly:
#       1. aligned correctly
#       2. using free ranges in the lid space
#       3. validate the new guid2lid file
#
# 7. Randomize lid changes:
#    a. zero some port lids
#    b. collide some port lids
#    c. invent some new lids
#
#  8. Inject sweep - set switch info change bit
#
#  9. wait for sweep.
#
# 10. goto 6
#
########################################################################

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

   # simply go over all ports of the node excluding port 0 and
   # set the link logic state on the port info to DOWN
   for {set pn 1} {$pn <= [IBNode_numPorts_get $node]} {incr pn} {
      set pi [IBMSNode_getPortInfo sim$node $pn]
      set speed_state [ib_port_info_t_state_info1_get $pi]
      ib_port_info_t_state_info1_set $pi [expr $speed_state & 0xf0 | $state]

      # try remote side:
      set port [IBNode_getPort $node $pn]
      if {$port != ""} {
         set remPort [IBPort_p_remotePort_get $port]
         if {$remPort != ""} {
            set remPn [IBPort_num_get $remPort]
            set remNode [IBPort_p_node_get $remPort]
            set remPortGuid [IBPort_guid_get $remPort]
            set pi [IBMSNode_getPortInfo sim$remNode $remPn]
            set speed_state [ib_port_info_t_state_info1_get $pi]
            ib_port_info_t_state_info1_set $pi \
               [expr $speed_state & 0xf0 | $state]
            # if the remote port is of an HCA we need to mark it too as
            # BAD or clean it out:
            if {[IBNode_type_get $remNode] != $IB_SW_NODE} {
               if {$state == 1} {
                  # disconnected
                  puts "-I- Disconnecting node:$remNode guid:$remPortGuid"
                  set DISCONNECTED_NODES($remNode) 1
               } else {
                  # connected
                  puts "-I- Connecting node:$remNode guid:$remPortGuid"
                  catch {unset DISCONNECTED_NODES($node)}
               }
            }
         }
      }
   }
}

# disconnect a node from the fabric bu setting all physical ports
# state to DOWN
proc disconnectNode {node} {
   global DISCONNECTED_NODES
   setNodePortsState $node 1
   set DISCONNECTED_NODES($node) 1
}

# disconnect a node from the fabric bu setting all physical ports
# state to DOWN
proc connectNode {node} {
   global DISCONNECTED_NODES
   setNodePortsState $node 2
   if {[info exists DISCONNECTED_NODES($node)]} {
      unset DISCONNECTED_NODES($node)
   }
}

# randomize lid assignment errors:
# 1. not assigned
# 2. colide with other ports
# 3. mis-aligned lids
# 4. modify and single
# 5. modify and collide
#
proc setLidAssignmentErrors {fabric lmc} {
   global PORT_LID BAD_LIDS
   global DISCONNECTED_NODES

   # we simply go over all ports again randomize errors
   # then inject them acordingly

   set randProfile {
      OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK
      NotAssigned NotAssigned
      MisAligned MisAligned
      ModifiedCollide ModifiedCollide
      ModifiedNotCollide ModifiedNotCollide
      DisconnectPort
   }

   set addrNodePortNumPairs [getAddressiblePorts $fabric]
   set lidStep [expr 1 << $lmc]
   set numBadLids 0
   set numDisc 0
   # go over al the ports in the fabric and set their lids
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

      set badLidCode [getRandomNumOfSequence $randProfile]

      switch $badLidCode {
         NotAssigned {
            puts "-I- Nulling node:$node guid:$guid port:$pn"
            assignPortLid $node $pn 0
            incr numBadLids
         }
         MisAligned {
            if {($lmc > 0) && ($pn != 0)} {
               puts "-I- Mis-align node:$node guid:$guid port:$pn"
               set badLid [expr $PORT_LID($node $pn) + 1]
               assignPortLid $node $pn $badLid
               # this is the only case where we need to
               # mark bad lids explicitly as the assignmemnt thinks it is ok
               set BAD_LIDS($badLid) 1
               incr numBadLids
            }
         }
         ModifiedCollide {
            set badLid [getUsedLid]
            puts [format \
                     "-I- Colliding node:$node guid:$guid port:$pn on lid:0x%04x" \
                     $badLid]
            assignPortLid $node $pn $badLid
            incr numBadLids
         }
         ModifiedNotCollide {
            set badLid [getFreeLid $lmc]
            puts [format "-I- Changing node:$node guid:$guid port:$pn to free lid:0x%04x" $badLid]
            assignPortLid $node $pn $badLid
            incr numBadLids
         }
         DisconnectPort {
            # never do more then 4 disconnects to avoid disconnecting
            # half subnet ...
            if {[array size DISCONNECTED_NODES] > 4} {
               continue
            }

            # avoid disconnect of the SM node.
            # HACK: SM is assumed to always run on H-1/U1
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
      }
   }
   # every node/port
   set res "-I- Created $numBadLids lid modifications, disconnected $numDisc nodes"
   puts "$res"
   return $res
}

# 3. Create guid2lid file:
#    a. invent extra guids
#    b. skip some existing guids
#    c. on top of collisions from the fabric add some extra collision
#    d. out of range lids
#    e. bad formatted guids
#    f. bad formatted lids
#
# we also track all lids that can not be known to the SM
# since their node is disconnected and the persistancy is hacked
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
proc connectAllDisconnected {fabric} {
   global DISCONNECTED_NODES

   set numConn 0
   foreach node [array names DISCONNECTED_NODES] {
      puts "-I- Re-Conneting $node"
      connectNode $node
      incr numConn
   }
   return "-I- Reconencted $numConn nodes"
}

# start from the SM node and BFS through non disconnected nodes
# tracking visited nodes into: BFS_FROM_SM_VISITED(node)
proc bfsConnectedFromSM {fabric} {
   global BFS_FROM_SM_VISITED DISCONNECTED_NODES

   # HACK: SM is assumed to always run on H-1/U1
   set smNode [IBFabric_getNode $fabric "H-1/U1"]
   set smPort [IBNode_getPort $smNode 1]

   if {[info exists BFS_FROM_SM_VISITED]} {
      unset BFS_FROM_SM_VISITED
   }

   set BFS_FROM_SM_VISITED($smNode) 1

   set remPort [IBPort_p_remotePort_get $smPort]
   set remNode [IBPort_p_node_get $remPort]

   set nodesQueue $remNode

   while {[llength $nodesQueue]} {
      set node [lindex $nodesQueue 0]
      set nodesQueue [lrange $nodesQueue 1 end]

      if {[info exists BFS_FROM_SM_VISITED($node)]} {continue}
      set BFS_FROM_SM_VISITED($node) 1

      if {[info exists DISCONNECTED_NODES($node)]} {continue}
      set numPorts [IBNode_numPorts_get $node]
      for {set pn 1} {$pn <= $numPorts} {incr pn} {
         set port [IBNode_getPort $node $pn]

         if {$port == ""} {continue}

         set remPort [IBPort_p_remotePort_get $port]
         if {$remPort == ""} {continue}

         set remNode [IBPort_p_node_get $remPort]
         if {![info exists BFS_FROM_SM_VISITED($remNode)]} {
            lappend nodesQueue $remNode
         }
      }
   }
   set numTraversed [llength [array names BFS_FROM_SM_VISITED]]
   puts "-I- Found $numTraversed nodes traversing from SM through connected nodes"
   foreach nodeNName [IBFabric_NodeByName_get $fabric] {
      set name [lindex $nodeNName 0]
      set node [lindex $nodeNName 1]
      if {![info exists BFS_FROM_SM_VISITED($node)]} {
         puts "-I- Unreachable node:$name"
      }
   }
}

# check that all the ports that have a valid lid
# still holds this value
proc checkLidValues {fabric lmc} {
   global PORT_LID LID_PORTS BAD_LIDS BAD_GUIDS
   global DISCONNECTED_NODES SM_UNKNOWN_LIDS
   global BFS_FROM_SM_VISITED

   # get all addressable nodes
   set addrNodePortNumPairs [getAddressiblePorts $fabric]

   # avoid checking disconnected nodes:
   bfsConnectedFromSM $fabric

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

      # check if the node is not masked by other disconnected nodes
      if {![info exists BFS_FROM_SM_VISITED($node)]} {
         puts "-I- Node [IBNode_name_get $node] is not accessible due to other disconnectes"
         continue
      }

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

# set the change bit on the switch accross from the SM
proc setOneSwitchChangeBit {fabric} {
	global IB_SW_NODE

   # HACK: SM is assumed to always run on H-1/U1
   set smNode [IBFabric_getNode $fabric "H-1/U1"]
	if {$smNode == ""} {
		return "-E- Fail to find SM node H-1/U1"
	}

   set smPort [IBNode_getPort $smNode 1]
	if {$smPort == ""} {
		return "-E- Fail to find SM Port H-1/U1/P1"
	}

   set remPort [IBPort_p_remotePort_get $smPort]
	if {$remPort  == ""} {
		return "-E- Fail to find SM Port H-1/U1/P1 remote port"
	}

   set node [IBPort_p_node_get $remPort]
	if {[IBNode_type_get $node] != $IB_SW_NODE} {
		return "-E- Fail to find SM Port H-1/U1/P1 remote node is not a switch!"
	}

	set swi [IBMSNode_getSwitchInfo sim$node]
	set lifeState [ib_switch_info_t_life_state_get $swi]
	set lifeState [expr ($lifeState & 0xf8) | 4 ]
	ib_switch_info_t_life_state_set $swi $lifeState
	puts "-I- Set change bit on switch:$node"
	return "-I- Set change bit on switch:$node"
}

set fabric [IBMgtSimulator getFabric]


# IBMgtSimulator init /home/eitan/CLUSTERS/Galactic.topo 42514 1
# source /home/eitan/SW/SVN/osm/branches/main2_0/osm/test/osmLidAssignment.sim.tcl
# assignPortLid node:1:SWL2/spine1/U3 0 99
#  checkLidValues $fabric 1
# set swi [IBMSNode_getSwitchInfo sim$node]
# set n simnode:1:SWL2/spine1/U3
# set x [IBMSNode_getPKeyTblB $n 0 0]
#

