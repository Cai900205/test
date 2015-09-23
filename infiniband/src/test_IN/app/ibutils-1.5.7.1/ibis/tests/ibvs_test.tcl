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

set Usage "Usage: $argv0 portNum i2c|flash <remLid1> <remLid2>"

set Help "

IBVS API Test

SYNOPSYS:
 $Usage

DESCRIPTION:
 This simple test works at the Vendor Specific lowest level interface.
 It either works on a flash or i2c controlled ROM.
 It perfoems the following checks:

vsMultiMaxGet
max1 - verify we get 64

vsI2cRead
vsI2cReadMulti
vsI2cWrite
vsI2cWriteMulti

We perform the following flow for verification:
read1 - read some addresses from the EEPROM assuming fixed addresses
read2 - repeat from teh other devices
read3 - do multi read and verify we got same result
read4 - try reading same multi read but this time with some garbadge lid too.
        verify we still get what expect.
writ1 - modify the data using vsI2cWrite (with some random data)
read5 - make sure we get back what we expect
writ2 - use vsI2cWriteMulti to recover old data
read6 - use vsI2cReadMulti to verify we got it.

vsFlashStartMulti
vsFlashStopMulti
vsFlashSetBankMulti
vsFlashEraseSectorMulti
vsFlashReadSectorMulti
vsFlashWriteSectorMulti

vsSWReset

vsCpuRead TODO
vsCpuWrite TODO
vsGpioRead TODO
vsGpioWrite TODO
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

set locPort [lindex $argv 0]
set mode    [lindex $argv 1]
set remLid1 [lindex $argv 2]
set remLid2 [lindex $argv 3]

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

package require ibvs

#
# Start API testing
#
set anyError 0

for {set i 0} {$i < $numLoops} {incr i} {
   if {$mode == "i2c"} {
      # read1 - read some addresses from the EEPROM assuming fixed addresses
      set data [ex "vsI2cRead $remLid2 1 0x56 32 0x30"]
      for {set i 0} {$i < 4} {incr i} {
         set DATA1($i) [assoc data$i $data]
      }
      if {$DATA1(0) != "0x56010000"} {
         puts "-E- Expected 0x56010000 as the data0 got:$DATA1(0)"
         incr anyError
      }
      if {$DATA1(1) != "0x57010000"} {
         puts "-E- Expected 0x57010000 as the data1 got:$DATA1(1)"
         incr anyError
      }
      if {$DATA1(2) != "0x52010000"} {
         puts "-E- Expected 0x52010000 as the data2 got:$DATA1(2)"
         incr anyError
      }
      if {$DATA1(3) != "0x53010000"} {
         puts "-E- Expected 0x53010000 as the data3 got:$DATA1(3)"
         incr anyError
      }

      # read2 - repeat from the other devices
      set data [ex "vsI2cRead $remLid1 1 0x56 32 0x30"]
      for {set i 0} {$i < 4} {incr i} {
         set DATA2($i) [assoc data$i $data]
      }
      if {$DATA2(0) != "0x56010000"} {
         puts "-E- Expected 0x56010000 as the data0 got:$DATA2(0)"
         incr anyError
      }
      if {$DATA2(1) != "0x57010000"} {
         puts "-E- Expected 0x57010000 as the data1 got:$DATA2(1)"
         incr anyError
      }
      if {$DATA2(2) != "0x52010000"} {
         puts "-E- Expected 0x52010000 as the data2 got:$DATA2(2)"
         incr anyError
      }
      if {$DATA2(3) != "0x53010000"} {
         puts "-E- Expected 0x53010000 as the data3 got:$DATA2(3)"
         incr anyError
      }

      # read3 - do multi read and verify we got same result
      set mdata [ex "vsI2cReadMulti 2 {$remLid1 $remLid2} 1 0x56 32 0x30"]
      if {[llength $mdata] != 2} {
         puts "-E- Expected 2 sub lists in result for multi. Got:$mdata"
         incr anyError
      } else {
         # OK SO GO CHEK IT:
         for {set d 1} { $d <= 2} {incr d} {
            set data [lindex $mdata [expr $d - 1]]
            for {set i 0} {$i < 4} {incr i} {
               set prev [lindex [array get DATA$d $i] 1]
               set this [assoc data$i $data]
               if {$prev != $this} {
                  puts "-E- Missmatch on set:$d data:$i $prev != $this"
                  incr anyError
               }
            }
         }
      }

      # read4 - try reading same multi read but this time with some garbadge
      #         lid too. verify we still get what expect.
      set mdata [ex "vsI2cReadMulti 3 {$remLid1 $remLid2 99} 1 0x56 32 0x30"]
      if {[llength $mdata] != 3} {
         puts "-E- Expected 2 sub lists in result for multi. Got:$mdata"
         incr anyError
      } else {
         # OK SO GO CHEK IT:
         for {set d 1} { $d <= 2} {incr d} {
            set data [lindex $mdata [expr $d - 1]]
            for {set i 0} {$i < 4} {incr i} {
               set prev [lindex [array get DATA$d $i] 1]
               set this [assoc data$i $data]
               if {$prev != $this} {
                  puts "-E- Missmatch on set:$d data:$i $prev != $this"
                  incr anyError
               }
            }
         }
         # check we did get an error on the last sublist:
         set data [lindex $mdata 2]
         if {![regexp ERROR $data]} {
            puts "-E- Missing expected error on lid:99 doing vsI2cReadMulti"
            incr anyError
         }
      }

      # writ1 - modify the data using vsI2cWrite (with some random data)
      set newWords {}
      for {set i 0} {$i < 4} {incr i} {
         lappend newWords [format "0x%08x" [expr int(rand()*0xfffffff*8)]]
      }
      set res [ex "vsI2cWrite $remLid1 1 0x56 16 0x30 {$newWords}"]
      if {$res != 0} {
         puts "-E- failed to write: sI2cWrite $remLid1 1 0x56 16 0x30 {$newWords}"
         incr anyError
      } else {
         after 200
         # read5 - make sure we get back what we expect
         set data [ex "vsI2cRead $remLid1 1 0x56 16 0x30"]
         for {set i 0} {$i < 4} {incr i} {
            set prev [lindex $newWords $i]
            set this [assoc data$i $data]
            if {$prev != $this} {
               puts "-E- Missmatch read back of written data-set:$d data:$i $prev != $this"
               incr anyError
            }
         }
      }
      # writ2 - use vsI2cWriteMulti to recover old data
      set oldWords "$DATA1(0) $DATA1(1) $DATA1(2) $DATA1(3)"
      set res [ex "vsI2cWrite $remLid1 1 0x56 16 0x30 {$oldWords}"]
      if {$res != 0} {
         puts "-E- failed to write: sI2cWrite $remLid1 1 0x56 16 0x30 {$oldWords}"
         incr anyError
      } else {
         # read6 - make sure we get back what we expect
         after 200
         set data [ex "vsI2cRead $remLid1 1 0x56 16 0x30"]
         for {set i 0} {$i < 4} {incr i} {
            set prev [lindex $oldWords $i]
            set this [assoc data$i $data]
            if {$prev != $this} {
               puts "-E- Missmatch read back of reverted data-set:$d data:$i $prev != $this"
               incr anyError
            }
         }
      }

      #
   } else {
      puts "-E- Unsupported Mode:$mode"
      exit 1
   }
}

if {$anyError} {
   puts "-E- TEST FAILED (with $anyError errors)"
}
exit $anyError
