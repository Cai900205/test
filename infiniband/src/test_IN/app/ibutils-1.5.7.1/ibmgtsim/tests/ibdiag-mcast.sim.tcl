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

puts "FLOW: have some multicats groups with some partial connectivity too"

# get a random order of all the fabric HCA endports:
# a list of {node port-num random}
proc getEndPortsByRandomOrder {fabric} {
   global IB_SW_NODE

   # get number of nodes:
   set nodesByName [IBFabric_NodeByName_get $fabric]

   set portNOrderLIst {}
   foreach nodeNameNId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameNId 1]

      if {[IBNode_type_get $node] != $IB_SW_NODE} {
         # only connected ports please:
         set numPorts [IBNode_numPorts_get $node]
         for {set pn 1} {$pn <= $numPorts} {incr pn} {
            set port [IBNode_getPort $node $pn]
            if {($port != "") && ([IBPort_p_remotePort_get $port] != "")} {
               lappend portNOrderLIst [list $port [rmRand]]
            }
         }
      }
   }

   set randPorts {}
   foreach portNRnd [lsort -index 1 -real $portNOrderLIst] {
      lappend randPorts [lrange $portNRnd 0 1]
   }
   return $randPorts
}

# get random list of switch nodes:
proc getRandomSwitchNodesList {fabric} {
   global IB_SW_NODE

   # get number of nodes:
   set nodesByName [IBFabric_NodeByName_get $fabric]

   set nodeNOrderList {}
   foreach nodeNameNId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameNId 1]

      # only switches please
      if {[IBNode_type_get $node] == $IB_SW_NODE} {
         lappend nodeNOrderList [list $node [rmRand]]
      }
   }

   set randNodes {}
   foreach nodeNRnd [lsort -index 1 -real $nodeNOrderList] {
      lappend randNodes [lindex $nodeNRnd 0]
   }
   return $randNodes
}

# send a single port join request
proc sendJoinForPort {mgid port} {
   puts "-I- Joining port $port"
   # allocate a new mc member record:
   set mcm [new_madMcMemberRec]

   # join the IPoIB broadcast gid:
   madMcMemberRec_mgid_set $mcm $mgid

   # we must provide our own port gid
   madMcMemberRec_port_gid_set $mcm \
      "0xfe80000000000000:[string range [IBPort_guid_get $port] 2 end]"

   # must require full membership:
   madMcMemberRec_scope_state_set $mcm 0x1

   # we need port number and sim node for the mad send:
   set portNum [IBPort_num_get $port]
   set node [IBPort_p_node_get $port]

   # we need the comp_mask to include the mgid, port gid and join state:
   set compMask "0x00000000000130c7"

   # send it assuming the SM_LID is always 1:
   madMcMemberRec_send_set $mcm sim$node $portNum 1 $compMask

   # deallocate
   delete_madMcMemberRec $mcm

   return 0
}

# scan the switches (randomly) for a MFT entry which is not zero
# delete the first entry foudn and return
proc removeMCastRouteEntry {fabric} {
   set nodes [getRandomSwitchNodesList $fabric]

   while {[llength $nodes]} {
      set node [lindex $nodes 0]

      set mftBlock [IBMSNode_getMFTBlock sim$node 0 0]
      if {[llength $mftBlock] == 32} {
         set idx [lsearch -regexp $mftBlock {0x0*[1-9a-fA-F]+0*}]
         if {$idx >= 0} {
            set done 1
            set newMftBlock [lreplace $mftBlock $idx $idx 0x0000]
            puts "-I- Replacing MFT block $node 0 0"
            puts "    from:$mftBlock"
            puts "    to:  $newMftBlock"
            IBMSNode_setMFTBlock sim$node 0 0 $newMftBlock
            return 0
         }
      }
      set nodes [lrange $nodes 1 end]
   }
   return 1
}

# setup post SM run changes:
proc postSmSettings {fabric} {
   global errorInfo
   if {[catch {
   puts "-I- Joining MGRPS and Disconnecting some MFT routes..."
   set endPorts [getEndPortsByRandomOrder $fabric]

   # now we need several mgrps:
   set mgids {
      0xff12401bffff0000:00000000ffffffff
      0xff12401bffff0000:0000000000000001
      0xff12401bffff0000:0000000000000002
   }

   # go one port at a time and join:
   set idx 0
   set nPorts 0
   foreach port $endPorts {
      set mgid [lindex $mgids $idx]
      incr idx
      incr nPorts
      if {$idx > 2} {set idx 0}

      set portName [IBPort_getName $port]
      if {[catch {sendJoinForPort $mgid $port} e]} {
         puts "-E- Fail to join $portName to $mgid : $e $errorInfo"
      } else {
         puts "-I- Port $portName joined $mgid"
      }
   }
   } e]} {
      puts "-E- $e"
      puts $errorInfo
   }
   set nDisconencted 0

   after 1000
if {[catch {
   # now go and delete some switch MC entries...
   for {set i 0} {$i < 3} {incr i} {
      # delete one entry
      if {![removeMCastRouteEntry $fabric]} {
         incr nDisconencted
      }
   }
} e]} {
puts $e
puts $errorInfo
}
   return "-I- Joined $nPorts Disconnected $nDisconencted"
}

# make sure ibdiagnet reported the bad links
proc verifyDiagRes {fabric logFile} {
   return "Could not figure out if OK yet"
}

set fabric [IBMgtSimulator getFabric]

