#!/bin/sh
# the next line restarts using tclsh \
   exec tclsh8.3 "$0" "$@"

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

set Usage "Usage: $argv0 portNum <rem_lid> <rem_port> <bad_lid> <bad_port>"

set Help "

IBPM API Test

SYNOPSYS:
 $Usage

DESCRIPTION:
 This simple test works at the PM lowest level interface.
 It perfoems the following checks.

API pmMultiMaxGet :
max_get_1 - simply invoke and verify we get 64

API pmGetPortCounters :
get_cnt_1 - Use a good lid and port pair - check we did obtain some values
get_cnt_2 - Use a bad  lid and port pair - check we fail

API pmGetPortCountersMulti :
get_multi1 - Use local and remote ports - check we did obtain some data.
get_multi2 - Use local, remote and bad ports - check we did obtain some data.

API pmClrAllCounters :
clr1 - Clear local port - make sure we got 0...
clr2 - Clear remote port - make sure we got 0...
clr3 - Clear bad port - make sure we got an error

API pmClrAllCountersMulti :
clr_multi1 - Clear local and remote ports - make sure we got 0...
clr_multi2 - Clear local, remote and bad port - make sure we got 0s abnd err

FLOWS:
flow1 - get ports on local and remote. See number of packets advanced on local
TODO: flow2 - Validate each counter by using CR-Space access.

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

# Make sure we got some reasonable counter values.
proc ibpm_validate_counters {counters} {
   return [regexp {port_select.*counter_select.*symbol_error_counter.*port_rcv_pkts} $counters]
}

# Make sure counters were cleared. return 1 if all zeros.
proc ibpm_validate_counters_cleared {counters} {
   if {[regexp {counter_select.*\s+.([a-z][a-z_15]+\s+[1-9][0-9]+)} $counters d1 viol]} {
      puts "-E- found non zero counters: $viol"
      return 0
   }
   return 1
}

# given a key and a list of ley/value pairs get the pair
proc assoc {key key_list} {
	foreach kv $key_list {
		if {[lindex $kv 0] == $key} {return [lrange $kv 1 end]}
	}
	return ""
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

if {[llength $argv] != 5} {
   puts $Usage
   exit
}

set doBadCases 1
set numLoops 10

set locPort [lindex $argv 0]
set remLid  [lindex $argv 1]
set remPort [lindex $argv 2]
set badLid  [lindex $argv 3]
set badPort [lindex $argv 4]

#
# Set ibis port:
#
package require ibis
ibis_set_verbosity 0xff
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

package require ibpm

#
# Start API testing
#
set anyError 0

for {set i 0} {$i < $numLoops} {incr i} {
   # API pmMultiMaxGet :
   # max_get_1 - simply invoke and verify we get 64
   if {[ex pmMultiMaxGet] != 64} {
      puts "-E- pmMultiMaxGet did not return 64"
      incr anyError
   }

   #
   # API pmGetPortCounters :
   # get_cnt_1 - Use a good lid and port pair - check we did obtain some values
   set cnts [ex "pmGetPortCounters $locLid $locPort"]
   if {![ibpm_validate_counters $cnts]} {
      puts "-E- Failed to validate counters."
      incr anyError
   }

   if {$doBadCases} {
      # get_cnt_2 - Use a bad  lid and port pair - check we fail
      set cnts [ex "pmGetPortCounters $badLid $badPort"]
      if {[ibpm_validate_counters $cnts]} {
         puts "-E- Failed to invalidate counters error."
         incr anyError
      }
   }

   #
   # API pmGetPortCountersMulti :
   # get_multi1 - Use local and remote ports - check we did obtain some data.
   set cnts [ex "pmGetPortCountersMulti 2 {$locLid $remLid} {$locPort $remPort}"]
   if {![ibpm_validate_counters [lindex $cnts 0]]} {
      puts "-E- Failed to validate counters:[lindex $cnts 0] "
      incr anyError
   }
   if {![ibpm_validate_counters [lindex $cnts 1]]} {
      puts "-E- Failed to validate counters:[lindex $cnts 1] "
      incr anyError
   }

   # get_multi2 - Use local, remote and bad ports - check we obtain some data.
   # Last is bad
   set cnts [ex "pmGetPortCountersMulti 3 {$locLid $remLid $badLid} {$locPort $remPort $badPort}"]
   if {![ibpm_validate_counters [lindex $cnts 0]]} {
      puts "-E- Failed to validate counters:[lindex $cnts 0] "
      incr anyError
   }
   if {![ibpm_validate_counters [lindex $cnts 1]]} {
      puts "-E- Failed to validate counters:[lindex $cnts 1] "
      incr anyError
   }
   if {[ibpm_validate_counters [lindex $cnts 2]]} {
      puts "-E- Failed to invalidate bad counters:[lindex $cnts 2] "
      incr anyError
   }
   # Middle BAD
   set cnts [ex "pmGetPortCountersMulti 3 {$locLid $badLid $remLid} {$locPort $badPort $remPort}"]
   if {![ibpm_validate_counters [lindex $cnts 0]]} {
      puts "-E- Failed to validate counters:[lindex $cnts 0] "
      incr anyError
   }
   if {![ibpm_validate_counters [lindex $cnts 2]]} {
      puts "-E- Failed to validate counters:[lindex $cnts 2] "
      incr anyError
   }
   if {[ibpm_validate_counters [lindex $cnts 1]]} {
      puts "-E- Failed to invalidate bad counters:[lindex $cnts 1] "
      incr anyError
   }
   # End BAD:
   set cnts [ex "pmGetPortCountersMulti 3 {$badLid $locLid $remLid} {$badPort $locPort $remPort}"]
   if {![ibpm_validate_counters [lindex $cnts 1]]} {
      puts "-E- Failed to validate counters:[lindex $cnts 1] "
      incr anyError
   }
   if {![ibpm_validate_counters [lindex $cnts 2]]} {
      puts "-E- Failed to validate counters:[lindex $cnts 2] "
      incr anyError
   }
   if {[ibpm_validate_counters [lindex $cnts 0]]} {
      puts "-E- Failed to invalidate bad counters:[lindex $cnts 0] "
      incr anyError
   }

   #
   # API pmClrAllCounters :
   # clr1 - Clear local port - make sure we got 0...
   set err [ex "pmClrAllCounters $locLid $locPort"]
   if {$err} {
      puts "-E- Failed to clear counters on lid:$locLid port:$locPort  "
      incr anyError
   }
   set cnts [ex "pmGetPortCounters $locLid $locPort"]
   if {![ibpm_validate_counters_cleared [lindex $cnts 0]]} {
      puts "-E- Some counters not cleared on lid:$locLid port:$locPort => [lindex $cnts 0]"
      incr anyError
   }
   # clr2 - Clear remote port - make sure we got 0...
   set err [ex "pmClrAllCounters $remLid $remPort"]
   if {$err} {
      puts "-E- Failed to clear counters on lid:$remLid port:$remPort  "
      incr anyError
   }
   set cnts [ex "pmGetPortCounters $remLid $remPort"]
   if {![ibpm_validate_counters_cleared [lindex $cnts 0]]} {
      puts "-E- Some counters not cleared on lid:$remLid port:$remPort => [lindex $cnts 0]"
      incr anyError
   }

   # clr3 - Clear bad port - make sure we got an error
   set err [ex "pmClrAllCounters $badLid $badPort"]
   if {$err == 0} {
      puts "-E- Failed to catch bad clear counters on lid:$badLid port:$badPort  "
      incr anyError
   }

   #
   # API pmClrAllCountersMulti :
   # clr_multi1 - Clear local and remote ports - make sure we got 0...
   set err \
      [ex "pmClrAllCountersMulti 2 {$locLid $remLid} {$locPort $remPort}"]
   if {$err != 0} {
      puts "-E- Failed to clear counters on lid:$locLid port:$locPort => $err "
      incr anyError
   }
   set multiCnts \
      [ex "pmGetPortCountersMulti 2 {$locLid $remLid} {$locPort $remPort}"]
   foreach cnt $multiCnts {
      if {![ibpm_validate_counters_cleared $cnts]} {
         puts "-E- Some counters not cleared on lid:$remLid port:$remPort => $cnts"
         incr anyError
      }
   }

   # clr_multi2 - Clear local, remote and bad - make sure we got 0s abnd err
   #
   set err \
      [ex "pmClrAllCountersMulti 2 {$locLid $remLid $badLid} {$locPort $remPort $badPort}"]
   if {$err != 0} {
      puts "-E- Failed to clear counters on lid:$locLid port:$locPort => $err "
      incr anyError
   }
   set multiCnts \
      [ex "pmGetPortCountersMulti 2 {$locLid $remLid} {$locPort $remPort}"]
   foreach cnt $multiCnts {
      if {![ibpm_validate_counters_cleared $cnts]} {
         puts "-E- Some counters not cleared on lid:$remLid port:$remPort => $cnts"
         incr anyError
      }
   }

   # FLOWS:
   # flow1 - get ports on local and remote. See number packets advanced...
   set cnts \
      [ex "pmGetPortCountersMulti 2 {$locLid $remLid} {$locPort $remPort}"]
   set xmitPkts1 [assoc port_xmit_pkts [lindex $cnts 0]]
   set cnts \
      [ex "pmGetPortCountersMulti 2 {$locLid $remLid} {$locPort $remPort}"]
   set xmitPkts2 [assoc port_xmit_pkts [lindex $cnts 0]]
   if {$xmitPkts1 >= $xmitPkts2} {
      puts "-E- Expected number of local port packets to grow. But xmitPkts1:$xmitPkts1 >= xmitPkts2:$xmitPkts2"
      incr anyError
   }

   # flow2 - Validate each counter by using CR-Space access.
}

if {$anyError} {
   puts "-E- TEST FAILED (with $anyError errors)"
}
exit $anyError
