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

### This script is running over ibis (/usr/bin/ibis)
source [file join [file dirname [info script]] ibdebug.tcl]

######################################################################
#  IB Debug Tools
#  NAME
#     ibdiagpath
#
# DATAMODEL
#     Note: all global variables are placed in the array G
#
#  FUNCTION
#     ibdiagpath traces a path between two LIDs/GUIDs
#     and provides information regarding the nodes and ports passed and their health.
#     ibdiagpath utilizes device-specific health queries for the different devices on the path.
#
#  AUTHOR
#     Danny Zarko. Mellanox Technologies LTD.
#
#  CREATION DATE
#     04/Aug/05
#
#  MODIFICATION HISTORY
#  $Revision: 2622 $
#  Initial Revision.
#
#  NOTES
#
######################################################################

######################################################################
proc ibdiagpathMain {} {
   # previously, consisted of 2 procs, ibdiagpathGetPaths & readPerfCountres
   global G errorInfo
   set addressingLocalPort 0
   # So we could use topology file names
   set G(bool:topology.matched) [info exists G(argv:topo.file)]
   # lid routing
   if {[info exists G(argv:lid.route)]} {
      set targets [split $G(argv:lid.route) ","]
      if { $G(argv:lid.route) == $G(data:root.port.lid) } {
         set addressingLocalPort 1
      }
      if {[llength $targets] == 2} {
         if {[lindex $targets 0] == [lindex $targets 1]} {
            set targets [lindex $targets 0]
         }
      }
   }

   # direct routing
   if [info exists G(argv:direct.route)] {
      set targets [list [split $G(argv:direct.route) ","]]
      if { $G(argv:direct.route) == "" } {
         set addressingLocalPort 1
      }
   }

   ## for the special case when addressing the local node
   if $addressingLocalPort {
      inform "-W-ibdiagpath:ardessing.local.node"
      set paths $G(argv:port.num)
   }

   # CHECK some Where that the names are legal
   if [info exists G(argv:by-name.route)] {
      array set mergedNodesArray [join [IBFabric_NodeByName_get $G(IBfabric:.topo)]]

      set localNodePtr  [IBFabric_getNode $G(IBfabric:.topo) $G(argv:sys.name)]
      set localPortPtr  [IBNode_getPort $localNodePtr $G(argv:port.num)]
      set localPortName [IBPort_getName $localPortPtr]
      if {[catch {set tmpRemote [IBPort_p_remotePort_get $localPortPtr]}]} {
         continue
      }
      foreach portPtr [GetArgvPortNames] {
         if { $portPtr == $localPortPtr } {
            lappend targets $G(data:root.port.lid)
         } else {
            if {[catch {set tmpDR [Name2Lid $tmpRemote $portPtr $G(argv:port.num)]} e]} {
               inform "-E-topology:bad.sysName.or.bad.topoFile" -name [IBPort_getName $portPtr]
            }
            if {$tmpDR == -1} {
               inform "-E-topology:no.route.to.host.in.topo.file" -name [IBPort_getName $portPtr] -topo.file $G(argv:topo.file)
            }
            if {[lindex $tmpDR end ] == 0} {
               if {[catch {set newTarget [GetParamValue LID [lrange $tmpDR 0 end-1] -port 0 -byDr]} e]} {
                  inform "-E-topology:bad.path.in.name.tracing" -path $tmpDR -name [IBPort_getName $portPtr]
               }
            } else {
               if {[catch {set newTarget [GetParamValue LID "$tmpDR" -port [IBPort_num_get $portPtr] -byDr]} e]} {
                  inform "-E-topology:bad.path.in.name.tracing" -path $tmpDR -name [IBPort_getName $portPtr]
               }
            }
            if {($newTarget == -1)} {
               inform "-E-topology:lid.by.name.failed" -name [IBPort_getName $portPtr]
            }
            if {($newTarget == 0)} {
               inform "-E-topology:lid.by.name.zero" -path $tmpDR -name [IBPort_getName $portPtr]
            }
            lappend targets $newTarget
            lappend targetsNames
         }
         lappend targetsNames [IBPort_getName $portPtr]
         if { "$targets" == $G(data:root.port.lid) } {
            set addressingLocalPort 1
         }
      }
      if {[llength $targets] == 2} {
         inform "-I-ibdiagpath:obtain.src.and.dst.lids" -name0 [lindex $targetsNames 0] \
            -name1 [lindex $targetsNames 1] -lid0 [lindex $targets 0] -lid1 [lindex $targets 1]
      } else {
         inform "-I-ibdiagpath:obtain.src.and.dst.lids" -name0 $localPortName \
            -name1 [lindex $targetsNames 0] -lid0 $G(data:root.port.lid) -lid1 [lindex $targets 0]
      }
   }
   set paths ""
   set G(bool:bad.links.detected) 1
   for {set i 0} {$i < [llength $targets]} {incr i} {
      set address [lindex $targets $i]
      if { !$addressingLocalPort} {
         if {!$i } {
            if {[llength $targets] < 2} {
               inform "-I-ibdiagpath:read.lft.header" local destination
            } else {
               inform "-I-ibdiagpath:read.lft.header" local source
            }
         } else {
            inform "-I-ibdiagpath:read.lft.header" source destination
         }
      }
      set paths [concat $paths [DiscoverPath [lindex $paths end] $address]]
   }

   set G(bool:bad.links.detected) 0

   # Translating $src2trgtPath (starting at node at index $startIndex) into a list of LIDs and ports
   set local2srcPath   [lindex [lreplace $paths end end] end]
   set src2trgtPath    [lindex $paths end]
   set startIdx        [llength $local2srcPath]

   # For the case when the source node is a remote HCA
   if { ( $startIdx != 0 ) && [GetParamValue Type $local2srcPath -byDr] != "SW" } {
      set sourceIsHca [lindex $local2srcPath end]
      incr startIdx -2
   }
   # the following loop is only for pretty-priting...
   set llen ""
   for { set i $startIdx } { $i < [llength $src2trgtPath] } { incr i } {
      set portNames [lindex [linkNamesGet [lrange $src2trgtPath 0 $i]] end]
      lappend llen [string length [lindex $portNames 0]] [string length [lindex $portNames 1]]
   }
   set maxLen [lindex [lsort -integer $llen] end]
   # preparing the list of lid-s and ports for reading the PM counters
   PMCounterQuery
   AnalyzePathPartitions $paths
   CheckPathIPoIB $paths
   CheckPathQoS $paths
   #SL_2_VL $paths $targets
   #foreach path $paths {
   #    puts DZ:[join $path ,]
   #}
   return
}
######################################################################

######################################################################
### Action
######################################################################
### Initialize ibis
InitializeIBDIAG
InitializeINFO_LST
StartIBDIAG

### Figuring out the paths to take and Reading Performance Counters
set G(bool:bad.links.detected) 1
ibdiagpathMain
set G(bool:bad.links.detected) 0
CheckAllinksSettings

### run packages provided procs
RunPkgProcs

### Finishing
FinishIBDIAG
######################################################################
