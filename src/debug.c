/*
 * Copyright (C) 2002,2003 Red Hat, Inc.
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

#include <config.h>

#include <glib.h>
#include "debug.h"

VteDebugFlags _vte_debug_flags;

void
_vte_debug_init(void)
{
#ifdef VTE_DEBUG
  const GDebugKey keys[] = {
    { "misc",         VTE_DEBUG_MISC         },
    { "io",           VTE_DEBUG_IO           },
    { "adj",          VTE_DEBUG_ADJ          },
    { "updates",      VTE_DEBUG_UPDATES      },
    { "events",       VTE_DEBUG_EVENTS       },
    { "parse",        VTE_DEBUG_PARSE        },
    { "signals",      VTE_DEBUG_SIGNALS      },
    { "selection",    VTE_DEBUG_SELECTION    },
    { "substitution", VTE_DEBUG_SUBSTITUTION },
    { "ring",         VTE_DEBUG_RING         },
    { "pty",          VTE_DEBUG_PTY          },
    { "cursor",       VTE_DEBUG_CURSOR       },
    { "keyboard",     VTE_DEBUG_KEYBOARD     },
    { "lifecycle",    VTE_DEBUG_LIFECYCLE    },
    { "trie",         VTE_DEBUG_TRIE         },
    { "work",         VTE_DEBUG_WORK         },
    { "cells",        VTE_DEBUG_CELLS        },
    { "timeout",      VTE_DEBUG_TIMEOUT      },
    { "draw",         VTE_DEBUG_DRAW         },
    { "ally",         VTE_DEBUG_ALLY         },
    { "pangocairo",   VTE_DEBUG_PANGOCAIRO   },
    { "widget-size",  VTE_DEBUG_WIDGET_SIZE  },
    { "bg",           VTE_DEBUG_BG           }
  };

  _vte_debug_flags = g_parse_debug_string (g_getenv("VTE_DEBUG"),
                                           keys, G_N_ELEMENTS (keys));
  _vte_debug_print(0xFFFFFFFF, "VTE debug flags = %x\n", _vte_debug_flags);
#endif /* VTE_DEBUG */
}
