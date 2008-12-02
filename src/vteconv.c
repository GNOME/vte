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


#include <config.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include "buffer.h"
#include "vteconv.h"
#include "vte-private.h"

typedef size_t (*convert_func)(GIConv converter,
			  const guchar **inbuf,
			  gsize *inbytes_left,
			  guchar **outbuf,
			  gsize *outbytes_left);
struct _VteConv {
	GIConv conv;
	convert_func convert;
	gint (*close)(GIConv converter);
	gboolean in_unichar, out_unichar;
	struct _vte_buffer *in_scratch, *out_scratch;
};

/* We can't use g_utf8_strlen as that's not nul-safe :( */
static glong
_vte_conv_utf8_strlen(const gchar *p, gssize max)
{
	const gchar *q = p + max;
	glong length = -1;
	do {
		length++;
		p = g_utf8_next_char(p);
	} while (p < q);
	return length;
}

/* A bogus UTF-8 to UTF-8 conversion function which attempts to provide the
 * same semantics as g_iconv(). */
static size_t
_vte_conv_utf8_utf8(GIConv converter,
		    const gchar **inbuf,
		    gsize *inbytes_left,
		    gchar **outbuf,
		    gsize *outbytes_left)
{
	gboolean validated;
	const gchar *endptr;
	size_t bytes;
	guint skip;

	/* We don't tolerate shenanigans! */
	g_assert(*outbytes_left >= *inbytes_left);

	/* The only error we can throw is EILSEQ, so check for that here. */
	validated = g_utf8_validate(*inbuf, *inbytes_left, &endptr);

	/* Copy whatever data was validated. */
	bytes = endptr - *inbuf;
	memcpy(*outbuf, *inbuf, bytes);
	*inbuf += bytes;
	*outbuf += bytes;
	*outbytes_left -= bytes;
	*inbytes_left -= bytes;

	/* Return 0 (number of non-reversible conversions performed) if everything
	 * looked good, else EILSEQ. */
	if (validated) {
		return 0;
	}

	/* Determine why the end of the string is not valid.
	 * We are pur b@stards for running g_utf8_next_char() on an
	 * invalid sequence. */
	skip = g_utf8_next_char(*inbuf) - *inbuf;
	if (skip > *inbytes_left) {
		/* We didn't have enough bytes to validate the character.
		 * That qualifies for EINVAL, but only if the part of the
		 * character that we have is a valid prefix to a character.
		 * Differentiating those requires verifying that all the
		 * remaining bytes after this one are UTF-8 continuation
		 * bytes.  Actually even that is not quite enough as not
		 * all continuation bytes are valid in the most strict
		 * interpretation of UTF-8, but we don't care about that.
		 */
		size_t i;

		for (i = 1; i < *inbytes_left; i++)
			if (((*inbuf)[i] & 0xC0) != 0x80) {
				/* Not a continuation byte */
				errno = EILSEQ;
				return (size_t) -1;
			}

		errno = EINVAL;
	} else {
		/* We had enough bytes to validate the character, and
		 * it failed.  It just doesn't look right. */
		errno = EILSEQ;
	}
	return (size_t) -1;
}

/* Open a conversion descriptor which, in addition to normal cases, provides
 * UTF-8 to UTF-8 conversions and a gunichar-compatible source and target
 * encoding. */
VteConv
_vte_conv_open(const char *target, const char *source)
{
	VteConv ret;
	GIConv conv;
	gboolean in_unichar, out_unichar, utf8;
	const char *real_target, *real_source;

	/* No shenanigans. */
	g_assert(target != NULL);
	g_assert(source != NULL);
	g_assert(strlen(target) > 0);
	g_assert(strlen(source) > 0);

	/* Assume normal iconv usage. */
	in_unichar = FALSE;
	out_unichar = FALSE;
	real_source = source;
	real_target = target;

	/* Determine if we need to convert gunichars to UTF-8 on input. */
	if (strcmp(target, VTE_CONV_GUNICHAR_TYPE) == 0) {
		real_target = "UTF-8";
		out_unichar = TRUE;
	}

	/* Determine if we need to convert UTF-8 to gunichars on output. */
	if (strcmp(source, VTE_CONV_GUNICHAR_TYPE) == 0) {
		real_source = "UTF-8";
		in_unichar = TRUE;
	}

	/* Determine if this is a UTF-8 to UTF-8 conversion. */
	utf8 = ((g_ascii_strcasecmp(real_target, "UTF-8") == 0) &&
		(g_ascii_strcasecmp(real_source, "UTF-8") == 0));

	/* If we're doing UTF-8 to UTF-8, just use a dummy function which
	 * checks for bad data. */
	conv = NULL;
	if (!utf8) {
		char *translit_target = g_strdup_printf ("%s//translit", real_target);
		conv = g_iconv_open(translit_target, real_source);
		g_free (translit_target);
		if (conv == ((GIConv) -1)) {
			conv = g_iconv_open(real_target, real_source);
		}
		if (conv == ((GIConv) -1)) {
			return VTE_INVALID_CONV;
		}
	}

	/* Set up the descriptor. */
	ret = g_slice_new0(struct _VteConv);
	if (utf8) {
		ret->conv = NULL;
		ret->convert = (convert_func) _vte_conv_utf8_utf8;
		ret->close = NULL;
	} else {
		g_assert((conv != NULL) && (conv != ((GIConv) -1)));
		ret->conv = conv;
		ret->convert = (convert_func) g_iconv;
		ret->close = g_iconv_close;
	}

	/* Initialize other elements. */
	ret->in_unichar = in_unichar;
	ret->out_unichar = out_unichar;

	/* Create scratch buffers. */
	ret->in_scratch = _vte_buffer_new();
	ret->out_scratch = _vte_buffer_new();

	return ret;
}

gint
_vte_conv_close(VteConv converter)
{
	g_assert(converter != NULL);
	g_assert(converter != VTE_INVALID_CONV);

	/* Close the underlying descriptor, if there is one. */
	if (converter->conv != NULL) {
		g_assert(converter->close != NULL);
		converter->close(converter->conv);
	}

	/* Free the scratch buffers. */
	_vte_buffer_free(converter->in_scratch);
	_vte_buffer_free(converter->out_scratch);

	/* Free the structure itself. */
	g_slice_free(struct _VteConv, converter);

	return 0;
}

size_t
_vte_conv(VteConv converter,
	  const guchar **inbuf, gsize *inbytes_left,
	  guchar **outbuf, gsize *outbytes_left)
{
	size_t ret, tmp;
	const guchar *work_inbuf_start, *work_inbuf_working;
	guchar *work_outbuf_start, *work_outbuf_working;
	gsize work_inbytes, work_outbytes;
	gsize in_converted, out_converted;

	g_assert(converter != NULL);
	g_assert(converter != VTE_INVALID_CONV);

	work_inbuf_start = work_inbuf_working = *inbuf;
	work_outbuf_start = work_outbuf_working = *outbuf;
	work_inbytes = *inbytes_left;
	work_outbytes = *outbytes_left;
	in_converted = 0;
	out_converted = 0;

	/* Possibly convert the input data from gunichars to UTF-8. */
	if (converter->in_unichar) {
		int i, char_count;
		guchar *p, *end;
		gunichar *g;
		/* Make sure the scratch buffer has enough space. */
		char_count = *inbytes_left / sizeof(gunichar);
		_vte_buffer_set_minimum_size(converter->in_scratch,
					     (char_count + 1) * VTE_UTF8_BPC);
		/* Convert the incoming text. */
		g = (gunichar*) *inbuf;
		p = converter->in_scratch->bytes;
		end = p + (char_count + 1) * VTE_UTF8_BPC;
		for (i = 0; i < char_count; i++) {
			p += g_unichar_to_utf8(g[i], (gchar *)p);
			g_assert(p <= end);
		}
		/* Update our working pointers. */
		work_inbuf_start = converter->in_scratch->bytes;
		work_inbuf_working = work_inbuf_start;
		work_inbytes = p - work_inbuf_start;
	}

	/* Possibly set the output pointers to point at our scratch buffer. */
	if (converter->out_unichar) {
		work_outbytes = *outbytes_left * VTE_UTF8_BPC;
		_vte_buffer_set_minimum_size(converter->out_scratch,
					     work_outbytes);
		work_outbuf_start = converter->out_scratch->bytes;
		work_outbuf_working = work_outbuf_start;
	}

	/* Call the underlying conversion. */
	ret = 0;
	do {
		tmp = converter->convert(converter->conv,
					 &work_inbuf_working,
					 &work_inbytes,
					 &work_outbuf_working,
					 &work_outbytes);
		if (tmp == (size_t) -1) {
			/* Check for zero bytes, which we pass right through. */
			if (errno == EILSEQ) {
				if ((work_inbytes > 0) &&
				    (work_inbuf_working[0] == '\0') &&
				    (work_outbytes > 0)) {
					work_outbuf_working[0] = '\0';
					work_outbuf_working++;
					work_inbuf_working++;
					work_outbytes--;
					work_inbytes--;
					ret++;
				} else {
					/* No go. */
					ret = -1;
					break;
				}
			} else {
				ret = -1;
				break;
			}
		} else {
			ret += tmp;
			break;
		}
	} while (work_inbytes > 0);

	/* We can't handle this particular failure, and it should
	 * never happen.  (If it does, our caller needs fixing.)  */
	g_assert((ret != (size_t)-1) || (errno != E2BIG));

	/* Possibly convert the output from UTF-8 to gunichars. */
	if (converter->out_unichar) {
		int  left = *outbytes_left;
		gunichar *g;
		gchar *p;

		g = (gunichar*) *outbuf;
		for(p = (gchar *)work_outbuf_start;
				p < (gchar *)work_outbuf_working;
				p = g_utf8_next_char(p)) {
		       g_assert(left>=0);
		       *g++ = g_utf8_get_char(p);
		       left -= sizeof(gunichar);
		}
		*outbytes_left = left;
		*outbuf = (guchar*) g;
	} else {
		/* Pass on the output results. */
		*outbuf = work_outbuf_working;
		*outbytes_left -= (work_outbuf_working - work_outbuf_start);
	}

	/* Advance the input pointer to the right place. */
	if (converter->in_unichar) {
		/* Get an idea of how many characters were converted, and
		 * advance the pointer as required. */
		int chars;
		chars = _vte_conv_utf8_strlen((const gchar *)work_inbuf_start,
					      work_inbuf_working - work_inbuf_start);
		*inbuf += (sizeof(gunichar) * chars);
		*inbytes_left -= (sizeof(gunichar) * chars);
	} else {
		/* Pass on the input results. */
		*inbuf = work_inbuf_working;
		*inbytes_left -= (work_inbuf_working - work_inbuf_start);
	}

	return ret;
}

size_t
_vte_conv_cu(VteConv converter,
	     const guchar **inbuf, gsize *inbytes_left,
	     gunichar **outbuf, gsize *outbytes_left)
{
	return _vte_conv(converter,
			 inbuf, inbytes_left,
			 (guchar**)outbuf, outbytes_left);
}

size_t
_vte_conv_uu(VteConv converter,
	     const gunichar **inbuf, gsize *inbytes_left,
	     gunichar **outbuf, gsize *outbytes_left)
{
	return _vte_conv(converter,
			 (const guchar**)inbuf, inbytes_left,
			 (guchar**)outbuf, outbytes_left);
}

size_t
_vte_conv_uc(VteConv converter,
	     const gunichar **inbuf, gsize *inbytes_left,
	     guchar **outbuf, gsize *outbytes_left)
{
	return _vte_conv(converter,
			 (const guchar**)inbuf, inbytes_left,
			 outbuf, outbytes_left);
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
	char mbyte_test[] = {0xe2, 0x94, 0x80};
	char mbyte_test_break[] = {0xe2, 0xe2, 0xe2};
	int i;

	i = _vte_conv_utf8_strlen("\0\0\0\0", 4);
	g_assert(i == 4);
	i = _vte_conv_utf8_strlen("\0A\0\0", 4);
	g_assert(i == 4);
	i = _vte_conv_utf8_strlen("\0A\0B", 4);
	g_assert(i == 4);
	i = _vte_conv_utf8_strlen("A\0B\0", 4);
	g_assert(i == 4);
	i = _vte_conv_utf8_strlen("ABCDE", 4);
	g_assert(i == 4);

	/* Test g_iconv, no gunichar stuff. */
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

	/* Test g_iconv, no gunichar stuff. */
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

	/* Test g_iconv + gunichar stuff. */
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

	/* Test g_iconv + gunichar stuff. */
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

	/* Test UTF-8 to UTF-8 "conversion". */
	clear(wide_test, narrow_test);
	memset(buf, 0, sizeof(buf));
	inbuf = (gchar*) narrow_test;
	inbytes = strlen(narrow_test);
	outbuf = buf;
	outbytes = sizeof(buf);
	conv = _vte_conv_open("UTF-8", "UTF-8");
	i = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
	g_assert(inbytes == 0);
	if (strcmp(narrow_test, buf) != 0) {
		g_error("Conversion 5 failed.\n");
	}
	_vte_conv_close(conv);

	/* Test zero-byte pass-through. */
	clear(wide_test, narrow_test);
	memset(wide_test, 0, sizeof(wide_test));
	inbuf = (gchar*) wide_test;
	inbytes = 3 * sizeof(gunichar);
	outbuf = narrow_test;
	outbytes = sizeof(narrow_test);
	conv = _vte_conv_open("UTF-8", VTE_CONV_GUNICHAR_TYPE);
	i = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
	g_assert(inbytes == 0);
	if ((narrow_test[0] != 0) ||
	    (narrow_test[1] != 0) ||
	    (narrow_test[2] != 0)) {
		g_error("Conversion 6 failed.\n");
	}
	_vte_conv_close(conv);

	/* Test zero-byte pass-through. */
	clear(wide_test, narrow_test);
	memset(wide_test, 'A', sizeof(wide_test));
	memset(narrow_test, 0, sizeof(narrow_test));
	inbuf = narrow_test;
	inbytes = 3;
	outbuf = (char*)wide_test;
	outbytes = sizeof(wide_test);
	conv = _vte_conv_open(VTE_CONV_GUNICHAR_TYPE, "UTF-8");
	i = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
	g_assert(inbytes == 0);
	if ((wide_test[0] != 0) ||
	    (wide_test[1] != 0) ||
	    (wide_test[2] != 0)) {
		g_error("Conversion 7 failed.\n");
	}
	_vte_conv_close(conv);

	/* Test zero-byte pass-through. */
	clear(wide_test, narrow_test);
	memset(wide_test, 'A', sizeof(wide_test));
	memset(narrow_test, 0, sizeof(narrow_test));
	inbuf = narrow_test;
	inbytes = 3;
	outbuf = (char*)wide_test;
	outbytes = sizeof(wide_test);
	conv = _vte_conv_open(VTE_CONV_GUNICHAR_TYPE, "ISO-8859-1");
	i = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
	g_assert(inbytes == 0);
	if ((wide_test[0] != 0) ||
	    (wide_test[1] != 0) ||
	    (wide_test[2] != 0)) {
		g_error("Conversion 8 failed.\n");
	}
	_vte_conv_close(conv);

	/* Test UTF-8 to UTF-8 error reporting, valid multibyte. */
	for (i = 0; i < sizeof(mbyte_test); i++) {
		int ret;
		inbuf = mbyte_test;
		inbytes = i + 1;
		outbuf = buf;
		outbytes = sizeof(buf);
		conv = _vte_conv_open("UTF-8", "UTF-8");
		ret = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
		switch (i) {
		case 0:
			g_assert((ret == -1) && (errno == EINVAL));
			break;
		case 1:
			g_assert((ret == -1) && (errno == EINVAL));
			break;
		case 2:
			g_assert(ret != -1);
			break;
		default:
			g_assert_not_reached();
			break;
		}
		_vte_conv_close(conv);
	}

	/* Test UTF-8 to UTF-8 error reporting, invalid multibyte. */
	for (i = 0; i < sizeof(mbyte_test_break); i++) {
		int ret;
		inbuf = mbyte_test_break;
		inbytes = i + 1;
		outbuf = buf;
		outbytes = sizeof(buf);
		conv = _vte_conv_open("UTF-8", "UTF-8");
		ret = _vte_conv(conv, &inbuf, &inbytes, &outbuf, &outbytes);
		_vte_conv_close(conv);
		switch (i) {
		case 0:
			g_assert((ret == -1) && (errno == EINVAL));
			break;
		case 1:
			g_assert((ret == -1) && (errno == EINVAL));
			break;
		case 2:
			g_assert((ret == -1) && (errno == EILSEQ));
			break;
		default:
			g_assert_not_reached();
			break;
		}
	}

	return 0;
}
#endif
