#!/usr/bin/env python3
# Copyright © 2019, 2024 Christian Persch
#
# This programme is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or (at your
# option) any later version.
#
# This programme is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this programme.  If not, see <https://www.gnu.org/licenses/>.

# This is a wrapper around `tic` to work around its inability to output
# a single file instead of a whole directory tree full of crap, and not
# having the same command line interface regardless of ncurses build options.
#
# We use a temporary directory here for `tic` to dump its output into,
# then just extract the file(s) we're interested in.
#
# Usage:
# ./run_tic.py SOURCE TERMINFO…

import os
import subprocess
import shutil
import sys
import tempfile

with tempfile.TemporaryDirectory() as terminfodir:
    rv = subprocess.call([
        'tic',
        '-x',
        '-o', terminfodir,
        sys.argv[1]])
    if rv != 0:
        sys.exit(rv)

    for ti in sys.argv[2:]:

        temppath = os.path.join(terminfodir, ti[0], ti)
        destpath = os.path.join('.', ti)
        shutil.move(temppath, destpath)

sys.exit(0)
