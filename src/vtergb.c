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


#include "../config.h"

#include <gdk/gdk.h>
#include <glib.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "vtergb.h"

struct _vte_rgb_buffer_p {
	guchar *pixels;
	gint width, height, stride;
	gint length;
};

struct _vte_rgb_buffer *
_vte_rgb_buffer_new(gint width, gint height)
{
	struct _vte_rgb_buffer_p *ret;

	ret = g_malloc0(sizeof(*ret));

	width = MAX(width, 1);
	height = MAX(height, 1);

	ret->width = width;
	ret->height = height;
	ret->stride = width * 3;
	ret->length = width * height * 3;
	ret->pixels = g_malloc(ret->length);

	return (struct _vte_rgb_buffer *) ret;
}

void
_vte_rgb_buffer_free(struct _vte_rgb_buffer *buffer)
{
	struct _vte_rgb_buffer_p *buf = (struct _vte_rgb_buffer_p *) buffer;

	g_free(buf->pixels);

	buf->length = 0;
	buf->stride = 0;
	buf->height = 0;
	buf->width = 0;
	buf->pixels = NULL;

	g_free(buf);
}

void
_vte_rgb_buffer_resize(struct _vte_rgb_buffer *buffer,
		       gint minimum_width,
		       gint minimum_height)
{
	struct _vte_rgb_buffer_p *buf = (struct _vte_rgb_buffer_p *) buffer;
	gssize size = minimum_width * minimum_height * 3;

	if (size > buf->length) {
		g_free(buf->pixels);
		buf->length = size;
		buf->pixels = g_malloc(buf->length);
	}

	buf->width = minimum_width;
	buf->height = minimum_height;
	buf->stride = buf->width * 3;
}

void
_vte_rgb_draw_color_rgb(struct _vte_rgb_buffer *buffer,
			gint x, gint y, gint width, gint height,
			guchar r, guchar g, guchar b)
{
	gint row, rows, col, cols;
	gint offset, dest, count;
	guchar *pixels;

	/* Perform a simple clipping check. */
	if (x > buffer->width) {
		return;
	}
	if (y > buffer->height) {
		return;
	}

	/* Find the lower right corner. */
	pixels = buffer->pixels;
	rows = MIN(y + height, buffer->height);
	cols = MIN(x + width, buffer->width);

	/* If we had negative or nonsensical width or height, bail. */
	if (rows <= y) {
		return;
	}
	if (cols <= x) {
		return;
	}

	/* Draw the first row by iteration. */
	dest = y * buffer->stride + x * 3;
	offset = dest;
	for (col = x; col < cols; col++) {
		pixels[dest++] = r;
		pixels[dest++] = g;
		pixels[dest++] = b;
	}

	/* Draw the other rows by copying the data. */
	count = width * 3;
	for (row = y + 1; row < rows; row++) {
		dest = row * buffer->stride + x * 3;
		memmove(pixels + dest, pixels + offset, count);
	}
}

void
_vte_rgb_draw_color(struct _vte_rgb_buffer *buffer,
		    gint x, gint y, gint width, gint height, GdkColor *color)
{
	guchar r, g, b;
	r = MIN(color->red >> 8, 0xff);
	g = MIN(color->green >> 8, 0xff);
	b = MIN(color->blue >> 8, 0xff);
	_vte_rgb_draw_color_rgb(buffer, x, y, width, height, r, g, b);
}

void
_vte_rgb_draw_pixbuf(struct _vte_rgb_buffer *buffer,
		     gint x, gint y, gint width, gint height,
		     GdkPixbuf *pixbuf, gint xbias, gint ybias)
{
	struct _vte_rgb_buffer_p *buf = (struct _vte_rgb_buffer_p *) buffer;
	gint row, col, rows, cols;
	guchar bits, channels, *ipixels, *pixels;
	gint ioffset, offset, istride, stride, iwidth, iheight, ix, iy, irange;

	/* Find the stopping points. */
	cols = MIN(x + width, buffer->width);
	rows = MIN(y + height, buffer->height);
	if (cols < x) {
		return;
	}
	if (rows < y) {
		return;
	}

	/* Check that we can handle the pixbuf format. */
	bits = gdk_pixbuf_get_bits_per_sample(pixbuf);
	g_assert(bits == 8);
	channels = gdk_pixbuf_get_n_channels(pixbuf);

	/* Get the addresses of the pixels and set things up. */
	ipixels = gdk_pixbuf_get_pixels(pixbuf);
	pixels = buf->pixels;
	istride = gdk_pixbuf_get_rowstride(pixbuf);
	stride = buf->stride;
	iwidth = gdk_pixbuf_get_width(pixbuf);
	iheight = gdk_pixbuf_get_height(pixbuf);
	xbias %= iwidth;
	ybias %= iheight;

	/* Start at the first row of the pixbuf we want. */
	iy = ybias;
	row = y;
	while (row < rows) {
		/* If the source layout is the same as the output, we can
		 * use memcpy, otherwise we need to do it the slow way. */
		if (channels == 3) {
			/* Get the offset for this row, and find the
			 * first column. */
			ix = xbias;
			col = x;
			while (col < cols) {
				/* Calculate the destination, the number of
				 * pixels to copy, and the source location. */
				irange = MIN(cols - col, iwidth - ix);
				offset = row * stride + col * 3;
				ioffset = iy * istride + ix * 3;
				/* Copy a range of pixels . */
				memcpy(pixels + offset,
				       ipixels + ioffset,
				       irange * 3);
				/* Move on to the next range, wrapping
				 * if necessary. */
				col += irange;
				ix += irange;
				ix %= iwidth;
			}
			/* Move on to the next row, wrapping if necessary. */
			iy++;
			iy %= iheight;
		} else {
			/* Get the offset for this row, and find the
			 * first column. */
			ix = xbias;
			offset = row * stride + x * 3;
			col = x;
			while (col < cols) {
				ioffset = iy * istride + ix * channels;
				/* Copy one pixel . */
				pixels[offset++] = ipixels[ioffset++];
				pixels[offset++] = ipixels[ioffset++];
				pixels[offset++] = ipixels[ioffset++];
				/* Move on to the next pixel, wrapping
				 * if necessary. */
				ix++;
				ix %= iwidth;
				col++;
			}
			/* Move on to the next row, wrapping if necessary. */
			iy++;
			iy %= iheight;
		}
		row++;
	}
}

void
_vte_rgb_draw_on_drawable(GdkDrawable *drawable, GdkGC *gc,
			  gint x, gint y, gint width, gint height,
			  struct _vte_rgb_buffer *buffer,
			  gint xbias, gint ybias)
{
	g_return_if_fail(xbias + width <= buffer->width);
	g_return_if_fail(ybias + height <= buffer->height);
	g_return_if_fail((xbias + width) * 3 <= buffer->stride);
	gdk_draw_rgb_image(drawable, gc, x, y, width, height,
			   GDK_RGB_DITHER_NORMAL,
			   buffer->pixels +
			   ybias * buffer->stride +
			   xbias * 3,
			   buffer->stride);
}

void
_vte_rgb_buffer_clear(struct _vte_rgb_buffer *buffer)
{
	struct _vte_rgb_buffer_p *buf = (struct _vte_rgb_buffer_p *) buffer;
	memset(buf->pixels, '\0', buf->length);
}
