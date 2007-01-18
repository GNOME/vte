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

#define ALIGN(x, a) (((x)+((a)-1))&~((a)-1))

struct _vte_rgb_buffer_p {
	guchar *pixels;
	gint width, height, stride;
	gint length;
};

struct _vte_rgb_buffer *
_vte_rgb_buffer_new(gint width, gint height)
{
	struct _vte_rgb_buffer_p *ret;

	ret = g_slice_new(struct _vte_rgb_buffer_p);

	width = MAX(width, 1);
	height = MAX(height, 1);

	ret->width  = width;
	ret->height = height;
	/* gdk_rgb prefers a row-alignment of 4 */
	ret->stride = 3 * ALIGN(width, 4);
	ret->length = ret->stride * height;
	ret->pixels = g_malloc(ret->length);

	return (struct _vte_rgb_buffer *) ret;
}

void
_vte_rgb_buffer_free(struct _vte_rgb_buffer *buffer)
{
	struct _vte_rgb_buffer_p *buf = (struct _vte_rgb_buffer_p *) buffer;

	g_free(buf->pixels);
	g_slice_free(struct _vte_rgb_buffer_p, buf);
}

void
_vte_rgb_buffer_resize(struct _vte_rgb_buffer *buffer,
		       gint minimum_width,
		       gint minimum_height)
{
	struct _vte_rgb_buffer_p *buf = (struct _vte_rgb_buffer_p *) buffer;
	gint stride, size;

	stride = 3 * ALIGN(minimum_width, 4);
	size = stride * minimum_height;
	if (size > buf->length) {
		g_free(buf->pixels);
		buf->length = size;
		buf->pixels = g_malloc(buf->length);
	}

	buf->width = minimum_width;
	buf->height = minimum_height;
	buf->stride = stride;
}

void
_vte_rgb_draw_color_rgb(struct _vte_rgb_buffer *buffer,
			gint x, gint y, gint width, gint height,
			guchar r, guchar g, guchar b)
{
	gint i, cols, rows;
	gint count, stride;
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
	cols = cols - x;
	rows = rows - y;

	stride = buffer->stride;
	pixels += y * stride + x * 3;
	count = cols * 3;
	/* Draw the first row by iteration. */
	i = 0;
	while(cols--) {
		pixels[i++] = r;
		pixels[i++] = g;
		pixels[i++] = b;
	}
	/* Draw the other rows by copying the data. */
	i = 0;
	while (--rows) {
		i += stride;
		memcpy(pixels + i, pixels, count);
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
_vte_rgb_copy(struct _vte_rgb_buffer *buffer,
		int src_x, int src_y, int width, int height, int dst_x, int dst_y)
{
	guchar *src, *dst;
	gint stride;

	g_assert (src_x > 0);
	g_assert (dst_x >= src_x + width);
	g_assert (dst_x <= buffer->width);

	stride = buffer->stride;
	src = buffer->pixels + src_y * stride + 3 * src_x;
	dst = buffer->pixels + dst_y * stride + 3 * dst_x;
	width *= 3;

	while (height--) {
		memcpy (dst, src, width);
		src += stride;
		dst += stride;
	}
}

void
_vte_rgb_buffer_clear(struct _vte_rgb_buffer *buffer)
{
	struct _vte_rgb_buffer_p *buf = (struct _vte_rgb_buffer_p *) buffer;
	memset(buf->pixels, '\0', buf->length);
}
