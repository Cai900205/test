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

puts "Running Simulation flow for Partitions test case"

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
# PORT_GROUP_PKEY_IDX(port) -> index of pkey (if set or -1)
proc setAllHcaPortsPKeyTable {fabric} {
   global PORT_PKEY_GROUP
   global GROUP_PKEY

   set G1 [list 0x8001]
   set G2 [list 0x8002]
   set G3 [list 0x8001 0x8002 0x8003]

   set GROUP_PKEY(1) 0x8001
   set GROUP_PKEY(2) 0x8002
   set GROUP_PKEY(3) "0x8002 0x8001"

   puts "-I- Group1 Pkeys:$G1"
   puts "-I- Group2 Pkeys:$G2"
   puts "-I- Group3 Pkeys:$G3"

   set hcaPorts [getAllActiveHCAPorts $fabric]

   foreach port $hcaPorts {
      set portNum [IBPort_num_get $port]
      # the H-1/P1 has a special treatment:
      set node [IBPort_p_node_get $port]
		set nodeName [IBNode_name_get $node]
		set group [expr int(rand()*3) + 1]
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

# Remove given group PKey from Switch port accross the given host port
proc removeGroupPKeyAccrosForHcaPort {fabric hostNodeName portNum groupNum} {
	global GROUP_PKEY
	set hostNode [IBFabric_getNode $fabric $hostNodeName]
	if {$hostNode == ""} {
		puts "-E- fail to find node $hostNodeName"
		return "ERR: fail to find node $hostNodeName"
	}
	set port [IBNode_getPort $hostNode $portNum]
	if {$port == ""} {
		puts "-E- fail to find node $hostNodeName port $portNum"
		return "ERR: fail to find node $hostNodeName port $portNum"
	}
	set remPort [IBPort_p_remotePort_get $port]
	if {$remPort == ""} {
		puts "-E- no remote port accross $hostNodeName port $portNum"
		return "ERR:no remote port accross $hostNodeName port $portNum"
	}

	set swPortNum [IBPort_num_get $remPort]
	set remNode [IBPort_p_node_get $remPort]
	set swName  [IBNode_name_get $remNode]

	set pkey [lindex $GROUP_PKEY($groupNum) 0]

	set ni [IBMSNode_getNodeInfo sim$remNode]
	set partcap [ib_node_info_t_partition_cap_get $ni]
	set done 0
	for {set blockNum 0 } {$blockNum < ($partcap+31)/32} {incr blockNum} {
		set block [IBMSNode_getPKeyTblBlock sim$remNode $swPortNum $blockNum]
		puts "-I- PKey get $remNode port:$swPortNum block:$blockNum is:$block"
		#updating the block
		for {set i 0 } {$i < 32} {incr i} {
			if {[lindex $block $i] == $pkey} {
				set block [lreplace $block $i $i 0]
				puts "-I- Removing $pkey from block:$blockNum idx:$i"
				set done 1
			}
		}
		IBMSNode_setPKeyTblBlock sim$remNode $swPortNum $blockNum $block
		set block [IBMSNode_getPKeyTblBlock sim$remNode $swPortNum $blockNum]
		puts "-I- PKey set $remNode port:$swPortNum block:$blockNum to:$block"
	}
	if {$done } {
		return "Removed Group:$groupNum PKey:$pkey from Switch:$swName port:$swPortNum accross:$hostNodeName"
	} else {
		return "ERR: fail to find pkey:$pkey on Switch:$swName port:$swPortNum accros:$hostNodeName"
	}
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
		puts $f " "
	}
	close $f
	return "Dumped pkeys into:pkeys.txt"
}

set fabric [IBMgtSimulator getFabric]
