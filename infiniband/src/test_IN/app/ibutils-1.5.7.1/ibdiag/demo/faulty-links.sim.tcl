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

puts "Asslow assigning drop rate on ports and running all to all traffic"

########################################################################
#
# Set node port bad link with specific drop %
proc setNodePortErrProfile {fabric nodeName portNum dropRate} {
	set node [IBFabric_getNode $fabric $nodeName]
	if {$node == ""} {
		puts "-E- fail to find node $nodeName"
		return "ERR: fail to find node $nodeName"
	}
	set port [IBNode_getPort $node $portNum]
	if {$port == ""} {
		puts "-E- fail to find node $nodeName port $portNum"
		return "ERR: fail to find node $nodeName port $portNum"
	}
	set remPort [IBPort_p_remotePort_get $port]
	if {$remPort == ""} {
		puts "-E- No remote port on node $nodeName port $portNum"
		return "ERR: No remote port on node $nodeName port $portNum"
	}

   # set the node drop rate
   puts "-I- Setting drop rate:$dropRate on node:$node port:$portNum"
   set portErrProf "-drop-rate-avg $dropRate -drop-rate-var 4"
   IBMSNode_setPhyPortErrProfile sim$node $portNum $portErrProf

	return "Set node:$nodeName port:$portNum packet drop rate to:$dropRate"
}

########################################################################
#
# Run all to all QP1 traffic
#

# obtain the list of lids of HCA ports in the fabric
# set the global LIDS to that list
proc getAddressiblePortLids {fabric} {
        global IB_SW_NODE
	global LIDS
	global HCA_PORT_BY_LID

   # go over all nodes
   foreach nodeNameNId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameNId 1]

      # switches has only one port - port 0
      if {[IBNode_type_get $node] != $IB_SW_NODE} {
         set pMin 1
         set pMax [IBNode_numPorts_get $node]
         for {set pn $pMin} {$pn <= $pMax} {incr pn} {
            set port [IBNode_getPort $node $pn]
            if {$port == ""} {continue}

            # if the port is not connected ignore it:
            if {[IBPort_p_remotePort_get $port] != ""} {
					set lid [IBPort_base_lid_get $port]
					set PORT_LID($lid) 1
					set HCA_PORT_BY_LID($lid) $port
            }
         }
      }
   }
	set LIDS [array names PORT_LID]
   return $LIDS
}

# randomaly select one of the numbers provided in the given sequence
proc getRandomNumOfSequence {seq} {
   set idx [expr int([rmRand]*[llength $seq])]
   return [lindex $seq $idx]
}

proc startAllToAllTraffic {fabric} {
	global LIDS HCA_PORT_BY_LID
	if {![info exists LIDS]} {
		 getAddressiblePortLids $fabric
	}

	set srcLid [getRandomNumOfSequence $LIDS]
	set dstLid [getRandomNumOfSequence $LIDS]

	set port $HCA_PORT_BY_LID($srcLid)
	set node [IBPort_p_node_get $port]
	set portNum [IBPort_num_get $port]

	# for now we will send a service record request from the source to dest
   set pam [new_madServiceRec]

   # send it to the SM_LID:
   madServiceRec_send_set $pam sim$node $portNum $dstLid 0
	puts "Sent service record req src:$srcLid dst:$dstLid"

   # deallocate
   delete_madServiceRec $pam

	after 1 "startAllToAllTraffic $fabric"
   return 0
}

set fabric [IBMgtSimulator getFabric]

