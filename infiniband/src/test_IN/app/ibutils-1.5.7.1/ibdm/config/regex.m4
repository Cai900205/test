#serial 5

dnl Initially derived from code in GNU grep.
dnl Mostly written by Jim Meyering.
dnl Modified by Eiatn Zahavi to test only regcomp and regexec

dnl Usage: IS_INCLUDED_REGEX()
dnl
AC_DEFUN([IS_INCLUDED_REGEX],
  [
    dnl Even packages that don't use regex.c can use this macro.
    dnl Of course, for them it doesn't do anything.

    # Assume we'll default to using the included regex.c.
    ac_use_included_regex=yes

    # However, if the system regex support is good enough that it passes the
    # the following run test, then default to *not* using the included regex.c.
    # If cross compiling, assume the test would fail and use the included
    # regex.c.  The failing regular expression is from `Spencer ere test #75'
    # in grep-2.3.
    AC_CACHE_CHECK([for working regcomp and regexec],
		   ez_cv_func_working_regcomp_regexec,
      AC_TRY_RUN(
	changequote(<<, >>)dnl
	<<
#include <stdio.h>
#include <regex.h>
	  int
	  main ()
	  {
            static regex_t re;
            static regmatch_t matches[[2]];
            int s;
            /* try a simple extended regcomp */
	    s = regcomp(&re, "^[[a-z]]?[[ \t]]+(.*)", REG_EXTENDED);
	    /* This should NOT fail */
            if (s != 0) {
                exit (1);
            }

            /* try matching - shoul NOT fail */
            if (regexec(&re, "g bl_331", 2, matches, 0)) {
                exit (1);                
            }
            exit (0);
	  }
	>>,
	changequote([, ])dnl

	       ez_cv_func_working_regcomp_regexec=yes,
	       ez_cv_func_working_regcomp_regexec=no,
	       dnl When crosscompiling, assume it's broken.
	       ez_cv_func_working_regcomp_regexec=no))

       if test "x$ez_cv_func_working_regcomp_regexec" = xno; then
	     REGEXP_INCLUDE="-I../replace"
	     AC_LIBOBJ([regex])
       fi
      ],
    )
	AC_SUBST(REGEXP_INCLUDE)
  ]
)
