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

puts "FLOW: have some bad lid assignments - either 0 or duplicated ..."

# get a random order of all the fabric nodes:
proc getNodesByRandomOreder {fabric} {
   # get number of nodes:
   set nodesByName [IBFabric_NodeByName_get $fabric]

   set nodeNOrderList {}
   foreach nodeNameNId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameNId 1]
      lappend nodeNOrderList [list $node [rmRand]]
   }

   set randNodes {}
   foreach nodeNRnd [lsort -index 1 -real $nodeNOrderList] {
      lappend randNodes [lindex $nodeNRnd 0]
   }
   return $randNodes
}

# randomly select a bad lid assignment flow and do it:
proc injectLidErrorOnNode {node} {
   # first decide which error to inject:
   set errTypes {zeroLid dupLid}
   set errType [lindex $errTypes [expr int([rmRand]*[llength $errTypes])]]

   switch $errType {
      zeroLid {
         set newLid 0
      }
      dupLid {
         set newLid [expr int([rmRand]*[IBFabric_maxLid_get [IBNode_p_fabric_get $node]])]
      }
   }

   set numPorts [IBNode_numPorts_get $node]
   for {set pn 1} {$pn <= $numPorts} {incr pn} {
      set port [IBNode_getPort $node $pn]
      if {($port != "") && ([IBPort_p_remotePort_get $port] != "")} {
         set tmpNewLid [format 0x%04x $newLid]
         puts "-I- Setting port:[IBPort_getName $port] lid to $tmpNewLid ($errType)"
         set pi [IBMSNode_getPortInfo sim$node $pn]
         ib_port_info_t_base_lid_set $pi $newLid
      }
   }
   return 0
}

# setup post SM run changes:
proc postSmSettings {fabric} {
   global errorInfo
   set nodes [getNodesByRandomOreder $fabric]
   set maxErrors [expr [llength $nodes] / 10]
   set numErrosSet 0
   if {[catch {
      for {set i 0} {($i < 10) && ($i < $maxErrors)} {incr i} {
         if {![injectLidErrorOnNode [lindex $nodes $i]]} {
            incr numErrosSet
         }
      }
   } e]} {
      puts "-E- $e"
      puts $errorInfo
   }
   return "-I- Set $numErrosSet LID errors"
}

# make sure ibdiagnet reported the bad links
proc verifyDiagRes {fabric logFile} {
   return "Could not figure out if OK yet"
}

set fabric [IBMgtSimulator getFabric]

