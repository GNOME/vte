/*
 * Copyright (C) 2004 Benjamin Otte <otte@gnome.org>
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

#include "vtetree.h"

VteTree *
_vte_tree_new(GCompareFunc key_compare_func)
{
  VteTree *tree = g_new0 (VteTree, 1);
  tree->tree = g_tree_new (key_compare_func);
  return tree;
}

void 
_vte_tree_destroy(VteTree *tree)
{
  g_tree_destroy (tree->tree);
  g_free (tree);
}

void 
_vte_tree_insert(VteTree *tree, gpointer key, gpointer value)
{
  guint index = GPOINTER_TO_UINT (key);
  
  if (index < VTE_TREE_ARRAY_SIZE) {
    tree->array[index] = value;
    return;
  }
  g_tree_insert (tree->tree, key, value);
}

gpointer
_vte_tree_lookup(VteTree *tree, gconstpointer key)
{
  const guint index = GPOINTER_TO_UINT (key);
  
  if (index < VTE_TREE_ARRAY_SIZE)
    return tree->array[index];

  return g_tree_lookup (tree->tree, key);
}

