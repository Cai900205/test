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

puts "Running Simulation flow for SM LINK SETUP test case"

proc setPortSpeed {fabric nodeName portNum speed} {
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

	switch $speed {
		2.5 {set code 1}
		5   {set code 2}
		10  {set code 4}
		default {
			return "ERR: unknown speed:$speed"
		}
	}
	set pi [IBMSNode_getPortInfo sim$node $portNum]
	set old [ib_port_info_t_link_speed_get $pi]
	set new [format %x [expr ($code << 4) | ($old & 0xf)]]
	ib_port_info_t_link_width_active_set $pi $new
	return "Set node:$nodeName port:$portNum LinkSpeedActive to ${speed}Gpbs was $old now $new"
}

proc setPortWidth {fabric nodeName portNum width} {
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

	switch $width {
		1x {set code 1}
		4x {set code 2}
		8x {set code 4}
		12x {set code 8}
		default {
			return "ERR: unknown width:$width"
		}
	}
	set pi [IBMSNode_getPortInfo sim$node $portNum]
	set old [ib_port_info_t_link_width_active_get $pi]
	ib_port_info_t_link_width_active_set $pi $code
	return "Set node:$nodeName port:$portNum LinkWidthActive to $width was $old now $code"
}

proc setPortOpVLs {fabric nodeName portNum vls} {
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

	set pi [IBMSNode_getPortInfo sim$node $portNum]
	set old [ib_port_info_t_vl_enforce_get $pi]
	set new [format %x [expr ($vls << 4) | ($old & 0xf)]]
	ib_port_info_t_vl_enforce_set $pi $new
	return "Set node:$nodeName port:$portNum OpVLs to $vls opvls_enforcement was $old now $new"
}

proc setPortMTU {fabric nodeName portNum mtu} {
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

	switch $mtu {
		256 {set mtuCode 1}
		512 {set mtuCode 2}
		1024 {set mtuCode 3}
		2048 {set mtuCode 4}
		4096 {set mtuCode 5}
		default {
			return "ERR: unknown MTU:$mtu"
		}
	}

	set pi [IBMSNode_getPortInfo sim$node $portNum]
	set old [ib_port_info_t_mtu_smsl_get $pi]
	set new [format %x [expr ($mtuCode << 4) | ($old & 0xf)]]
	ib_port_info_t_mtu_smsl_set $pi $new
	return "Set node:$nodeName port:$portNum NeighborMTU to $mtu mtu_smsl was $old now $new"
}


set fabric [IBMgtSimulator getFabric]
