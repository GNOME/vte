#!/bin/sh

# rudimentary Vim performance benchmark

# scrolling (just the cursor)
time vim -u scroll.vim -c ':quit' UTF-8-demo.txt
time vim -u scroll.vim -c ':call AutoScroll(1000)' UTF-8-demo.txt
