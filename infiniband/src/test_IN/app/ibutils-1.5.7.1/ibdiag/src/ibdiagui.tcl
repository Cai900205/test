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

# This code should be sourced into ibis through ibdiagui wrapper
source [file join [file dirname [info script]] ibdebug.tcl]

if {[catch {package require ibdm} e]} {
   puts "-E- ibdiagui depends on a 'IBDM' installation"
   puts "    Your ib_utils installation must be broken. Please reinstall"
   puts "    Error: $e"
   exit 1
}

if {[catch {package require ibis} e]} {
   puts "-E- ibdiagui depends on a 'ibis' installation"
   puts "    Your ib_utils installation must be broken. Please reinstall"
   puts "    Error: $e"
   exit 1
}

##############################################################################
#
# GENERIC CANVAS ZOOMING UTILITIES
#
##############################################################################

#--------------------------------------------------------
#
#  zoomMark
#
#  Mark the first (x,y) coordinate for zooming.
#
#--------------------------------------------------------
proc zoomMark {c x y} {
   global zoomArea
   set zoomArea(x0) [$c canvasx $x]
   set zoomArea(y0) [$c canvasy $y]
   $c create rectangle $x $y $x $y -outline black -tag zoomArea
}

#--------------------------------------------------------
#
#  zoomStroke
#
#  Zoom in to the area selected by itemMark and
#  itemStroke.
#
#--------------------------------------------------------
proc zoomStroke {c x y} {
   global zoomArea
   set zoomArea(x1) [$c canvasx $x]
   set zoomArea(y1) [$c canvasy $y]
   $c coords zoomArea $zoomArea(x0) $zoomArea(y0) $zoomArea(x1) $zoomArea(y1)
}

#--------------------------------------------------------
#
#  zoomArea
#
#  Zoom in to the area selected by itemMark and
#  itemStroke.
#
#--------------------------------------------------------
proc zoomArea {c x y} {
   global zoomArea

   #--------------------------------------------------------
   #  Get the final coordinates.
   #  Remove area selection rectangle
   #--------------------------------------------------------
   set zoomArea(x1) [$c canvasx $x]
   set zoomArea(y1) [$c canvasy $y]
   $c delete zoomArea

   #--------------------------------------------------------
   #  Check for zero-size area
   #--------------------------------------------------------
   if {($zoomArea(x0)==$zoomArea(x1)) || ($zoomArea(y0)==$zoomArea(y1))} {
      return
   }

   #--------------------------------------------------------
   #  Determine size and center of selected area
   #--------------------------------------------------------
   set areaxlength [expr {abs($zoomArea(x1)-$zoomArea(x0))}]
   set areaylength [expr {abs($zoomArea(y1)-$zoomArea(y0))}]
   set xcenter [expr {($zoomArea(x0)+$zoomArea(x1))/2.0}]
   set ycenter [expr {($zoomArea(y0)+$zoomArea(y1))/2.0}]

   #--------------------------------------------------------
   #  Determine size of current window view
   #  Note that canvas scaling always changes the coordinates
   #  into pixel coordinates, so the size of the current
   #  viewport is always the canvas size in pixels.
   #  Since the canvas may have been resized, ask the
   #  window manager for the canvas dimensions.
   #--------------------------------------------------------
   set winxlength [winfo width $c]
   set winylength [winfo height $c]

   #--------------------------------------------------------
   #  Calculate scale factors, and choose smaller
   #--------------------------------------------------------
   set xscale [expr {$winxlength/$areaxlength}]
   set yscale [expr {$winylength/$areaylength}]
   if { $xscale > $yscale } {
      set factor $yscale
   } else {
      set factor $xscale
   }

   #--------------------------------------------------------
   #  Perform zoom operation
   #--------------------------------------------------------
   zoom $c $factor $xcenter $ycenter $winxlength $winylength
}

#--------------------------------------------------------
#
#  fit
#
#  Fit to all objects
#--------------------------------------------------------
proc fit { canvas } {
   set bbox [$canvas bbox all]
   # provided view is the start and end of the viewed window in 0.0-1.0 of the
   # entire region.
   set xv [$canvas xview]
   set yv [$canvas yview]
   set xf [expr [lindex $xv 1] - [lindex $xv 0]]
   set yf [expr [lindex $yv 1] - [lindex $yv 0]]
   if {$yf < $xf} {
      set scale $yf
   } else {
      set scale $xf
   }
   # we want to set the center of the canvas to the bbox / 2
   foreach {x0 y0 x1 y1} $bbox {break}
   set x [expr ($x0 + $x1)/2.0]
   set y [expr ($y0 + $y1)/2.0]
   zoom $canvas $scale $x $y
}

#--------------------------------------------------------
#
#  zoom
#
#  Zoom the canvas view, based on scale factor
#  and centerpoint and size of new viewport.
#  If the center point is not provided, zoom
#  in/out on the current window center point.
#
#  This procedure uses the canvas scale function to
#  change coordinates of all objects in the canvas.
#
#--------------------------------------------------------
proc zoom { canvas factor \
               {xcenter ""} {ycenter ""} \
               {winxlength ""} {winylength ""} } {

   #--------------------------------------------------------
   #  If (xcenter,ycenter) were not supplied,
   #  get the canvas coordinates of the center
   #  of the current view.  Note that canvas
   #  size may have changed, so ask the window
   #  manager for its size
   #--------------------------------------------------------
   if { [string equal $winxlength ""] } {
      set winxlength [winfo width $canvas]
   }
   if { [string equal $winylength ""] } {
      set winylength [winfo height $canvas]
   }
   if { [string equal $xcenter ""] } {
      set xcenter [$canvas canvasx [expr {$winxlength/2.0}]]
   }
   if { [string equal $ycenter ""] } {
      set ycenter [$canvas canvasy [expr {$winylength/2.0}]]
   }

   #--------------------------------------------------------
   #  Scale all objects in the canvas
   #  Adjust our viewport center point
   #--------------------------------------------------------
   $canvas scale all 0 0 $factor $factor
   set xcenter [expr {$xcenter * $factor}]
   set ycenter [expr {$ycenter * $factor}]

   #--------------------------------------------------------
   #  Get the size of all the items on the canvas.
   #
   #  This is *really easy* using
   #      $canvas bbox all
   #  but it is also wrong.  Non-scalable canvas
   #  items like text and windows now have a different
   #  relative size when compared to all the lines and
   #  rectangles that were uniformly scaled with the
   #  [$canvas scale] command.
   #
   #  It would be better to tag all scalable items,
   #  and make a single call to [bbox].
   #  Instead, we iterate through all canvas items and
   #  their coordinates to compute our own bbox.
   #--------------------------------------------------------
   set x0 1.0e30; set x1 -1.0e30 ;
   set y0 1.0e30; set y1 -1.0e30 ;
   foreach item [$canvas find all] {
      switch -exact [$canvas type $item] {
         "arc" -
         "line" -
         "oval" -
         "polygon" -
         "rectangle" {
            set coords [$canvas coords $item]
            foreach {x y} $coords {
               if { $x < $x0 } {set x0 $x}
               if { $x > $x1 } {set x1 $x}
               if { $y < $y0 } {set y0 $y}
               if { $y > $y0 } {set y1 $y}
            }
         }
      }
   }

   #--------------------------------------------------------
   #  Now figure the size of the bounding box
   #--------------------------------------------------------
   set xlength [expr {$x1-$x0}]
   set ylength [expr {$y1-$y0}]

   #--------------------------------------------------------
   #  But ... if we set the scrollregion and xview/yview
   #  based on only the scalable items, then it is not
   #  possible to zoom in on one of the non-scalable items
   #  that is outside of the boundary of the scalable items.
   #
   #  So expand the [bbox] of scaled items until it is
   #  larger than [bbox all], but do so uniformly.
   #--------------------------------------------------------
   foreach {ax0 ay0 ax1 ay1} [$canvas bbox all] {break}

   while { ($ax0<$x0) || ($ay0<$y0) || ($ax1>$x1) || ($ay1>$y1) } {
      # triple the scalable area size
      set x0 [expr {$x0-$xlength}]
      set x1 [expr {$x1+$xlength}]
      set y0 [expr {$y0-$ylength}]
      set y1 [expr {$y1+$ylength}]
      set xlength [expr {$xlength*3.0}]
      set ylength [expr {$ylength*3.0}]
   }

   #--------------------------------------------------------
   #  Now that we've finally got a region defined with
   #  the proper aspect ratio (of only the scalable items)
   #  but large enough to include all items, we can compute
   #  the xview/yview fractions and set our new viewport
   #  correctly.
   #--------------------------------------------------------
   set newxleft [expr {($xcenter-$x0-($winxlength/2.0))/$xlength}]
   set newytop  [expr {($ycenter-$y0-($winylength/2.0))/$ylength}]
   $canvas configure -scrollregion [list $x0 $y0 $x1 $y1]
   $canvas xview moveto $newxleft
   $canvas yview moveto $newytop

   #--------------------------------------------------------
   #  Change the scroll region one last time, to fit the
   #  items on the canvas.
   #--------------------------------------------------------
   $canvas configure -scrollregion [$canvas bbox all]
}

##############################################################################
#
# NETWORK GRAPH UTILITIES
#
##############################################################################

# provide back color based on port speed / speed
proc portColor {port} {
   set width [IBPort_width_get $port]
   set speed [IBPort_speed_get $port]

   set color [getColor $width${speed}G]

   return $color
}

proc LoadAnnotationsFile {} {
   global ANNOTATIONS
   global ANNOTATION_FILE P

   if {![info exists ANNOTATION_FILE]} {
      return
   }
   if {![file readable $ANNOTATION_FILE]} {
      return
   }

   set f [open $ANNOTATION_FILE r]

   if {[info exists ANNOTATIONS]} {unset ANNOTATIONS}

   while {[gets $f sLine] >= 0} {
      # TODO: Support not only sysPort annotations
      if {![regexp {(\S+)\s+(.+)} $sLine d1 name anno]} {
         puts "-W- Skipping annotation file line:$sLine"
         continue
      }
      set ANNOTATIONS(sysport:$name) $anno
   }
}

proc DrawAnnotationFromFile {} {
   global ANNOTATIONS
   global C gFabric

   # clear all annotations
   $C delete withtag anno

   # TODO: Support not only sysPort annotations
   foreach e [array names ANNOTATIONS sysport:*] {
      set sysPortName [string range $e [string length sysport:] end]
      set anno $ANNOTATIONS($e)

      # find the sys port
      set sysPort [findSysPortByName $sysPortName]
      if {$sysPort == ""} {
         puts "-W- failed to find sys port:$sysPortName"
         continue
      }

      set sysName [IBSystem_name_get [IBSysPort_p_system_get $sysPort]]
      set portName [IBSysPort_name_get $sysPort]
      # get the items of this port
      set items [$C find withtag ${portName}&&sysport&&of:$sysName]
      if {[llength $items] == 0} {
         puts "-W- No items for sys port:$sysPortName"
         continue
      }

      set bbox [$C bbox $items]
      set outCoords [bboxCenter $bbox [expr rand()*0.95]]
      $C create text $outCoords -tags anno -fill red \
         -text $anno
      puts "-I- Annotated $sysPortName with $anno"
   }
}


# draw a single node
proc drawNode {node graph} {
   global NODE
   global IB_CA_NODE
   set nodeName [IBNode_name_get $node]

   if {[regexp {^node:(.*)} $nodeName d1 n]} {
      set nodeName "0x$n"
   }

   set nodeLabel "\{$nodeName|"
   set numPorts [IBNode_numPorts_get $node]
   switch  $numPorts {
      1 {append nodeLabel "{<f1> P1}\}"}
      2 {append nodeLabel "{<f1> P1|<f2> P2}\}"}
      8 {
         append nodeLabel "{<f1> P1|<f2> P2|<f3> P3|<f4> P4}|"
         append nodeLabel "{<f5> P5|<f6> P6|<f7> P7|<f8> P8}\}"
      }
      24 {
         append nodeLabel "{<f1> P1|<f2> P2|<f3> P3|<f4> P4}|"
         append nodeLabel "{<f5> P5|<f6> P6|<f7> P7|<f8> P8}|"
         append nodeLabel "{<f9> P9|<f10> P10|<f11> P11|<f12> P12}|"
         append nodeLabel "{<f13> P13|<f14> P14|<f15> P15|<f16> P16}|"
         append nodeLabel "{<f17> P17|<f18> P18|<f19> P19|<f20> P20}|"
         append nodeLabel "{<f21> P21|<f22> P22|<f23> P23|<f24> P24}\}"
      }
      default {
         puts "-E- Fail to handle $nodeName with $numPorts ports"
      }
   }
   set NODE($node) \
      [$graph addnode $nodeName shape record \
          fontsize 7 label $nodeLabel \
          fillcolor lightblue2 style filled \
         ]

   if {[IBNode_type_get $node] == $IB_CA_NODE} {
      $NODE($node) setattributes fillcolor lightgrey
   }
}

proc drawSystem {sys graph} {
   global SYSTEM
   global SYS_PORT_IDX_BY_NAME
   global EXPAND_SYSTEMS
   global ANNOTATIONS

   # puts "-I- Drawing system $sys"
   set sysName [IBSystem_name_get $sys]

   # remove extra "system" from auto systems
   if {[regexp {^system:(.*)} $sysName d1 n]} {
      set sysName "0x$n"
   }

   # the system might be expanded
   if {[info exists EXPAND_SYSTEMS($sysName)]} {
      set subGraph [$graph addsubgraph \
                       cluster_$sysName label $sysName labelfontsize 7 \
                       bgcolor wheat color black]
      foreach nameNNode [IBSystem_NodeByName_get $sys] {
         set node [lindex $nameNNode 1]
         drawNode $node $subGraph
      }
      return
   }

   set sysLabel "\{$sysName|"

   # we only draw system ports that are connected
   set connSysPorts {}
   set prevPrefix "-"
   set first 1
   set sysPortIdx 0
   set numInLine 0
   set sysPorts [IBSystem_PortByName_get $sys]
   foreach sysNameNPort [lsort -dictionary -index 0 $sysPorts] {
      foreach {portName sysPort} $sysNameNPort {break}

      set fullName "$sysName/$portName"
      set isAnnotated [info exists ANNOTATIONS(sysport:$fullName)]
      set remSysPort [IBSysPort_p_remoteSysPort_get $sysPort]
      if {$isAnnotated == 0 && $remSysPort == ""} {continue}

      # we use heuristic to know when to break the ports line
      if {![regexp {(.*)/[^/]+$} $portName d1 prefix]} {
         set prefix ""
      }

      if {$prefix != $prevPrefix || $numInLine == 6} {
         set numInLine 0
         if {$first} {
            append sysLabel "\{"
            set first 0
         } else {
            append sysLabel "\}|\{"
         }
         set prevPrefix $prefix
      } else {
         if {$first == 0} {
            append sysLabel "|"
         } else {
            set first 0
         }
      }
      incr numInLine
      append sysLabel "<f$sysPortIdx> $portName"
      set SYS_PORT_IDX_BY_NAME($sys,$portName) $sysPortIdx
      incr sysPortIdx
   }
   if {$first == 0} {
      append sysLabel "\}\}"
   } else {
      append sysLabel "\}"
   }

   if {[regexp {^S[0-9a-fA-F]+$} $sysName]} {
      set fillColor lightgrey
   } else {
      set fillColor lightyellow
   }

   global SYSTEM_ORDER
   if {[info exist SYSTEM_ORDER] && [lsearch $SYSTEM_ORDER $sysName] >= 0} {
      set SYSTEM($sys) \
         [$graph addnode $sysName shape record \
             fontsize 7 label $sysLabel labelfontcolor red \
             fillcolor $fillColor style filled \
             pos 10,10 ]
   } else {
      set SYSTEM($sys) \
         [$graph addnode $sysName shape record \
             fontsize 7 label $sysLabel labelfontcolor red \
             fillcolor $fillColor style filled \
            ]
   }
}

# draw a single node connections
proc drawNodeConns {node graph} {
   global SYS_PORT_IDX_BY_NAME
   global NODE SYSTEM
   global EXPAND_SYSTEMS
   global CONN

   #  puts "-V- Drawing connections of node:[IBNode_name_get $node]"
   set sys [IBNode_p_system_get $node]
   set sysName [IBSystem_name_get $sys]
   set isExpanded [info exists EXPAND_SYSTEMS($sysName)]
   for {set pn 1} {$pn <= [IBNode_numPorts_get $node]} {incr pn} {
      set port [IBNode_getPort $node $pn]
      if {$port == ""} {continue}
      set portName [IBPort_getName $port]

      set remPort [IBPort_p_remotePort_get $port]
      if {$remPort == ""} {continue}

      set remPortName [IBPort_getName $remPort]
      if {[info exists CONN($remPortName)] } {continue}

      set toNode [IBPort_p_node_get $remPort]
      set toPortNum [IBPort_num_get $remPort]
      set toSys  [IBNode_p_system_get $toNode]
      set toSysName [IBSystem_name_get $toSys]

      # we can skip connections within same system if it
      # is not expanded
      if {($sys == $toSys) && !$isExpanded} {continue}

      # now we need to figure out if we are connecting
      # system ports or not
      set sysPort [IBPort_p_sysPort_get $port]
      if {$sysPort == "" || $isExpanded} {
         set isDrawn [info exists NODE($node)]
         if {$isDrawn == 0} {continue}
         set fromRec $NODE($node)
         set fromPort f$pn
      } else {
         if {![info exists SYSTEM($sys)]} {
            puts "-W- System $sys is not drawn???"
            continue
         }
         set fromRec $SYSTEM($sys)
         set fromPortName [IBSysPort_name_get $sysPort]
         if {![info exists SYS_PORT_IDX_BY_NAME($sys,$fromPortName)]} {
            puts "-W- System $sys port $fromPortName is not drawn???"
            continue
         }
         set fromPort "f$SYS_PORT_IDX_BY_NAME($sys,$fromPortName)"
      }

      set remSysPort [IBPort_p_sysPort_get $remPort]
      set isRemExpanded [info exists EXPAND_SYSTEMS($toSysName)]
      if {$remSysPort == "" || $isRemExpanded} {
         set toRec $NODE($toNode)
         set toPort f$toPortNum
      } else {
         set toRec $SYSTEM($toSys)
         set toPortName [IBSysPort_name_get $remSysPort]
         set toPort "f$SYS_PORT_IDX_BY_NAME($toSys,$toPortName)"
      }

      #     puts  "-V- Connecting from:$fromRec / $fromPort -> $toRec / $toPort ... "
      set conn \
         [$graph addedge "$toRec" "$fromRec" \
             tailport $toPort headport $fromPort \
             arrowhead normal arrowtail normal \
            ]

      set CONN($portName) $conn

      # use coloring for link speed/width
      $conn setattributes color [portColor $port]
   }
}

# process the code generated by graphviz
proc tagGraphVizCode {fabric code} {
   global NODE SYSTEM
   set newCode {}

   # We scan through the code for text and on the first
   # appearence of a node tag. Then try matching against known
   # systems and nodes
   set newCode ""
   set numSystems 0
   set numNodes 0
   set numPorts 0
   set prevNode ""
   foreach sLine [split $code "\n"] {
      if {[regexp {^(.*-text.*-tags\s+)(.*graph.*)} $sLine d1 pf tags]} {
         append newCode "$pf {$tags system}\n"
         incr numSystems
      } elseif {[regexp {^(.*-text\s+(\S+).*-tags\s+)(.*node.*)} \
                    $sLine d1 pf txt tags]} {
         # we can be on a new node -
         if {$prevNode != $tags} {
            # new node tag is it a system or node?
            set sys [IBFabric_getSystem $fabric $txt]
            if {$sys != ""} {
               #              puts "-V- TAGS: new sys $tags txt:$txt"
               # a system
               append newCode "$pf {$tags system}\n"
               incr numSystems
               set portTagType sysport
               set parent $txt
            } else {
               #              puts "-V- TAGS: new node $tags txt:$txt"
               set portTagType port
               append newCode "$pf {$tags node}\n"
               incr numNodes
               set parent $txt
            }
            set prevNode $tags
         } else {
            #           puts "-V- TAGS: new $portTagType $tags txt:$txt"

            # it must be a port
            append newCode "$pf {$tags $portTagType of:$parent}\n"
            incr numPorts
         }
      } else {
         append newCode "$sLine\n"
      }
   }
   # avoid the disabling of the widgets
   regsub -all -- {-disabled} $newCode {} newCode
   puts "-I- Marked $numSystems systems $numNodes nodes $numPorts ports"
   return $newCode
}

# create selection box for each object type and assign bindings
proc bindMenusToTags {c} {

   set objNHdl {
      system  showSysMenu
      node    showNodeMenu
      port    showPortMenu
      sysport showSysPortMenu
   }

   foreach {type hdlFunc} $objNHdl {
      foreach item [$c find withtag $type] {
         foreach {x0 y0 x1 y1} [$c bbox $item] {break}
         set dy [expr $y1 - $y0]
         if {[catch {set name [$c itemcget $item -text]}]} {continue}
         set tags [$c itemcget $item -tags]
         $c addtag $name withtag $item
         lappend tags name:$name
         lappend tags ${type}Handle
         set handleItem [$c create rectangle $x0 \
                            [expr $y0 - $dy] $x1 [expr $y1 + $dy] \
                            -outline {} -tags $tags]

         $c bind $handleItem <1> [list $hdlFunc %W %x %y]
      }
   }
}

# provide a system list in the order stored by system names
# return a list of {name id} pairs
proc getSysList {fabric} {
   global SYSTEM_ORDER

   # first get all the systems sorted by name
   set sysList {}
   set nameList {}
   if {[info exists SYSTEM_ORDER]} {
      foreach sysName $SYSTEM_ORDER {
         set sys [IBFabric_getSystem $fabric $sysName]
         if {$sys != ""} {
            puts "-I- Adding root $sysName"
            lappend sysList [list $sysName $sys]
            lappend nameList $sysName
         }
      }
   }

   # now build the name list not including the
   foreach nameNSys [lsort -index 0 [IBFabric_SystemByName_get $fabric]] {
      set name [lindex $nameNSys 0]
      set sys  [lindex $nameNSys 1]
      if {[lsearch -exact $nameList $name] < 0} {
         lappend sysList [list $name $sys]
         lappend nameList $name
      }
   }
   return $sysList
}

# take a canvans and a fabric and draw the fabric on the canvas
proc drawFabric {fabric c} {
   global NODE SYSTEM CONN SYS_PORT_IDX_BY_NAME
   global EXPAND_SYSTEMS

   foreach g {CONN NODE SYSTEM SYS_PORT_IDX_BY_NAME} {
      if {[info exists $g]} {
         unset $g
      }
   }

   # cleanup the canvas
   $c delete all

   #   set graph [dotnew graph mode hier rankdir TB fontsize 7 \
      #                 ranksep equaly labelfontsize 7 size 300,300]
   set cbg [option get $c background *]
   set graph [dotnew graph mode hier fontsize 7 \
                 ranksep equaly labelfontsize 7 bgcolor $cbg]

   # we add each system as a subgraph and then
   foreach nameNSys [getSysList $fabric] {
      set sys [lindex $nameNSys 1]
      drawSystem $sys $graph
   }

   # go over all nodes and connect them
   foreach nameNNode [IBFabric_NodeByName_get $fabric] {
      set node [lindex $nameNNode 1]
      drawNodeConns $node $graph
   }

   SetStatus "-I- Calculating graph layout ..."
   $graph layout NEATO
   SetStatus "-I- Packing graph ..."
   set code [$graph render]
   SetStatus "-I- Packing graph ... done"

   set newCode [tagGraphVizCode $fabric $code]
   eval $newCode
   bindMenusToTags $c

   # fit the canvas
   # fit $c
}

#assume there is a name:* tag in teh list return the name
proc getNameTag {tags} {
   set idx [lsearch -glob $tags name:*]
   if {$idx < 0} {
      return ""
   }
   return [string range [lindex $tags $idx] 5 end]
}

proc getOfTag {tags} {
   set idx [lsearch -glob $tags of:*]
   if {$idx < 0} {
      return ""
   }
   return [string range [lindex $tags $idx] 3 end]
}

# set the EXPANDED for the system under the cursor and
# call redraw
proc expand {c x y} {
   global EXPAND_SYSTEMS
   global gFabric C
   set tags [$c itemcget current -tags]
   if {[llength $tags] == 0} {return}
   set sysName [getNameTag $tags]

   SetStatus "-I- Expanding System: $sysName ..."
   puts "-I- Expanding System: $sysName ..."

   set EXPAND_SYSTEMS($sysName) 1

   after 100 drawFabric $gFabric $C
}

# set the EXPANDED for the system under the cursor and
# call redraw
proc deExpand {c x y} {
   global EXPAND_SYSTEMS
   global gFabric C
   set tags [$c itemcget current -tags]
   if {[llength $tags] == 0} {return}
   set sysName [getNameTag $tags]

   SetStatus "-I- De-Expanding System: $sysName ..."
   puts "-I- De-Expanding System: $sysName ..."

   if {[info exists EXPAND_SYSTEMS($sysName)]} {
      unset EXPAND_SYSTEMS($sysName)
   }

   after 100 drawFabric $gFabric $C
}

proc showSysMenu {c x y} {
   global gFabric
   set tags [$c itemcget current -tags]
   set sysName [getNameTag $tags]
   puts "System: $sysName"
   # find the port
   set sys [IBFabric_getSystem $gFabric $sysName]
   if {$sys == ""} {
      puts "-E- fail to find system $sysName in the fabric"
      return
   }
   PropsUpdate system $sys
}

proc showNodeMenu {c x y} {
   global gFabric
   set tags [$c itemcget current -tags]
   set nodeName [getNameTag $tags]
   # add node: if guid:
   if {[regexp {0x([0-9a-fA-F]{16})} $nodeName d1 n]} {
      set nodeName "node:$n"
   }
   puts "Node: $nodeName"

   set node [IBFabric_getNode $gFabric $nodeName]
   if {$node == ""} {
      puts "-E- fail to find node $nodeName in the fabric"
      puts "    [IBFabric_NodeByName_get $gFabric]"
      return
   }

   PropsUpdate node $node
}

proc showPortMenu {c x y} {
   global gPort gFabric
   set tags [$c itemcget current -tags]
   #  puts "-V- $tags"
   set ntag [getOfTag $tags]
   set node [$c find withtag ${ntag}&&node ]
   set nodeName [$c itemcget $node -text]
   set portName [getNameTag $tags]
   puts "Port: $nodeName $portName"

   # find the port
   set node [IBFabric_getNode $gFabric $nodeName]
   if {$node == ""} {
      puts "-E- fail to find node $nodeName in the fabric"
      puts "    [IBFabric_NodeByName_get $gFabric]"
      return
   }

   regexp {[0-9]+} $portName portNum
   set port [IBNode_getPort $node $portNum]
   if {$port == ""} {
      puts "-E- fail to find port $nodeName/$portName in the fabric"
      return
   }

   PropsUpdate port $port
}

proc showSysPortMenu {c x y} {
   global gPort gFabric
   set tags [$c itemcget current -tags]
   set ntag [getOfTag $tags]
   set systag [$c find withtag ${ntag}&&system ]
   set sysName [$c itemcget $systag -text]
   # add node: if guid:
   if {[regexp {0x([0-9a-fA-F]{16})} $sysName d1 n]} {
      set nodeName "system:$n"
   }
   set portName [getNameTag $tags]
   puts "SysPort: $sysName $portName"

   # find the port
   set sys [IBFabric_getSystem $gFabric $sysName]
   if {$sys == ""} {
      puts "-E- fail to find system $sysName in the fabric"
      return
   }

   set sysPort [IBSystem_getSysPort $sys $portName]
   if {$sysPort == ""} {
      puts "-E- fail to find system port $sysName/$portName in the fabric"
      return
   }

   PropsUpdate sysport $sysPort
}

# Perform the fabric update based on the availability of a topology
# and the LST file
proc GraphUpdate {lstFile} {
   global G
   global gTopoFabric
   global gLstFabric
   global gFabric
   global C

   # cleanup
   foreach fType {gFabric gTopoFabric gLstFabric} {
      if {[info exists $fType]} {
         delete_IBFabric [set $fType]
      }
   }

   set gFabric [new_IBFabric]

   if {![info exists G(argv:topo.file)]} {
      puts "-I- Parsing subnet lst: $lstFile"
      IBFabric_parseSubnetLinks $gFabric $lstFile
   } else {
      # load the topo
      set gTopoFabric [new_IBFabric]
      IBFabric_parseTopology $gTopoFabric $G(argv:topo.file)

      # load the lst
      set gLstFabric [new_IBFabric]
      IBFabric_parseSubnetLinks $gLstFabric $lstFile

      # compare and merge
      set m [ibdmMatchFabrics $gTopoFabric $gLstFabric \
                $G(argv:sys.name) $G(argv:port.num) $G(data:root.port.guid)]
      puts $m

      ibdmBuildMergedFabric $gTopoFabric $gLstFabric $gFabric
      puts "-I- Topo merged"
   }

   drawFabric $gFabric $C
}

# clear all highlights
proc guiClearAllMarking {} {
   global C

   set items [$C find withtag mark]
   puts "-I- Clearing mark on $items"
   foreach item $items {
      if {[llength [$C gettags $item]] == 1} {
         $C delete $item
      } else {
         $C dtag $item mark
         $C itemconfigure $item -fill black -activefill black
      }
   }
}

proc SetStatus {msg} {
   global S O StatusLine
   $S configure -state normal
   set StatusLine $msg
   $S configure -state readonly
   set color $O(color:txtDef)
   if {[regexp {^-([WEI])-} $msg d1 type]} {
      switch $type {
         E {set color $O(color:txtErr)}
         W {set color $O(color:txtWarn)}
         I {set color $O(color:txtInfo)}
      }
   }
   $S configure -foreground [lindex $color 2]
   update
}

# zoom to object by ibdm id
proc zoomToObjByIbdmId {type obj} {
   global C

   switch $type {
      system {
         set name [IBSystem_name_get $obj]
         set items [$C find withtag ${name}&&system]
      }
      node {
         set name [IBNode_name_get $obj]
         set items [$C find withtag ${name}&&node]
      }
      sysport {
         set sys [IBSysPort_p_system_get $obj]
         set sysName [IBSystem_name_get $sys]
         set name [IBSysPort_name_get $obj]
         set items [$C find withtag ${name}&&sysport&&of:$sysName]
      }
      port {
         set node [IBPort_p_node_get $obj]
         set nodeName [IBNode_name_get $node]
         set name "P[IBPort_num_get $obj]"
         set items [$C find withtag ${name}&&port&&of:$nodeName]
      }
   }
   if {[llength $items]} {
      set bbox [$C bbox $items]
      set xy [bboxCenter $bbox]
      zoom $C 1.0 [lindex $xy 0] [lindex $xy 1]
      puts "-I- Zooming on $bbox"
   } else {
      puts "-I- No items for $type $obj"
   }
}

# find and highlight a system by name
proc guiHighLightByName {objType name} {
   global gFabric C
   set items ""
   switch $objType {
      system {
         set sys [IBFabric_getSystem $gFabric $name]
         if {$sys == ""} {
            SetStatus "-W- Fail to find system by name:$name"
            return
         }
         PropsUpdate system $sys

         set items [$C find withtag ${name}&&system]
      }
      sysport {
         # we need to try each hier sep
         set sysName ""
         set sys ""
         set subNames [split $name /]

         while {[llength $subNames]} {
            set n [lindex $subNames 0]
            set subNames [lrange $subNames 1 end]
            if {$sysName != ""} { append sysName / }
            append sysName $n
            set sys [IBFabric_getSystem $gFabric $sysName]
            if {$sys != ""} { break }
         }

         if {$sys == ""} {
            SetStatus "-W- Fail to find system for port by name:\"$name\""
            return
         }

         set portName [join $subNames /]
         set sysPort [IBSystem_getSysPort $sys $portName]
         if {$sysPort == ""} {
            SetStatus "-W- Fail to find system port by name:\"$name\""
            return
         }
         PropsUpdate sysport $sysPort
         set items [$C find withtag ${portName}&&sysport&&of:$sysName]
      }
      node {
         set node [IBFabric_getNode $gFabric $name]
         if {$node == ""} {
            SetStatus "-W- Fail to find node by name:$name"
            return
         }
         PropsUpdate node $node
         set items [$C find withtag ${name}&&node]
         # we might need to look for a system...
         if {[llength $items] == 0} {
            set sys [IBNode_p_system_get $node]
            set sysName [IBSystem_name_get $sys]
            return [guiHighLightByName system $sysName]
         }
      }
      port {
         if {![regexp {(.*)/P([0-9]+)} $name d1 nodeName portNum]} {
            SetStatus "-W- Fail to find node for port by name:\"$name\""
            return
         }
         set node [IBFabric_getNode $gFabric $nodeName]
         if {$node == ""} {
            SetStatus "-W- Fail to find node for port by name:\"$name\""
            return
         }

         set port [IBNode_getPort $node $portNum]
         if {$port == ""} {
            SetStatus "-W- Fail to find port by name:\"$name\""
            return
         }
         PropsUpdate port $port
         set portName "P$portNum"

         set items [$C find withtag ${portName}&&port&&of:$nodeName]

         if {[llength $items] == 0} {
            set sysPort [IBPort_p_sysPort_get $port]
            set sys [IBNode_p_system_get $node]
            set sysName [IBSystem_name_get $sys]
            if {$sysPort == ""} {
               # it is internal - just highlight the sys
               return [guiHighLightByName system $sysName]
            } else {
               set sysPortName "$sysName/[IBSysPort_name_get $sysPort]"
               return [guiHighLightByName sysport $sysPortName]
            }
         }

      }
   }

   if {![llength $items]} {
      SetStatus "-W- Fail to find any displayed obejct for $objType name:\"$name\""
      return
   }

   set bbox [$C bbox $items]
   zoom $C 1.0 [lindex $bbox 0] [lindex $bbox 1]
   foreach item $items {
      $C itemconfigure $item -fill [getColor mark] -activefill [getColor mark]
      $C addtag mark withtag $item
   }

   return $items
}

# find and highlight a system by name
proc guiHighLightByGuid {objType guid} {
   global gFabric C

   # we try getting by system/node/port
   set sys [IBFabric_getSystemByGuid $gFabric $guid]
   set node [IBFabric_getNodeByGuid $gFabric $guid]
   set port [IBFabric_getPortByGuid $gFabric $guid]

   switch $objType {
      system {
         if {$sys != ""} {
            set name [IBSystem_name_get $sys]
         } elseif {$node != ""} {
            set sys [IBNode_p_system_get $node]
            set name [IBSystem_name_get $sys]
         } elseif {$port != ""} {
            set node [IBPort_p_node_get $port]
            set sys [IBNode_p_system_get $node]
            set name [IBSystem_name_get $sys]
         } else {
            SetStatus "-W- Fail to find system by guid:$guid"
            return
         }

         set obj $sys
         set items [$C find withtag ${name}&&system]
      }
      node {
         if {$node != ""} {
            set name [IBNode_name_get $node]
         } elseif {$port != ""} {
            set node [IBPort_p_node_get $port]
            set name [IBNode_name_get $node]
         } else {
            SetStatus "-W- Fail to find node by guid:$guid"
            return
         }
         set obj $node
         set items [$C find withtag ${name}&&node]
      }
      port {
         if {$port == ""} {
            SetStatus "-W- Fail to find port by guid:$guid"
            return
         }
         set obj $port

         set nodeName [IBNode_name_get [IBPort_p_node_get $port]]
         set name "P[IBPort_num_get $port]"
         set items [$C find withtag ${name}&&port&&of:$nodeName]
      }
      sysport {
         if {$port == ""} {
            SetStatus "-W- Fail to find system port by guid:$guid"
            return
         }

         set sysPort [IBPort_p_sysPort_get $port]
         if {$sysPort == ""} {
            SetStatus "-W- Fail to find system port for port with guid:$guid"
            return
         }

         set sys [IBSysPort_p_system_get $sysPort]
         set sysName [IBSystem_name_get $sys]
         set name [IBSysPort_name_get $sysPort]
         set obj $sysPort
         set items [$C find withtag ${name}&&sysport&&of:$sysName]
      }
   }

   if {![llength $items]} {
      SetStatus "-W- Fail to find any displayed obejct for $objType name:\"$name\""
      return
   }

   PropsUpdate $objType $obj
   set bbox [$C bbox $items]
   zoom $C 1.0 [lindex $bbox 0] [lindex $bbox 1]
   foreach item $items {
      $C itemconfigure $item -fill [getColor mark] -activefill [getColor mark]
      $C addtag mark withtag $item
   }
   return $items
}

# find and highlight a system by name
proc guiHighLightByLid {objType lid} {
   global gFabric C

   # we try getting port by lid:
   set port [IBFabric_getPortByLid $gFabric $lid]
   if {$port == ""} {
      SetStatus "-W- Fail to find port by lid:$lid"
      return
   }

   switch $objType {
      system {
         set node [IBPort_p_node_get $port]
         set sys [IBNode_p_system_get $node]
         set name [IBSystem_name_get $sys]
         set items [$C find withtag ${name}&&system]
      }
      node {
         set node [IBPort_p_node_get $port]
         set name [IBNode_name_get $node]
         set items [$C find withtag ${name}&&node]
      }
      port {
         set nodeName [IBNode_name_get [IBPort_p_node_get $port]]
         set name "P[IBPort_num_get $port]"
         set items [$C find withtag ${name}&&port&&of:$nodeName]
      }
      sysport {
         set sysPort [IBPort_p_sysPort_get $port]
         if {$sysPort == ""} {
            SetStatus "-W- Fail to find system port for port with lid:$lid"
            return
         }

         set sys [IBSysPort_p_system_get $sysPort]
         set sysName [IBSystem_name_get $sys]
         set name [IBSysPort_name_get $sysPort]
         set items [$C find withtag ${name}&&sysport&&of:$sysName]
      }
   }

   if {![llength $items]} {
      SetStatus "-W- Fail to find any displayed obejct for $objType lid:$lid"
      return
   }

   set bbox [$C bbox $items]
   zoom $C 1.0 [lindex $bbox 0] [lindex $bbox 1]
   foreach item $items {
      $C itemconfigure $item -fill [getColor mark] -activefill [getColor mark]
      $C addtag mark withtag $item
   }
   return $items
}

# return a sys port if exists
proc findSysPortByName {name} {
   global gFabric
   # we need to try each hier sep
   set sysName ""
   set sys ""
   set subNames [split $name /]
   set sysPort ""
   while {[llength $subNames]} {
      set n [lindex $subNames 0]
      set subNames [lrange $subNames 1 end]
      if {$sysName != ""} { append sysName / }
      append sysName $n
      set sys [IBFabric_getSystem $gFabric $sysName]
      if {$sys != ""} { break }
   }

   if {$sys != ""} {
      set portName [join $subNames /]
      set sysPort [IBSystem_getSysPort $sys $portName]
   }
   return $sysPort
}

# simpler as we know the node ports end with P[0-9]+
proc findPortByName {name} {
   global gFabric

   if {![regexp {(.*)/P([0-9]+)$} $name d1 nodeName portNum]} {
      return ""
   }

   set node [IBFabric_getNode $gFabric $nodeName]
   if {$node == ""} {
      return ""
   }

   return [IBNode_getPort $node $portNum]
}

proc bboxCenter {bbox {xScale 0.5} {yScale 0.5}} {
   foreach {x0 y0 x1 y1} $bbox {break}
   return [list [expr ($x0*(1-$xScale) + $x1*$xScale)] \
              [expr ($y0*(1-$yScale) + $y1*$yScale)] ]
}

# highlight all objects accross the directed route
proc guiHighLightByDR {startPort route} {
   global C
   # first we try to get the given start port
   set allItems {}

   set sysPort [findSysPortByName $startPort]
   if {$sysPort == ""} {
      # try to get a port by that name
      set port [findPortByName $startPort]
      if {$port == ""} {
         SetStatus "-W- Fail to find system port or port with name:\"$startPort\""
         return
      }
   } else {
      set port [IBSysPort_p_nodePort_get $sysPort]
   }

   # need to traverse from that port/sysport
   # if the given path is made of [] we need to convert hex to dec
   if {[regexp {^\s*([[][0-9a-fA-F][]])+\s*$} $route]} {
      set dr {}
      foreach h [split $route {[]}] {
         if {$h != ""} {
            lappend dr [expr 0x$h]
         }
      }
   } else {
      set dr [split $route ", "]
   }

   if {[lindex $dr 0] == 0} {
      set dr [lrange $dr 1 end]
   }

   # traverse the path
   set hop 0
   foreach p $dr {
      set items ""
      set node [IBPort_p_node_get $port]
      set outPort [IBNode_getPort $node $p]
      if {$outPort == ""} {
         SetStatus "-W- Got dead end on path at node:\"[IBNode_name_get $node]\" port:$p\""
         break
      }

      # highlight outgoing port and sysport
      set nodeName [IBNode_name_get [IBPort_p_node_get $outPort]]
      set name "P[IBPort_num_get $outPort]"
      set iItems [$C find withtag ${name}&&port&&of:$nodeName]
      set allItems [concat $allItems $iItems]

      set sysPort [IBPort_p_sysPort_get $outPort]
      if {$sysPort != ""} {
         set sys [IBSysPort_p_system_get $sysPort]
         set sysName [IBSystem_name_get $sys]
         set name [IBSysPort_name_get $sysPort]
         set items [$C find withtag ${name}&&sysport&&of:$sysName]
         set iItems [concat $iItems $items]
      }
      set allItems [concat $allItems $iItems]

      if {[llength $iItems]} {
         set outCoords [bboxCenter [$C bbox [lindex $iItems 0]] [expr rand()*0.95]]
      }

      set port [IBPort_p_remotePort_get $outPort]
      if {$port == ""} {
         SetStatus "-W- No remote port on path at node:\"[IBNode_name_get $node]\" port:$p\""
         $C create text $outCoords -tags mark -fill [getColor mark] -text "DEAD END ($p)"
         break
      }

      # highlight input port and sysport
      set items ""
      set nodeName [IBNode_name_get [IBPort_p_node_get $port]]
      set name "P[IBPort_num_get $port]"
      set items [$C find withtag ${name}&&port&&of:$nodeName]
      set allItems [concat $allItems $items]
      set oItems $items

      set sysPort [IBPort_p_sysPort_get $port]
      if {$sysPort != ""} {
         set sys [IBSysPort_p_system_get $sysPort]
         set sysName [IBSystem_name_get $sys]
         set name [IBSysPort_name_get $sysPort]
         set items [$C find withtag ${name}&&sysport&&of:$sysName]
         set oItems [concat $oItems $items]
      }
      set allItems [concat $allItems $oItems]

      if {[llength $oItems]} {
         set inCoords [bboxCenter [$C bbox [lindex $oItems 0]] [expr rand()*0.95]]

         # create a marker
         $C create line [concat $outCoords $inCoords] \
            -tags mark -fill [getColor mark] -arrow last
         set x [expr ([lindex $outCoords 0] + [lindex $inCoords 0]) / 2.0]
         set y [expr ([lindex $outCoords 1] + [lindex $inCoords 1]) / 2.0]
         $C create text $x $y -anchor w -text $hop -tags mark -fill [getColor mtxt]
      }

      incr hop
   }

   foreach item $allItems {
      $C itemconfigure $item -fill [getColor mark] -activefill [getColor mark]
      $C addtag mark withtag $item
   }
   return $allItems
}

##############################################################################
#
# PROPS Widget Commands
#
##############################################################################
proc PropsUpdate {objType ibdmHandle {zoom 0}} {
   global P
   # prevents recursion loop
   global _PropsUpdate_inside
   if {$ibdmHandle == ""} {return}

   if {[info exists _PropsUpdate_inside] && $_PropsUpdate_inside} {return}
   set _PropsUpdate_inside 1

   if {$ibdmHandle == ""} { return }

   foreach c [winfo child $P] {
      pack forget $c
   }

   switch $objType {
      system  {PropsSystem  $ibdmHandle}
      node    {PropsNode    $ibdmHandle}
      port    {PropsPort    $ibdmHandle}
      sysport {PropsSysPort $ibdmHandle}
   }

   # zoom to that object
   if {$zoom} {
      zoomToObjByIbdmId $objType $ibdmHandle
   }
   set _PropsUpdate_inside 0
}

proc PropsSystem {sys} {
   global P PROPS
   set PROPS(sys,id)   $sys
   set PROPS(sys,name) [IBSystem_name_get $sys]
   set PROPS(sys,type) [IBSystem_type_get $sys]
   set PROPS(sys,guid) [IBSystem_guid_get $sys]
   set PROPS(sys,nodes,id) [IBSystem_NodeByName_get $sys]
   set PROPS(sys,nodes) [llength $PROPS(sys,nodes,id)]
   set b $PROPS(sys,nodes,menu)
   $b delete 0 end
   set i 0
   foreach nameNNode $PROPS(sys,nodes,id) {
      $b insert $i command -label [lindex $nameNNode 0] \
         -command "PropsUpdate node [lindex $nameNNode 1]"
      incr i
   }
   pack $P.sys -expand yes -fill x -anchor nw
}

proc PropsNode {node} {
   global P PROPS
   pack $P.node -expand yes -fill x -anchor nw
   set PROPS(node,id)     $node
   set PROPS(node,name)   [IBNode_name_get $node]
   set PROPS(node,guid)   [IBNode_guid_get $node]
   set PROPS(node,ports)  [IBNode_numPorts_get $node]
   set PROPS(node,dev)    [IBNode_devId_get $node]
   set PROPS(node,rev)    [IBNode_revId_get $node]
   set PROPS(node,vend)   [IBNode_vendId_get $node]
   set PROPS(node,sys,id) [IBNode_p_system_get $node]
   set PROPS(node,sys)    [IBSystem_name_get $PROPS(node,sys,id)]
   set PROPS(node,dr)     [getDrToNode $node]
   set b $PROPS(node,ports,menu)
   $b delete 0 end
   set i 0
   for {set pn 1} {$pn <= $PROPS(node,ports)} {incr pn} {
      set port [IBNode_getPort $node $pn]
      if {$port != ""} {
         $b insert $i command -label "P$pn" \
            -command "PropsUpdate port $port 1"
         incr i
      }
   }
}

proc PropsPort {port} {
   global P PROPS
   pack $P.port -expand yes -fill x -anchor nw
   set PROPS(port,id)    $port
   set PROPS(port,name)  [IBPort_getName $port]
   set PROPS(port,guid)  [IBPort_guid_get $port]
   set PROPS(port,lid)   [IBPort_base_lid_get $port]
   set PROPS(port,speed) [IBPort_speed_get $port]
   set PROPS(port,width) [IBPort_width_get $port]
   set node [IBPort_p_node_get $port]
   set PROPS(port,node,id) $node
   set PROPS(port,node)  [IBNode_name_get $node]
   set remPort [IBPort_p_remotePort_get $port]
   set PROPS(port,rem,id) $remPort
   if {$remPort != ""} {
      set PROPS(port,rem) [IBPort_getName $remPort]
   } else {
      set PROPS(port,rem) "NOT CONNECTED"
   }
   set sysPort [IBPort_p_sysPort_get $port]
   set PROPS(port,sysp,id) $sysPort
   if {$sysPort != ""} {
      set sys [IBSysPort_p_system_get $sysPort]
      set PROPS(port,sysp) \
         "[IBSystem_name_get $sys]/[IBSysPort_name_get $sysPort]"
   } else {
      set PROPS(port,sysp) "NONE"
   }
}

proc PropsSysPort {sysPort} {
   global P PROPS ANNOTATIONS
   pack $P.sysport -expand yes -fill x -anchor nw
   set PROPS(sysport,id)    $sysPort
   set PROPS(sysport,name)   [IBSysPort_name_get $sysPort]
   set PROPS(sysport,sys,id) [IBSysPort_p_system_get $sysPort]
   set PROPS(sysport,sys)    [IBSystem_name_get $PROPS(sysport,sys,id)]
   set port [IBSysPort_p_nodePort_get $sysPort]
   set PROPS(sysport,width) [IBPort_width_get $port]
   set PROPS(sysport,speed) [IBPort_speed_get $port]
   set node [IBPort_p_node_get $port]
   set PROPS(sysport,port,id) $port
   set PROPS(sysport,port) \
      "[IBNode_name_get $node]/P[IBPort_num_get $port]"
   set remSysPort [IBSysPort_p_remoteSysPort_get $sysPort]
   set PROPS(sysport,rem,id) $remSysPort
   if {$remSysPort != ""} {
      set remSys [IBSysPort_p_system_get $remSysPort]
      set PROPS(sysport,rem) \
         "[IBSystem_name_get $remSys]/[IBSysPort_name_get $remSysPort]"
   } else {
      set PROPS(sysport,rem) "NOT CONNECTED"
   }
   set fullName "$PROPS(sysport,sys)/$PROPS(sysport,name)"
   if {[info exists ANNOTATIONS(sysport:$fullName)]} {
      set PROPS(sysport,anno) $ANNOTATIONS(sysport:$fullName)
   } else {
      set PROPS(sysport,anno) ""
   }
}

# get a DR to a port by its ID
# BFS untill finding it ...
proc getDrToNode {targetNode} {
   global G
   global gFabric

   set startPort [IBFabric_getPortByGuid $gFabric $G(data:root.port.guid)]
   if {$startPort == ""} {
      puts "-E- Fail to find start port !"
      return -1
   }

   set Q [list [list [IBPort_p_node_get $startPort] "0"]]
   while {[llength $Q]} {
      set nodeNPath [lindex $Q 0]
      set Q [lreplace $Q 0 0]

      set node [lindex $nodeNPath 0]
      set path [lindex $nodeNPath 1]

      if {$node == $targetNode} {
         puts "-I- Found node [IBNode_name_get $targetNode] at path:$path"
         return $path
      }

      set VISITED($node) 1

      for {set pn 1} {$pn <= [IBNode_numPorts_get $node]} {incr pn} {
         set port [IBNode_getPort $node $pn]
         if {$port == ""} {continue}
         set remPort [IBPort_p_remotePort_get $port]
         if {$remPort == ""} {continue}
         set remNode [IBPort_p_node_get $remPort]
         if {[info exists VISITED($remNode)]} {continue}
         lappend Q [list $remNode "$path,$pn"]
      }
   }
   puts "-W- Failed to find node [IBNode_name_get $targetNode]"
   return -1
}

# select a port number gui
proc numSelector {maxNum title} {
   global numSelectorVal
   if {![winfo exists .num_select]} {
      set t [toplevel .num_select]
      wm withdraw $t
      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]
      label $t.l -text $title
      pack $t.l -side top -expand yes -fill x
      set o [tk_optionMenu $f.b numSelectorVal 1]
      for {set i 2} {$i < $maxNum} {incr i} {
         $o insert $i command -label $i \
            -command "global numSelectorVal; update;set numSelectorVal $i"
      }
      set numSelectorVal 1
      pack $f.b -side left -padx 2 -pady 2
      pack $f
   }
   wm title .num_select $title
   wm deiconify .num_select
   update
   tkwait variable numSelectorVal
   destroy .num_select
   return $numSelectorVal
}

# we rely on the current PROP for
proc setPortState {state {port 0}} {
   global PROPS

   if {$port == 0} {
      set port $PROPS(port,id)
   }

   set node [IBPort_p_node_get $port]

   set drPath [getDrToNode $node]
   if {$drPath == -1} {
      return
   }

   set portNum [IBPort_num_get $port]
   if {[catch {set res [exec ibportstate -D $drPath $portNum $state]} e]} {
      LogAppend "\n-E---------------------------------------------------\n$e"
   } else {
      LogAppend "\n-I---------------------------------------------------\n$res"
   }
}

proc setNodePortState {state} {
   global PROPS
   set node $PROPS(node,id)

   set drPath [getDrToNode $node]
   if {$drPath == -1} {
      return
   }

   set portNum [numSelector [IBNode_numPorts_get $node] \
                   "Select a port number"]
   if {$portNum == ""} { return }

   if {[catch {set res [exec ibportstate -D $drPath $portNum $state]} e]} {
      LogAppend "\n-E---------------------------------------------------\n$e"
   } else {
      LogAppend "\n-I---------------------------------------------------\n$res"
   }
}

proc setSysPortState {state} {
   global PROPS

   set sysPort $PROPS(sysport,id)
   set port [IBSysPort_p_nodePort_get $sysPort]
   setPortState $state $port
}

proc portCounters {op {port 0}} {
   global PROPS

   if {$port == 0} {
      set port $PROPS(port,id)
   }

   set lid [IBPort_base_lid_get $port]
   if {$lid == 0} {
   }
   set portNum [IBPort_num_get $port]
   if {$op == "clr"} {
      set opt -R
   } else {
      set opt ""
   }

   set cmd "perfquery $opt $lid $portNum"
   if {[catch {eval "set res \[exec $cmd\]"} e]} {
      LogAppend "\n-E---------------------------------------------------\n$cmd\n$e"
   } else {
      LogAppend "\n-I---------------------------------------------------\n$cmd\n$res"
   }
}

# when port counters are queries from Node
# NOTE: we can not rely on the existance of the port
proc nodePortCounters {op} {
   global PROPS
   set node $PROPS(node,id)

   set portNum [numSelector [IBNode_numPorts_get $node] \
                   "Select a port number"]
   if {$portNum == ""} { return }

   # find first port that match
   set port ""
   for {set pn 1} {$pn < [IBNode_numPorts_get $node]} {incr pn} {
      set port [IBNode_getPort $node $pn]
      if {$port != ""} {break}
   }
   if {$port == ""} {return}

   set lid [IBPort_base_lid_get $port]
   if {$op == "clr"} {
      set opt -R
   } else {
      set opt ""
   }

   if {[catch {eval "set res [exec perfquery $opt $lid $portNum]"} e]} {
      LogAppend "\n-E---------------------------------------------------\n$e"
   } else {
      LogAppend "\n-I---------------------------------------------------\n$res"
   }
}

proc sysPortCounters {op} {
   global PROPS

   set sysPort $PROPS(sysport,id)
   set port [IBSysPort_p_nodePort_get $sysPort]
   portCounters $op $port
}

##############################################################################
#
# LOG WIDGET COMMANDS
#
##############################################################################

# perform log analysis from the given index
proc LogAnalyze {{startIndex 0.0}} {
   global L
   set text [$L get $startIndex end]
   # loop through the text for sections:
   set startChar 0
   set numErrs 0
   set numWarnings 0
   set numInfos 0

   set rex "\n-(\[IWE\])-\[^\n\]*(\n\[^-\]\[^\n\]*)*"
   while {[regexp -start $startChar -indices -- $rex $text all type]} {
      set start [lindex $all 0]
      set type [string range $text [lindex $type 0] [lindex $type 1]]
      set sIdx [lindex $all 0]
      set eIdx [expr [lindex $all 1] + 1]
      switch $type {
         E { set tagName errors; incr numErrs }
         W { set tagName warnings; incr numWarnings }
         I { set tagName infos; incr numInfos }
      }
      $L tag add $tagName "$startIndex + $sIdx chars" "$startIndex + $eIdx chars"
      set startChar [lindex $all 1]
   }
   puts "-I- Found $numErrs errors $numWarnings warnings $numInfos infos"

   # Now scan for names guids and routes...
   set startChar 0
   set numNames 0

   set rex "\\s+\"(\[^0-9\]\[^\"\]+)\"\\s+"
   while {[regexp -start $startChar -indices -- $rex $text all name]} {
      set sIdx [lindex $name 0]
      set eIdx [expr [lindex $name 1] + 1]
      $L tag add NAME "$startIndex + $sIdx chars" "$startIndex + $eIdx chars"
      set startChar [lindex $all 1]
      incr numNames
   }

   set startChar 0
   set numLids 0
   set rex {(lid|LID)[\s:=]*(0x[0-9a-fA-F]+|[0-9]+)}
   while {[regexp -start $startChar -indices -- $rex $text all pre lid]} {
      set sIdx [lindex $lid 0]
      set eIdx [expr [lindex $lid 1] + 1]
      $L tag add LID "$startIndex + $sIdx chars" "$startIndex + $eIdx chars"
      set startChar [lindex $all 1]
      incr numLids
   }

   set startChar 0
   set numGuids 0
   set rex {[Gg][Uu][Ii][Dd]=*(0x[0-9a-fA-F]+)}
   while {[regexp -start $startChar -indices -- $rex $text all guid]} {
      set sIdx [lindex $guid 0]
      set eIdx [expr [lindex $guid 1] + 1]
      $L tag add GUID "$startIndex + $sIdx chars" "$startIndex + $eIdx chars"
      set startChar [lindex $all 1]
      incr numGuids
   }

   set startChar 0
   set numRoutes 0
   set rex {\"([0-9]+(,[0-9]+)*)\"}
   while {[regexp -start $startChar -indices -- $rex $text all route]} {
      set sIdx [lindex $route 0]
      set eIdx [expr [lindex $route 1] + 1]
      $L tag add ROUTE "$startIndex + $sIdx chars" "$startIndex + $eIdx chars"
      set startChar [lindex $all 1]
      incr numRoutes
   }

   puts "-I- Found $numNames names $numLids LIDS $numGuids GUIDs $numRoutes Directed-Routes"
}

proc LogUpdate {log} {
   global L

   #puts $log

   # Filter out any "discovring" message
   set nlog $log
   set discRex  "-I- Discovering the subnet ... \[0-9\]+ nodes .\[0-9\]+ Switches & \[0-9\]+ CA-s. discovered.\\s*\n"
   regsub -all -- $discRex $log "" nlog

   # perform the log area update
   $L configure -state normal
   $L delete 0.0 end
   $L insert 0.0 $nlog

   # Do some hypertexting
   LogAnalyze
   $L configure -state disabled
}

proc LogAppend {log {scrollToPos 1}} {
   global L
   set start [$L index end]
   $L configure -state normal
   $L insert end "$log\n"
   $L configure -state disabled
   if {$scrollToPos} {
      $L see end
   }
   LogAnalyze $start
   update
}

# an object tag was selected
proc LogObjSelect {log type w x y} {
   global G
   # get the tag text under the x y
   set startNEnd [$log tag prevrange $type @$x,$y]
   set val [$log get [lindex $startNEnd 0] [lindex $startNEnd 1]]
   switch $type {
      NAME {
         if {[guiHighLightByName port $val] != ""} {
            SetStatus "-I- Found Port $val"
         } elseif {[guiHighLightByName node $val] != ""} {
            SetStatus "-I- Found Node $val"
         } elseif {[guiHighLightByName system $val] != ""} {
            SetStatus "-I- Found System $val"
         } elseif {[guiHighLightByName sysport $val] != ""} {
            SetStatus "-I- Found System Port $val"
         } else {
            SetStatus "-I- Failed to find object with name $val"
         }
      }
      LID {
         set x [guiHighLightByLid sysport $val]
         set y [guiHighLightByLid port $val]
         if {$x != "" || $y != ""} {
            SetStatus "-I- Find by LID succeeded"
         }
      }
      ROUTE {
         guiHighLightByDR  "$G(argv:sys.name)/P$G(argv:port.num)" $val
      }
      GUID {
         set x [guiHighLightByGuid system $val]
         set y [guiHighLightByGuid sysport $val]
         if {$x != "" || $y != ""} {
            SetStatus "-I- Find by GUID succeeded"
         }
      }
   }
}

# initialize the props guid such that we have a pannel
# for each object type
proc initPropsGui {p} {
   global PROPS O

   set props {
      {sys SYSTEM
         {
            name Name ""
            type Type ""
            guid GUID ""
            nodes "\#Node" {PropsUpdate node $PROPS(sys,node,id) 1}
         }
      }
      {node NODE
         {
            name Name ""
            guid GUID ""
            dr   "Directed Route" ""
            sys  System     {PropsUpdate system $PROPS(node,sys,id) 1}
            ports "\#Ports" {PropsUpdate port  $PROPS(node,port,id) 1}
            dev "Device ID" ""
            rev "Revision ID" ""
            vend "Vendor ID"
         }
      }
      {port PORT
         {
            name Name ""
            guid GUID ""
            lid LID ""
            speed Speed ""
            width Width ""
            node Node    {PropsUpdate node    $PROPS(port,node,id) 1}
            rem Conn     {PropsUpdate port    $PROPS(port,rem,id)  1}
            sysp SysPort {PropsUpdate sysport $PROPS(port,sysp,id) 1}
         }
      }
      {sysport "FRONT PANEL PORT"
         {
            name Name ""
            sys System         {PropsUpdate system  $PROPS(sysport,sys,id)  1}
            port "Node Port"   {PropsUpdate port    $PROPS(sysport,port,id) 1}
            rem "Connected to" {PropsUpdate sysport $PROPS(sysport,rem,id)  1}
            width Width ""
            speed Speed ""
            anno Annotation ""
         }
      }
   }

   set cmds {
      {sys}
      {node
         {{UP "setNodePortState enable"} {DOWN "setNodePortState disable"}}
         {{"PM Get" "nodePortCounters get"} {"PM Clr" "nodePortCounters clr"}}
      }
      {port
         {{UP "setPortState enable"} {DOWN "setPortState disable"}}
         {{"PM Get" "portCounters get"} {"PM Clr" "portCounters clr"}}
      }
      {sysport
         {{UP "setSysPortState enable"} {DOWN "setSysPortState disable"}}
         {{"PM Get" "sysPortCounters get"} {"PM Clr" "sysPortCounters clr"}}
      }
   }

   foreach propSet $props {
      set obj [lindex $propSet 0]
      frame $p.$obj -background [lindex $O(color:$obj) 2] -padx 2 -pady 2
      set f $p.$obj.f
      frame $f
      set header [lindex $propSet 1]
      label $f.l -text $header
      pack $f.l -side top
      foreach {attr lbl cmd} [lindex $propSet 2] {
         frame $f.$attr -borderwidth 2 -relief ridge
         label $f.$attr.l -text "$lbl:"

         if {[string range $lbl 0 0] == "\#"} {
            label $f.$attr.v -textvariable PROPS($obj,$attr)
            set PROPS($obj,$attr,menu) \
               [tk_optionMenu $f.$attr.m PROPS($obj,$attr,sel) \
                   "Select a [string range $lbl 1 end]"]
            pack $f.$attr.l -side top -anchor w
            pack $f.$attr.m -side right -anchor e -expand yes -fill x
            pack $f.$attr.v -side left -anchor w
            set PROPS($obj,$attr,cb) $cmd
            set cmd ""
         } else {
            entry $f.$attr.v -textvariable PROPS($obj,$attr) \
               -exportselection 1 -state readonly -relief flat
            pack $f.$attr.l $f.$attr.v -side top -anchor nw \
               -expand true -fill x
         }
         pack $f.$attr -side top -fill x -anchor nw
         if {$cmd != ""} {
            bind $f.$attr.l <ButtonPress-2> $cmd
            bind $f.$attr.v <ButtonPress-2> $cmd
         }
      }
      pack $f -side top -expand yes -fill both
   }

   foreach cmdSet $cmds {
      set lineIdx 0
      set obj [lindex $cmdSet 0]
      foreach lineDef [lrange $cmdSet 1 end] {
         set f $p.$obj.f
         incr lineIdx
         frame $f.cmds$lineIdx
         set bIdx 0
         foreach bnc $lineDef {
            set b [lindex $bnc 0]
            set c [lindex $bnc 1]
            incr bIdx
            button $f.cmds$lineIdx.$bIdx -text $b -command $c
            pack $f.cmds$lineIdx.$bIdx -side left -anchor w
         }
         pack $f.cmds$lineIdx -side top -anchor w
      }
   }
}

##############################################################################
#
# MAIN MENU COMMANDS
#
##############################################################################
proc getNodeLid {node} {
   set port ""
   for {set pn 1} {$pn <= [IBNode_numPorts_get $node]} {incr pn} {
      set port [IBNode_getPort $node $pn]
      if {$port != ""} {
         set remPort [IBPort_p_remotePort_get $port]
         if {$remPort != ""} {break}
      }
   }
   if {$remPort == ""} {return 0}
   set lid [IBPort_base_lid_get $port]
   return $lid
}

# given a key and a list of ley/value pairs get the pair
proc assoc {key key_list} {
   foreach kv $key_list {
      if {[lindex $kv 0] == $key} {return [lindex $kv 1]}
   }

   return ""
}

proc SetVL0Statics {} {
   global gFabric

   set staticCredits 0x68
   foreach nNNode [IBFabric_NodeByName_get $gFabric] {
      set node [lindex $nNNode 1]
      set sys [IBNode_p_system_get $node]
      set name "[IBSystem_name_get $sys]/[lindex $nNNode 0]"
      set devId [IBNode_devId_get $node]
      set lid [getNodeLid $node]
      if {$lid == 0} {
         puts "-W- Ignoring node $name with zero LID"
         continue
      }

      # differet treatment for switches and HCAs
      switch $devId {
         23108 -
         25204 -
         25208 -
         25218 {
            # port 1 0x100A0.24 (len 7)
            set v [crRead $lid 0x100A0]
            set d [assoc data $v]
            if {$d == ""} {
               puts "-W- Failed to obtain data from $name lid:$lid"
               continue
            }

            set nd [format 0x%x [expr $d & 0x8fffffff | ($staticCredits << 24)]]
            if {$d != $nd} {
               puts "-I- Updating $name P1 $d -> $nd"
               crWrite $lid $nd 0x100A0
            }
            if {$devId != 25208} {
               # port 2 0x108A0.24
               set v [crRead $lid 0x108A0]
               set d [assoc data $v]
               if {$d == ""} {
                  puts "-W- Failed to obtain data from $name lid:$lid"
                  continue
               }

               set nd [format 0x%x [expr $d & 0x8fffffff | ($staticCredits << 24)]]
               if {$d != $nd} {
                  puts "-I- Updating $name P2 $d -> $nd"
                  crWrite $lid $nd 0x108A0
               }
            }
         }
         47396 {
            set addr 0x101280
            for {set i 0} {$i < 24} {incr i} {
               # IB port 1 101280.16
               # CR 0  101280.16 (len 16)
               # CR 1  102280.16
               # CR 23 118280.16
               set v [crRead $lid $addr]
               set d [assoc data $v]
               if {$d == ""} {
                  puts "-W- Failed to obtain data from $name lid:$lid"
                  continue
               }
               set nd [format 0x%x [expr $d & 0xffff | ($staticCredits << 16)]]
               if {$d != $nd} {
                  puts "-I- Updating $name P[expr $i + 1] $d -> $nd"
                  crWrite $lid $nd $addr
               }

               incr addr 0x1000
            }
         }
         default {
            puts "-W- Ignoring node $name with devId:$devId"
         }
      }
   }

}

proc DiagNet {} {
   global G
   global testModeDir
   global IBDIAGNET_FLAGS

   # can we just load existing files?
   if {$testModeDir != 0} {
      set f [open [file join $testModeDir ibdiagnet.stdout.log] r]
      set res [read $f]
      close $f
      set lstFile [file join $testModeDir ibdiagnet.lst]
   } else {
      set lstFile /tmp/ibdiagnet.lst
      set r ""
      LogAppend "-I-Invoking ibdiagnet ...."
      # puts "-I- Invoking ibdiagnet ...."
      if {[catch {set r [eval "exec ibdiagnet $IBDIAGNET_FLAGS"]} e]} {
         set res "-E- Error calling ibdiagnet:$e\n"
         append res $r
      } else {
         set res $r
      }
   }

   LogUpdate $res

   GraphUpdate $lstFile
}

# reread the annotations file and enforce DISABLED state
# and UP for the rest
proc EnforceAnnotations {} {
   global ANNOTATIONS
   LoadAnnotationsFile
   set numEn 0
   set numDis 0
   foreach e [array names ANNOTATIONS sysport:*] {
      set sysPortName [string range $e [string length sysport:] end]
      set anno $ANNOTATIONS($e)

      # find the sys port
      set sysPort [findSysPortByName $sysPortName]
      if {$sysPort == ""} {
         puts "-W- failed to find sys port:$sysPortName"
         continue
      }

      set port [IBSysPort_p_nodePort_get $sysPort]
      set node [IBPort_p_node_get $port]

      if {[regexp DISABLED $anno]} {
         SetStatus "-I- Disabling $sysPortName"
         set state disable
         incr numDis
      } else {
         SetStatus "-I- Enabling $sysPortName"
         set state enable
         incr numEn
      }

      set drPath [getDrToNode $node]
      if {$drPath == -1} {
         return
      }

      set portNum [IBPort_num_get $port]
      catch {set res [exec ibportstate -D $drPath $portNum $state]}
   }
   SetStatus "-I- Annotation Enforced: Enabled:$numEn Disbled:$numDis"

}

proc FindByName {} {
   global FindByName
   if {![winfo exists .find_by_name]} {
      set t [toplevel .find_by_name]
      wm withdraw $t

      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]

      labelframe $f.e -text "Name:" -padx 2 -pady 2 -borderwidth 2
      entry $f.e.e -textvariable FindByName(name)
      pack $f.e.e -side left -fill x -expand yes

      labelframe $f.b -text "Object Type:" -padx 2 -pady 2 -borderwidth 2
      foreach {type name} {system System sysport "System Port" node Node port Port} {
         radiobutton $f.b.b$type -text "$name" -variable FindByName(type) \
            -relief flat -value $type
         pack $f.b.b$type -side top -pady 2 -anchor w
      }
      pack $f.e $f.b -side top -expand yes -fill both
      frame $f.x
      button $f.x.f -text FIND \
         -command {guiHighLightByName $FindByName(type) $FindByName(name)}
      button $f.x.c -text CLEAR -command guiClearAllMarking
      pack $f.x.f $f.x.c -side left -fill x -expand yes
      pack $f.x -side bottom -fill x -expand yes
      pack $f
      wm title .find_by_name "IBDiagUI - Find object by name"
      set FindByName(type) system

   }
   wm deiconify .find_by_name
}

proc FindByGUID {} {
   global FindByGuid
   if {![winfo exists .find_by_guid]} {
      set t [toplevel .find_by_guid]
      wm withdraw $t

      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]

      labelframe $f.e -text "GUID:" -padx 2 -pady 2 -borderwidth 2
      entry $f.e.e -textvariable FindByGuid(guid)
      pack $f.e.e -side left -fill x -expand yes

      labelframe $f.b -text "Object Type:" -padx 2 -pady 2 -borderwidth 2
      foreach {type name} {system "System" sysport "System Port" node "Node" port "Port"} {
         radiobutton $f.b.b$type -text "$name" -variable FindByGuid(type) \
            -relief flat -value $type
         pack $f.b.b$type -side top -pady 2 -anchor w
      }
      pack $f.e $f.b -side top -expand yes -fill both
      frame $f.x
      button $f.x.f -text FIND \
         -command {guiHighLightByGuid $FindByGuid(type) $FindByGuid(guid)}
      button $f.x.c -text CLEAR -command guiClearAllMarking
      pack $f.x.f $f.x.c -side left -fill x -expand yes
      pack $f.x -side bottom -fill x -expand yes
      pack $f
      wm title .find_by_guid "IBDiagUI - Find object by GUID"
      set FindByGuid(type) system

   }
   wm deiconify .find_by_guid
}

proc FindByLID {} {
   global FindByLid
   if {![winfo exists .find_by_lid]} {
      set t [toplevel .find_by_lid]
      wm withdraw $t

      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]

      labelframe $f.e -text "LID:" -padx 2 -pady 2 -borderwidth 2
      entry $f.e.e -textvariable FindByLid(lid)
      pack $f.e.e -side left -fill x -expand yes

      labelframe $f.b -text "Object Type:" -padx 2 -pady 2 -borderwidth 2
      foreach {type name} {system "System" sysport "System Port" node "Node" port "Port"} {
         radiobutton $f.b.b$type -text "$name" -variable FindByLid(type) \
            -relief flat -value $type
         pack $f.b.b$type -side top -pady 2 -anchor w
      }
      pack $f.e $f.b -side top -expand yes -fill both
      frame $f.x
      button $f.x.f -text FIND \
         -command {guiHighLightByLid $FindByLid(type) $FindByLid(lid)}
      button $f.x.c -text CLEAR -command guiClearAllMarking
      pack $f.x.f $f.x.c -side left -fill x -expand yes
      pack $f.x -side bottom -fill x -expand yes
      pack $f
      wm title .find_by_lid "IBDiagUI - Find object holding a LID"
      set FindByLid(type) system

   }
   wm deiconify .find_by_lid
}

proc FindByDR {} {
   global FindByDR G
   if {![winfo exists .find_by_dr]} {
      set t [toplevel .find_by_dr]
      wm withdraw $t

      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]

      labelframe $f.e -text "Directed Route:" -padx 2 -pady 2 -borderwidth 2
      entry $f.e.e -textvariable FindByDR(DR)
      pack $f.e.e -side left -fill x -expand yes

      labelframe $f.p -text "Start Port:" -padx 2 -pady 2 -borderwidth 2
      entry $f.p.e -textvariable FindByDR(port)
      pack $f.p.e -side left -fill x -expand yes
      pack $f.e $f.p -side top -expand yes -fill both
      frame $f.x
      button $f.x.f -text FIND \
         -command {guiHighLightByDR $FindByDR(port) $FindByDR(DR)}
      button $f.x.c -text CLEAR -command guiClearAllMarking
      pack $f.x.f $f.x.c -side left -fill x -expand yes
      pack $f.x -side bottom -fill x -expand yes
      pack $f
      wm title .find_by_dr "IBDiagUI - Find objects on a Directed Route"
      set FindByDR(port) "$G(argv:sys.name)/P$G(argv:port.num)"
   }
   wm deiconify .find_by_dr
}


proc setColor {b opt} {
   global O
   foreach {w desc val} $O($opt) {break}
   set color [tk_chooseColor -title "Choose a $desc color" -initialcolor $val]
   if {$color != ""} {
      set O($opt) [list $w $desc $color]
      $b configure -background $color
   }
}

proc getColor {col} {
   global O
   if {[info exists O(color:$col)]} {
      return [lindex $O(color:$col) 2]
   } else {
      puts "-W- could not find color $col"
      return black
   }
   setLogColors
}

# Display a form for setting fabric roots
proc SetRoots {} {
   global C gFabric
   if {![winfo exists .set_roots_opts]} {
      set t [toplevel .set_roots_opts]
      wm withdraw $t

      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]
      label $f.l -text "Root systems names:"
      entry $f.e -textvariable SYSTEM_ORDER
      button $f.b -text REDRAW -command "drawFabric $gFabric $C"
      pack $f.l $f.e $f.b -side top -expand true -fill x
      pack $f
      wm title .set_roots_opts "IBDiagUI - Set Roots Options"
   }
   wm deiconify .set_roots_opts
}

# Display a form for setting colors
proc SetColorOpts {} {
   global O
   if {![winfo exists .set_color_opts]} {
      set t [toplevel .set_color_opts]
      wm withdraw $t

      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]
      set prevFirstWord ""
      foreach opt [lsort [array name O color:*]] {
         foreach {w desc val} $O($opt) {break}
         set firstWord [lindex $desc 0]
         if {$firstWord != $prevFirstWord} {
            set wName [string tolower $firstWord]
            set wf [labelframe $f.$wName -text "$firstWord:" \
                       -padx 2 -pady 2 -borderwidth 2]
            pack $wf -side top -expand yes -fill x
            set prevFirstWord $firstWord
         }
         button $wf.$w -text [lrange $desc 1 end] \
            -command "setColor $wf.$w $opt" \
            -background $val
         pack $wf.$w -side left -pady 2 -anchor w -fill x
      }
      pack $f
      wm title .set_color_opts "IBDiagUI - Set Color Options"
   }
   wm deiconify .set_color_opts
}

proc SetAnnotationsFile {} {
   global O
   if {![winfo exists .load_annos]} {
      set t [toplevel .load_annos]
      wm withdraw $t

      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]
      label $f.l -text "Annotation File Name"
      entry $f.e -textvariable ANNOTATION_FILE
      button $f.b -text LOAD -command LoadAnnotationsFile
      pack $f.l $f.e $f.b -side top -expand yes -fill x
      pack $f

      wm title .load_annos "IBDiagUI - Set Color Options"

      if {![info exists ANNOTATION_FILE]} {
         set ANNOTATION_FILE ""
      }
   }
   wm deiconify .load_annos
}

proc SetIBDiagFlags {} {
   global O
   if {![winfo exists .ibdiag_flags]} {
      set t [toplevel .ibdiag_flags]
      wm withdraw $t

      set f [frame $t.f -padx 2 -pady 2 -borderwidth 2]
      label $f.l -text "IBDiagNet Flags:"
      entry $f.e -textvariable IBDIAGNET_FLAGS
      pack $f.l $f.e -side top -expand yes -fill x
      pack $f

      wm title .ibdiag_flags "IBDiagUI - Set IBDiagNet Flags"
   }
   wm deiconify .ibdiag_flags
}


proc HelpAbout {} {
   catch {destroy .help_about}
   set tl [toplevel  .help_about]
   label $tl.l -text {
      IBDIAG GUI

      Version: 1.0
      Date: Sep 2006
      Author: Eitan Zahavi <eitan@mellanox.co.il>
   }
   pack $tl.l
}

##############################################################################
#
# GUI INITIALIZATION
#
##############################################################################

# save as much as possible in .ibdiagui
proc guiQuit {} {
   global PANES O
   global ANNOTATION_FILE

   if {[catch {set f [open .ibdiagui w]} ]} {
      return
   }

   puts $f "wm geometry . [wm geometry .]"
   puts $f "update"
   foreach w [array names PANES] {
      foreach idx $PANES($w) {
         set coords [$w sash coord $idx]
         puts $f "$w sash place $idx [lindex $coords 0] [lindex $coords 1]"
      }
   }

   foreach opt [array names O] {
      puts $f "set O($opt) {$O($opt)}"
   }

   puts $f "set ANNOTATION_FILE \"$ANNOTATION_FILE\""
   close $f
   exit
}

# init the menu bar
proc initMenuBar {m} {
   menubutton $m.file -text File -underline 0 -menu $m.file.menu
   menubutton $m.refresh -text Refresh -underline 0 -menu $m.refresh.menu
   menubutton $m.find -text Find -underline 0 -menu $m.find.menu
   menubutton $m.opts -text Options -underline 0 -menu $m.opts.menu

   menu $m.file.menu -tearoff no
   $m.file.menu add command -label Quit -command guiQuit

   menu $m.refresh.menu -tearoff no
   $m.refresh.menu add command -label Network -command DiagNet
   $m.refresh.menu add command -label "Enforce Annotations" \
      -command EnforceAnnotations
   $m.refresh.menu add command -label "Add Statics to VL0" \
      -command SetVL0Statics

   menu $m.find.menu -tearoff no
   $m.find.menu add command -label Name -command FindByName
   $m.find.menu add command -label GUID -command FindByGUID
   $m.find.menu add command -label LID -command FindByLID
   $m.find.menu add command -label Route -command FindByDR

   menu $m.opts.menu -tearoff no
   $m.opts.menu add command -label Colors -command SetColorOpts
   $m.opts.menu add command -label "Set Roots" -command SetRoots
   $m.opts.menu add command -label "Set Annotation File" \
      -command SetAnnotationsFile
   $m.opts.menu add command -label "Set IBDiagNet Options" \
      -command SetIBDiagFlags

   menubutton $m.help -text Help -underline 0 -menu $m.help.menu
   menu $m.help.menu -tearoff no
   $m.help.menu add command -label About -command HelpAbout

   pack $m.file $m.refresh $m.find $m.opts -side left

   pack $m.help -side right
}

#--------------------------------------------------------
#  Init the main windows and provide their ids in globals:
#  G - the graphic canvas widget id
#  P - the props frame
#  L - the LOG text widget
#--------------------------------------------------------
proc initMainFrame {f} {
   global C P L PANES

   #--------------------------------------------------------
   #  The hierarchy of widgets we build is defined below
   #  f
   #   pw1 - the main pane split vertically
   #    tf - the top frame
   #     pw2 - the second pane - this time horizontal
   #      gf - graphic frame
   #       gg - graphic grid
   #       chs - canvas horizonal scroll
   #       cvs - canvas vertical scroll
   #       c - canvas
   #      pf - props frame
   #    bf - the bottom frame
   #     tg - text grid
   #     ths - text horizonal srcolll
   #     tvs - text vertical srcolll
   #     t - text widget
   #--------------------------------------------------------

   #   pw1 - the main pane split vertically
   set pw1 [panedwindow $f.pw1 -orient vertical -showhandle yes]
   set PANES($pw1) 0

   #    tf - the top frame
   set tf [frame $pw1.tf]
   #     pw2 - the second pane - this time horizontal
   set pw2 [panedwindow $tf.pw2 -showhandle yes]
   set PANES($pw2) 0

   #      gf - graphic frame
   set gf [frame $tf.gf]
   #       gg - graphic grid
   set gg [frame $gf.g]
   #       chs - canvas horizonal scroll
   set chs [scrollbar $gf.chs -orient horiz -command "$gf.c xview"]
   #       cvs - canvas vertical scroll
   set cvs [scrollbar $gf.cvs -orient vertical -command "$gf.c yview"]
   #       c - canvas
   set c [canvas $gf.c -relief sunken -borderwidth 2 \
             -scrollregion {-11c -11c 11c 11c} \
             -xscrollcommand "$chs set" \
             -yscrollcommand "$cvs set" ]
   #      pf - props frame
   set pf [frame $tf.pf]
   #    bf - the bottom frame
   set bf [frame $pw1.bf]
   #     tg - text grid
   set tg [frame $bf.g]
   #     ths - text horizonal srcolll
   set ths [scrollbar $bf.ths -orient horiz -command "$bf.t xview"]
   #     tvs - text vertical srcolll
   set tvs [scrollbar $bf.tvs -orient vertical -command "$bf.t yview"]
   #     t - text widget
   set t [text $bf.t \
             -yscrollcommand "$tvs set" \
             -xscrollcommand "$ths set" \
             -state disabled]

   #--------------------------------------------------------
   # Packing...
   #--------------------------------------------------------

   # Graphic area
   pack $gg -expand yes -fill both -padx 1 -pady 1
   grid rowconfig    $gg 0 -weight 1 -minsize 0
   grid columnconfig $gg 0 -weight 1 -minsize 0
   grid $c -padx 1 -in $gg -pady 1 \
      -row 0 -column 0 -rowspan 1 -columnspan 1 -sticky news
   grid $cvs -in $gg -padx 1 -pady 1 \
      -row 0 -column 1 -rowspan 1 -columnspan 1 -sticky news
   grid $chs -in $gg -padx 1 -pady 1 \
      -row 1 -column 0 -rowspan 1 -columnspan 1 -sticky news

   # graphic / prop pane
   $pw2 add $pf
   $pw2 add $gf

   pack $pw2 -side top -expand yes -fill both -pady 2 -padx 2m
   $pw2 paneconfigure $gf -sticky news -width 10c
   $pw2 paneconfigure $pf -sticky news -minsize 4c

   # the frame holding it
   # pack $tf -side top -expand yes -fill both
   pack $tf -side top -fill both

   # log text area
   pack $tg -expand yes -fill both -padx 1 -pady 1
   grid rowconfig    $tg 0 -weight 1 -minsize 0
   grid columnconfig $tg 0 -weight 1 -minsize 0
   grid $t -padx 1 -in $tg -pady 1 \
      -row 0 -column 0 -rowspan 1 -columnspan 1 -sticky news
   grid $tvs -in $tg -padx 1 -pady 1 \
      -row 0 -column 1 -rowspan 1 -columnspan 1 -sticky news
   grid $ths -in $tg -padx 1 -pady 1 \
      -row 1 -column 0 -rowspan 1 -columnspan 1 -sticky news

   # the frame holding it
   #   pack $bf -side top -expand yes -fill x
   pack $bf -side top -fill x

   # the main pane window
   $pw1 add $tf $bf
   pack $pw1 -side top -expand yes -fill both -pady 2 -padx 2m
   # $pw1 paneconfigure $tf -minsize 15c
   # $pw1 paneconfigure $bf

   bind $c <3> "zoomMark $c %x %y"
   bind $c <B3-Motion> "zoomStroke $c %x %y"
   bind $c <ButtonRelease-3> "zoomArea $c %x %y"
   bind $c <KeyPress-z> "zoom $c 1.25"
   bind $c <KeyPress-Z> "zoom $c 0.8"
   bind $c <KeyPress-f> "fit $c"
   bind . <3> "zoomMark $c %x %y"
   bind . <B3-Motion> "zoomStroke $c %x %y"
   bind . <ButtonRelease-3> "zoomArea $c %x %y"
   bind . <KeyPress-z> "zoom $c 1.25"
   bind . <KeyPress-Z> "zoom $c 0.8"
   bind . <KeyPress-f> "fit $c"
   bind . <KeyPress-e> "expand $c %x %y"
   bind . <KeyPress-d> "deExpand $c %x %y"
   bind . <KeyPress-c> "guiClearAllMarking"

   set C $c
   set L $t
   set P $pf

   $L tag bind NAME  <Button-1> "LogObjSelect $L NAME %W %x %y"
   $L tag bind LID   <Button-1> "LogObjSelect $L LID  %W %x %y"
   $L tag bind GUID  <Button-1> "LogObjSelect $L GUID %W %x %y"
   $L tag bind ROUTE <Button-1> "LogObjSelect $L ROUTE %W %x %y"
}

proc setLogColors {} {
   global L
   $L tag configure errors   -foreground [getColor txtErr]
   $L tag configure warnings -foreground [getColor txtWarn]
   $L tag configure infos    -foreground [getColor txtInfo]
   $L tag configure NAME   -background   [getColor txtName]
   $L tag configure LID    -background   [getColor txtLid]
   $L tag configure GUID   -background   [getColor txtGuid]
   $L tag configure ROUTE  -background   [getColor txtRoute]
}

proc initGui {} {
   global O L S StatusLine P
   global ANNOTATION_FILE
   # . configure -background white -width 10i -height 10i

   # menu is a separate line at the top
   frame .m -relief raised -padx 2 -pady 2
   pack .m -side top -expand no -fill x -anchor nw
   initMenuBar .m

   # the  pane structure
   frame .r -relief ridge -height 10i -width 10i
   pack .r -side top -expand yes -fill both

   # status line
   frame .s
   entry .s.e -relief flat -state readonly -textvariable StatusLine
   pack .s.e -fill x -expand true -side bottom
   pack .s -side bottom -fill x
   set S .s.e

   # the main frame
   initMainFrame .r

   set O(color:txtDef)   {ld "Log Msg Default"  black}
   set O(color:txtErr)   {le "Log Msg Error"    red }
   set O(color:txtWarn)  {lw "Log Msg Warning" "#704000"}
   set O(color:txtInfo)  {li "Log Msg Info"     darkgreen }
   set O(color:txtName)  {ln "Log Tag Name"  "#909000" }
   set O(color:txtLid)   {ll "Log Tag LID"   "#fb9933" }
   set O(color:txtGuid)  {lg "Log Tag GUID"  "#906070" }
   set O(color:txtRoute) {lr "Log Tag Route" "#aa40a0"}

   set O(color:1x2.5G)    {p1x25g "Link 1x 2.5G"   "#ff0000"}
   set O(color:1x5G)      {p1x5g  "Link 1x 5G"     "#c80000"}
   set O(color:1x10G)     {p1x10g "Link 1x 10G"    "#960000"}
   set O(color:4x2.5G)    {p4x25g "Link 4x 2.5G"   "#00ff00"}
   set O(color:4x5G)      {p4x5g  "Link 4x 5G"     "#00c800"}
   set O(color:4x10G)     {p4x10g "Link 4x 10G"    "#009600"}
   set O(color:12x2.5G)   {p12x25g "Link 12x 2.5G" "#0000ff"}
   set O(color:12x5G)     {p12x5g  "Link 12x 5G"   "#00ff40"}
   set O(color:12x10G)    {p12x10g "Link 12x 10G"  "#00ff80"}

   set O(color:sys)       {sys  "Props System"      "#ff5e1b"}
   set O(color:node)      {node "Props Node"        "#00beff"}
   set O(color:port)      {port "Props Port"        "#00ff96"}
   set O(color:sysport)   {sysp "Props System Port" "#f400cc"}

   set O(color:mark)      {mark "Marking Selected"  "#f400f1"}
   set O(color:mtxt)      {mtxt "Marking Text"      "#0000ff"}

   if {[file exists .ibdiagui]} {
      source .ibdiagui
   }

   # actuall set the colors on the text tags
   setLogColors
   initPropsGui $P
   LoadAnnotationsFile
   SetStatus "Initializing ... "
}

##############################################################################
#
# Main flow
#

# we provide a way to load the results of ibdiagnet for testing
# to do this provide -D <dir name> that dir needs to have:
# ibdiagnet.stdout.log
# ibdiahnet.lst
# OPTIONAL: ibdiagnet.topo
set testModeDirIdx [lsearch $argv "-D"]
if {$testModeDirIdx >= 0} {
   set testModeDir [lindex $argv [expr $testModeDirIdx + 1]]
   if {![file exists [file join $testModeDir ibdiagnet.lst]]} {
      puts "-E- No [file join $testModeDir ibdiagnet.lst]"
      exit 1
   }
   if {![file exists [file join $testModeDir ibdiagnet.stdout.log]]} {
      puts "-E- No [file join $testModeDir ibdiagnet.stdout.log]"
      exit 1
   }
   set argv [lreplace $argv $testModeDirIdx [expr $testModeDirIdx + 1]]
} else {
   set testModeDir 0
}

set IBDIAGNET_FLAGS $argv

InitializeIBDIAG
StartIBDIAG
if {! [info exists G(argv:sys.name)]} {
   set G(argv:sys.name) [lindex [split [info hostname] .] 0]
}

# We init the Tk only after parsing the command line
# to avoid the interpretation of args by Tk.
if {[catch {package require Tk} e]} {
   puts "-E- ibdiagui depends on a Tk installation"
   puts "    Please download and install tk8.4"
   puts "    Error: $e"
   exit 1
}

if {[catch {package require Tcldot} e]} {
   puts "-E- ibdiagui depends on a Tcldot installation"
   puts "    Please download and install Graphviz"
   puts "    Error: $e"
   exit 1
}

if {[catch {initGui} e]} {
   puts "-E- $e"
   puts "    $errorInfo"
   exit
}

if {[catch {DiagNet} e]} {
   puts "-E- $e"
   puts "    $errorInfo"
}

package provide ibdiagui 1.0

