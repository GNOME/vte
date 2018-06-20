/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
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

/*
 * This file used to contain a full iso2022 decoder which was removed for
 * version 0.40. Now it only performs input conversion from the given
 * character encoding. TODO: probably this layer could be removed completely.
 */

#include <config.h>
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include "debug.h"
#include "buffer.h"
#include "iso2022.h"
#include "vteconv.h"
#include "vtedefines.hh"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include <glib/gi18n-lib.h>

/* An invalid codepoint. */
#define INVALID_CODEPOINT 0xFFFD

struct _vte_iso2022_state {
	const gchar *codeset, *native_codeset, *utf8_codeset, *target_codeset;
	VteConv conv;
};

struct _vte_iso2022_state *
_vte_iso2022_state_new(const char *native_codeset)
{
	struct _vte_iso2022_state *state;

	state = g_slice_new0(struct _vte_iso2022_state);
	state->native_codeset = state->codeset = g_intern_string(native_codeset);
	if (native_codeset == NULL) {
                const char *codeset;
		g_get_charset(&codeset);
		state->native_codeset = state->codeset = g_intern_string(codeset);
        }
	state->utf8_codeset = g_intern_string("UTF-8");
	state->target_codeset = "UTF-8";
	_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
			"Native codeset \"%s\", currently %s\n",
			state->native_codeset, state->codeset);
	state->conv = _vte_conv_open(state->target_codeset, state->codeset);
	if (state->conv == VTE_INVALID_CONV) {
		g_warning(_("Unable to convert characters from %s to %s."),
			  state->codeset, state->target_codeset);
		_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
				"Using UTF-8 instead.\n");
		state->codeset = state->utf8_codeset;
		state->conv = _vte_conv_open(state->target_codeset,
					     state->codeset);
		if (state->conv == VTE_INVALID_CONV) {
			g_error(_("Unable to convert characters from %s to %s."),
				state->codeset, state->target_codeset);
		}
	}
	return state;
}

void
_vte_iso2022_state_free(struct _vte_iso2022_state *state)
{
	if (state->conv != VTE_INVALID_CONV) {
		_vte_conv_close(state->conv);
	}
	g_slice_free(struct _vte_iso2022_state, state);
}

void
_vte_iso2022_state_set_codeset(struct _vte_iso2022_state *state,
			       const char *codeset)
{
	VteConv conv;

	g_return_if_fail(state != NULL);
	g_return_if_fail(codeset != NULL);
	g_return_if_fail(strlen(codeset) > 0);

	_vte_debug_print(VTE_DEBUG_SUBSTITUTION, "%s\n", codeset);
	conv = _vte_conv_open(state->target_codeset, codeset);
	if (conv == VTE_INVALID_CONV) {
		g_warning(_("Unable to convert characters from %s to %s."),
			  codeset, state->target_codeset);
		return;
	}
	if (state->conv != VTE_INVALID_CONV) {
		_vte_conv_close(state->conv);
	}
	state->codeset = g_intern_string (codeset);
	state->conv = conv;
}

const char *
_vte_iso2022_state_get_codeset(struct _vte_iso2022_state *state)
{
	return state->codeset;
}

gsize
_vte_iso2022_process(struct _vte_iso2022_state *state,
                     const guchar *cdata, gsize length,
                     VteByteArray *unibuf)
{

		auto inbuf = cdata;
		size_t inbytes = length;
		_vte_byte_array_set_minimum_size(unibuf,
                                                 VTE_UTF8_BPC * length);
		auto outbuf = unibuf->data;
		size_t outbytes = unibuf->len;
                bool stop = false;
		do {
			auto converted = _vte_conv(state->conv,
                                              &inbuf, &inbytes,
                                              &outbuf, &outbytes);
			switch (converted) {
			case ((gsize)-1):
				switch (errno) {
				case EILSEQ: {
                                        /* Munge the input. */
                                        inbuf++;
                                        inbytes--;
                                        auto l = g_unichar_to_utf8(INVALID_CODEPOINT, (char*)outbuf);
                                        outbuf += l;
                                        outbytes -= l;
					break;
                                }
				case EINVAL:
					/* Incomplete. Save for later. */
					stop = true;
					break;
				case E2BIG:
					/* Should never happen. */
					g_assert_not_reached();
					break;
				default:
					/* Should never happen. */
					g_assert_not_reached();
					break;
				}
			default:
				break;
			}
		} while ((inbytes > 0) && !stop);

                /* FIXMEchpe this code used to skip NUL bytes,
                 * while the _vte_conv call passes NUL bytes through
                 * specifically. What's goint on!?
                 */

		/* Done. */
		auto processed = length - inbytes;
                unibuf->len = unibuf->len - outbytes;

	_vte_debug_print(VTE_DEBUG_SUBSTITUTION,
                        "Consuming %ld bytes.\n", (long) processed);
        return processed;
}
