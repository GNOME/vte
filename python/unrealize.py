#!/usr/bin/python2.2
import sys
import getopt
import gtk
import vte

# FIXME: figure out why we don't get a PID here.
def exited_cb(terminal):
	gtk.mainquit()

def nuke(button, (box, terminal)):
	box.remove(terminal)
	box.pack_start(terminal)

if __name__ == '__main__':
	child_pid = -1;
	# Defaults.
	emulation = "xterm"
	font = "fixed 12"
	command = None
	# Let the user override them.
	(shorts, longs) = getopt.getopt(sys.argv[1:], "c:t:f:", ["command=", "terminal=", "font="])
	for argpair in (shorts + longs):
		if ((argpair[0] == '-c') or (argpair[0] == '--command')):
			print "Running command `" + argpair[1] + "'."
			command = argpair[1]
		if ((argpair[0] == '-f') or (argpair[0] == '--font')):
			print "Setting font to `" + argpair[1] + "'."
			font = argpair[1]
		if ((argpair[0] == '-t') or (argpair[0] == '--terminal')):
			print "Setting terminal type to `" + argpair[1] + "'."
			emulation = argpair[1]
	window = gtk.Window()
	terminal = vte.Terminal()
	terminal.set_emulation(emulation)
	terminal.set_font_from_string(font)
	terminal.connect("child-exited", exited_cb)
	if (command):
		# Start up the specified command.
		child_pid = terminal.fork_command(command)
	else:
		# Start up the default command, the user's shell.
		child_pid = terminal.fork_command()
	box = gtk.VBox()
	box.pack_start(terminal)
	button = gtk.Button("remove")
	button.connect("pressed", nuke, (box, terminal))
	box.pack_start(button)
	window.add(box)
	window.show_all()
	gtk.main()
