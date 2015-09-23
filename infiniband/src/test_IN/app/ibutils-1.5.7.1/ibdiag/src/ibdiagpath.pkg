proc ibdiagpath-load {dir} {
   puts "Loading IBDIAGPATH from: $dir"
   uplevel #0 source [file join $dir ibdiagpath.tcl]
}
package ifneeded ibdiagpath 1.0 [list ibdiagpath-load $dir]

