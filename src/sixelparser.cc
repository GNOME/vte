/*
 * Copyright © 2016-2020 Hayaki Saito
 * Copyright © 2020 Hans Petter Jansson
 * originally written by kmiya@cluti (https://github.com/saitoha/sixel/blob/master/fromsixel.c)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>   /* isdigit */
#include <string.h>  /* memcpy */
#include <glib.h>

#include "sixelparser.hh"

#define PACK_RGB(r, g, b) ((r) + ((g) << 8) +  ((b) << 16))
#define SCALE_VALUE(n,a,m) (((n) * (a) + ((m) / 2)) / (m))
#define SCALE_AND_PACK_RGB(r,g,b) \
        PACK_RGB(SCALE_VALUE(r, 255, 100), SCALE_VALUE(g, 255, 100), SCALE_VALUE(b, 255, 100))

static const int sixel_default_color_table[] = {
        SCALE_AND_PACK_RGB(0,  0,  0),   /*  0 Black    */
        SCALE_AND_PACK_RGB(20, 20, 80),  /*  1 Blue     */
        SCALE_AND_PACK_RGB(80, 13, 13),  /*  2 Red      */
        SCALE_AND_PACK_RGB(20, 80, 20),  /*  3 Green    */
        SCALE_AND_PACK_RGB(80, 20, 80),  /*  4 Magenta  */
        SCALE_AND_PACK_RGB(20, 80, 80),  /*  5 Cyan     */
        SCALE_AND_PACK_RGB(80, 80, 20),  /*  6 Yellow   */
        SCALE_AND_PACK_RGB(53, 53, 53),  /*  7 Gray 50% */
        SCALE_AND_PACK_RGB(26, 26, 26),  /*  8 Gray 25% */
        SCALE_AND_PACK_RGB(33, 33, 60),  /*  9 Blue*    */
        SCALE_AND_PACK_RGB(60, 26, 26),  /* 10 Red*     */
        SCALE_AND_PACK_RGB(33, 60, 33),  /* 11 Green*   */
        SCALE_AND_PACK_RGB(60, 33, 60),  /* 12 Magenta* */
        SCALE_AND_PACK_RGB(33, 60, 60),  /* 13 Cyan*    */
        SCALE_AND_PACK_RGB(60, 60, 33),  /* 14 Yellow*  */
        SCALE_AND_PACK_RGB(80, 80, 80),  /* 15 Gray 75% */
};

/*
 *  HLS-formatted color handling.
 *  (0 degree = blue, double-hexcone model)
 *  http://odl.sysworks.biz/disk$vaxdocdec021/progtool/d3qsaaa1.p64.bkb
 */
static int
hls_to_rgb(int hue, int lum, int sat)
{
    double min, max;
    int r, g, b;

    if (sat == 0) {
        r = g = b = lum;
        return SCALE_AND_PACK_RGB(r, g, b);
    }

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/17e876f7e3260ea7fed73f69e19c71eb715dd09d */
    max = lum + sat * (100 - (lum > 50 ? 1 : -1) * ((lum << 1) - 100)) / 200.0;

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/f6721b57985ad83db3d5b800dc38c9980eedde1d */
    min = lum - sat * (100 - (lum > 50 ? 1 : -1) * ((lum << 1) - 100)) / 200.0;

    /* HLS hue color ring is rotated -120 degree from HSL's one. */
    hue = (hue + 240) % 360;

    /* https://wikimedia.org/api/rest_v1/media/math/render/svg/937e8abdab308a22ff99de24d645ec9e70f1e384 */
    switch (hue / 60) {
    case 0:  /* 0 <= hue < 60 */
        r = max;
        g = (min + (max - min) * (hue / 60.0));
        b = min;
        break;
    case 1:  /* 60 <= hue < 120 */
        r = min + (max - min) * ((120 - hue) / 60.0);
        g = max;
        b = min;
        break;
    case 2:  /* 120 <= hue < 180 */
        r = min;
        g = max;
        b = (min + (max - min) * ((hue - 120) / 60.0));
        break;
    case 3:  /* 180 <= hue < 240 */
        r = min;
        g = (min + (max - min) * ((240 - hue) / 60.0));
        b = max;
        break;
    case 4:  /* 240 <= hue < 300 */
        r = (min + (max - min) * ((hue - 240) / 60.0));
        g = min;
        b = max;
        break;
    case 5:  /* 300 <= hue < 360 */
    default:
        r = max;
        g = min;
        b = (min + (max - min) * ((360 - hue) / 60.0));
        break;
    }

    return SCALE_AND_PACK_RGB(r, g, b);
}

static int
set_default_color(sixel_image_t *image)
{
        int r, g, b;
        int i, n;

        /* Palette initialization */
        for (n = 1; n < 17; n++) {
                image->palette[n] = sixel_default_color_table[n - 1];
        }

        /* Colors 17-232 are a 6x6x6 color cube */
        for (r = 0; r < 6; r++) {
                for (g = 0; g < 6; g++) {
                        for (b = 0; b < 6; b++) {
                                image->palette[n++] = PACK_RGB(r * 51, g * 51, b * 51);
                        }
                }
        }

        /* Colors 233-256 are a grayscale ramp */
        for (i = 0; i < 24; i++) {
                image->palette[n++] = PACK_RGB(i * 11, i * 11, i * 11);
        }

        for (; n < DECSIXEL_PALETTE_MAX; n++) {
                image->palette[n] = PACK_RGB(255, 255, 255);
        }

        return 0;
}

static int
sixel_image_init(sixel_image_t *image,
                 int width, int height,
                 int fgcolor, int bgcolor,
                 int use_private_register)
{
        int status = -1;
        size_t size;

        size = (size_t)(width * height) * sizeof(sixel_color_no_t);
        image->width = width;
        image->height = height;
        image->data = (sixel_color_no_t *) g_try_malloc0(size);
        image->ncolors = 2;
        image->use_private_register = use_private_register;

        if (image->data == NULL)
                goto out;

        image->palette[0] = bgcolor;

        if (image->use_private_register)
                image->palette[1] = fgcolor;

        image->palette_modified = 0;

        status = 0;

out:
        return status;
}


static int
image_buffer_resize(sixel_image_t *image, int width, int height)
{
        int status = -1;
        size_t size;
        sixel_color_no_t *alt_buffer;
        int n;
        int min_height;

        if (width == image->width && height == image->height)
                return 0;

        size = (size_t)(width * height) * sizeof(sixel_color_no_t);
        alt_buffer = (sixel_color_no_t *) g_try_malloc(size);
        if (alt_buffer == NULL) {
                status = -1;
                goto out;
        }

        min_height = height > image->height ? image->height: height;
        if (width > image->width) {
                /* Wider */
                for (n = 0; n < min_height; ++n) {
                        /* Copy from source image */
                        memcpy(alt_buffer + width * n,
                               image->data + image->width * n,
                               (size_t)image->width * sizeof(sixel_color_no_t));
                        /* Fill extended area with background color */
                        memset(alt_buffer + width * n + image->width,
                               0,
                               (size_t)(width - image->width) * sizeof(sixel_color_no_t));
                }
        } else {
                /* Narrower or the same width */
                for (n = 0; n < min_height; ++n) {
                        /* Copy from source image */
                        memcpy(alt_buffer + width * n,
                               image->data + image->width * n,
                               (size_t)width * sizeof(sixel_color_no_t));
                }
        }

        if (height > image->height) {
                /* Fill extended area with background color */
                memset(alt_buffer + width * image->height,
                       0,
                       (size_t)(width * (height - image->height)) * sizeof(sixel_color_no_t));
        }

        /* Free source image */
        g_free(image->data);

        image->data = alt_buffer;
        image->width = width;
        image->height = height;

        status = 0;

out:
        if (status < 0) {
                /* Free source image */
                g_free(image->data);
                image->data = NULL;
        }

        return status;
}

static int
image_buffer_ensure_min_size(sixel_image_t *image, int req_width, int req_height)
{
        int status = 0;

        if ((image->width < req_width || image->height < req_height)
            && image->width < DECSIXEL_WIDTH_MAX && image->height < DECSIXEL_HEIGHT_MAX) {
                int sx = image->width * 2;
                int sy = image->height * 2;

                while (sx < req_width || sy < req_height) {
                        sx *= 2;
                        sy *= 2;
                }

                if (sx > DECSIXEL_WIDTH_MAX)
                        sx = DECSIXEL_WIDTH_MAX;
                if (sy > DECSIXEL_HEIGHT_MAX)
                        sy = DECSIXEL_HEIGHT_MAX;

                status = image_buffer_resize(image, sx, sy);
        }

        return status;
}

static void
sixel_image_deinit(sixel_image_t *image)
{
        g_free(image->data);
        image->data = NULL;
}

static int
parser_transition(sixel_state_t *st, parse_state_t new_state)
{
        st->state = new_state;
        st->nparams = 0;
        st->param = -1;

        return 0;
}

static int
parser_push_param_ascii_dec_digit(sixel_state_t *st, uint32_t ascii_dec_digit)
{
        if (st->param < 0)
                st->param = 0;

        st->param = st->param * 10 + ascii_dec_digit - '0';
        return 0;
}

static int
parser_collect_param(sixel_state_t *st)
{
        if (st->param < 0 || st->nparams >= DECSIXEL_PARAMS_MAX)
                return 0;

        if (st->param > DECSIXEL_PARAMVALUE_MAX)
                st->param = DECSIXEL_PARAMVALUE_MAX;

        st->params[st->nparams++] = st->param;
        st->param = -1;
        return 0;
}

static int
parser_collect_param_or_zero(sixel_state_t *st)
{
        if (st->param < 0)
                st->param = 0;

        parser_collect_param (st);
        return 0;
}

static void
draw_sixels(sixel_state_t *st, uint32_t bits)
{
        sixel_image_t *image = &st->image;

        if (bits == 0)
                return;

        if (st->repeat_count <= 1) {
                uint32_t sixel_vertical_mask = 0x01;

                for (int i = 0; i < 6; i++) {
                        if ((bits & sixel_vertical_mask) != 0) {
                                int pos = image->width * (st->pos_y + i) + st->pos_x;
                                image->data[pos] = st->color_index;
                                if (st->max_x < st->pos_x)
                                        st->max_x = st->pos_x;
                                if (st->max_y < (st->pos_y + i))
                                        st->max_y = st->pos_y + i;
                        }
                        sixel_vertical_mask <<= 1;
                }
        } else {
                uint32_t sixel_vertical_mask = 0x01;

                /* st->repeat_count > 1 */
                for (int i = 0; i < 6; i++) {
                        if ((bits & sixel_vertical_mask) != 0) {
                                uint32_t c = sixel_vertical_mask << 1;
                                int n;

                                for (n = 1; (i + n) < 6; n++) {
                                        if ((bits & c) == 0)
                                                break;
                                        c <<= 1;
                                }
                                for (int y = st->pos_y + i; y < st->pos_y + i + n; ++y) {
                                        for (int x = st->pos_x; x < st->pos_x + st->repeat_count; ++x)
                                                image->data[image->width * y + x] = st->color_index;
                                }
                                if (st->max_x < (st->pos_x + st->repeat_count - 1))
                                        st->max_x = st->pos_x + st->repeat_count - 1;
                                if (st->max_y < (st->pos_y + i + n - 1))
                                        st->max_y = st->pos_y + i + n - 1;
                                i += (n - 1);
                                sixel_vertical_mask <<= (n - 1);
                        }

                        sixel_vertical_mask <<= 1;
                }
        }
}

static int
parser_action_sixel_char(sixel_state_t *st, uint32_t raw)
{
        sixel_image_t *image = &st->image;
        int status = -1;

        if (raw >= '?' && raw <= '~') {
                status = image_buffer_ensure_min_size (image,
                                                       st->pos_x + st->repeat_count,
                                                       st->pos_y + 6);
                if (status < 0)
                        goto out;

                if (st->color_index > image->ncolors)
                        image->ncolors = st->color_index;

                st->repeat_count = MIN(st->repeat_count, image->width - st->pos_x);

                if (st->repeat_count > 0 && st->pos_y + 5 < image->height) {
                        uint32_t bits = raw - '?';
                        draw_sixels(st, bits);
                }

                if (st->repeat_count > 0)
                        st->pos_x += st->repeat_count;
                st->repeat_count = 1;
        }

        status = 0;

out:
        return status;
}

static int
parser_action_decgcr(sixel_state_t *st)
{
        st->pos_x = 0;
        return 0;
}

static int
parser_action_decgnl(sixel_state_t *st)
{
        st->pos_x = 0;

        if (st->pos_y < DECSIXEL_HEIGHT_MAX - 5 - 6)
                st->pos_y += 6;
        else
                st->pos_y = DECSIXEL_HEIGHT_MAX + 1;

        return 0;
}


static int
parser_action_decgra(sixel_state_t *st)
{
        sixel_image_t *image = &st->image;
        int status = 0;

        if (st->nparams > 0)
                st->attributed_pad = st->params[0];
        if (st->nparams > 1)
                st->attributed_pan = st->params[1];
        if (st->nparams > 2 && st->params[2] > 0)
                st->attributed_ph = st->params[2];
        if (st->nparams > 3 && st->params[3] > 0)
                st->attributed_pv = st->params[3];

        if (st->attributed_pan <= 0)
                st->attributed_pan = 1;
        if (st->attributed_pad <= 0)
                st->attributed_pad = 1;

        if (image->width < st->attributed_ph ||
            image->height < st->attributed_pv) {
                int sx = MIN (MAX (image->width, st->attributed_ph), DECSIXEL_WIDTH_MAX);
                int sy = MIN (MAX (image->height, st->attributed_pv), DECSIXEL_HEIGHT_MAX);

                status = image_buffer_resize(image, sx, sy);
        }

        return status;
}

static int
parser_action_decgri(sixel_state_t *st)
{
        st->repeat_count = MAX (st->param, 1);
        return 0;
}

static int
parser_action_decgci(sixel_state_t *st)
{
        sixel_image_t *image = &st->image;

        if (st->nparams > 0) {
                st->color_index = 1 + st->params[0];  /* offset 1 (background color) added */
                if (st->color_index < 0)
                        st->color_index = 0;
                else if (st->color_index >= DECSIXEL_PALETTE_MAX)
                        st->color_index = DECSIXEL_PALETTE_MAX - 1;
        }

        if (st->nparams > 4) {
                st->image.palette_modified = 1;
                if (st->params[1] == 1) {
                        /* HLS */
                        if (st->params[2] > 360)
                                st->params[2] = 360;
                        if (st->params[3] > 100)
                                st->params[3] = 100;
                        if (st->params[4] > 100)
                                st->params[4] = 100;
                        image->palette[st->color_index]
                                = hls_to_rgb(st->params[2], st->params[3], st->params[4]);
                } else if (st->params[1] == 2) {
                        /* RGB */
                        if (st->params[2] > 100)
                                st->params[2] = 100;
                        if (st->params[3] > 100)
                                st->params[3] = 100;
                        if (st->params[4] > 100)
                                st->params[4] = 100;
                        image->palette[st->color_index]
                                = SCALE_AND_PACK_RGB(st->params[2], st->params[3], st->params[4]);
                }
        }

        return 0;
}

static int
parser_feed_char(sixel_state_t *st, uint32_t raw)
{
        int status = -1;

        for (;;) {
                switch (st->state) {
                case DECSIXEL_PS_DECSIXEL:
                        switch (raw) {
                        case 0x1b:
                                return parser_transition(st, DECSIXEL_PS_ESC);
                        case '"':
                                return parser_transition(st, DECSIXEL_PS_DECGRA);
                        case '!':
                                return parser_transition(st, DECSIXEL_PS_DECGRI);
                        case '#':
                                return parser_transition(st, DECSIXEL_PS_DECGCI);
                        case '$':
                                /* DECGCR Graphics Carriage Return */
                                return parser_action_decgcr(st);
                        case '-':
                                /* DECGNL Graphics Next Line */
                                return parser_action_decgnl(st);
                        }
                        return parser_action_sixel_char(st, raw);

                case DECSIXEL_PS_DECGRA:
                        /* DECGRA Set Raster Attributes " Pan; Pad; Ph; Pv */
                        switch (raw) {
                        case 0x1b:
                                return parser_transition(st, DECSIXEL_PS_ESC);
                        case '0' ... '9':
                                return parser_push_param_ascii_dec_digit(st, raw);
                        case ';':
                                return parser_collect_param(st);
                        }

                        parser_collect_param(st);
                        status = parser_action_decgra(st);
                        if (status < 0)
                                return status;
                        parser_transition(st, DECSIXEL_PS_DECSIXEL);
                        continue;

                case DECSIXEL_PS_DECGRI:
                        /* DECGRI Graphics Repeat Introducer ! Pn Ch */
                        switch (raw) {
                        case 0x1b:
                                return parser_transition(st, DECSIXEL_PS_ESC);
                        case '0' ... '9':
                                return parser_push_param_ascii_dec_digit(st, raw);
                        }

                        parser_action_decgri(st);
                        parser_transition(st, DECSIXEL_PS_DECSIXEL);
                        continue;

                case DECSIXEL_PS_DECGCI:
                        /* DECGCI Graphics Color Introducer # Pc; Pu; Px; Py; Pz */
                        switch (raw) {
                        case 0x1b:
                                return parser_transition(st, DECSIXEL_PS_ESC);
                        case '0' ... '9':
                                return parser_push_param_ascii_dec_digit(st, raw);
                        case ';':
                                return parser_collect_param_or_zero(st);
                        }

                        parser_collect_param(st);
                        parser_action_decgci(st);
                        parser_transition(st, DECSIXEL_PS_DECSIXEL);
                        continue;

                case DECSIXEL_PS_ESC:
                        /* The only escape code that can occur is end-of-input, "\x1b\\".
                         * When we get to this state, just consume the rest quietly. */
                        return 0;
                }
        } /* for (;;) */
}

int
sixel_parser_init(sixel_state_t *st,
                  int fgcolor, int bgcolor,
                  int use_private_register)
{
        int status = -1;

        st->state = DECSIXEL_PS_DECSIXEL;
        st->pos_x = 0;
        st->pos_y = 0;
        st->max_x = 0;
        st->max_y = 0;
        st->attributed_pan = 2;
        st->attributed_pad = 1;
        st->attributed_ph = 0;
        st->attributed_pv = 0;
        st->repeat_count = 1;
        st->color_index = 16;
        st->nparams = 0;
        st->param = -1;

        status = sixel_image_init(&st->image, 1, 1, fgcolor, bgcolor, use_private_register);

        return status;
}

void
sixel_parser_deinit(sixel_state_t *st)
{
        sixel_image_deinit(&st->image);
}

int
sixel_parser_finalize(sixel_state_t *st, unsigned char *pixels)
{
        int status = -1;
        int sx;
        int sy;
        sixel_image_t *image = &st->image;
        int x, y;
        sixel_color_no_t *src;
        unsigned char *dst;
        int color;

        if (++st->max_x < st->attributed_ph)
                st->max_x = st->attributed_ph;

        if (++st->max_y < st->attributed_pv)
                st->max_y = st->attributed_pv;

        sx = st->max_x;
        sy = st->max_y;

        status = image_buffer_resize(image,
                                     MIN(image->width, sx),
                                     MIN(image->height, sy));
        if (status < 0)
                goto out;

        if (image->use_private_register && image->ncolors > 2 && !image->palette_modified) {
                status = set_default_color(image);
                if (status < 0)
                        goto out;
        }

        src = st->image.data;
        dst = pixels;
        for (y = 0; y < st->image.height; ++y) {
                for (x = 0; x < st->image.width; ++x) {
                        sixel_color_no_t pen = *src++;

                        /* FIXME-hpj: We may want to make this branch-free, among other
                         * performance improvements. */

                        if (pen == 0) {
                                /* Cairo wants premultiplied alpha: Transparent
                                 * areas must be all zeroes. */

                                *dst++ = 0;
                                *dst++ = 0;
                                *dst++ = 0;
                                *dst++ = 0;
                        } else {
                                color = st->image.palette[pen];
                                *dst++ = color >> 16 & 0xff;   /* b */
                                *dst++ = color >> 8 & 0xff;    /* g */
                                *dst++ = color >> 0 & 0xff;    /* r */
                                *dst++ = 0xff;                 /* a */
                        }
                }
        }

        status = 0;

out:
        return status;
}

int
sixel_parser_set_default_color(sixel_state_t *st)
{
        return set_default_color(&st->image);
}

int
sixel_parser_feed(sixel_state_t *st, const uint32_t *raw, size_t len)
{
        int status = 0;

        if (!st->image.data)
                return -1;

        for (const uint32_t *p = raw; p < raw + len; p++) {
                status = parser_feed_char(st, *p);
                if (status < 0)
                        break;
        }

        return status;
}
