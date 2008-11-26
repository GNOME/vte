#ifndef GNOME_PTY_H
#define GNOME_PTY_H

typedef enum {
	GNOME_PTY_OPEN_PTY_UTMP = 1,
	GNOME_PTY_OPEN_PTY_UWTMP,
	GNOME_PTY_OPEN_PTY_WTMP,
	GNOME_PTY_OPEN_PTY_LASTLOG,
	GNOME_PTY_OPEN_PTY_LASTLOGUTMP,
	GNOME_PTY_OPEN_PTY_LASTLOGUWTMP,
	GNOME_PTY_OPEN_PTY_LASTLOGWTMP,
	GNOME_PTY_OPEN_NO_DB_UPDATE,
	GNOME_PTY_RESET_TO_DEFAULTS,
	GNOME_PTY_CLOSE_PTY,
	GNOME_PTY_SYNCH
} GnomePtyOps;

void *update_dbs         (int utmp, int wtmp, int lastlog, char *login_name, char *display_name, char *term_name);
void *write_login_record (char *login_name, char *display_name, char *term_name, int utmp, int wtmp, int lastlog);
void write_logout_record (char *login_name, void *data, int utmp, int wtmp);

#endif
