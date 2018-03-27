/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2018 Christian Persch
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <assert.h>

typedef int vte_seq_arg_t;

#define VTE_SEQ_ARG_INIT_DEFAULT (-1)
#define VTE_SEQ_ARG_INIT(value) (value)

static inline void vte_seq_arg_push(vte_seq_arg_t* arg,
                                    uint32_t c)
{
        auto value = *arg;

        if (value < 0)
                value = 0;
        value = value * 10 + (c - '0');

        /*
         * VT510 tells us to clamp all values to [0, 9999], however, it
         * also allows commands with values up to 2^15-1. We simply use
         * 2^16 as maximum here to be compatible to all commands, but
         * avoid overflows in any calculations.
         */
        if (value > 0xffff)
                value = 0xffff;

        *arg = value;
}

static inline void vte_seq_arg_finish(vte_seq_arg_t* arg)
{
}

static inline bool vte_seq_arg_started(vte_seq_arg_t arg)
{
        return arg >= 0;
}

static inline bool vte_seq_arg_finished(vte_seq_arg_t arg)
{
        return arg >= 0;
}

static inline bool vte_seq_arg_default(vte_seq_arg_t arg)
{
        return arg == -1;
}

static inline int vte_seq_arg_value(vte_seq_arg_t arg)
{
        return arg;
}
