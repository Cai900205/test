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

puts "FLOW: set some unicast partial connectivity"

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

# scan the switches (randomly) for a LFT entry which is not zero
# delete one entry ...
proc removeUCastRouteEntry {fabric} {
   set nodes [getRandomSwitchNodesList $fabric]

   while {[llength $nodes]} {
      set node [lindex $nodes 0]
      set nodeName [IBNode_name_get $node]
      set lft [IBNode_LFT_get $node]

      # convert to LID Port list
      set lidPortList {}
      for {set lid 0 } {$lid < [llength $lft]} {incr lid} {
         set outPort [lindex $lft $lid]
         if {($outPort != 0xff) && ($outPort != 0) } {
            lappend lidPortList [list $lid $outPort]
         }
      }

      # select a random entry
      if {[llength $lidPortList]} {
         set badLidIdx [expr int([rmRand]*[llength $lidPortList])]
         set badLidNPort [lindex $lidPortList $badLidIdx]
         set badLid [lindex $badLidNPort 0]
         set wasPort [lindex $badLidNPort 1]
         puts "-I- Deleting LFT on $nodeName lid:$badLid (was $wasPort)"
         IBNode_setLFTPortForLid $node $badLid 0xff
         return 0
      }
      set nodes [lrange $nodes 1 end]
   }
   return 1
}

# setup post SM run changes:
proc postSmSettings {fabric} {
   global errorInfo
   set nDisconencted 0
   # now go and delete some switch MC entries...
   for {set i 0} {$i < 3} {incr i} {
      # delete one entry
      if {![removeUCastRouteEntry $fabric]} {
         incr nDisconencted
      }
   }
   return "-I- Disconnected $nDisconencted LFT Entries"
}

# make sure ibdiagnet reported the bad links
proc verifyDiagRes {fabric logFile} {
   return "Could not figure out if OK yet"
}

set fabric [IBMgtSimulator getFabric]

