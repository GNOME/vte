/*
 * Copyright (C) 2003 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* The interfaces in this file are subject to change at any time. */

#ident "$Id$"

#include "../config.h"
#include <sys/types.h>
#include <string.h>
#include <glib.h>
#include "buffer.h"
#include "vteconv.h"

struct _VteConv {
	GIConv conv;
	gboolean in_unichar, out_unichar;
	struct _vte_buffer *in_scratch, *out_scratch;
};

VteConv
_vte_conv_open(const char *target, const char *source)
{
	VteConv ret;
	GIConv conv;
	gboolean in_unichar, out_unichar;
	const char *real_target, *real_source;

	in_unichar = FALSE;
	out_unichar = FALSE;
	real_source = source;
	real_target = target;
	if (strcmp(target, VTE_CONV_GUNICHAR_TYPE) == 0) {
		real_target = "UTF-8";
		out_unichar = TRUE;
	}
	if (strcmp(source, VTE_CONV_GUNICHAR_TYPE) == 0) {
		real_source = "UTF-8";
		in_unichar = TRUE;
	}
	conv = g_iconv_open(real_target, real_source);
	if (conv == ((GIConv) -1)) {
		return (VteConv) -1;
	}

	ret = g_malloc0(sizeof(struct _VteConv));
	ret->conv = conv;
	ret->in_unichar = in_unichar;
	ret->out_unichar = out_unichar;
	ret->in_scratch = _vte_buffer_new();
	ret->out_scratch = _vte_buffer_new();

	return ret;
}

gint
_vte_conv_close(VteConv converter)
{
	g_iconv_close(converter->conv);
	converter->in_unichar = FALSE;
	converter->out_unichar = FALSE;
	_vte_buffer_free(converter->in_scratch);
	_vte_buffer_free(converter->out_scratch);
	converter->in_scratch = NULL;
	converter->out_scratch = NULL;
	g_free(converter);
}

size_t
_vte_conv(VteConv converter,
	  gchar **inbuf, gsize *inbytes_left,
	  gchar **outbuf, gsize *outbytes_left)
{
	size_t ret;
	gchar *p;
	gunichar *u, c;
	gchar *inbuf_start, *inbuf_working;
	gchar *outbuf_start, *outbuf_working;
	gsize inbytes, outbytes, in_converted, out_converted;

	inbuf_start = inbuf_working = *inbuf;
	outbuf_start = outbuf_working = *outbuf;
	inbytes = *inbytes_left;
	outbytes = *outbytes_left;
	in_converted = 0;
	out_converted = 0;

	if (converter->in_unichar) {
		_vte_buffer_set_minimum_size(converter->in_scratch,
					     inbytes * VTE_UTF8_BPC);
		u = (gunichar*) *inbuf;
		p = converter->in_scratch->bytes;
		while (((gchar*)u) < (*inbuf + *inbytes_left)) {
			p += g_unichar_to_utf8(*u, p);
			u++;
		}
		inbuf_start = inbuf_working = converter->in_scratch->bytes;
		inbytes = ((unsigned char*) p) - converter->in_scratch->bytes;
		in_converted = ((gchar*)u - (gchar*)*inbuf);
	}

	if (converter->out_unichar) {
		outbytes = *outbytes_left * VTE_UTF8_BPC;
		_vte_buffer_set_minimum_size(converter->out_scratch,
					     outbytes);
		outbuf_start = outbuf_working = converter->out_scratch->bytes;
	}

	ret = g_iconv(converter->conv,
		      &inbuf_working, &inbytes,
		      &outbuf_working, &outbytes);

	if (converter->in_unichar) {
		*inbuf += in_converted;
		*inbytes_left -= in_converted;
	} else {
		*inbuf = *inbuf + (inbuf_working - inbuf_start);
		*inbytes_left = *inbytes_left - (inbuf_working - inbuf_start);
	}

	if (converter->out_unichar) {
		p = outbuf_start;
		u = (gunichar*) *outbuf;
		while ((p < outbuf_working) && (*outbytes_left > 0)) {
			c = g_utf8_get_char(p);
			p = g_utf8_next_char(p);
			*u = c;
			u++;
			if (*outbytes_left >= sizeof(gunichar)) {
				*outbytes_left -= sizeof(gunichar);
			} else {
				*outbytes_left = 0;
			}
		}
		*outbuf = (gchar*) u;
	} else {
		*outbuf = *outbuf + (outbuf_working - outbuf_start);
		*outbytes_left = *outbytes_left - (outbuf_working - outbuf_start);
	}

	return ret;
}

#ifdef VTECONV_MAIN
static void
clear(gunichar wide[5], gchar narrow[5])
{
	wide[0] = 'T';
	wide[1] = 'E';
	wide[2] = 'S';
	wide[3] = 'T';
	wide[4] = '\0';
	strcpy(narrow, "test");
}

static int
mixed_strcmp(gunichar *wide, gchar *narrow)
{
	while (*wide && *narrow) {
		if (*wide != *narrow) {
			return -1;
		}
		wide++;
		narrow++;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	gunichar wide_test[5];
	gchar narrow_test[5], buf[10];
	VteConv conv;
	gchar *inbuf, *outbuf;
	gsize inbytes, outbytes;
	int i;

	clear(wide_test, narrow_test);
	memset(buf, 0, sizeof(buf));
	inbuf = narrow_test;
	inbytes = strlen(narrow_test);
	outbuf = buf;
	outbytes = sizeof(buf);
	conv = _vte_conv_open("UTF-8", "ISO-8859-1");
	i = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
	g_assert(inbytes == 0);
	if (strcmp(narrow_test, buf) != 0) {
		g_error("Conversion 1 failed.\n");
	}
	_vte_conv_close(conv);

	clear(wide_test, narrow_test);
	memset(buf, 0, sizeof(buf));
	inbuf = narrow_test;
	inbytes = strlen(narrow_test);
	outbuf = buf;
	outbytes = sizeof(buf);
	conv = _vte_conv_open("ISO-8859-1", "UTF-8");
	i = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
	g_assert(inbytes == 0);
	if (strcmp(narrow_test, buf) != 0) {
		g_error("Conversion 2 failed.\n");
	}
	_vte_conv_close(conv);

	clear(wide_test, narrow_test);
	memset(buf, 0, sizeof(buf));
	inbuf = narrow_test;
	inbytes = strlen(narrow_test);
	outbuf = (gchar*) wide_test;
	outbytes = sizeof(wide_test);
	conv = _vte_conv_open(VTE_CONV_GUNICHAR_TYPE, "ISO-8859-1");
	i = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
	g_assert(inbytes == 0);
	if (mixed_strcmp(wide_test, narrow_test) != 0) {
		g_error("Conversion 3 failed.\n");
	}
	_vte_conv_close(conv);

	clear(wide_test, narrow_test);
	memset(buf, 0, sizeof(buf));
	inbuf = (gchar*) wide_test;
	inbytes = 4 * sizeof(gunichar);
	outbuf = buf;
	outbytes = sizeof(buf);
	conv = _vte_conv_open("ISO-8859-1", VTE_CONV_GUNICHAR_TYPE);
	i = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
	g_assert(inbytes == 0);
	if (mixed_strcmp(wide_test, buf) != 0) {
		g_error("Conversion 4 failed.\n");
	}
	_vte_conv_close(conv);

	return 0;
}
#endif
