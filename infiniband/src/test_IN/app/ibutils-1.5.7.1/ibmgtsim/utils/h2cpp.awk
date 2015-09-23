#!/bin/awk -f

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

{
  if (h == 0) {
    print;
  }
}
/[\#]--/{
  h = 1;
  print "*/";
}
/^class/{
  if (h) {
    print "\n\n\n";
    print "//////////////////////////////////////////////////////////////";
    print "//";
    print "// CLASS ", $2;
    print "//";

    if (comment != "") {
      print comment;
    }
    c = $2;
    p = 0;
  }
}
/[^\}];/{
  if (p && h) {
    if (comment != "") {
      print comment;
    }

    for (f = 1; f <= NF; f++) {
      if (f == 2) {
        printf("%s::%s ",c,$f);
      } else {
        printf("%s ",$f);
      }
    }
    print " ";
  }
}
/\/[\*]/{
  m = 1;
  comment = "";
}
{
  if (m && p && h) {
    comment = comment "\n" $0;
  }
}
/[\*]\// {
  m = 0;
}
/public/{
  p = 1;
}
