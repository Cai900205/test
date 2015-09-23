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

puts "Running Simulation flow for IPoIB test case"

# Assign partitions randomly
# Group 1 : 0x8001
# Group 2 : 0x8002
# Group 3 : 0x8001 0x8002 0x8003
#
# NOTE: default partition set by the SM ...

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

# prepare the three PKey groups G1 G2 abd G3
# then set the active HCA ports PKey tables (based on node name % 3)
# Note that the H-1/P1 has to have a slightly different PKey table
# with 0xffff such that all nodes can query the SA:
# we track the assignments in the arrays:
# PORT_PKEY_GROUP(port) -> group
proc setAllHcaPortsPKeyTable {fabric} {
   global PORT_PKEY_GROUP
   global GROUP_PKEY

   set G1 [list 0x8001]
   set G2 [list 0x8002]
   set G3 [list 0x8001 0x8002 0x8003]

   set GROUP_PKEY(1) 0x8001
   set GROUP_PKEY(2) 0x8002
   set GROUP_PKEY(3) "0x8003 0x8002 0x8001"

   puts "-I- Group1 Pkeys:$G1"
   puts "-I- Group2 Pkeys:$G2"
   puts "-I- Group3 Pkeys:$G3"

   set hcaPorts [getAllActiveHCAPorts $fabric]

   foreach port $hcaPorts {
      set portNum [IBPort_num_get $port]
      set node [IBPort_p_node_get $port]
		set nodeName [IBNode_name_get $node]

		# the H-1/P1 has a special treatment: we want it to have access to
		# all the MCGs
		if {$nodeName == "H-1/U1"} {
			set group 3
		} else {
			set group [expr int(rand()*3) + 1]
		}
		set PORT_PKEY_GROUP($port) $group
		switch $group {
			1 {set pkeys $G1}
			2 {set pkeys $G2}
			3 {set pkeys $G3}
			default {
				puts "-E- How come we got group $group ?"
			}
		}
   }
   # all HCA active ports
   return "Set PKeys on [array size PORT_PKEY_GROUP] ports"
}

# Dump out the HCA ports and their groups:
proc dumpHcaPKeyGroupFile {simDir} {
   global PORT_PKEY_GROUP
   global GROUP_PKEY

   set fn [file join $simDir "port_pkey_groups.txt"]
   set f [open $fn w]
	puts $f [format "\#%-6s %-4s %-5s %-18s %s" HOST PORT GROUP GUID PKEYS]
   foreach port [array names PORT_PKEY_GROUP] {
      set node [IBPort_p_node_get $port]
      set sys  [IBNode_p_system_get $node]
      set num  [IBPort_num_get $port]
      set name [IBSystem_name_get $sys]
      set guid [IBPort_guid_get $port]
      set grp  $PORT_PKEY_GROUP($port)
      set pkeys $GROUP_PKEY($grp)
		set idx  1
      puts $f [format " %-6s %4d %5d %s %s" $name $num $grp $guid $pkeys]
   }
   close $f
   return "Dumpped Group info into:$fn"
}

# dump out the current set of pkey tables:
proc dumpPKeyTables {fabric} {
	set f [open "pkeys.txt" w]
   set hcaPorts [getAllActiveHCAPorts $fabric]
   foreach port $hcaPorts {
      set portNum [IBPort_num_get $port]
      set node [IBPort_p_node_get $port]
		set name [IBPort_getName $port]
      set ni [IBMSNode_getNodeInfo sim$node]
      set partcap [ib_node_info_t_partition_cap_get $ni]
		puts $f "PORT: $name  PartCap:$partcap"
      for {set blockNum 0 } {$blockNum < $partcap/32} {incr blockNum} {
         set block [IBMSNode_getPKeyTblBlock sim$node $portNum $blockNum]
         puts $f "BLOCK:$blockNum pkeys:$block"
		}
	}
	close $f
	return "Dumped pkeys into:pkeys.txt"
}

# set the given port width-supported to 1X
proc setPortTo1X {fabric nodeName portNum} {
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
	if {$remPort != ""} {
		set remNode [IBPort_p_node_get $remPort]
		set remPortNum [IBPort_num_get $remPort]
		set pi [IBMSNode_getPortInfo sim$remNode $remPortNum]
		ib_port_info_t_link_width_supported_set $pi 1
		ib_port_info_t_link_width_active_set $pi 1
		ib_port_info_t_link_width_enabled_set $pi 1
	}

	set pi [IBMSNode_getPortInfo sim$node $portNum]
	set old [ib_port_info_t_link_width_supported_get $pi]
	ib_port_info_t_link_width_supported_set $pi 1
	ib_port_info_t_link_width_active_set $pi 1
	ib_port_info_t_link_width_enabled_set $pi 1
	return "Set node:$nodeName port:$portNum LinkWidthSupported to 1x (0x1) was $old"
}

# set the given port speed-supported to SDR
proc setPortToSDR {fabric nodeName portNum} {
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
	if {$remPort != ""} {
		set remNode [IBPort_p_node_get $remPort]
		set remPortNum [IBPort_num_get $remPort]
		set pi [IBMSNode_getPortInfo sim$remNode $remPortNum]
		ib_port_info_t_state_info1_set $pi 0x12
		ib_port_info_t_link_speed_set $pi 0x11
	}
	set pi [IBMSNode_getPortInfo sim$node $portNum]
	# note LSB nibble is port state ... we set it to INIT
	ib_port_info_t_state_info1_set $pi 0x12
	ib_port_info_t_link_speed_set $pi 0x11
	return "Set node:$nodeName port:$portNum LinkSpeedSupported to SDR"
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

proc joinPortsByPartition {fabric} {
   global PORT_PKEY_GROUP
	set numJoins 0
	set hcaPorts [getAllActiveHCAPorts $fabric]
	foreach port $hcaPorts {
		set group $PORT_PKEY_GROUP($port)
		switch $group {
			1 {set pkeys 0x8001}
			2 {set pkeys 0x8002}
			3 {set pkeys {0x8001 0x8002 0x8003}}
			default {
				puts "-E- How come we got group $group ?"
				return "ERR: How come we got group $group ?"
			}
		}

		foreach pkey $pkeys {
			set mgid [format "0xff12401b%04x0000:00000000ffffffff" $pkey]
			joinPortToMGID $port $mgid
			incr numJoins
		}
	}
	return "Performed $numJoins joins"
}

set fabric [IBMgtSimulator getFabric]
