/*
 * Copyright (C) 2003 Red Hat, Inc.
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

/* The interfaces in this file are subject to change at any time. */

#include "config.h"

#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include "buffer.h"
#include "vteconv.h"
#include "vtedefines.hh"

struct _VteConv {
	GIConv conv;
};

VteConv
_vte_conv_open(const char *real_target, const char *real_source)
{
	VteConv ret;
	GIConv conv;

	conv = NULL;
        char *translit_target = g_strdup_printf ("%s//translit", real_target);
        conv = g_iconv_open(translit_target, real_source);
        g_free (translit_target);
        if (conv == ((GIConv) -1)) {
                conv = g_iconv_open(real_target, real_source);
        }
        if (conv == ((GIConv) -1)) {
                return VTE_INVALID_CONV;
        }

	/* Set up the descriptor. */
	ret = g_slice_new0(struct _VteConv);
        g_assert((conv != NULL) && (conv != ((GIConv) -1)));
        ret->conv = conv;

	return ret;
}

gint
_vte_conv_close(VteConv converter)
{
	g_assert(converter != NULL);
	g_assert(converter != VTE_INVALID_CONV);

	/* Close the underlying descriptor, if there is one. */
	if (converter->conv != (GIConv)-1) {
		g_iconv_close(converter->conv);
	}

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
	gchar *work_inbuf_start, *work_inbuf_working;
	gchar *work_outbuf_start, *work_outbuf_working;
	gsize work_inbytes, work_outbytes;

	g_assert(converter != NULL);
	g_assert(converter != VTE_INVALID_CONV);

	work_inbuf_start = work_inbuf_working = *(char**)inbuf;
	work_outbuf_start = work_outbuf_working = *(char**)outbuf;
	work_inbytes = *inbytes_left;
	work_outbytes = *outbytes_left;

	/* Call the underlying conversion. */
	ret = 0;
	do {
		tmp = g_iconv(converter->conv,
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

        /* Pass on the output results. */
        *outbuf = (guchar*)work_outbuf_working;
        *outbytes_left -= (work_outbuf_working - work_outbuf_start);

        /* Pass on the input results. */
        *inbuf = (const guchar*)work_inbuf_working;
        *inbytes_left -= (work_inbuf_working - work_inbuf_start);

	return ret;
}
