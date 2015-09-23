
puts "Randomally picking 10 ports and assigning random drop rate on"
########################################################################
#
#  Inject Random Multicast Requests
#

########################################################################
#
# Set Random Bad Links
proc setNodePortErrProfile {node} {
   # pick a random port number - but make sure you picked a connected one
   set done 0
   set cnt 0
   while {!$done && ($cnt < 100) } {
      set portNum [expr int([rmRand]*[IBNode_numPorts_get $node])+1]
      set port [IBNode_getPort $node $portNum]
      if {$port != ""} {
         if {[IBPort_p_remotePort_get $port] != ""} {
             set done 1
         }
      }
      incr cnt
   }
   if {!$done} {
      puts "-E- Fail to get connected port for node $node"
      return
   }

   # pick a random drop rate in the range 0 - 1 . The higher the number
   # the more chances for drop.
   set dropRate [rmRand]

   # set the node drop rate
   puts "-I- Setting drop rate:$dropRate on node:$node port:$portNum"
   set portErrProf "-drop-rate-avg $dropRate -drop-rate-var 4"
   IBMSNode_setPhyPortErrProfile sim$node $portNum $portErrProf
}

# get a random order of all the fabric nodes:
proc getNodesByRandomOreder {fabric} {
   # get number of nodes:
   set nodesByName [IBFabric_NodeByName_get $fabric]

   set nodeNameNOrderList {}
   foreach nodeNameNId [IBFabric_NodeByName_get $fabric] {
      lappend nodeNameNOrderList [list [lindex $nodeNameNId 1] [rmRand]]
   }

   set randNodes {}
   foreach nodeNameNOrder [lsort -index 1 -real $nodeNameNOrderList] {
      lappend randNodes [lindex $nodeNameNOrder 0]
   }
   return $randNodes
}

set fabric [IBMgtSimulator getFabric]

# get a random order of the nodes:
set randNodes [getNodesByRandomOreder $fabric]
set numNodes [llength $randNodes]

# now get the first N nodes for err profile ...
set numNodesUsed 0
set idx 0
while {($numNodesUsed < $numNodes / 10) && ($numNodesUsed < 12) && ($idx < $numNodes)} {
   set node [lindex $randNodes $idx]
   # ignore the root node:
   if {[IBNode_name_get $node] != "H-1/U1"} {
      if {[IBNode_type_get $node] != $IB_SW_NODE} {
         setNodePortErrProfile $node
         incr numNodesUsed
      }
   }
   incr idx
}
