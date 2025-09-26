#!/usr/bin/env python3
#
# Copyright © 2025 Egmont Koblinger
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

# Generate C++ code that quickly checks if a sequence of characters is
# a valid emoji sequence or a prefix thereof.
#
# Usage:
#   emoji-generate.py emoji-test.txt
# where the specified file is a local copy of
#   https://www.unicode.org/Public/17.0.0/emoji/emoji-test.txt
# or newer.

import sys
import gi
from gi.repository import GLib


def numbers_to_intervals(nums):
  nums = sorted(nums)
  intervals = []
  start = 0
  while start < len(nums):
    end = start
    while end + 1 < len(nums) and nums[end + 1] == nums[end] + 1:
      end += 1
    intervals.append((nums[start], nums[end]))
    start = end + 1
  return intervals


def fmt_interval(interval):
  if interval[0] == interval[1]:
    return f'0x{interval[0]:04X}'
  else:
    return f'0x{interval[0]:04X} ... 0x{interval[1]:04X} /* {interval[1] - interval[0] + 1} */'


emoji_sequences = []

# Side quest: collect emoji combining chars, and also collect the
# single-width ones specially.
combining_chars = {}
single_width_combining_chars = {}

# Bonus: try to copy the Unicode version number to the generated file.
version_string = None

with open(sys.argv[1]) as f:
  for line in f:
    # Basic parsing
    if line == '' or line[0] == '#':
      if line.startswith('# Version:'):
        version_string = line[10:].strip()
      continue
    fields = line.split(';', 1);
    if len(fields) != 2:
      continue
    chars = fields[0].split()

    # Hex to int
    chars = [int(c, 16) for c in chars]

    # Only care about emojis consisting of at least 2 characters
    if len(chars) < 2:
      continue

    # Filter out subdivision flags. There are many more of them than
    # included in the test, and they work out of the box anyway.
    if chars[-1] == 0xE007F:
      continue

    emoji_sequences.append(chars)

    # Side quest continues.
    for c in chars[1:]:
      combining_chars[c] = True
      if (not GLib.unichar_iszerowidth(chr(c)) and
          not GLib.unichar_iswide(chr(c))):
        single_width_combining_chars[c] = True

# Add all pairs of regional indicators. We want all of them to pair up,
# not just the current list of actual country codes. The ones that appeared
# in the file will be duplicated, this doesn't cause any problem.
for i in range(0x1F1E6, 0x1F200):
  for j in range(0x1F1E6, 0x1F200):
    emoji_sequences.append([i, j])

# Build up a lookup tree. Beginning with nodes[0], every node is a dictionary
# mapping from the list of possible characters to the node number where lookup
# will continue with the next character.
#
# No empty dictionaries as leaf nodes. Instead, the previous node contains -1
# which acts like a "NULL pointer". This will result in a slightly faster
# lookup in C.
#
# Some emoji sequences are a prefix of others. We never need to tell whether
# the seen sequence is a complete emoji sequence, we only care whether it's
# a valid prefix (including complete match, of course). So we don't store
# in the tree which of the non-leaf nodes are possible endpoints.
#
# Example: if the strings are "a", "ab", "c" and "de" then this might be built up:
#   node 0:  { "a" -> node 1, "c" -> node 3, "d" -> node 2 }
#   node 1:  { "b" -> node 3 }
#   node 2:  { "e" -> node 3 }
#   node 3:  {}

nodes = [{}]
for chars in emoji_sequences:
  index = 0

  for c in chars:
    if c in nodes[index]:
      # Have already seen this prefix
      index = nodes[index][c]
    else:
      # Haven't seen this prefix yet, add a new node
      nodes[index][c] = len(nodes)
      index = len(nodes)
      nodes.append({})

# Optimize...

prev_len = -1
while len(nodes) != prev_len:
  prev_len = len(nodes)

  # pass 1: deduplication
  renumbering_pass1 = [x for x in range(len(nodes))]
  for i in range(len(nodes)):
    if renumbering_pass1[i] == i:
      for j in range(i + 1, len(nodes)):
        if nodes[i] == nodes[j]:
          renumbering_pass1[j] = i

  # pass2: compressing by getting rid of empty slots
  # also remove the deleted nodes
  renumbering_pass2 = [-1] * len(nodes)
  j = 0
  nodes2 = []
  for i in range(len(nodes)):
    if renumbering_pass1[i] == i:
      # keep the entry
      renumbering_pass2[i] = j
      j += 1
      nodes2.append(nodes[i])
  nodes = nodes2

  # adjust the references
  for node in nodes:
    for key in node.keys():
      node[key] = renumbering_pass2[renumbering_pass1[node[key]]]


# Print

print(f'''/* Auto-generated by emoji-generate.py, do not edit! */

/* Source file version: {version_string if version_string else "unknown"} */

/*
 * The code here cannot tell if a certain string is a complete valid emoji sequence, we're not interested in that.
 * It tells whether a certain string is a (possibly complete) prefix of a valid emoji sequence.
 *
 * Starting at node 0 for the first character of a string, each node tells which next node to jump to based on the next character.
 * If during lookup a NULL is encountered then it's not an emoji prefix.
 *
 * The functions' return type is the same as the functions' type (recursion, yay!), hence the necessary (void *) castings.
 */
''')

print('/* Forward declarations */')
for i in range(len(nodes)):
  print(f'static void *emoji_lookup_node_{i} (gunichar c);');
print()

print('/* Jump tables')
print(' *')
print(' * Note that gcc/clang-optimized switch() seems to be significantly faster')
print(' * than binary searching in predefined arrays. */')
print()

for i, node in enumerate(nodes):
  print(f'static void *emoji_lookup_node_{i} (gunichar c)')
  print('{')
  print('  switch (c) {')

  # node is a dict of codepoint -> node_to_jump_to. Twist it inside out to
  # a dict of node_to_jump_to -> [list of codepoints].
  revmap = {}
  for key, val in node.items():
    if val not in revmap:
      revmap[val] = []
    revmap[val].append(key)

  # sort each list
  for key, _ in revmap.items():
    revmap[key] = sorted(revmap[key])

  # print the lists
  for key, vals in revmap.items():
    intervals = numbers_to_intervals(vals)
    for interval in intervals:
      print(f'  case {fmt_interval(interval)}:')
    print(f'    return (void *) emoji_lookup_node_{key};')
  print('  default:')
  print('    return NULL;')
  print('  }')
  print('}')
  print()


# Complete the side quest.

print('/* Whather the character is potentially the second or later character of an emoji sequence */')
print('bool is_emoji_combining(gunichar c)')
print('{')
print('  switch (c) {')
for start, end in numbers_to_intervals(combining_chars):
  if start == end:
    print(f'  case 0x{start:04X}:')
  else:
    print(f'  case 0x{start:04X} ... 0x{end:04X}:')
print('    return true;')
print('  default:')
print('    return false;')
print('  }')
print('}')
print()

print('/* A hopefully slightly faster variant for when we already know that the character is narrow */')
print('bool is_single_width_emoji_combining(gunichar c)')
print('{')
print('  switch (c) {')
for start, end in numbers_to_intervals(single_width_combining_chars):
  if start == end:
    print(f'  case 0x{start:04X}:')
  else:
    print(f'  case 0x{start:04X} ... 0x{end:04X}:')
print('    return true;')
print('  default:')
print('    return false;')
print('  }')
print('}')
print()
