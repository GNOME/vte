/*
 * Copyright (C) 2001-2004,2009,2010 Red Hat, Inc.
 * Copyright © 2008, 2009, 2010, 2011 Christian Persch
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>
#include <gdk/gdk.h>

static const char color_names[8][8] = {
        "black",
        "red",
        "green",
        "yellow",
        "blue",
        "magenta",
        "cyan",
        "white"
};

static void
generate_bold(const GdkRGBA *foreground,
              const GdkRGBA *background,
              double factor,
              GdkRGBA *bold)
{
        double fy, fcb, fcr, by, bcb, bcr, r, g, b, a;
        g_assert(foreground != NULL);
        g_assert(background != NULL);
        g_assert(bold != NULL);
        fy =   0.2990 * foreground->red +
               0.5870 * foreground->green +
               0.1140 * foreground->blue;
        fcb = -0.1687 * foreground->red +
              -0.3313 * foreground->green +
               0.5000 * foreground->blue;
        fcr =  0.5000 * foreground->red +
              -0.4187 * foreground->green +
              -0.0813 * foreground->blue;
        by =   0.2990 * background->red +
               0.5870 * background->green +
               0.1140 * background->blue;
        bcb = -0.1687 * background->red +
              -0.3313 * background->green +
               0.5000 * background->blue;
        bcr =  0.5000 * background->red +
              -0.4187 * background->green +
              -0.0813 * background->blue;
        fy = (factor * fy) + ((1.0 - factor) * by);
        fcb = (factor * fcb) + ((1.0 - factor) * bcb);
        fcr = (factor * fcr) + ((1.0 - factor) * bcr);
        r = fy + 1.402 * fcr;
        g = fy + 0.34414 * fcb - 0.71414 * fcr;
        b = fy + 1.722 * fcb;
        a = (factor * foreground->alpha) + ((1.0 - factor) * background->alpha);
        bold->red = CLAMP (r, 0., 1.);
        bold->green = CLAMP (g, 0., 1.);
        bold->blue = CLAMP (b, 0., 1.);
        bold->alpha = CLAMP (a, 0., 1.);
}

typedef void (* PropertyWriteFunc) (const char *property_name,
                                    const GdkRGBA *color);

static void
write_style_property (const char *property_name,
                      const GdkRGBA *color)
{
        g_print ("/**\n"
                 " * VteTerminal: %s\n"
                 " *\n"
                 " * Since: 0.30\n"
                 " */\n"
                 "\n"
                 "gtk_widget_class_install_style_property\n"
                 "  (widget_class,\n"
                 "   g_param_spec_boxed (\"%s\", NULL, NULL,\n"
                 "                       GDK_TYPE_RGBA,\n"
                 "                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));\n"
                 "\n",
                 property_name, property_name);
}

static void
write_css_property (const char *property_name,
                    const GdkRGBA *color)
{
        char *color_string;

        if (strcmp (property_name, "selection-background-color") == 0 ||
            strcmp (property_name, "bold-foreground-color") == 0 ||
            strcmp (property_name, "dim-foreground-color") == 0)
                return;

        color_string = gdk_rgba_to_string (color);
        g_print ("\"-VteTerminal-%s: %s;\\n\"\n",
                 property_name,
                 color_string);
        g_free (color_string);
}

static void
write_property_va (PropertyWriteFunc func,
                   const GdkRGBA *color,
                   const char *format,
                   ...) G_GNUC_PRINTF (3, 4);

static void
write_property_va (PropertyWriteFunc func,
                   const GdkRGBA *color,
                   const char *format,
                   ...)
{
        va_list args;
        char *property_name;

        va_start (args, format);
        property_name = g_strdup_vprintf (format, args);
        va_end (args);

        func (property_name, color);

        g_free (property_name);
}

static void
write_properties (PropertyWriteFunc func)
{
        GdkRGBA color, fore, back;
        int i;

        for (i = 0; i < 8; ++i) {
                color.blue = (i & 4) ? 0.75 : 0.;
                color.green = (i & 2) ? 0.75 : 0.;
                color.red = (i & 1) ? 0.75 : 0.;
                color.alpha = 1.0;

                write_property_va (func, &color, "%s-color", color_names[i]);
        }

        for (i = 0; i < 8; ++i) {
                color.blue = (i & 4) ? 1. : 0.;
                color.green = (i & 2) ? 1. : 0.;
                color.red = (i & 1) ? 1. : 0.;
                color.alpha = 1.0;

                write_property_va (func, &color, "bright-%s-color", color_names[i]);
        }

        for (i = 0 ; i < 216; ++i) {
                int r, g, b, red, green, blue;

                r = i / 36;
                g = (i / 6) % 6;
                b = i % 6;
                red =   (r == 0) ? 0 : r * 40 + 55;
                green = (g == 0) ? 0 : g * 40 + 55;
                blue =  (b == 0) ? 0 : b * 40 + 55;
                color.red   = (red | red << 8) / 65535.;
                color.green = (green | green << 8) / 65535.;
                color.blue  = (blue | blue << 8) / 65535.;
                color.alpha = 1.;

                write_property_va (func, &color, "color-6-cube-%d-%d-%d-color", r + 1, g + 1, b + 1);
        }

        for (i = 0; i < 24; ++i) {
                int shade = 8 + i * 10;
                color.red = color.green = color.blue = (shade | shade << 8) / 65535.;
                color.alpha = 1.;

                write_property_va (func, &color, "shade-24-shades-%d-color", i + 1);
        }

        fore.red = fore.green = fore.blue = .75;
        fore.alpha = 1.;
        write_property_va (func, &fore, "foreground-color");

        back.red= back.green = back.blue = 0.;
        back.alpha = 1.;
        write_property_va (func, &back, "background-color");

        generate_bold(&fore, &back, 1.8, &color);
        write_property_va (func, &color, "bold-foreground-color");

        generate_bold(&fore, &back, 0.5, &color);
        write_property_va (func, &color, "dim-foreground-color");

        color.red = color.green = color.blue = 0.75;
        color.alpha = 1.;
        write_property_va (func, &color, "selection-background-color");

        color.red = color.green = color.blue = .75;
        color.alpha = 1.;
        write_property_va (func, &color, "cursor-background-color");
}

int
main (int argc,
      char *argv[])
{
        gboolean do_properties = FALSE, do_css = FALSE;
        const GOptionEntry options[] = {
                { "properties", 0, 0, G_OPTION_ARG_NONE, &do_properties, NULL, NULL },
                { "css", 0, 0, G_OPTION_ARG_NONE, &do_css, NULL, NULL },
                { NULL }
        };

        GOptionContext *context;
        GError *error = NULL;
        int i;

        g_type_init ();

        g_print ("/* Generated file, DO NOT EDIT\n"
                 " * Command:");
        for (i = 0; i < argc; ++i)
                g_print (" %s", argv[i]);
        g_print ("\n */\n\n");

        context = g_option_context_new ("");
        g_option_context_add_main_entries (context, options, NULL);

        if (!g_option_context_parse (context, &argc, &argv, &error))
                g_error ("Error parsing arguments: %s\n", error->message);
        g_option_context_free (context);

        if (do_properties)
                write_properties (write_style_property);
        else if (do_css)
                write_properties (write_css_property);

        return EXIT_SUCCESS;
}
