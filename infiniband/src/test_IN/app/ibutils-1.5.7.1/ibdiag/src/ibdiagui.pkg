
proc ibdiagui-load {dir} {
   puts "Loading IBDIAGUI from: $dir"
   uplevel #0 source [file join $dir ibdiagui.tcl]
}
package ifneeded ibdiagui 1.0 [list ibdiagui-load $dir]
