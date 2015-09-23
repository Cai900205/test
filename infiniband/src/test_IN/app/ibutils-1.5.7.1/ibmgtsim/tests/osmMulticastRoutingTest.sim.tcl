puts "Running Simulation flow for Muticast Routing test"

puts "Randomally Joining all the Fabric Ports with random delays"

# send a single port join request
proc sendJoinForPort {port smLid} {
   puts "-I- Joining port $port"
   # allocate a new mc member record:
   set mcm [new_madMcMemberRec]

   # join the IPoIB broadcast gid:
   madMcMemberRec_mgid_set $mcm 0xff12401bffff0000:00000000ffffffff

   # we must provide our own port gid
   madMcMemberRec_port_gid_set $mcm \
      "0xfe80000000000000:[string range [IBPort_guid_get $port] 2 end]"

   # must require full membership:
   madMcMemberRec_scope_state_set $mcm 0x1

   # we need port number and sim node for the mad send:
   set portNum [IBPort_num_get $port]
   set node [IBPort_p_node_get $port]

   # we need the comp_mask to include the mgid, port gid and join state:
   set compMask [format "0x%X" [expr (1<<16) | 3]]

   # send it
   madMcMemberRec_send_set $mcm sim$node $portNum $smLid $compMask

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
proc randomJoinAllHCAPorts {fabric maxDelay_ms} {
   # get all HCA ports:
   set hcaPorts [getAllActiveHCAPorts $fabric]

   # set a random order:
   set orederedPorts {}
   foreach port $hcaPorts {
      lappend orederedPorts [list $port [rmRand]]
   }

   # sort:
   set orederedPorts [lsort -index 1 -real $orederedPorts]
   set numHcasJoined 0

   # get the SM LID
   # HACK: Assumes the SM node is H-1/U1 Port 1
   set smNode [IBFabric_getNode $fabric "H-1/U1"]
   set smPort [IBNode_getPort $smNode 1]
   set smPortInfo [IBMSNode_getPortInfo sim$smNode 1]
   set smLid [ib_port_info_t_base_lid_get $smPortInfo]

   # Now do the joins - waiting random time between them:
   foreach portNOrder $orederedPorts {
      set port [lindex $portNOrder 0]

      if {![sendJoinForPort $port $smLid]} {
         incr numHcasJoined
      }

      after [expr int([rmRand]*$maxDelay_ms)]
   }
   return $numHcasJoined
}
