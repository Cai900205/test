#--
# Copyright (c) 2004-2010 Mellanox Technologies LTD. All rights reserved.
#
# This software is available to you under a choice of one of two
# licenses.  You may choose to be licensed under the terms of the GNU
# General Public License (GPL) Version 2, available from the file
# COPYING in the main directory of this source tree, or the
# OpenIB.org BSD license below:
#
#     Redistribution and use in source and binary forms, with or
#     without modification, are permitted provided that the following
#     conditions are met:
#
#      - Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      - Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials
#        provided with the distribution.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
# BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
# ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#--

puts "Randomally picking some ports and assigning random drop rate on"

proc setPortErrProfile {node portNum} {
   # pick a random drop rate in the range 0 - 1 . The higher the number the more chances for
   # drop.
   set dropRate [expr [rmRand]*0.6 + 0.2]

   # set the node drop rate
   puts "-I- Setting drop rate:$dropRate on node:$node port:$portNum"
   set portErrProf "-drop-rate-avg $dropRate -drop-rate-var 4"
   IBMSNode_setPhyPortErrProfile sim$node $portNum $portErrProf
}

proc setNodePortErrProfile {node} {
   # first deicde if the entire node is broken:
   set allPorts [expr [rmRand] > 0.9]

   if {$allPorts != 0} {
      for {set pn 1} {$pn <= [IBNode_numPorts_get $node]} {incr pn} {
         setPortErrProfile $node $pn
      }
   } else {
      # pick a random port number
      set portNum [expr int([rmRand]*[IBNode_numPorts_get $node])+1]
      setPortErrProfile $node $portNum
   }
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

# setup post SM run changes:
proc postSmSettings {fabric} {
   return "-I- Nothing to be done post SM"
}

# make sure ibdiagnet reported the bad links
proc verifyDiagRes {fabric logFile} {
   return "-I- Could not figure out if OK yet"
}

set fabric [IBMgtSimulator getFabric]

# get a random order of the nodes:
set randNodes [getNodesByRandomOreder $fabric]
set numNodes [llength $randNodes]

###########################################
set NumberOfBadPorts 4
###########################################

# now get the first NumberOfBadPorts Nodes for err profile ...
set numNodesUsed 0
set idx 0
while {($numNodesUsed < $numNodes / 10) && ($numNodesUsed < $NumberOfBadPorts) && ($idx < $numNodes)} {
   set node [lindex $randNodes $idx]
   # ignore the root node:
   if {[IBNode_name_get $node] != "H-1/U1"} {
      setNodePortErrProfile $node
      incr numNodesUsed
   }
   incr idx
}

