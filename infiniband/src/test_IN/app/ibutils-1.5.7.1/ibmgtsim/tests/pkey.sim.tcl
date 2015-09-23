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

puts "Running Simulation flow for PKey test"

# Randomally assign PKey tables of 3 types:
# Group 1 : .. 0x81
# Group 2 : ........ 0x82 ...
# Group 3 : ... 0x82 ... 0x81 ...
#
# So osmtest run from nodes of group1 should only see group1
# Group2 should only see group 2 and group 3 should see all.

# to prevent the case where randomized pkeys match (on ports
# from different group we only randomize limited membership
# pkeys (while the group pkeys are full)

# In order to prevent cases where limited Pkey matches Full Pkey
# we further split the space:
# Partials are: 0x1000 - 0x7fff
# Full are    : 0x8000 - 0x8fff
proc getPartialMemberPkey {} {
   return [format 0x%04x [expr int([rmRand] * 0x6fff) + 0x1000]]
}

proc getFullMemberPkey {} {
   return [format 0x%04x [expr int([rmRand] * 0xffe) + 0x8001]]
}

# produce a random PKey containing the given pkeys
proc getPartialMemberPkeysWithGivenPkey {numPkeys pkeys} {

   # randomally select indexes for the given pkeys:
   # fill in the result list of pkeys with random ones,
   # also select an index for each of the given pkeys and
   # replace the random pkey with the given one


   # flat pkey list (no blocks)
   set res {}

   # init both lists
   for {set i 0} {$i < $numPkeys - [llength $pkeys] } {incr i} {
      lappend res [getPartialMemberPkey]
   }

   # select where to insert the given pkeys
   for {set i 0} {$i < [llength $pkeys]} {incr i} {
      set pkeyIdx [expr int([rmRand] * $numPkeys)]
      set res [linsert $res $pkeyIdx [lindex $pkeys $i]]
   }

   # making sure:
   for {set i 0} {$i < [llength $pkeys]} {incr i} {
      set pk [lindex $pkeys $i]
      set idx [lsearch $res $pk]
      if {($idx < 0) || ($idx > $numPkeys)} {
         puts "-E- fail to find $pk in $res idx=$idx i=$i num=$numPkeys"
         exit 1
      }
   }
   puts "-I- got random pkeys:$res"
   # making sure:
   for {set i 0} {$i < [llength $pkeys]} {incr i} {
      set pk [lindex $pkeys $i]
      set idx [lsearch $res $pk]
      if {$idx < 0 || $idx > $numPkeys} {
         puts "-E- fail to find $pk in $res"
         exit 1
      }
   }
   puts "-I- got random pkeys:$res"
   return $res
}

# get a flat list of pkeys and partition into blocks:
proc getPkeyBlocks {pkeys} {
   set blocks {}
   set extra {0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}

   set nKeys [llength $pkeys]
   while {$nKeys} {
      if {$nKeys < 32} {
         append pkeys " [lrange $extra 0 [expr 32 - $nKeys - 1]]"
      }
      lappend blocks [lrange $pkeys 0 31]
      set pkeys [lrange $pkeys 32 end]
      set nKeys [llength $pkeys]
   }
   return $blocks
}

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
# then randomly set the active HCA ports PKey tables
# Note that the H-1/P1 has to have a slightly different PKey table
# with 0xffff such that all nodes can query the SA:
# we track the assignments in the arrays:
# PORT_PKEY_GROUP(port) -> group
# PORT_GROUP_PKEY_IDX(port) -> index of pkey (if set or -1)
proc setAllHcaPortsPKeyTable {fabric} {
   global PORT_PKEY_GROUP PORT_GROUP_PKEY_IDX
   global GROUP_PKEY

   # while setting pkeys, make sure that they are not equal
   set pkey1 [getFullMemberPkey]
   set pkey2 $pkey1
   while {$pkey2 == $pkey1} {
      set pkey2 [getFullMemberPkey]
   }
   set pkey3 [getPartialMemberPkey]

   set G1 [list $pkey1 $pkey3]
   set G2 [list $pkey2 $pkey3]
   set G3 [list $pkey1 $pkey2 $pkey3]

   set GROUP_PKEY(1) $pkey1
   set GROUP_PKEY(2) $pkey2
   set GROUP_PKEY(3) $pkey3

   puts "-I- Group1 Pkeys:$G1"
   puts "-I- Group2 Pkeys:$G2"
   puts "-I- Group3 Pkeys:$G3"

   set hcaPorts [getAllActiveHCAPorts $fabric]

   foreach port $hcaPorts {
      set portNum [IBPort_num_get $port]
      # the H-1/P1 has a special treatment:
      set node [IBPort_p_node_get $port]
      if {[IBNode_name_get $node] == "H-1/U1"} {
         set group [list 0xffff $pkey1 $pkey2]
         set PORT_PKEY_GROUP($port) 3
			set pkey $pkey3
      } else {
         # randomly select a group for this port:
         set r [expr int([rmRand] * 3) + 1]
         set PORT_PKEY_GROUP($port) $r
         switch $r {
            1 {set group $G1; set pkey $pkey1}
            2 {set group $G2; set pkey $pkey2}
            3 {set group $G3; set pkey $pkey3}
            default {
               puts "-E- How come we got $r ?"
            }
         }
      }

      # we need to decide how we setup the pkeys on that port:
      set nPkeys [expr 3 + int([rmRand]*48)]

      # we also need to decide if we actually setup the partition ourselves
      # or leave it for the SM
      set r [rmRand]
      if {$r < 0.333} {
         puts "-I- Set incorrect group for $node port:$portNum"
         if {$group == $G1} {
            set group $G2
         } elseif {$group == $G2} {
            set group $G3
         } elseif {$group == $G3} {
            set group $G1
         }
      } elseif {$r < 0.66} {
         puts "-I- Set common group for $node port:$portNum"
         set group $G3
      } else {
         # use the correct group
         puts "-I- Set correct group for $node port:$portNum"
      }

      set pkeys [getPartialMemberPkeysWithGivenPkey $nPkeys $group]
      set blocks [getPkeyBlocks $pkeys]

		# we track the pkey index of the assigned pkey (or -1)
		set PORT_GROUP_PKEY_IDX($port) [lsearch $pkeys $pkey]

      set blockNum 0
      foreach block $blocks {
         # now set the PKey tables
         puts "-I- PKey set $node port:$portNum block:$blockNum to:$block"
         IBMSNode_setPKeyTblBlock sim$node $portNum $blockNum $block
         incr blockNum
      }
   }
   # all HCA active ports
   return "Set PKeys on [array size PORT_PKEY_GROUP] ports"
}


# Remove 0x7fff or 0xffff from the PKey table for all HCA ports - except the SM
proc removeDefaultPKeyFromTableForHcaPorts {fabric} {
   set hcaPorts [getAllActiveHCAPorts $fabric]
   foreach port $hcaPorts {
      set portNum [IBPort_num_get $port]
      # the H-1/P1 has a special treatment:
      set node [IBPort_p_node_get $port]
      if {[IBNode_name_get $node] == "H-1/U1"} {
         #Do nothing - do not remove the default PKey
      } else {
         set ni [IBMSNode_getNodeInfo sim$node]
         set partcap [ib_node_info_t_partition_cap_get $ni]
         for {set blockNum 0 } {$blockNum < $partcap/32} {incr blockNum} {
            set block [IBMSNode_getPKeyTblBlock sim$node $portNum $blockNum]
            puts "-I- PKey get $node port:$portNum block:$blockNum to:$block"
            #updating the block
            for {set i 0 } {$i < 32} {incr i} {
               if {[lindex $block $i] == 0x7fff || \
                      [lindex $block $i] == 0xffff} {
                  set block [lreplace $block $i $i 0]
                  puts "-I- Removing 0x7fff or 0xffff from the PKeyTableBlock"
               }
            }
            IBMSNode_setPKeyTblBlock sim$node $portNum $blockNum $block
            puts "-I- Default PKey set for $node port:$portNum block:$blockNum to:$block"
         }
      }
   }
   # all HCA active ports
   return "Remove Default PKey from HCA ports"
}

# Verify correct PKey index is used
proc verifyCorrectPKeyIndexForAllHcaPorts {fabric} {
   global PORT_PKEY_GROUP PORT_GROUP_PKEY_IDX GROUP_PKEY
   set hcaPorts [getAllActiveHCAPorts $fabric]
	set anyErr 0


   foreach port $hcaPorts {
      set portNum [IBPort_num_get $port]
      set node [IBPort_p_node_get $port]
      set ni [IBMSNode_getNodeInfo sim$node]
      set partcap [ib_node_info_t_partition_cap_get $ni]
		set grp  $PORT_PKEY_GROUP($port)
		set pkey $GROUP_PKEY($grp)

		set pkey_idx $PORT_GROUP_PKEY_IDX($port)
		if {$pkey_idx == -1} {
			puts "-I- Ignoring non-definitive port [IBPort_getName $port]"
			continue
		}

		set blockIdx [expr $pkey_idx / 32]
		set idx [expr $pkey_idx % 32]

		if {$blockIdx >= $partcap/32} {
			puts "-E- [IBPort_getName $port] Required block $blockIdx is too high for partition cap $partcap"
			incr anyErr
		}

		set block [IBMSNode_getPKeyTblBlock sim$node $portNum $blockIdx]
		set bPkey [lindex $block $idx]
		if {$bPkey != $pkey} {
         puts "-E- [IBPort_getName $port] block:$blockIdx idx:$idx pkey:$bPkey does match required:$pkey "
			puts "    pkeys:$block"
         incr anyErr
      } else {
         puts "-I- [IBPort_getName $port] found pkey:$pkey at block:$blockIdx idx:$idx "
		}
   }
   # all HCA active ports
   return $anyErr
}

# Verify that 0x7fff or 0xffff is in the PKey table for all HCA ports
proc verifyDefaultPKeyForAllHcaPorts {fabric} {
   global PORT_PKEY_GROUP
   set hcaPorts [getAllActiveHCAPorts $fabric]
   foreach port $hcaPorts {
      set portNum [IBPort_num_get $port]
      set node [IBPort_p_node_get $port]
      set ni [IBMSNode_getNodeInfo sim$node]
      set partcap [ib_node_info_t_partition_cap_get $ni]
      set hasDefaultPKey 0
      for {set blockNum 0 } {$blockNum < $partcap/32} {incr blockNum} {
         set block [IBMSNode_getPKeyTblBlock sim$node $portNum $blockNum]
         puts "-I- [IBPort_getName $port] block:$blockNum pkeys:$block"
         #Verifying Default PKey in the block
         for {set i 0 } {$i < 32} {incr i} {
            if {[lindex $block $i] == 0x7fff || \
                   [lindex $block $i] == 0xffff } {
               set hasDefaultPKey 1
               break
            }
         }
         if {$hasDefaultPKey == 1} {
            break
         }
      }
      if {$hasDefaultPKey == 0} {
         puts "-E- Default PKey not found for $node port:$portNum"
         return 1
      }
   }
   # all HCA active ports
   return 0
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
		puts " "
	}
	close $f
	return "Dumped pkeys into:pkeys.txt"
}

# set the change bit on one of the switches:
proc setOneSwitchChangeBit {fabric} {
   global IB_SW_NODE

   set allNodes [IBFabric_NodeByName_get $fabric]
   foreach nameNNode $allNodes {
      set node [lindex $nameNNode 1]
      #if Switch
      if {[IBNode_type_get $node] == $IB_SW_NODE} {
			set numPorts [IBNode_numPorts_get $node]
			for {set pn 1} {$pn <= $numPorts} {incr pn} {
				set pi [IBMSNode_getPortInfo sim$node $pn]
				set old [ib_port_info_t_state_info1_get $pi]
				set new [expr ($old & 0xf0) | 0x2]
				ib_port_info_t_state_info1_set $pi $new
			}

         set swi [IBMSNode_getSwitchInfo sim$node]
         set lifeState [ib_switch_info_t_life_state_get $swi]
         set lifeState [expr ($lifeState & 0xf8) | 4 ]
         ib_switch_info_t_life_state_set $swi $lifeState
         puts "-I- Set change bit on switch:$node"
         return "-I- Set change bit and INIT all ports on switch:$node"
      }
   }
   return "-E- Fail to set any change bit. Could not find a switch"
}

# Validate the inventory generated from a particular node
# matches the partition. Return number of errors. 0 is OK.
proc validateOsmTestInventory {queryNode fileName} {
   global PORT_PKEY_GROUP
}

# Dump out the HCA ports and their groups:
proc dumpHcaPKeyGroupFile {simDir} {
   global PORT_PKEY_GROUP
   global GROUP_PKEY
	global PORT_GROUP_PKEY_IDX

   set fn [file join $simDir "port_pkey_groups.txt"]
   set f [open $fn w]

   foreach port [array names PORT_PKEY_GROUP] {
      set node [IBPort_p_node_get $port]
      set sys  [IBNode_p_system_get $node]
      set num  [IBPort_num_get $port]
      set name [IBSystem_name_get $sys]
      set guid [IBPort_guid_get $port]
      set grp  $PORT_PKEY_GROUP($port)
      set pkey $GROUP_PKEY($grp)
		set idx  $PORT_GROUP_PKEY_IDX($port)
      puts $f "$name $num $grp $guid $pkey"
   }
   close $f
   return "Dumpped Group info into:$fn"
}

set fabric [IBMgtSimulator getFabric]
