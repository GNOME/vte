# Copyright © 2025 Christian Persch
#
# This library is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library.  If not, see <https://www.gnu.org/licenses/>.

JQ = jq
SED = sed

all: org.gnome.vte.gtk3.json org.gnome.vte.gtk4.json

org.gnome.vte.gtk3.json: org.gnome.vte.gtk.json.in
	$(SED) -e "s/@gtk@/3/g" \
	       -e "s/@gtk3@/true/g" \
	       -e "s/@gtk4@/false/g" \
	       -e "s/@exesuffix@//g" \
	       $< | $(JQ) . > $@

org.gnome.vte.gtk4.json: org.gnome.vte.gtk.json.in
	$(SED) -e "s/@gtk@/4/g" \
	       -e "s/@gtk3@/false/g" \
	       -e "s/@gtk4@/true/g" \
	       -e "s/@exesuffix@/-gtk4/g" \
	       $< | $(JQ) . > $@
