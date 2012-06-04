#!/bin/bash
# Copyright © 2006 Shaun McCance <shaunm@gnome.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

__vte_urlencode() (
  # This is important to make sure string manipulation is handled
  # byte-by-byte.
  LANG=C
  arg="$1"
  i="0"
  while [ "$i" -lt ${#arg} ]; do
    c=${arg:$i:1}
    if echo "$c" | grep -q '[a-zA-Z/:_\.\-]'; then
      echo -n "$c"
    else
      echo -n "%"
      printf "%X" "'$c'"
    fi
    i=$((i+1))
  done
)

__vte_ps1() {
  printf "\e]7;file://%s" $HOSTNAME
  __vte_urlencode "$PWD"
  printf "\a"
}
