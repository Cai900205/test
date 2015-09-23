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

puts "Randomally Set Duplicated Port Guid (do not touch the SM port)"

proc duplicatePortGuid {fromPort toPort } {
   IBPort_guid_set $toPort [IBPort_guid_get $fromPort]
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
