/*
 * Copyright (C) 2001,2002,2003 Red Hat, Inc.
 * Copyright © 2017, 2018 Christian Persch
 *
 * This programme is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This programme is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>

#include <cassert>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include "debug.h"
#include "parser.hh"
#include "utf8.hh"

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
        case VTE_SEQ_SCI: return "SCI";
        case VTE_SEQ_APC: return "APC";
        case VTE_SEQ_PM: return "PM";
        case VTE_SEQ_SOS: return "SOS";
        default:
                assert(false);
        }
}

static char const*
cmd_to_str(unsigned int command)
{
        switch (command) {
#define _VTE_CMD(cmd) case VTE_CMD_##cmd: return #cmd;
#define _VTE_NOP(cmd) _VTE_CMD(cmd)
#include "parser-cmd.hh"
#undef _VTE_CMD
#undef _VTE_NOP
        default:
                return nullptr;
        }
}

#if 0
static char const*
charset_alias_to_str(unsigned int cs)
{
        switch (cs) {
#define _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2) case VTE_CHARSET_##name1: return #name1 "(" ## #name2 ## ")";
#define _VTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
        default:
                return nullptr; /* not an alias */
        }
}

static char const*
charset_to_str(unsigned int cs)
{
        auto alias = charset_alias_to_str(cs);
        if (alias)
                return alias;

        switch (cs) {
#define _VTE_CHARSET_PASTE(name) case VTE_CHARSET_##name: return #name;
#define _VTE_CHARSET(name) _VTE_CHARSET_PASTE(name)
#define _VTE_CHARSET_ALIAS_PASTE(name1,name2)
#define _VTE_CHARSET_ALIAS(name1,name2)
#include "parser-charset.hh"
#undef _VTE_CHARSET_PASTE
#undef _VTE_CHARSET
#undef _VTE_CHARSET_ALIAS_PASTE
#undef _VTE_CHARSET_ALIAS
        default:
                static char buf[32];
                snprintf(buf, sizeof(buf), "UNKOWN(%u)", cs);
                return buf;
        }
}
#endif

#define SEQ_START "\e[7m"
#define SEQ_END   "\e[27m"

#define SEQ_START_RED "\e[7;31m"
#define SEQ_END_RED   "\e[27;39m"

class printer {
public:
        printer(GString* str,
                bool plain,
                char const* intro,
                char const* outro)
                : m_str(str),
                  m_plain(plain),
                  m_outro(outro) {
                if (!m_plain)
                        g_string_append(m_str, intro);
        }
        ~printer() {
                if (!m_plain)
                        g_string_append(m_str, m_outro);
        }
private:
        GString* m_str;
        bool m_plain;
        char const* m_outro;
};

static void
print_params(GString* str,
             struct vte_seq const* seq)
{
        if (seq->n_args > 0)
                g_string_append_c(str, ' ');

        for (unsigned int i = 0; i < seq->n_args; i++) {
                auto arg = seq->args[i];
                if (!vte_seq_arg_default(arg))
                        g_string_append_printf(str, "%d", vte_seq_arg_value(arg));
                if (i + 1 < seq->n_args)
                        g_string_append_c(str, vte_seq_arg_nonfinal(arg) ? ':' : ';');
        }
}

static void
print_pintro(GString* str,
             unsigned int type,
             unsigned int intermediates)
{
        if (type != VTE_SEQ_CSI &&
            type != VTE_SEQ_DCS)
                return;

        unsigned int p = intermediates & 0x7;
        if (p == 0)
                return;

        g_string_append_c(str, ' ');
        g_string_append_c(str, 0x40 - p);
}

static void
print_intermediates(GString* str,
                    unsigned int type,
                    unsigned int intermediates)
{
        if (type == VTE_SEQ_CSI ||
            type == VTE_SEQ_DCS)
                intermediates = intermediates >> 3; /* remove pintro */

        while (intermediates != 0) {
                unsigned int i = intermediates & 0x1f;
                char c = 0x20 + i - 1;

                g_string_append_c(str, ' ');
                if (c == 0x20)
                        g_string_append(str, "SP");
                else
                        g_string_append_c(str, c);

                intermediates = intermediates >> 5;
        }
}

static void
print_string(GString* str,
             struct vte_seq const* seq)
{
        size_t len;
        auto buf = vte_seq_string_get(&seq->arg_str, &len);

        g_string_append_c(str, '\"');
        for (size_t i = 0; i < len; ++i)
                g_string_append_unichar(str, buf[i]);
        g_string_append_c(str, '\"');
}

static void
print_seq_and_params(GString* str,
                     const struct vte_seq *seq,
                     bool plain)
{
        printer p(str, plain, SEQ_START, SEQ_END);

        if (seq->command != VTE_CMD_NONE) {
                g_string_append_printf(str, "{%s", cmd_to_str(seq->command));
                print_params(str, seq);
                g_string_append_c(str, '}');
        } else {
                g_string_append_printf(str, "{%s", seq_to_str(seq->type));
                print_pintro(str, seq->type, seq->intermediates);
                print_params(str, seq);
                print_intermediates(str, seq->type, seq->intermediates);
                g_string_append_printf(str, " %c}", seq->terminator);
        }
}

static void
print_seq(GString* str,
          struct vte_seq const* seq,
          bool codepoints,
          bool plain)
{
        switch (seq->type) {
        case VTE_SEQ_NONE: {
                printer p(str, plain, SEQ_START_RED, SEQ_END_RED);
                g_string_append(str, "{NONE}");
                break;
        }

        case VTE_SEQ_IGNORE: {
                printer p(str, plain, SEQ_START_RED, SEQ_END_RED);
                g_string_append(str, "{IGN}");
                break;
        }

        case VTE_SEQ_GRAPHIC: {
                bool printable = g_unichar_isprint(seq->terminator);
                if (codepoints || !printable) {
                        if (printable) {
                                char ubuf[7];
                                ubuf[g_unichar_to_utf8(seq->terminator, ubuf)] = 0;
                                g_string_append_printf(str, "[%04X %s]",
                                                       seq->terminator, ubuf);
                        } else {
                                g_string_append_printf(str, "[%04X]",
                                                       seq->terminator);
                        }
                } else {
                        g_string_append_unichar(str, seq->terminator);
                }
                break;
        }

        case VTE_SEQ_CONTROL:
        case VTE_SEQ_ESCAPE: {
                printer p(str, plain, SEQ_START, SEQ_END);
                g_string_append_printf(str, "{%s}", cmd_to_str(seq->command));
                break;
        }

        case VTE_SEQ_CSI:
        case VTE_SEQ_DCS: {
                print_seq_and_params(str, seq, plain);
                break;
        }

        case VTE_SEQ_OSC: {
                printer p(str, plain, SEQ_START, SEQ_END);
                g_string_append(str, "{OSC ");
                print_string(str, seq);
                g_string_append_c(str, '}');
                break;
        }

        case VTE_SEQ_SCI: {
                if (seq->terminator <= 0x20)
                  g_string_append_printf(str, "{SCI %d/%d}",
                                         seq->terminator / 16,
                                         seq->terminator % 16);
                else
                  g_string_append_printf(str, "{SCI %c}", seq->terminator);
                break;
        }

        default:
                assert(false);
        }
}

static void
printout(GString* str)
{
        g_print("%s\n", str->str);
        g_string_truncate(str, 0);
}

static gsize seq_stats[VTE_SEQ_N];
static gsize cmd_stats[VTE_CMD_N];
static GArray* bench_times;

static void
process_file_utf8(int fd,
                  bool codepoints,
                  bool plain,
                  bool quiet)
{
        struct vte_parser parser;
        vte_parser_init(&parser);

        gsize const buf_size = 16384;
        guchar* buf = g_new0(guchar, buf_size);
        auto outbuf = g_string_sized_new(buf_size);

        auto start_time = g_get_monotonic_time();

        vte::base::UTF8Decoder decoder;

        gsize buf_start = 0;
        for (;;) {
                auto len = read(fd, buf + buf_start, buf_size - buf_start);
                if (!len)
                        break;
                if (len == -1) {
                        if (errno == EAGAIN)
                                continue;
                        break;
                }

                auto const bufend = buf + len;

                struct vte_seq *seq = &parser.seq;

                for (auto sptr = buf; sptr < bufend; ++sptr) {
                        switch (decoder.decode(*sptr)) {
                        case vte::base::UTF8Decoder::REJECT_REWIND:
                                /* Rewind the stream.
                                 * Note that this will never lead to a loop, since in the
                                 * next round this byte *will* be consumed.
                                 */
                                --sptr;
                                /* [[fallthrough]]; */
                        case vte::base::UTF8Decoder::REJECT:
                                decoder.reset();
                                /* Fall through to insert the U+FFFD replacement character. */
                                /* [[fallthrough]]; */
                        case vte::base::UTF8Decoder::ACCEPT: {
                                auto ret = vte_parser_feed(&parser, decoder.codepoint());
                                if (G_UNLIKELY(ret < 0)) {
                                        g_printerr("Parser error!\n");
                                        goto out;
                                }

                                seq_stats[ret]++;
                                if (ret != VTE_SEQ_NONE) {
                                        cmd_stats[seq->command]++;
                                        if (!quiet) {
                                                print_seq(outbuf, seq, codepoints, plain);
                                                if (seq->command == VTE_CMD_LF)
                                                        printout(outbuf);
                                        }
                                }
                                break;
                        }

                        default:
                                break;
                        }
                }
        }

 out:
        if (!quiet)
                printout(outbuf);

        int64_t time_spent = g_get_monotonic_time() - start_time;
        g_array_append_val(bench_times, time_spent);

        g_string_free(outbuf, TRUE);
        g_free(buf);
        vte_parser_deinit(&parser);
}

static bool
process_file(int fd,
             bool codepoints,
             bool plain,
             bool quiet,
             int repeat)
{
        if (fd == STDIN_FILENO && repeat != 1) {
                g_printerr("Cannot consume STDIN more than once\n");
                return false;
        }

        for (auto i = 0; i < repeat; ++i) {
                if (i > 0 && lseek(fd, 0, SEEK_SET) != 0) {
                        g_printerr("Failed to seek: %m\n");
                        return false;
                }

                process_file_utf8(fd, codepoints, plain, quiet);
        }

        return true;
}

int
main(int argc,
     char *argv[])
{
        gboolean benchmark = false;
        gboolean codepoints = false;
        gboolean plain = false;
        gboolean quiet = false;
        gboolean statistics = false;
        int repeat = 1;
        char** filenames = nullptr;
        GOptionEntry const entries[] = {
                { "benchmark", 'b', 0, G_OPTION_ARG_NONE, &benchmark,
                  "Measure time spent parsing each file", nullptr },
                { "codepoints", 'u', 0, G_OPTION_ARG_NONE, &codepoints,
                  "Output unicode code points by number", nullptr },
                { "plain", 'p', 0, G_OPTION_ARG_NONE, &plain,
                  "Output plain text without attributes", nullptr },
                { "quiet", 'q', 0, G_OPTION_ARG_NONE, &quiet,
                  "Suppress output except for statistics and benchmark", nullptr },
                { "repeat", 'r', 0, G_OPTION_ARG_INT, &repeat,
                  "Repeat each file COUNT times", "COUNT" },
                { "statistics", 's', 0, G_OPTION_ARG_NONE, &statistics,
                  "Output statistics", nullptr },
                { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames,
                  nullptr, nullptr },
                { nullptr }
        };

        setlocale(LC_ALL, "");
        _vte_debug_init();

        auto context = g_option_context_new("[FILE…] — parser cat");
        g_option_context_set_help_enabled(context, true);
        g_option_context_add_main_entries(context, entries, nullptr);

        GError* err = nullptr;
        bool rv = g_option_context_parse(context, &argc, &argv, &err);
        g_option_context_free(context);

        if (!rv) {
                g_printerr("Failed to parse arguments: %s\n", err->message);
                g_error_free(err);
                return EXIT_FAILURE;
        }

        int exit_status = EXIT_FAILURE;

        memset(&seq_stats, 0, sizeof(seq_stats));
        memset(&cmd_stats, 0, sizeof(cmd_stats));
        bench_times = g_array_new(false, true, sizeof(int64_t));

        if (filenames != nullptr) {
                for (auto i = 0; filenames[i] != nullptr; i++) {
                        char const* filename = filenames[i];

                        int fd = -1;
                        if (g_str_equal(filename, "-")) {
                                fd = STDIN_FILENO;
                        } else {
                                fd = open(filename, O_RDONLY);
                                if (fd == -1) {
                                        g_printerr("Error opening file %s: %m\n", filename);
                                }
                        }
                        if (fd != -1) {
                                bool r = process_file(fd, codepoints, plain, quiet, repeat);
                                close(fd);
                                if (!r)
                                        break;
                        }
                }

                g_strfreev(filenames);
                exit_status = EXIT_SUCCESS;
        } else {
                if (process_file(STDIN_FILENO, codepoints, plain, quiet, repeat))
                        exit_status = EXIT_SUCCESS;
        }

        if (statistics) {
                for (unsigned int s = VTE_SEQ_NONE + 1; s < VTE_SEQ_N; s++) {
                        g_printerr("%\'16" G_GSIZE_FORMAT " %s\n",  seq_stats[s], seq_to_str(s));
                }

                g_printerr("\n");
                for (unsigned int s = 0; s < VTE_CMD_N; s++) {
                        if (cmd_stats[s] > 0) {
                                g_printerr("%\'16" G_GSIZE_FORMAT " %s%s\n",
                                           cmd_stats[s],
                                           cmd_to_str(s),
                                           s >= VTE_CMD_NOP_FIRST ? " [NOP]" : "");
                        }
                }
        }

        if (benchmark) {
                g_array_sort(bench_times,
                             [](void const* p1, void const* p2) -> int {
                                     int64_t const t1 = *(int64_t const*)p1;
                                     int64_t const t2 = *(int64_t const*)p2;
                                     return t1 == t2 ? 0 : (t1 < t2 ? -1 : 1);
                             });

                int64_t total_time = 0;
                for (unsigned int i = 0; i < bench_times->len; ++i)
                        total_time += g_array_index(bench_times, int64_t, i);

                g_printerr("\nTimes: best %\'" G_GINT64_FORMAT "µs "
                           "worst %\'" G_GINT64_FORMAT "µs "
                           "average %\'" G_GINT64_FORMAT "µs\n",
                           g_array_index(bench_times, int64_t, 0),
                           g_array_index(bench_times, int64_t, bench_times->len - 1),
                           total_time / (int64_t)bench_times->len);
                for (unsigned int i = 0; i < bench_times->len; ++i)
                        g_printerr("  %\'" G_GINT64_FORMAT "µs\n",
                                   g_array_index(bench_times, int64_t, i));
        }

        g_array_free(bench_times,true);

        return exit_status;
}
