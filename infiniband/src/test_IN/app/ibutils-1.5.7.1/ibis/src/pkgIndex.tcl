proc ibis_load%VERSION% {dir} {
   puts "Loading package ibis from: $dir"
   uplevel \#0 load [file join $dir libibis.so.%VERSION%]
}

package ifneeded ibis %VERSION% [list ibis_load%VERSION% $dir]
