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

puts "Running Simulation flow for QoS test case"

proc setSl2VlTableToPortAccross {fabric nodeName portNum SL2VL} {

	# reformat the SL2VL
#	set SL2VL {}
#	for {set i 0} {$i < 16} {incr i} {
#		set v [lindex $SL2VLList $i]
#		if {$i % 2} {
#			lappend SL2VL [format 0x%02x [expr ($p << 4) | $v]]
#		}
#		set p $v
#	}

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
		puts "-E- No remote port for node $nodeName port $portNum"
		return "ERR: No remote port for node $nodeName port $portNum"
	}

	set remPortNum [IBPort_num_get $remPort]
	set remNode [IBPort_p_node_get $remPort]
	set remNodeName [IBNode_name_get $remNode]

	set numPorts [IBNode_numPorts_get $remNode]

	for {set pn 0} {$pn < $numPorts} {incr pn} {
		set old [IBMSNode_getSL2VLTable sim$remNode $pn $remPortNum]
		puts "-I- SL2VL on node:$remNodeName from port:$pn to:$remPortNum was $old"
		if {[IBMSNode_setSL2VLTable sim$remNode $pn $remPortNum $SL2VL]} {
			puts "-E- fail to set SL2VL on node $remNodeName from port $pn to $remPortNum"
			return "ERR: fail to set SL2VL on node $remNodeName from port $pn to $remPortNum"
		}
	}

	return "Set SL2VL on node:$remNodeName to port:$remPortNum accross from node:$nodeName port:$portNum to $SL2VL"
}

proc setVlArbAccross {fabric nodeName portNum VLA} {
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
		puts "-E- No remote port for node $nodeName port $portNum"
		return "ERR: No remote port for node $nodeName port $portNum"
	}

	set remPortNum [IBPort_num_get $remPort]
	set remNode [IBPort_p_node_get $remPort]
	set remNodeName [IBNode_name_get $remNode]

	set old [IBMSNode_getVLArbLTable sim$remNode $remPortNum 1]
	puts "-I- Old LOW VLA on node:$remNodeName port:$remPortNum was:$old"
	if {[IBMSNode_setVLArbLTable sim$remNode $remPortNum 1 $VLA]} {
		return "ERR: failed to update LOW VLArb on node:$remNodeName port:$remPortNum"
	}

	set old [IBMSNode_getVLArbLTable sim$remNode $remPortNum 3]
	puts "-I- Old LOW VLA on node:$remNodeName port:$remPortNum was:$old"
	if {[IBMSNode_setVLArbLTable sim$remNode $remPortNum 3 $VLA]} {
		return "ERR: failed to update LOW VLArb on node:$remNodeName port:$remPortNum"
	}
	return "Set VLArb High and Low on $remNodeName port:$remPortNum accross from node:$nodeName port:$portNum to $VLA"
}

set fabric [IBMgtSimulator getFabric]
