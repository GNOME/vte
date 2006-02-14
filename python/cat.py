#!/usr/bin/python
import sys
import string
import getopt
import gtk
import vte

def main_quit(object, *args):
	gtk.main_quit()

def commit_cb(object, *args):
	(text, length) = args
	# Echo the text input by the user to stdout.  Note that the string's
	# length isn't always going to be right.
	if (0):
		sys.stdout.write(text)
		sys.stdout.flush()
	else:
	# Test the get_text() function.
		for line in (string.splitfields(object.get_text(),"\n")):
			if (line.__len__() > 0):
				print line
	# Also display it.
	object.feed(text, length)

if __name__ == '__main__':
	font = "fixed 12"
	scrollback = 100
	# Let the user override them.
	(shorts, longs) = getopt.getopt(sys.argv[1:], "f:", ["font="])
	for argpair in (shorts + longs):
		if ((argpair[0] == '-f') or (argpair[0] == '--font')):
			print "Setting font to `" + argpair[1] + "'."
			font = argpair[1]
	window = gtk.Window()
	window.connect("delete-event", main_quit)

	terminal = vte.Terminal()
	terminal.set_cursor_blinks(gtk.TRUE)
	terminal.set_emulation("xterm")
	terminal.set_font_from_string(font)
	terminal.set_scrollback_lines(1000)
	terminal.set_audible_bell(gtk.TRUE)
	terminal.set_visible_bell(gtk.FALSE)
	terminal.connect("commit", commit_cb)
	terminal.show()

	scrollbar = gtk.VScrollbar()
	scrollbar.set_adjustment(terminal.get_adjustment())

	box = gtk.HBox()
	box.pack_start(terminal)
	box.pack_start(scrollbar)

	window.add(box)
	window.show_all()
	gtk.main()
