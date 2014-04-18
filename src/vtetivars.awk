BEGIN {
    print "/******************************************************************************"
    print " * Copyright (c) 1998-2010,2011 Free Software Foundation, Inc.                *"
    print " *                                                                            *"
    print " * Permission is hereby granted, free of charge, to any person obtaining a    *"
    print " * copy of this software and associated documentation files (the 'Software'), *"
    print " * to deal in the Software without restriction, including without limitation  *"
    print " * the rights to use, copy, modify, merge, publish, distribute, distribute    *"
    print " * with modifications, sublicense, and/or sell copies of the Software, and to *"
    print " * permit persons to whom the Software is furnished to do so, subject to the  *"
    print " * following conditions:                                                      *"
    print " *                                                                            *"
    print " * The above copyright notice and this permission notice shall be included in *"
    print " * all copies or substantial portions of the Software.                        *"
    print " *                                                                            *"
    print " * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR *"
    print " * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   *"
    print " * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    *"
    print " * THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      *"
    print " * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    *"
    print " * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        *"
    print " * DEALINGS IN THE SOFTWARE.                                                  *"
    print " *                                                                            *"
    print " * Except as contained in this notice, the name(s) of the above copyright     *"
    print " * holders shall not be used in advertising or otherwise to promote the sale, *"
    print " * use or other dealings in this Software without prior written               *"
    print " * authorization.                                                             *"
    print " ******************************************************************************/"
    print ""
    print "/* Generated from ncurses/include/Caps from the ncurses sources; you can get it here for example:"
    print " * http://anonscm.debian.org/gitweb/?p=collab-maint/ncurses.git;a=blob_plain;f=include/Caps;h=cb650a6be900c9d460498aa46d7843a11da57446;hb=refs/heads/upstream"
    print " */"
    print ""
    print "#ifndef __VTE_TERMINFO_VARS_H__"
    print "#define __VTE_TERMINFO_VARS_H__"
    print ""
}

$2 == "%%-STOP-HERE-%%" {
    nextfile;
}

/^#/ {
    next; 
}

{ 
    printf "#define VTE_TERMINFO_CAP_%-30s \"%s\"\n", toupper($1), $2;
}
# {
#     printf "#define VTE_TERMINFO_IS_CAP_%-27s (", toupper($1) "(c)";
#     for (i = 0; i < length($2); i++) {
#         printf "(c)[%d] == '%c' && ", i, substr($2, i + 1, 1);
#     }
#     printf "(c)[%d] == 0)\n", i;
# }
# {
#     printf "#define VTE_TERMCAP_CAP_%-32s \"%s\"\n", toupper($1), $4;
# }

$3 == "bool" {
    printf "#define VTE_TERMINFO_VAR_%-30s (%3d | VTE_TERMINFO_VARTYPE_BOOLEAN)\n",
        toupper($1), nbools++;
}
$3 == "num" {
    printf "#define VTE_TERMINFO_VAR_%-30s (%3d | VTE_TERMINFO_VARTYPE_NUMERIC)\n",
        toupper($1), nnums++;
}
$3 == "str" {
    printf "#define VTE_TERMINFO_VAR_%-30s (%3d | VTE_TERMINFO_VARTYPE_STRING)\n",
        toupper($1), nstrs++;
}

END {
    print ""
#   print "#define VTE_TERMINFO_NUM_BOOLEAN_VARS ", nbools
#   print "#define VTE_TERMINFO_NUM_INT_VARS     ", nnums
#   print "#define VTE_TERMINFO_NUM_STRING_VARS  ", nstrs
#   print ""
#   print "/* FIXME! Defines for the common extended names, used e.g.f for extended xterm key strings */"
    print ""
    print "#endif /* __VTE_TERMINFO_VARS_H__ */"
}
