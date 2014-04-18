/*
 * Copyright © 2014 Christian Persch
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

#include "config.h"

#include "vteti.h"

#include <limits.h>

#include <ncurses.h>
#include <term.h>
#include <term_entry.h>
#include <tic.h>

struct _vte_terminfo
{
        int ref_count;
        TERMTYPE termtype;
};

#define VTE_TERMINFO_VARTYPE_MASK (~(VTE_TERMINFO_VARTYPE_BOOLEAN | VTE_TERMINFO_VARTYPE_NUMERIC | VTE_TERMINFO_VARTYPE_STRING))

static void
_vte_terminfo_destroy(struct _vte_terminfo *terminfo)
{
        if (terminfo == NULL)
                return;

        _nc_free_termtype(&terminfo->termtype);
        g_slice_free(struct _vte_terminfo, terminfo);
}

static struct _vte_terminfo *
_vte_terminfo_create(const char *term)
{
        struct _vte_terminfo *terminfo;
        char filename[PATH_MAX];
        int r;

        g_return_val_if_fail(term != NULL, NULL);

        terminfo = g_slice_new0(struct _vte_terminfo);
        terminfo->ref_count = 1;

        r = _nc_read_entry(term, filename, &terminfo->termtype);
        if (r != 1) {
                _vte_terminfo_destroy(terminfo);
                return NULL;
        }

        return terminfo;
}

/*
 * _vte_terminfo_ref:
 * @terminfo: a #_vte_terminfo
 *
 * Increases the ref count of @terminfo.
 *
 * Returns: @terminfo
 */
struct _vte_terminfo *
_vte_terminfo_ref(struct _vte_terminfo *terminfo)
{
        g_return_val_if_fail(terminfo != NULL, NULL);
        g_return_val_if_fail(terminfo->ref_count > 0, NULL);

        g_atomic_int_inc(&terminfo->ref_count);
        return terminfo;
}

/*
 * _vte_terminfo_is_xterm_like:
 * @terminfo: a #_vte_terminfo
 *
 * Checks whether the terminfo is for a xterm or xterm-like terminal.
 *
 * Returns: %TRUE if the terminfo is for a xterm or xterm-like terminal
 */
gboolean
_vte_terminfo_is_xterm_like(struct _vte_terminfo *terminfo)
{
        /* const */ char *first_name;

        g_return_val_if_fail(terminfo != NULL, FALSE);
        first_name = _nc_first_name(terminfo->termtype.term_names);

        return first_name != NULL &&
                (g_str_has_prefix(first_name, "xterm") ||
                 g_str_has_prefix(first_name, "vte") ||
                 g_str_equal(first_name, "dtterm") /* FIXME: obsolete? */);
}

/*
 * _vte_terminfo_get_boolean:
 * @terminfo: a #_vte_terminfo
 * @var: a terminfo variable of type %VTE_TERMINFO_VARTYPE_BOOLEAN
 *
 * Looks up the boolean terminfo capability @var.
 *
 * Returns: the value of the capability, or %FALSE if the capability is not set
 */
gboolean
_vte_terminfo_get_boolean(struct _vte_terminfo *terminfo,
                          guint variable)
{
        NCURSES_SBOOL b;

        g_return_val_if_fail(terminfo != NULL, FALSE);
        g_return_val_if_fail(variable & VTE_TERMINFO_VARTYPE_BOOLEAN, FALSE);
        variable &= VTE_TERMINFO_VARTYPE_MASK;
        g_return_val_if_fail(variable < BOOLCOUNT, FALSE);

        b = terminfo->termtype.Booleans[variable];
        return VALID_BOOLEAN(b) ? b != 0 : FALSE;
}

/*
 * _vte_terminfo_get_numeric:
 * @terminfo: a #_vte_terminfo
 * @var: a terminfo variable of type %VTE_TERMINFO_VARTYPE_NUMERIC
 *
 * Looks up the numeric terminfo capability @var.
 *
 * Returns: the value of the capability, or -1 if the capability is not set
 */
int
_vte_terminfo_get_numeric(struct _vte_terminfo *terminfo,
                          guint variable)
{
        short n;

        g_return_val_if_fail(terminfo != NULL, -1);
        g_return_val_if_fail(variable & VTE_TERMINFO_VARTYPE_NUMERIC, -1);
        variable &= VTE_TERMINFO_VARTYPE_MASK;
        g_return_val_if_fail(variable < NUMCOUNT, -1);

        n = terminfo->termtype.Numbers[variable];
        return VALID_NUMERIC(n) ? (int)n : -1;
}

/*
 * _vte_terminfo_get_string:
 * @terminfo: a #_vte_terminfo
 * @var: a terminfo variable of type %VTE_TERMINFO_VARTYPE_STRING
 *
 * Looks up the string terminfo capability @var.
 *
 * Returns: the value of the capability, or %NULL if the capability is not set
 */
const char *
_vte_terminfo_get_string(struct _vte_terminfo *terminfo,
                         guint variable)
{
        /* const */ char *str;

        g_return_val_if_fail(terminfo != NULL, NULL);
        g_return_val_if_fail(variable & VTE_TERMINFO_VARTYPE_STRING, NULL);
        variable &= VTE_TERMINFO_VARTYPE_MASK;
        g_return_val_if_fail(variable < STRCOUNT, NULL);

        str = terminfo->termtype.Strings[variable];
        return VALID_STRING(str) ? str : NULL;
}

/*
 * _vte_terminfo_get_boolean_by_cap:
 * @terminfo: a #_vte_terminfo
 * @cap: a capability string for a boolean capability
 * @compat: %TRUE if @cap is a termcap capability rather than a terminfo capability
 *
 * Looks up the boolean capability @cap.
 * If @compat is %FALSE, @cap is a terminfo capability, else a termcap capability.
 *
 * Returns: the value of the capability, or %FALSE if the capability is not set
 */
gboolean
_vte_terminfo_get_boolean_by_cap(struct _vte_terminfo *terminfo,
                                 const char *cap,
                                 gboolean compat)
{
        NCURSES_SBOOL b;
        const struct name_table_entry *e;

        g_return_val_if_fail(terminfo != NULL, FALSE);
        g_return_val_if_fail(cap != NULL, FALSE);

        e = _nc_find_entry(cap, _nc_get_hash_table(compat));
        if (e == NULL)
                return FALSE;

        g_return_val_if_fail(e->nte_index < NUM_BOOLEANS(&terminfo->termtype), FALSE);

        b = terminfo->termtype.Booleans[e->nte_index];
        return VALID_BOOLEAN(b) ? b != 0 : FALSE;
}

/*
 * _vte_terminfo_get_numeric_by_cap:
 * @terminfo: a #_vte_terminfo
 * @cap: a capability string for a numeric capability
 * @compat: %TRUE if @cap is a termcap capability rather than a terminfo capability
 *
 * Looks up the numeric capability @cap.
 * If @compat is %FALSE, @cap is a terminfo capability, else a termcap capability.
 *
 * Returns: the value of the capability, or -1 if the capability is not set
 */
int
_vte_terminfo_get_numeric_by_cap(struct _vte_terminfo *terminfo,
                                 const char *cap,
                                 gboolean compat)
{
        short n;
        const struct name_table_entry *e;

        g_return_val_if_fail(terminfo != NULL, -1);
        g_return_val_if_fail(cap != NULL, -1);

        e = _nc_find_entry(cap, _nc_get_hash_table(compat));
        if (e == NULL)
                return -1;

        g_return_val_if_fail(e->nte_index < NUM_NUMBERS(&terminfo->termtype), -1);

        n = terminfo->termtype.Numbers[e->nte_index];
        return VALID_NUMERIC(n) ? (int)n : -1;
}

/*
 * _vte_terminfo_get_string_by_cap:
 * @terminfo: a #_vte_terminfo
 * @cap: a capability string for a string capability
 * @compat: %TRUE if @cap is a termcap capability rather than a terminfo capability
 *
 * Looks up the string capability @cap.
 * If @compat is %FALSE, @cap is a terminfo capability, else a termcap capability.
 *
 * Returns: the value of the capability, or %NULL if the capability is not set
 */
const char *
_vte_terminfo_get_string_by_cap(struct _vte_terminfo *terminfo,
                                const char *cap,
                                gboolean compat)
{
        /* const */ char *str;
        const struct name_table_entry *e;

        g_return_val_if_fail(terminfo != NULL, NULL);
        g_return_val_if_fail(cap != NULL, NULL);

        e = _nc_find_entry(cap, _nc_get_hash_table(compat));
        if (e == NULL)
                return NULL;

        g_return_val_if_fail(e->nte_index < NUM_STRINGS(&terminfo->termtype), NULL);

        str = terminfo->termtype.Strings[e->nte_index];
        return VALID_STRING(str) ? str : NULL;
}

/*
 * _vte_terminfo_foreach_boolean_func:
 * @terminfo: the #_vte_terminfo
 * @cap: the terminfo capability
 * @compat_cap: the corresponding termcap capability, or it does not exist in termcap
 * @value: the value of the capability in @terminfo
 * @user_data: user data
 *
 * A function type to pass to _vte_terminfo_foreach_boolean().
 */

/*
 * _vte_terminfo_foreach_boolean:
 * @terminfo: a #_vte_terminfo
 * @include_extensions: whether to include extended capabilities
 * @func: a #_vte_terminfo_foreach_boolean_func
 * @user_data: user data to pass to @func
 *
 * Iterates over all boolean capabilities that are set in @terminfo.
 * If @include_extensions is %TRUE, this includes extended capabilities, if there are any.
 */
void
_vte_terminfo_foreach_boolean(struct _vte_terminfo *terminfo,
                              gboolean include_extensions,
                              _vte_terminfo_foreach_boolean_func func,
                              gpointer user_data)
{
        NCURSES_SBOOL b;
        TERMTYPE *tt;
        int i;

        g_return_if_fail(terminfo != NULL);
        g_return_if_fail(func != NULL);

        tt = &terminfo->termtype;
        for_each_boolean(i, tt) {
                if (G_UNLIKELY (i >= BOOLCOUNT && !include_extensions))
                        continue;

                b = tt->Booleans[i];
                if (!VALID_BOOLEAN(b))
                        continue;

                func(terminfo,
                     ExtBoolname(tt, i, boolnames),
                     i < BOOLCOUNT ? boolcodes[i] : "",
                     b != 0,
                     user_data);
        }
}

/*
 * _vte_terminfo_foreach_numeric_func:
 * @terminfo: the #_vte_terminfo
 * @cap: the terminfo capability
 * @compat_cap: the corresponding termcap capability, or it does not exist in termcap
 * @value: the value of the capability in @terminfo
 * @user_data: user data
 *
 * A function type to pass to _vte_terminfo_foreach_numeric().
 */

/*
 * _vte_terminfo_foreach_numeric:
 * @terminfo: a #_vte_terminfo
 * @include_extensions: whether to include extended capabilities
 * @func: a #_vte_terminfo_foreach_numeric_func
 * @user_data: user data to pass to @func
 *
 * Iterates over all numeric capabilities that are set in @terminfo.
 * If @include_extensions is %TRUE, this includes extended capabilities, if there are any.
 */
void
_vte_terminfo_foreach_numeric(struct _vte_terminfo *terminfo,
                              gboolean include_extensions,
                              _vte_terminfo_foreach_numeric_func func,
                              gpointer user_data)
{
        short n;
        TERMTYPE *tt;
        int i;

        g_return_if_fail(terminfo != NULL);
        g_return_if_fail(func != NULL);

        tt = &terminfo->termtype;
        for_each_number(i, tt) {
                if (G_UNLIKELY (i >= NUMCOUNT && !include_extensions))
                        continue;

                n = tt->Numbers[i];
                if (!VALID_NUMERIC(n))
                        continue;

                func(terminfo,
                     ExtNumname(tt, i, numnames),
                     i < NUMCOUNT ? numcodes[i] : "",
                     n,
                     user_data);
        }
}

/*
 * _vte_terminfo_foreach_string_func:
 * @terminfo: the #_vte_terminfo
 * @cap: the terminfo capability
 * @compat_cap: the corresponding termcap capability, or it does not exist in termcap
 * @value: the value of the capability in @terminfo
 * @user_data: user data
 *
 * A function type to pass to _vte_terminfo_foreach_string().
 */

/*
 * _vte_terminfo_foreach_string:
 * @terminfo: a #_vte_terminfo
 * @include_extensions: whether to include extended capabilities
 * @func: a #_vte_terminfo_foreach_string_func
 * @user_data: user data to pass to @func
 *
 * Iterates over all string capabilities that are set in @terminfo.
 * If @include_extensions is %TRUE, this includes extended capabilities, if there are any.
 */
void
_vte_terminfo_foreach_string(struct _vte_terminfo *terminfo,
                             gboolean include_extensions,
                             _vte_terminfo_foreach_string_func func,
                             gpointer user_data)
{
        /* const */ char *str;
        TERMTYPE *tt;
        int i;

        g_return_if_fail(terminfo != NULL);
        g_return_if_fail(func != NULL);

        tt = &terminfo->termtype;
        for_each_string(i, tt) {
                if (G_UNLIKELY (i >= STRCOUNT && !include_extensions))
                        continue;

                str = tt->Strings[i];
                if (!VALID_STRING(str))
                        continue;

                func(terminfo,
                     ExtStrname(tt, i, strnames),
                     i < STRCOUNT ? strcodes[i] : "",
                     str,
                     user_data);
        }
}

const char *
_vte_terminfo_sequence_to_string(const char *str)
{
#if defined(VTE_DEBUG) || defined(TERMINFO_MAIN)
        static const char codes[][6] = {
                "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
                "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
                "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
                "CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US",
                "SPACE"
        };
        static GString *buf;
        int i;

        if (str == NULL)
                return "(nil)";

        if (buf == NULL)
                buf = g_string_new(NULL);

        g_string_truncate(buf, 0);
        for (i = 0; str[i]; i++) {
                guint8 c = (guint8)str[i];
                if (i > 0)
                        g_string_append_c(buf, ' ');

                if (c <= 0x20)
                        g_string_append(buf, codes[c]);
                else if (c == 0x7f)
                        g_string_append(buf, "DEL");
                else if (c >= 0x80)
                        g_string_append_printf(buf, "\\%02x ", c);
                else
                        g_string_append_c(buf, c);
        }

        return buf->str;
#else
        return NULL;
#endif /* VTE_DEBUG || TERMINFO_MAIN */
}

/* Terminfo cache */

static GHashTable *_vte_terminfo_cache = NULL;

/*
 * _vte_terminfo_new:
 * @term: a terminfo name
 *
 * Looks up the #_vte_terminfo for @term in a cache, and if it does not exist,
 * creates it.
 *
 * Returns: (transfer full): a reference to a #_vte_terminfo
 */
struct _vte_terminfo *
_vte_terminfo_new(const char *term)
{
        struct _vte_terminfo *terminfo;

        if (g_once_init_enter(&_vte_terminfo_cache)) {
                GHashTable *cache;

                cache = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              (GDestroyNotify)g_free,
                                              (GDestroyNotify)_vte_terminfo_destroy);
                g_once_init_leave(&_vte_terminfo_cache, cache);
        }

        terminfo = g_hash_table_lookup(_vte_terminfo_cache, term);
        if (terminfo == NULL) {
                terminfo = _vte_terminfo_create(term);
                g_hash_table_insert(_vte_terminfo_cache, g_strdup(term), terminfo);
        }

        return _vte_terminfo_ref(terminfo);
}

/*
 * _vte_terminfo_ref:
 * @terminfo: a #_vte_terminfo
 *
 * Decreases the ref count of @terminfo, and frees it if this had been
 * the last reference.
 */
void
_vte_terminfo_unref(struct _vte_terminfo *terminfo)
{
        if (g_atomic_int_dec_and_test(&terminfo->ref_count))
                g_hash_table_remove(_vte_terminfo_cache, terminfo);
}

/* Main */

#ifdef TERMINFO_MAIN

#include <string.h>

static void
dump_boolean(struct _vte_terminfo *terminfo,
             const char *cap,
             const char *compat_cap,
             gboolean value,
             gpointer user_data)
{
        g_print("%-8s [ %2s ] = %s\n", cap, compat_cap ? compat_cap : "", value ? "true":"false");
}

static void
dump_numeric(struct _vte_terminfo *terminfo,
             const char *cap,
             const char *compat_cap,
             int value,
             gpointer user_data)
{
        g_print("%-8s [ %2s ] = %d\n", cap, compat_cap ? compat_cap : "", value);
}

static void
dump_string(struct _vte_terminfo *terminfo,
            const char *cap,
            const char *compat_cap,
            const char *val,
            gpointer user_data)
{
        g_print("%-8s [ %2s ] = %s\n", cap, compat_cap ? compat_cap : "",
                _vte_terminfo_sequence_to_string(val));
}

static void
dump(struct _vte_terminfo *terminfo)
{
        _vte_terminfo_foreach_boolean(terminfo, TRUE, dump_boolean, NULL);
        _vte_terminfo_foreach_numeric(terminfo, TRUE, dump_numeric, NULL);
        _vte_terminfo_foreach_string(terminfo, TRUE, dump_string, NULL);
}

int
main(int argc, char *argv[])
{
        struct _vte_terminfo *terminfo;
        const char *str;
        int i;

        if (argc < 2) {
                g_printerr("%s TERM [ATTR..]\n"
                           "  where ATTR are\n"
                           "    :xx for boolean\n"
                           "    #xx for numeric\n"
                           "    =xx for string for display\n"
                           "    +xx for string raw\n", argv[0]);
                return 1;
        }

        terminfo = _vte_terminfo_create(argv[1]);
        if (terminfo == NULL) {
                g_printerr("Terminfo for \"%s\" not found.\n", argv[1]);
                return 1;
        }
        if (argc == 2)
                dump(terminfo);
        else for (i = 2; i < argc; i++) {
                gboolean compat = FALSE;

                if (argv[i][0] == '-') {
                        compat = TRUE;
                        argv[i]++;
                }

                g_print("%s -> ", argv[i] + 1);
                switch (argv[i][0])  {
                case ':':
                        g_print("%s\n",
                                _vte_terminfo_get_boolean_by_cap(terminfo, argv[i] + 1, compat)
                                ? "true" : "false");
                        break;
                case '=':
                case '+':
                        str = _vte_terminfo_get_string_by_cap(terminfo, argv[i] + 1, compat);
                        if (argv[i][0] == '=') {
                                g_print("%s\n", _vte_terminfo_sequence_to_string(str));
                        } else {
                                g_print("%s\n", str);
                        }
                        break;
                case '#':
                        g_print("%d\n", _vte_terminfo_get_numeric_by_cap(terminfo, argv[i] + 1, compat));
                        break;
                default:
                        g_printerr("unrecognised type '%c'\n", argv[i][0]);
                }
        }

        _vte_terminfo_destroy(terminfo);

        return 0;
}

#endif /* TERMINFO_MAIN */
