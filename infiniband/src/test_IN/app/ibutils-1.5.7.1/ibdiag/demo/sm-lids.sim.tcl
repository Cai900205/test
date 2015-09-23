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


# obtain the list of addressible ports of the fabric:
# since the IBDM does not support port 0 (all port are really physp)
# we need to provide back the list of node/portNum pairs
proc getAddressiblePorts {fabric} {
   global PORT_LID LID_PORTS
   global IB_SW_NODE

   set nodePortNumPairs {}

   # go over all nodes
   foreach nodeNameNId [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nodeNameNId 1]

      # switches has only one port - port 0
      if {[IBNode_type_get $node] == $IB_SW_NODE} {
         lappend nodePortNumPairs [list $node 0]
         for {set pn $pMin} {$pn <= $pMax} {incr pn} {
            set port [IBNode_getPort $node $pn]
            if {$port == ""} {continue}
				set lid [IBPort_base_lid_get $port]
				set key "$node $pn"
				set PORT_LID($key) $lid
				if {![info exists LID_PORTS($lid)]} {
					set LID_PORTS($lid) "{$key}"
				} else {
					lappend LID_PORTS($lid) $key
				}
			}
      } else {
         set pMin 1
         set pMax [IBNode_numPorts_get $node]
         for {set pn $pMin} {$pn <= $pMax} {incr pn} {
            set port [IBNode_getPort $node $pn]
            if {$port == ""} {continue}

				set lid [IBPort_base_lid_get $port]
				set key "$node $pn"
				set PORT_LID($key) $lid
				if {![info exists LID_PORTS($lid)]} {
					set LID_PORTS($lid) "{$key}"
				} else {
					lappend LID_PORTS($lid) $key
				}
            # if the port is not connected ignore it:
            if {[IBPort_p_remotePort_get $port] != ""} {
               lappend nodePortNumPairs [list $node $pn]
            }
         }
      }
   }
   return $nodePortNumPairs
}

# assign a specify port lid:
proc assignPortLid {node portNum lid} {
   global PORT_LID LID_PORTS BAD_LIDS

   # HACK we can not trust the Fabric PortByLid anymore...

   # first we set the IBDM port lid
   if {$portNum == 0} {
      set pMin 1
      set pMax [IBNode_numPorts_get $node]
      for {set pn $pMin} {$pn <= $pMax} {incr pn} {
         set port [IBNode_getPort $node $pn]
         if {$port != ""} {
            IBPort_base_lid_set $port $lid
         }
      }
   } else {
      set port [IBNode_getPort $node $portNum]
      if {$port != ""} {
         IBPort_base_lid_set $port $lid
      }
   }
   set pi [IBMSNode_getPortInfo sim$node $portNum]
   set prevLid [ib_port_info_t_base_lid_get $pi]

   if {$lid == $prevLid} {return 0}

   ib_port_info_t_base_lid_set $pi $lid

   # track it:
   set key "$node $portNum"
   if {[info exists LID_PORTS($prevLid)]} {
      set idx [lsearch $LID_PORTS($prevLid) $key]
      if {$idx >= 0} {
         set LID_PORTS($prevLid) [lreplace $LID_PORTS($prevLid) $idx $idx]
         if {[llength $LID_PORTS($prevLid)] < 2} {
            if {[info exists BAD_LIDS($prevLid)]} {
               unset BAD_LIDS($prevLid)
            }
         }
      }
   }

   set PORT_LID($key) $lid
   if {![info exists LID_PORTS($lid)]} {
      set LID_PORTS($lid) "{$key}"
   } else {
      if {[lsearch $LID_PORTS($lid) $key] < 0} {
         lappend LID_PORTS($lid) $key
         if {[llength $LID_PORTS($lid)] > 1} {
            set BAD_LIDS($lid) 1
         }
      }
   }
}

# randomaly select one of the numbers provided in the given sequence
proc getRandomNumOfSequence {seq} {
   set idx [expr int([rmRand]*[llength $seq])]
   return [lindex $seq $idx]
}

# get a free lid value by randomizing it and avoiding used ones
proc getFreeLid {lmc} {
   global LID_PORTS
   set lid 0
   while {$lid == 0} {
      set lid [expr int( [array size LID_PORTS] * 3 * [rmRand])]
      set lid [expr ($lid >> $lmc) << $lmc]
      if {[info exists LID_PORTS($lid)]} {set lid 0}
   }
   return $lid
}

# get a used lid
proc getUsedLid {} {
   global LID_PORTS
   return [getRandomNumOfSequence [array names LID_PORTS]]
}

# randomize lid assignment errors:
# 1. not assigned
# 2. colide with other ports
# 3. mis-aligned lids
# 4. modify and single
# 5. modify and collide
#
proc setLidAssignmentErrors {fabric lmc} {
   global PORT_LID BAD_LIDS
   global DISCONNECTED_NODES
	global errorInfo
   # we simply go over all ports again randomize errors
   # then inject them acordingly

   set randProfile {
      OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK OK
      NotAssigned NotAssigned
      MisAligned MisAligned
      ModifiedCollide ModifiedCollide
      ModifiedNotCollide ModifiedNotCollide
   }

	if {[catch {
		set addrNodePortNumPairs [getAddressiblePorts $fabric]
	} e]} {
		puts "$e"
		puts $errorInfo
	}
	puts "Got [llength $addrNodePortNumPairs]"
   set lidStep [expr 1 << $lmc]
   set numBadLids 0
   set numDisc 0
   # go over al the ports in the fabric and set their lids
   foreach nodePortNum $addrNodePortNumPairs {
      set node [lindex $nodePortNum 0]
      set pn   [lindex $nodePortNum 1]

      if {$pn != 0} {
         set port [IBNode_getPort $node $pn]
         if {$port == ""} {continue}
         set guid [IBPort_guid_get $port]
      } else {
         set guid [IBNode_guid_get $node]
      }

      set badLidCode [getRandomNumOfSequence $randProfile]

      switch $badLidCode {
         NotAssigned {
            puts "-I- Nulling node:$node guid:$guid port:$pn"
            assignPortLid $node $pn 0
            incr numBadLids
         }
         MisAligned {
            if {($lmc > 0) && ($pn != 0)} {
               puts "-I- Mis-align node:$node guid:$guid port:$pn"
               set badLid [expr $PORT_LID($node $pn) + 1]
               assignPortLid $node $pn $badLid
               # this is the only case where we need to
               # mark bad lids explicitly as the assignmemnt thinks it is ok
               set BAD_LIDS($badLid) 1
               incr numBadLids
            }
         }
         ModifiedCollide {
            set badLid [getUsedLid]
            puts [format \
                     "-I- Colliding node:$node guid:$guid port:$pn on lid:0x%04x" \
                     $badLid]
            assignPortLid $node $pn $badLid
            incr numBadLids
         }
         ModifiedNotCollide {
            set badLid [getFreeLid $lmc]
            puts [format "-I- Changing node:$node guid:$guid port:$pn to free lid:0x%04x" $badLid]
            assignPortLid $node $pn $badLid
            incr numBadLids
         }
      }
   }
   # every node/port
   set res "-I- Created $numBadLids lid modifications, disconnected $numDisc nodes"
   puts "$res"
   return $res
}

set fabric [IBMgtSimulator getFabric]

