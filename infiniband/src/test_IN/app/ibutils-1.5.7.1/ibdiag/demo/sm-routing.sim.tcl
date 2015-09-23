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

# A flow to support the LFT MFT diagnostic demo

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

# set the change bit on the switch accross from the SM
proc setSwitchChangeBit {fabric nodeName} {
	global IB_SW_NODE

   set node [IBFabric_getNode $fabric $nodeName]
	if {$node == ""} {
		return "ERR: Fail to find node:$nodeName"
	}

	if {[IBNode_type_get $node] != $IB_SW_NODE} {
		return "ERR: Given node:$nodeName is not a switch"
	}

	set swi [IBMSNode_getSwitchInfo sim$node]
	set lifeState [ib_switch_info_t_life_state_get $swi]
	set lifeState [expr ($lifeState & 0xf8) | 4 ]
	ib_switch_info_t_life_state_set $swi $lifeState

	# OpenSM will not set the switch unless some port changed state...
	# so pick some other port and play with it state
	set numPorts [IBNode_numPorts_get $node]
	for {set pn 1} {$pn <= $numPorts} {incr pn} {
		set pi [IBMSNode_getPortInfo sim$node $pn]
		set old [ib_port_info_t_state_info1_get $pi]
		set new [expr ($old & 0xf0) | 0x2]
		ib_port_info_t_state_info1_set $pi $new
	}
	puts "-I- Set change bit on switch:$node"

	if {0} {
		# send a trap...
		set ni [IBMSNode_getNodeInfo sim$node]
		set trap [madNotice128]
		madNotice128_issuer_lid_set    $trap [ib_port_info_t_base_lid_get $pi]
		madNotice128_generic_type_set  $trap 0x8001
		madNotice128_issuer_gid_set    $trap [ib_node_info_t_port_guid_get $ni]
		madNotice128_sw_lid_set        $trap [ib_port_info_t_base_lid_get $pi]
		madNotice128_trap_num_set      $trap 128
		# HACK assume the SM LID is 1
		madNotice128_send_trap         $trap sim$node 1 1
	}
	return "-I- Set change bit on switch:$node"
}

# register all ports to MGID
proc joinPortToMGID {port mgid} {
   puts "-I- Joining port [IBPort_getName $port] to MGID:$mgid"

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
   set compMask [format "0x%X" [expr (1<<16) | 3]]

   # send it assuming the SM_LID is always 1:
   madMcMemberRec_send_set $mcm sim$node $portNum 1 $compMask

   # deallocate
   delete_madMcMemberRec $mcm

   return 0
}

# join each ports to all the given pkes IPoIB groups
proc joinPortsByPartition {fabric pkeys} {
	set numJoins 0
	set hcaPorts [getAllActiveHCAPorts $fabric]
	foreach port $hcaPorts {
		foreach pkey $pkeys {
			set mgid [format "0xff12401b%04x0000:00000000ffffffff" $pkey]
			joinPortToMGID $port $mgid
			incr numJoins
		}
	}
	return "Performed $numJoins joins"
}

proc causeDeadEndOnPath {fabric srcName srcPortNum dstName dstPortNum} {
	# find the LID of DST PORT:
	set dstNode [IBFabric_getNode $fabric $dstName]
	if {$dstNode == ""} {
		return "ERR: could not find the dst node:$dstName"
	}

	set dstPort [IBNode_getPort $dstNode $dstPortNum]
	if {$dstPort == ""} {
		return "ERR: could not find the dst node:$dstName port:$dstPortNum"
	}
	set dLid [IBPort_base_lid_get $dstPort]

	set srcNode [IBFabric_getNode $fabric $srcName]
	if {$srcNode == ""} {
		return "ERR: could not find the src node:$srcName"
	}

	set srcPort [IBNode_getPort $srcNode $srcPortNum]
	if {$srcPort == ""} {
		return "ERR: could not find the src node:$srcName port:$srcPortNum"
	}

	set sLid [IBPort_base_lid_get $srcPort]

	if {[ibdmTraceRouteByLFT $fabric $sLid $dLid hops nodeList]} {
		return "ERR: failed to obtain path"
	}

	set swIdx [expr int(rand()*$hops)]
	set swNode [lindex $nodeList $swIdx]
	set swName [IBNode_name_get $swNode]
	IBNode_setLFTPortForLid $swNode $dLid 255
	return "Dead-End path from $srcName/$srcPortNum to $dstName/$dstPortNum at switch:$swName"
}

proc causeLoopOnPath {fabric srcName srcPortNum dstName dstPortNum} {
	# find the LID of DST PORT:
	set dstNode [IBFabric_getNode $fabric $dstName]
	if {$dstNode == ""} {
		return "ERR: could not find the dst node:$dstName"
	}

	set dstPort [IBNode_getPort $dstNode $dstPortNum]
	if {$dstPort == ""} {
		return "ERR: could not find the dst node:$dstName port:$dstPortNum"
	}
	set dLid [IBPort_base_lid_get $dstPort]

	set srcNode [IBFabric_getNode $fabric $srcName]
	if {$srcNode == ""} {
		return "ERR: could not find the src node:$srcName"
	}

	set srcPort [IBNode_getPort $srcNode $srcPortNum]
	if {$srcPort == ""} {
		return "ERR: could not find the src node:$srcName port:$srcPortNum"
	}

	set sLid [IBPort_base_lid_get $srcPort]

	if {[ibdmTraceRouteByLFT $fabric $sLid $dLid hops nodeList]} {
		return "ERR: failed to obtain path"
	}
	if {$hops < 2} {
		return "ERR: need > 1 switches for a loop"
	}
	set swIdx [expr int(rand()*($hops-1))]
	set sw1Node [lindex $nodeList $swIdx]
	set sw2Node [lindex $nodeList [expr $swIdx+1]]
	set sw1OutPortNum [IBNode_getLFTPortForLid $sw1Node $dLid]
	set sw1OutPort    [IBNode_getPort $sw1Node $sw1OutPortNum]
	set sw2InPort     [IBPort_p_remotePort_get $sw1OutPort]

	set swName [IBNode_name_get $sw2Node]
	IBNode_setLFTPortForLid $sw2Node $dLid [IBPort_num_get $sw2InPort]
	return "Loop path from $srcName/$srcPortNum to $dstName/$dstPortNum at switch:$swName"
}

proc breakMCG {fabric mlid} {
        global IB_SW_NODE

	# simply find a switch with MFT set for the mlid and remove one bit ...

	set blockIdx [expr ($mlid - 0xc000)/32]
	set mlidIdx  [expr ($mlid - 0xc000)%32]
	set portIdx 0
	puts "Using blockIdx:$blockIdx mlidIdx:$mlidIdx"

   # go over all nodes:
   foreach nodeNameId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameId 1]
		set swName [IBNode_name_get $node]

		# HACK: skip level 1 switches
		if {[regexp {^SL1} $swName]} {continue}

      # we do care about switches only
      if {[IBNode_type_get $node] == $IB_SW_NODE} {
			for {set portIdx 0} {$portIdx < 2} {incr portIdx} {
				set mft [IBMSNode_getMFTBlock sim$node $blockIdx $portIdx]
				puts "-I- Switch:$swName block:$blockIdx ports:$portIdx MFT:$mft"
				set portMask [lindex $mft $mlidIdx]
				if {$portMask != 0} {
					for {set bit 0} {$bit < 16} {incr bit} {
						set mask [expr 1 << $bit]
						if {$portMask & $mask} {
							set portMask [format 0x%04x [expr $portMask & ~$mask]]
							set mft [lreplace $mft $mlidIdx $mlidIdx $portMask]
							IBMSNode_setMFTBlock sim$node $blockIdx $portIdx $mft
							puts "-I- Switch:$swName block:$blockIdx ports:$portIdx NEW MFT:$mft"
							return "Broke MLID:$mlid routing at port:$bit of switch:$swName"
						}
					}
				}
			}
      }
   }
	return "ERR: could not find any switch to break the MLID:$mlid routing"
}

set fabric [IBMgtSimulator getFabric]

