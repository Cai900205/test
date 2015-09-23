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

puts "FLOW: duplicate some port guid"

# duplicate the port guid from the source to the dest
proc dupPortGuid {fromNodeNPort toNodeNPort} {

   # IBDM has a limitation of not holding "end ports"
   # instead only physical ports are available.
   # So in case of a switch port (port num 0) we need to handle all physical ports
   # instead...

   # do we have a switch as the guid to duplicate?
   set node [lindex $fromNodeNPort 0]
   if {[lindex $fromNodeNPort 1] == 0} {
      # just use the port 1 instead
      set port [IBNode_getPort $node  1]
      set fromPortName "[IBNode_name_get $node]/P0"
   } else {
      set port [IBNode_getPort $node [lindex $fromNodeNPort 1]]
      set fromPortName [IBPort_getName $port]
   }
   set newGuid [IBPort_guid_get $port]

   # do we have a switch port 0 as a target?
   set node [lindex $toNodeNPort 0]
   if {[lindex $toNodeNPort 1] == 0} {
      set numPorts [IBNode_numPorts_get $node]
      for {set pn 1} {$pn <= $numPorts} {incr pn} {
         set port [IBNode_getPort $node $pn]
         if {$port != ""} {
            lappend targetPorts $port
         }
      }
   } else {
      set port [IBNode_getPort $node [lindex $toNodeNPort 1]]
      set targetPorts $port
   }

   # do the copy
   foreach port $targetPorts {
      puts "-I- Overriding port:[IBPort_getName $port] guid to $newGuid (dup of $fromPortName)"
      IBPort_guid_set $port $newGuid
   }
}

# get a random order of all the fabric endports:
# a list of {node port-num random}
proc getEndPortsByRandomOreder {fabric} {
   global IB_SW_NODE

   # get number of nodes:
   set nodesByName [IBFabric_NodeByName_get $fabric]

   set nodePortNOrderList {}
   foreach nodeNameNId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameNId 1]
      set nodeName [lindex $nodeNameNId 0]
      # each node might be a switch (then take port 0)
      if {[IBNode_type_get $node] != $IB_SW_NODE} {
         # only connected ports please:
         set numPorts [IBNode_numPorts_get $node]
         for {set pn 1} {$pn <= $numPorts} {incr pn} {
            # not the local node please on port 1
            if {$nodeName == "H-1/U1" && $pn == 1} { continue }
            set port [IBNode_getPort $node $pn]
            if {($port != "") && ([IBPort_p_remotePort_get $port] != "")} {
               lappend nodePortNOrderList [list $node $pn [rmRand]]
            }
         }
      }
   }

   set randNodes {}
   foreach nodePortNRnd [lsort -index 2 -real $nodePortNOrderList] {
      lappend randNodes [lrange $nodePortNRnd 0 1]
   }
   return $randNodes
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

# every 2 seconds cause switch change bit to be set
proc causeHeavySweeps {fabric} {
   setOneSwitchChangeBit $fabric
   after 2000 "causeHeavySweeps $fabric"
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

# get a random order of the end ports:
set randEndPorts [getEndPortsByRandomOreder $fabric]
set smNode [IBFabric_getNode $fabric "H-1/U1"]
set fromNodeNPort [list $smNode 1]
set toNodeNPort [lindex $randEndPorts 0]

dupPortGuid $fromNodeNPort $toNodeNPort

causeHeavySweeps $fabric

