/*
 * Copyright © 2020 Hans Petter Jansson <hpj@cl.no>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#include <termios.h>  /* TIOCGWINSZ */
#include <sys/ioctl.h>  /* ioctl() */

#include <glib.h>

/* The image data is stored as a series of palette indexes, with 16 bits
 * per pixel and TRANSPARENT_SLOT indicating transparency. This allows for
 * palette sizes up to 65535 colors.
 *
 * TRANSPARENT_SLOT can be any u16 value. Typically, the first or last
 * slot (0, n_colors) is used. The transparency index is never emitted;
 * instead pixels with this value are left blank in the output. */

#define N_COLORS_MAX 65536
#define TRANSPARENT_SLOT ((N_COLORS_MAX) - 1)

#define WIDTH_MAX 65536
#define HEIGHT_MAX 65536

#define N_PIXELS_IN_SIXEL 6

#define PRE_SEQ "\x1bP"
#define POST_SEQ "\x1b\\"

#define TEST_IMAGE_SIZE_MIN 16
#define TEST_IMAGE_SIZE_MAX 512

/* Big palettes make our toy printer extremely slow; use with caution */
#define TEST_PALETTE_SIZE_MIN 1
#define TEST_PALETTE_SIZE_MAX 16

/* --- Helpers --- */

static int
random_int_in_range (int min, int max)
{
        if (min == max)
                return min;

        if (min > max) {
                int t = max;
                max = min;
                min = t;
        }

        return min + (random () % (max - min));
}

static int
round_up_to_multiple (int n, int m)
{
        n += m - 1;
        return n - (n % m);
}

static int
round_down_to_multiple (int n, int m)
{
        return round_up_to_multiple (n, m + 1) - m;
}

static void
memset_u16 (uint16_t *buf, uint16_t val, int n)
{
        int i;

        for (i = 0; i < n; i++) {
                buf [i] = val;
        }
}

static uint16_t
pen_to_slot (int i)
{
        if (i >= TRANSPARENT_SLOT)
                return i + 1;

        return i;
}

static uint8_t
interp_u8 (uint8_t a, uint8_t b, int fraction, int total)
{
        uint32_t ta, tb;

        assert (fraction >= 0 && fraction <= total);

        /* Only one color in palette */
        if (total == 0)
                return a;

        ta = (uint32_t) a * (total - fraction) / total;
        tb = (uint32_t) b * fraction / total;

        return ta + tb;
}

static uint32_t
interp_colors (uint32_t a, uint32_t b, int fraction, int total)
{
        return interp_u8 (a, b, fraction, total)
                | (interp_u8 (a >> 8, b >> 8, fraction, total) << 8)
                | (interp_u8 (a >> 16, b >> 16, fraction, total) << 16)
                | (interp_u8 (a >> 24, b >> 24, fraction, total) << 24);
}

static int
transform_range (int n, int old_min, int old_max, int new_min, int new_max)
{
        if (new_min == new_max)
                return new_min;

        if (n < old_min)
                n = old_min;
        if (n > old_max)
                n = old_max;

        return ((n - old_min) * (new_max - new_min) / (old_max - old_min)) + new_min;
}

/* Transform to sixel color channels, which are in the 0..100 range. */
static void
argb_to_sixel_rgb (uint32_t argb, int *r, int *g, int *b)
{
        *r = transform_range ((argb >> 16) & 0xff, 0, 256, 0, 101);
        *g = transform_range ((argb >> 8) & 0xff, 0, 256, 0, 101);
        *b = transform_range (argb & 0xff, 0, 256, 0, 101);
}

/* --- Image gen and sixel conversion --- */

typedef struct
{
        int width, height;
        int n_colors;
        uint32_t palette [N_COLORS_MAX];
        uint16_t *pixels;
}
Image;

static void
image_init (Image *image, int width, int height, int n_colors)
{
        int alloc_height;
        int n_pixels;

        assert (width > 0 && width <= WIDTH_MAX);
        assert (height > 0 && height <= HEIGHT_MAX);
        assert (n_colors > 0 && n_colors < N_COLORS_MAX);

        image->width = width;
        image->height = height;
        image->n_colors = n_colors;

        alloc_height = round_up_to_multiple (height, N_PIXELS_IN_SIXEL);

        n_pixels = width * alloc_height;
        image->pixels = (uint16_t *) malloc (n_pixels * sizeof (uint16_t));
        memset_u16 (image->pixels, TRANSPARENT_SLOT, n_pixels);
}

static void
image_deinit (Image *image)
{
        free (image->pixels);
        image->pixels = NULL;
}

static void
image_generate_palette (Image *image, uint32_t first_color, uint32_t last_color)
{
        int pen;

        for (pen = 0; pen < image->n_colors; pen++) {
                image->palette [pen_to_slot (pen)]
                        = interp_colors (first_color, last_color, pen, image->n_colors - 1);
        }
}

static void
image_set_pixel (Image *image, int x, int y, uint16_t value)
{
        image->pixels [y * image->width + x] = value;
}

static uint16_t
image_get_pixel (const Image *image, int x, int y)
{
        return image->pixels [y * image->width + x];
}

static uint8_t
image_get_sixel (const Image *image, int x, int y, uint16_t value)
{
        uint8_t sixel = 0;
        int i;

        for (i = 0; i < N_PIXELS_IN_SIXEL; i++) {
                uint16_t p = image_get_pixel (image, x, y + N_PIXELS_IN_SIXEL - 1 - i);

                sixel <<= 1;
                if (p == value)
                        sixel |= 1;
        }

        return sixel;
}

static void
image_draw_shape (Image *image)
{
        int y, x;

        for (y = 0; y < image->height; y++) {
                int pen = ((image->n_colors - 1) * y + image->height / 2) / image->height;

                for (x = 0; x < image->width; x++) {
                        if (x == 0 || x == image->width - 1          /* Box left/right */
                            || y == 0 || y == image->height - 1      /* Box top/bottom */
                            || y == x || y == image->width - 1 - x)  /* X diagonals */
                                image_set_pixel (image, x, y, pen_to_slot (pen));
                }
        }
}

static void
image_generate (Image *image, uint32_t first_color, uint32_t last_color)
{
        image_generate_palette (image, first_color, last_color);
        image_draw_shape (image);
}

static void
image_print_sixels_palette (const Image *image, GString *gstr)
{
        int pen;

        for (pen = 0; pen < image->n_colors; pen++) {
                uint32_t col = image->palette [pen_to_slot (pen)];
                int r, g, b;

                argb_to_sixel_rgb (col, &r, &g, &b);
                g_string_append_printf (gstr, "#%d;2;%d;%d;%d",
                                        pen_to_slot (pen), r, g, b);
        }
}

static void
emit_sixels (GString *gstr, uint8_t sixel, int n, uint16_t slot,
             bool pass_ended, uint16_t *emitted_slot_inout,
             bool *need_emit_cr_inout, bool *need_emit_cr_inout_next)
{
        if (n == 0) {
                return;
        }

        if (!pass_ended || sixel != 0) {
                char c = '?' + (char) sixel;

                if (*need_emit_cr_inout) {
                        g_string_append_c (gstr, '$');
                        *need_emit_cr_inout = FALSE;
                }

                if (slot != *emitted_slot_inout) {
                        g_string_append_printf (gstr, "#%d", slot);
                        *emitted_slot_inout = slot;
                }

                while (n > 255) {
                        g_string_append_printf (gstr, "!255%c", c);
                        n -= 255;
                }

                if (n >= 4) {
                        g_string_append_printf (gstr, "!%d%c", n, c);
                        n = 0;
                } else {
                        for ( ; n > 0; n--)
                                g_string_append_c (gstr, c);
                }
        }

        if (sixel != 0)
                *need_emit_cr_inout_next = TRUE;
}

static void
image_print_sixels_row (const Image *image, GString *gstr, int y, uint16_t *emitted_slot_inout)
{
        bool need_emit_cr = FALSE;
        bool need_emit_cr_next = FALSE;
        int pen;

        for (pen = 0; pen < image->n_colors; pen++) {
                uint16_t slot = pen_to_slot (pen);
                uint8_t cur_sixel = 0;
                int n_cur_sixel = 0;
                int x;

                for (x = 0; x < image->width; x++) {
                        uint8_t next_sixel = image_get_sixel (image, x, y, slot);

                        if (next_sixel == cur_sixel) {
                                n_cur_sixel++;
                                continue;
                        }

                        emit_sixels (gstr, cur_sixel, n_cur_sixel, slot, FALSE,
                                     emitted_slot_inout, &need_emit_cr, &need_emit_cr_next);
                        cur_sixel = next_sixel;
                        n_cur_sixel = 1;
                }

                emit_sixels (gstr, cur_sixel, n_cur_sixel, slot, TRUE,
                             emitted_slot_inout, &need_emit_cr, &need_emit_cr_next);
                need_emit_cr = need_emit_cr_next;
        }

        /* CR + Linefeed */
        g_string_append_c (gstr, '-');
}

static void
image_print_sixels_data (const Image *image, GString *gstr)
{
        uint16_t emitted_slot = TRANSPARENT_SLOT;
        int y;

        for (y = 0; y < image->height; y += N_PIXELS_IN_SIXEL) {
                image_print_sixels_row (image, gstr, y, &emitted_slot);
        }
}

static void
image_print_sixels (const Image *image, GString *gstr)
{
        g_string_append_printf (gstr, PRE_SEQ "0;0;0q\"1;1;%d;%d",
                                image->width, image->height);
        image_print_sixels_palette (image, gstr);
        image_print_sixels_data (image, gstr);
        g_string_append (gstr, POST_SEQ);
}

/* --- Main loop and printing --- */

typedef enum
{
        TEST_MODE_UNSET,
        TEST_MODE_FUZZ
}
TestMode;

typedef struct
{
        TestMode mode;

        float delay;
        int n_errors;
        int n_frames;
        int seed;
        int n_scroll;

        int term_width_cells, term_height_cells;
        int term_width_pixels, term_height_pixels;

        int term_cell_width, term_cell_height;
}
Options;

static void
cursor_to_offset (gint x, gint y, GString *gstr)
{
        g_string_printf (gstr, "\x1b[%d;%df", y, x);
}

static void
cursor_to_random_offset (gint x_max, gint y_max, GString *gstr)
{
        cursor_to_offset (random_int_in_range (0, x_max),
                          random_int_in_range (0, y_max),
                          gstr);
}

static void
scroll_n_lines (const Options *options, int n, GString *gstr)
{
        if (n < 1)
                return;

        cursor_to_offset (0, options->term_height_cells, gstr);
        for (int i = 0; i < n; i++)
                g_string_append_printf (gstr, "\n");
}

static void
print_text (const gchar *text, GString *gstr)
{
        g_string_append (gstr, text);
}

static void
print_random_image (const Options *options, GString *gstr)
{
        int dim_max = MIN (TEST_IMAGE_SIZE_MAX,
                           MAX (TEST_IMAGE_SIZE_MIN,
                                MIN (options->term_width_pixels,
                                     round_down_to_multiple (options->term_height_pixels - options->term_cell_height,
                                                             N_PIXELS_IN_SIXEL))));
        int dim = random_int_in_range (TEST_IMAGE_SIZE_MIN, dim_max + 1);
        Image image;

        image_init (&image, dim, dim,
                    random_int_in_range (TEST_PALETTE_SIZE_MIN, TEST_PALETTE_SIZE_MAX));

        /* In order to produce colors that contrast both white and black backgrounds,
         * limit the range of the red component. This doesn't work reliably with grey
         * backgrounds, but eh. */
        image_generate (&image,
                        random_int_in_range (0x00400000, 0x00a00000),
                        random_int_in_range (0x00400000, 0x00a00000));

        cursor_to_random_offset ((options->term_width_pixels - dim) / options->term_cell_width,
                                 (options->term_height_pixels - dim) / options->term_cell_height,
                                 gstr);
        image_print_sixels (&image, gstr);

        image_deinit (&image);
}

static void
print_random_text (const Options *options, GString *gstr)
{
        cursor_to_random_offset (options->term_width_cells - strlen ("Hallo!"),
                                 options->term_height_cells,
                                 gstr);
        print_text ("Hallo!", gstr);
}

typedef enum
{
        FUZZ_REPLACE,
        FUZZ_COPY,
        FUZZ_SWAP,

        FUZZ_MAX
}
FuzzType;

static void
fuzz_replace (GString *gstr)
{
        int a, b;

        a = random_int_in_range (0, gstr->len - 1);
        b = a + random_int_in_range (0, MIN (gstr->len - a, 64));

        for (int i = a; i < b; i++) {
                gstr->str [i] = random_int_in_range (1, 256);
        }
}

static void
fuzz_copy (GString *gstr)
{
        int a, b, c;

        a = random_int_in_range (0, gstr->len - 1);
        b = random_int_in_range (0, MIN (gstr->len - a, 64));
        c = random_int_in_range (0, gstr->len - b);

        memcpy (gstr->str + c, gstr->str + a, b);
}

static void
fuzz_swap (GString *gstr)
{
        unsigned char buf [64];
        int a, b, c;

        a = random_int_in_range (0, gstr->len - 1);
        b = random_int_in_range (0, MIN (gstr->len - a, 64));
        c = random_int_in_range (0, gstr->len - b);

        memcpy (buf, gstr->str + c, b);
        memcpy (gstr->str + c, gstr->str + a, b);
        memcpy (gstr->str + c, buf, b);
}

static void
random_fuzz (const Options *options, GString *gstr)
{
        if (gstr->len < 1)
                return;

        for (int i = 0; i < options->n_errors; i++) {
                FuzzType fuzz_type = FuzzType(random () % FUZZ_MAX);

                switch (fuzz_type) {
                        case FUZZ_REPLACE:
                                fuzz_replace (gstr);
                                break;
                        case FUZZ_COPY:
                                fuzz_copy (gstr);
                                break;
                        case FUZZ_SWAP:
                                fuzz_swap (gstr);
                                break;
                        default:
                                break;
                }
        }
}

static void
print_loop (const Options *options)
{
        for (int i = 0; options->n_frames == 0 || i < options->n_frames; i++) {
                GString *gstr;

                gstr = g_string_new ("");

                scroll_n_lines (options, options->n_scroll, gstr);

                if (random () % 2) {
                        print_random_image (options, gstr);
                } else {
                        print_random_text (options, gstr);
                }

                random_fuzz (options, gstr);

                fwrite (gstr->str, sizeof (char), gstr->len, stdout);
                g_string_free (gstr, TRUE);
                fflush (stdout);

                if (options->delay > 0.000001f)
                        g_usleep (options->delay * 1000000.0f);
        }
}

/* --- Argument parsing and init --- */

static bool
parse_int (const char *arg, const char *val, int *out)
{
        char *endptr;
        int ret;
        bool result = FALSE;

        assert (arg != NULL);
        assert (val != NULL);
        assert (out != NULL);

        if (*val == '\0') {
                fprintf (stderr, "Empty value for argument '%s'. Aborting.\n", arg);
                goto out;
        }

        ret = strtol (val, &endptr, 10);

        if (*endptr != '\0') {
                fprintf (stderr, "Unrecognized value for argument '%s': '%s'. Aborting.\n", arg, val);
                goto out;
        }

        *out = ret;
        result = TRUE;

out:
        return result;
}

static bool
parse_float (const char *arg, const char *val, float *out)
{
        char *endptr;
        float ret;
        bool result = FALSE;

        assert (arg != NULL);
        assert (val != NULL);
        assert (out != NULL);

        if (*val == '\0') {
                fprintf (stderr, "Empty value for argument '%s'. Aborting.\n", arg);
                goto out;
        }

        ret = strtof (val, &endptr);

        if (*endptr != '\0') {
                fprintf (stderr, "Unrecognized value for argument '%s': '%s'. Aborting.\n", arg, val);
                goto out;
        }

        *out = ret;
        result = TRUE;

out:
        return result;
}

static bool
parse_options (Options *options, int argc, char **argv)
{
        bool result = FALSE;
        int i;

        if (argc < 2) {
                fprintf (stderr, "Usage: %s <mode> [options]\n\n"
                         "Modes:\n"
                         "    fuzz        Perform fuzzing test.\n\n"
                         "Options:\n"
                         "    -d <float>  Delay between frames, in seconds (default: 0.0).\n"
                         "    -e <int>    Maximum number of random errors per frame (default: 0).\n"
                         "    -n <int>    Number of frames to output (default: infinite).\n"
                         "    -r <int>    Random seed to use (default: current time).\n"
                         "    -s <int>    Number of lines to scroll for each frame (default: 0).\n\n",
                         argv [0]);
                goto out;
        }

        for (i = 1; i < argc; ) {
                const char *arg = argv [i];
                const char *val;

                if (!strcmp (arg, "fuzz")) {
                        options->mode = TEST_MODE_FUZZ;
                        i++;
                        continue;
                }

                if (i + 1 >= argc)
                        break;

                val = argv [i + 1];

                if (!strcmp (arg, "-d")) {
                        if (!parse_float (arg, val, &options->delay))
                                goto out;
                        i += 2;
                } else if (!strcmp (arg, "-e")) {
                        if (!parse_int (arg, val, &options->n_errors))
                                goto out;
                        i += 2;
                } else if (!strcmp (arg, "-n")) {
                        if (!parse_int (arg, val, &options->n_frames))
                                goto out;
                        i += 2;
                } else if (!strcmp (arg, "-r")) {
                        if (!parse_int (arg, val, &options->seed))
                                goto out;
                        i += 2;
                } else if (!strcmp (arg, "-s")) {
                        if (!parse_int (arg, val, &options->n_scroll))
                                goto out;
                        i += 2;
                } else {
                        fprintf (stderr, "Unrecognized option '%s'. Aborting.\n", arg);
                        goto out;
                }
        }

        if (i != argc) {
                fprintf (stderr, "Stray option '%s'. Aborting.\n", argv [i]);
                goto out;
        }

        if (options->mode == TEST_MODE_UNSET) {
                fprintf (stderr, "No test mode specified. Try \"fuzz\".\n");
                goto out;
        }

        result = TRUE;

out:
        return result;
}

static bool
query_terminal (Options *options)
{
        struct winsize wsz;

        if (ioctl (fileno (stdout), TIOCGWINSZ, &wsz) != 0) {
                fprintf (stderr, "ioctl() failed: %s\n", strerror (errno));
                return FALSE;
        }

        options->term_width_cells = wsz.ws_col;
        options->term_height_cells = wsz.ws_row;
        options->term_width_pixels = wsz.ws_xpixel;
        options->term_height_pixels = wsz.ws_ypixel;

        if (options->term_width_cells < 4
            || options->term_height_cells < 4) {
                fprintf (stderr, "Terminal window is too small (must be greater than 4x4 cells).\n");
                return FALSE;
        }

        if (options->term_width_pixels == 0
            || options->term_height_pixels == 0) {
                fprintf (stderr, "Terminal did not report its pixel size.\n");
                return FALSE;
        }

        if (options->term_width_pixels < 16
            || options->term_height_pixels < 16) {
                fprintf (stderr, "Terminal window is too small (must be greater than 16x16 pixels).\n");
                return FALSE;
        }

        options->term_cell_width = wsz.ws_xpixel / wsz.ws_col;
        options->term_cell_height = wsz.ws_ypixel / wsz.ws_row;

        return TRUE;
}

/* --- Entry point --- */

int
main (int argc, char *argv [])
{
        static Options options = { };

        options.seed = (int) time (NULL);

        if (!parse_options (&options, argc, argv))
                return 1;

        if (!query_terminal (&options))
                return 2;

        print_loop (&options);
        return 0;
}
