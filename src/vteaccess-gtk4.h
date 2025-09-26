/*
 * Copyright © 2024 Christian Hergert
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

#include <gtk/gtk.h>

G_BEGIN_DECLS

void _vte_accessible_text_iface_init (GtkAccessibleTextInterface *iface);
void _vte_accessible_text_init       (GtkAccessibleText          *accessible);
void _vte_accessible_text_scrolled   (GtkAccessibleText          *accessible, long delta);

#if GTK_CHECK_VERSION(4, 21, 0)
void _vte_accessible_hypertext_iface_init (GtkAccessibleHypertextInterface *iface);
#endif

G_END_DECLS
