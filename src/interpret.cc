/*
 * Copyright (C) 2001,2002,2003 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>
#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <glib-object.h>
#include <locale.h>
#include "caps.hh"
#include "debug.h"
#include "iso2022.h"
#include "parser.hh"
#include "assert.h"

static bool quiet = false;

static void
print_array(GValueArray* array)
{
        GValue *value;
        if (array != NULL) {
                g_print("(");
                for (unsigned int i = 0; i < array->n_values; i++) {
                        value = g_value_array_get_nth(array, i);
                        if (i > 0) {
                                g_print(", ");
                        }
                        if (G_VALUE_HOLDS_LONG(value)) {
                                g_print("%ld", g_value_get_long(value));
                        } else
                        if (G_VALUE_HOLDS_STRING(value)) {
                                g_print("\"%s\"", g_value_get_string(value));
                        } else
                        if (G_VALUE_HOLDS_POINTER(value)) {
                                g_print("\"%ls\"",
                                        (wchar_t*) g_value_get_pointer(value));
                        } else
                        if (G_VALUE_HOLDS_BOXED(value)) {
                                print_array((GValueArray *)g_value_get_boxed(value));
                        }
                }
                g_print(")");
        }
}

static char const*
seq_to_str(unsigned int type)
{
        switch (type) {
        case VTE_SEQ_NONE: return "NONE";
        case VTE_SEQ_IGNORE: return "IGNORE";
        case VTE_SEQ_GRAPHIC: return "GRAPHIC";
        case VTE_SEQ_CONTROL: return "CONTROL";
        case VTE_SEQ_ESCAPE: return "ESCAPE";
        case VTE_SEQ_CSI: return "CSI";
        case VTE_SEQ_DCS: return "DCS";
        case VTE_SEQ_OSC: return "OSC";
        default:
                assert(false);
        }
}

static char const*
cmd_to_str(unsigned int command)
{
        switch (command) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd;
#include "parser-cmd.hh"
#undef _VTE_CMD
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%u)", command);
                return buf;
        }
}

static  void print_seq(const struct vte_seq *seq)
{
        auto c = seq->terminator;
        if (seq->command == VTE_CMD_GRAPHIC) {
                char buf[7];
                buf[g_unichar_to_utf8(c, buf)] = 0;
                g_print("%s U+%04X [%s]\n", cmd_to_str(seq->command),
                        c,
                        g_unichar_isprint(c) ? buf : "�");
        } else {
                g_print("%s", cmd_to_str(seq->command));
                if (seq->n_args) {
                        g_print(" ");
                        for (unsigned int i = 0; i < seq->n_args; i++) {
                                if (i > 0)
                                        g_print(";");
                        g_print("%d", seq->args[i]);
                        }
                }
                g_print("\n");
        }
}

int
main(int argc, char **argv)
{
        GArray *array;
        int infile;
        struct _vte_iso2022_state *subst;

        setlocale(LC_ALL, "");

        _vte_debug_init();

        if (argc < 1) {
                g_print("usage: %s [file] [--quiet]\n", argv[0]);
                return 1;
        }

        if ((argc > 1) && (strcmp(argv[1], "-") != 0)) {
                infile = open (argv[1], O_RDONLY);
                if (infile == -1) {
                        g_print("error opening %s: %s\n", argv[1],
                                strerror(errno));
                        exit(1);
                }
        } else {
                infile = 1;
        }

        if (argc > 2)
                quiet = g_str_equal(argv[2], "--quiet") || g_str_equal(argv[2], "-q");

        g_type_init();

        array = g_array_new(FALSE, FALSE, sizeof(gunichar));

        struct vte_parser *parser;
        if (vte_parser_new(&parser) != 0)
                return 1;

        subst = _vte_iso2022_state_new(NULL);

        gsize buf_size = 1024*1024;
        guchar* buf = g_new0(guchar, buf_size);

        gsize* seq_stats = g_new0(gsize, VTE_SEQ_N);
        gsize* cmd_stats = g_new0(gsize, VTE_CMD_N);

        for (;;) {
                auto l = read (infile, buf, buf_size);
                if (!l)
                        break;
                if (l == -1) {
                        if (errno == EAGAIN)
                                continue;
                        break;
                }
                _vte_iso2022_process(subst, buf, (unsigned int) l, array);

                gunichar* wbuf = &g_array_index(array, gunichar, 0);
                gsize wcount = array->len;

                struct vte_seq *seq;
                for (gsize i = 0; i < wcount; i++) {
                        auto ret = vte_parser_feed(parser,
                                                   &seq,
                                                   wbuf[i]);
                        if (ret < 0) {
                                if (!quiet)
                                        g_print("Parser error\n");
                                goto done;
                        }

                        seq_stats[ret]++;
                        if (ret != VTE_SEQ_NONE) {
                                cmd_stats[seq->command]++;

                                if (!quiet)
                                        print_seq(seq);
                        }
                }

                g_array_set_size(array, 0);
        }
done:
        if (!quiet)
                g_printerr("End of data.\n");

        for (unsigned int s = VTE_SEQ_NONE + 1; s < VTE_SEQ_N; s++) {
                g_printerr("%-7s: %" G_GSIZE_FORMAT "\n", seq_to_str(s), seq_stats[s]);
        }

        g_printerr("\n");
        for (unsigned int s = 0; s < VTE_CMD_N; s++) {
                if (cmd_stats[s] > 0) {
                        g_printerr("%-12s: %" G_GSIZE_FORMAT "\n", cmd_to_str(s), cmd_stats[s]);
                }
        }

        g_free(seq_stats);
        g_free(cmd_stats);

        close (infile);

        _vte_iso2022_state_free(subst);
        g_array_free(array, TRUE);
        vte_parser_free(parser);
        g_free(buf);
        return 0;
}
