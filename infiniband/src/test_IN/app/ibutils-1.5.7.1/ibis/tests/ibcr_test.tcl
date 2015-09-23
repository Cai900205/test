#!/bin/sh
# the next line restarts using tclsh \
   exec tclsh "$0" "$@"

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

set Usage "Usage: $argv0 portNum <is3Lid> <remTavorLid> <badLid>"

set Help "

IBCR API Test

SYNOPSYS:
 $Usage

DESCRIPTION:
 This simple test works at the CR Space lowest level interface.
 It perfoems the following checks.

API crMultiMaxGet :
max_get_1 - simply invoke and verify we get 64

API crRead :
get_1 - Use a good IS3 lid - check we did obtain device id
get_2 - Use a good Tavor lid - check we did obtain device id
get_3 - Use a bad  lid - check we fail

API crReadMulti :
get_multi1 - Use local and remote lids - check we did obtain some data.
get_multi2 - Use local, remote and bad lids - check we did obtain some data.

API crWrite :
write1 - write to Tavor  and Read to see if got it.
write2 - write to IS3  and Read to see if got it.
write3 - write to bad lid - make sure we got an error

API crWriteMulti :
write_multi1 - write to the 2 tavors
write_multi2 - write twice to the IS3

"
########################################################################

# provide a verbose evaluation method:
proc ex {expr} {
   global verbose

   if {$verbose} {puts "EX: $expr"}
   if {[catch {eval "set res \[$expr\]"} e]} {
      if {$verbose} {puts "EX: ERR: $e"}
      return $e
   } else {
      if {$verbose} {puts "EX: => $res"}
   }
   return $res
}

# given a key and a list of ley/value pairs get the pair
proc assoc {key key_list} {
	foreach kv $key_list {
		if {[lindex $kv 0] == $key} {return [lrange $kv 1 end]}
	}
	return ""
}

# check if the given result is an error
proc ibcr_is_error_res {data idx} {
   if {[llength $data] < $idx + 1} {
      puts "-E- Resulting list too short: idx:$idx $data"
      return 0
   }
   return [regexp {ERROR} [lindex $data $idx]]
}

########################################################################

#
# PARSE COMMAND LINE
#
if {[llength $argv] && [lindex $argv 0] == "-v"} {
   set argv [lrange $argv 1 end]
   set verbose 1
} else {
   set verbose 0
}

if {[llength $argv] != 4} {
   puts $Usage
   exit
}

set doBadCases 1
set numLoops 1

set locPort     [lindex $argv 0]
set is3Lid      [lindex $argv 1]
set remTavorLid [lindex $argv 2]
set badLid      [lindex $argv 3]

#
# Set ibis port:
#
package require ibis
ibis_set_verbosity 0xff
ibis_opts configure -log_file /tmp/ibcr_test.log
ibis_init

set availPorts [ibis_get_local_ports_info]

# make sure we can bind to the requested port
if {[llength $availPorts] < $locPort} {
   puts "-E- Not enough local ports: [llength $availPorts] (required $locPort)"
   exit
}

set portInfo [lindex $availPorts [expr $locPort - 1]]
# make sure it is active...
if {[lindex $portInfo 2] != "ACTIVE"} {
   puts "-E- Requested port: $locPort status is:$portInfo"
   exit
}

set locLid [expr [lindex $portInfo 1]]

# bind to the port
ibis_set_port [lindex $portInfo 0]

package require ibcr

#
# Start API testing
#
set anyError 0

for {set loop 0} {$loop < $numLoops} {incr loop} {
   # API crMultiMaxGet :
   # max_get_1 - simply invoke and verify we get 64
   if {[ex crMultiMaxGet] != 64} {
      puts "-E- crMultiMaxGet did not return 64"
      incr anyError
   }

   #
   # API crRead :
   # get_1 - Use a good lid - check we did obtain some values
   set data [ex "crRead $is3Lid 0x60014"]
   set anafa2_id [lindex [lindex $data 1 ] 1 ]
   if {[expr $anafa2_id & 0xffff] != 0xB924} {
      puts "-E- Failed to InfiniScale3 Device ID. crRead failed."
      incr anyError
   }


   set data [ex "crRead $remTavorLid 0xF0014"]
   set tavor_id [lindex [lindex $data 1 ] 1 ]
   if {[expr $tavor_id & 0xffff] != 0x5A44} {
      puts "-E- Failed to Tavor Device ID. crRead failed."
      incr anyError
   }

   if {$doBadCases} {
      # get_cnt_2 - Use a bad  lid and port pair - check we fail
      set data [ex "crRead $badLid 0xF0014"]
      if {[lindex $data 0] != "TARGET_ERROR"} {
         puts "-E- Failed to get an error on bad read."
         incr anyError
      }
   }

   #
   # API crReadMulti :
   # get_multi1 - Use local and remote lids - check we did devcie ids
   set data [ex "crReadMulti 3 {$locLid $remTavorLid $is3Lid} 0xF0014"]
   if {[llength $data] != 3} {
      puts "-E- Fail to read multiple lids..."
      incr anyError
   }
   # first and second should be Tavors:
   set tavor_id [lindex [lindex [lindex $data 0] 1 ] 1 ]
   if {[expr $tavor_id & 0xffff] != 0x5A44} {
      puts "-E- Failed to validate Tavor Device ID for lid $locLid."
      incr anyError
   }
   set tavor_id [lindex [lindex [lindex $data 1] 1 ] 1 ]
   if {[expr $tavor_id & 0xffff] != 0x5A44} {
      puts "-E- Failed to validate Tavor Device ID for lid $remTavorLid."
      incr anyError
   }
   set data [ex "crReadMulti 3 {$locLid $remTavorLid $is3Lid} 0x60014"]
   if {[llength $data] != 3} {
      puts "-E- Fail to read multiple lids..."
      incr anyError
   }
   # Third is an IS3:
   set is3_id [lindex [lindex [lindex $data 2] 1 ] 1 ]
   if {[expr $is3_id & 0xffff] != 0xB924} {
      puts "-E- Failed to validate IS3 Device ID for lid $is3Lid."
      incr anyError
   }

   # get_multi2 - Use local, remote and bad lids - check we obtain some data.
   set lidsList "$locLid $remTavorLid $is3Lid"
   for {set badIdx 0} {$badIdx < 4} {incr badIdx} {
      set lids [linsert $lidsList $badIdx $badLid]
      set data [ex "crReadMulti 4 {$lids} 0x60014"]
      for {set i 0} {$i < 4} {incr i} {
         if {$i == $badIdx} {
            if {![ibcr_is_error_res $data $i]} {
               puts "-E- Failed to get error in idx:$badIdx of lids:$lids $data"
               incr anyError
            }
         } else {
            if {[ibcr_is_error_res $data $i]} {
               puts "-E- Un-expected error in idx:$badIdx of lids:$lids $data"
               incr anyError
            }
         }
      }
   }

   # write to cr ...
   # we are going to write to the PortRcvErrors ...
   set addr 0x10130
   set data [format "0x%08x" [expr int(rand()*0xffff)]]
   set res [ex "crWrite $locLid $data $addr"]
   if {$res != 0} {
      puts "-E- Fail to write : crWrite $locLid $data $addr"
      incr anyError
   } else {
      set rdata [ex "crRead $locLid $addr"]
      set rdata [assoc data $rdata]
      if {[expr $rdata & 0xffff] != $data} {
         puts "-E- Failed to Read back written data. "
         incr anyError
      }
   }

   # do a multi write ...
   set res [ex "crWriteMulti 2 {$locLid $remTavorLid} $data $addr"]
   if {$res != 0} {
      puts "-E- Fail to multi write : crWriteMulti 2 {$locLid $remTavorLid} $data $addr"
      incr anyError
   } else {
      set rdata [ex "crRead $locLid $addr"]
      set rdata [assoc data $rdata]
      if {[expr $rdata & 0xffff] != $data} {
         puts "-E- Failed to Read back written data. "
         incr anyError
      }
      set rdata [ex "crRead $remTavorLid $addr"]
      set rdata [assoc data $rdata]
      if {[expr $rdata & 0xffff] != $data} {
         puts "-E- Failed to Read back written data. "
         incr anyError
      }
   }

}

if {$anyError} {
   puts "-E- TEST FAILED (with $anyError errors)"
}
exit $anyError
