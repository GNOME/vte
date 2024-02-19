# Copyright © 2024 Christian Persch
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

terminfo_sources = files(
  'terminfo.src',
)

terminfos = [
  'xterm-256color',
]

foreach terminfo: terminfos
  install_data(
    files(terminfo),
    install_dir: vte_prefix / vte_terminfodir / terminfo[0],
    install_tag: 'runtime',
  )
endforeach
