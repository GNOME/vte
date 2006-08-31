/*
 * Copyright (C) 2000-2002 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "../config.h"
#include <sys/types.h>
#include <ctype.h>
#include <limits.h>
#ifdef HAVE_SYS_SYSLIMITS_H
#include <sys/syslimits.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#ifndef TERMCAP_MAYBE_STATIC
#define TERMCAP_MAYBE_STATIC
#include "vtetc.h"
#endif

#include "debug.h"

struct _vte_termcap {
	char *comment;
	struct _vte_termcap_entry {
		char *comment;
		char *string;
		gssize length;
		struct _vte_termcap_entry *next;
	} *entries;
	struct _vte_termcap_alias {
		char *name;
		struct _vte_termcap_entry *entry;
		struct _vte_termcap_alias *next;
	} *names;
	GTree *nametree;
};

static GStaticMutex _vte_termcap_mutex = G_STATIC_MUTEX_INIT;
static GCache *_vte_termcap_cache = NULL;

static char *
nextline(FILE *fp, gssize *outlen)
{
	char buf[LINE_MAX];
	gssize len = 0;
	char *ret = NULL;
	gssize retlen = 0;
	char *tmp = NULL;

	if (!feof(fp)) do {
		if (fgets(buf, sizeof(buf), fp) != buf) {
			break;
		}
		len = strlen(buf);
		tmp = g_malloc(retlen + len + 1);
		if (retlen > 0) {
			memcpy(tmp, ret, retlen);
		}
		memcpy(tmp + retlen, buf, len + 1);
		if (ret != NULL) {
			g_free(ret);
		}
		retlen += len;
		ret = tmp;
		ret[retlen] = '\0';
	} while ((len > 0) && (buf[retlen - 1] != '\n') && !feof(fp));

	if ((ret != NULL) && (retlen > 0) && (ret[retlen - 1] == '\n')) {
		retlen--;
		ret[retlen] = '\0';
	}

	if ((ret != NULL) && (retlen > 0) && (ret[retlen - 1] == '\r')) {
		retlen--;
		ret[retlen] = '\0';
	}

	*outlen = retlen;
	return ret;
}

static char *
nextline_with_continuation(FILE *fp)
{
	char *ret = NULL;
	gssize rlen = 0, slen = 0;
	char *s, *tmp;
	gboolean continuation = FALSE;
	do {
		s = nextline(fp, &slen);
		if (s == NULL) {
			break;
		}
		tmp = g_malloc(slen + rlen + 1);
		if (rlen > 0) {
			memcpy(tmp, ret, rlen);
		}
		memcpy(tmp + rlen, s, slen + 1);
		if (ret != NULL) {
			g_free(ret);
		}
		g_free(s);
		ret = tmp;
		rlen += slen;
		if ((rlen > 0) && (ret[rlen - 1] == '\\')) {
			ret[rlen - 1] = '\0';
			rlen--;
			continuation = TRUE;
		} else {
			continuation = FALSE;
		}
	} while ((rlen == 0) || continuation);
	return ret;
}

static void
_vte_termcap_add_aliases(struct _vte_termcap *termcap,
			 struct _vte_termcap_entry *entry,
			 const char *aliases)
{
	gssize l;
	struct _vte_termcap_alias *alias = NULL;
	const char *p;

	for (p = aliases, l = 0; p != NULL; l++) {
		if (aliases[l] == '\\') {
			l++;
		} else
		if ((aliases[l] == '|') ||
		   (aliases[l] == ':') ||
		   (aliases[l] == '\0')) {
			alias = g_slice_new0(struct _vte_termcap_alias);
			alias->name = g_strndup(p, &aliases[l] - p);
			alias->entry = entry;
			alias->next = termcap->names;
			termcap->names = alias;
			if (aliases[l] == '\0') {
				p = NULL;
			} else {
				p = &aliases[l + 1];
			}
			g_tree_insert(termcap->nametree,
				      GINT_TO_POINTER(g_quark_from_string(alias->name)),
				      alias);
			l++;
		}
	}
}

static void
_vte_termcap_add_entry(struct _vte_termcap *termcap,
		       const char *s, gssize length, char *comment)
{
	struct _vte_termcap_entry *entry = NULL;
	char *p = NULL;
	gssize l;

	entry = g_slice_new0(struct _vte_termcap_entry);
	entry->string = g_malloc(length + 1);
	if (length > 0) {
		memcpy(entry->string, s, length);
	}
	entry->string[length] = '\0';
	entry->length = length;
	entry->comment = comment;
	entry->next = termcap->entries;
	termcap->entries = entry;
	for (l = 0; l < length; l++) {
		if (s[l] == '\\') {
			l++;
			continue;
		}
		if (s[l] == ':') {
			break;
		}
	}
	if (l <= length) {
		p = g_malloc(l + 1);
		strncpy(p, s, l);
		p[l] = '\0';
		_vte_termcap_add_aliases(termcap, entry, p);
		g_free(p);
	}
}

static void
_vte_termcap_strip_with_pad(const char *termcap, char **stripped, gssize *len)
{
	char *ret;
	gssize i, o, length;
	length = strlen(termcap);

	ret = g_malloc(length + 2);
	for (i = o = 0; i < length; i++) {
		ret[o++] = termcap[i];
		if (termcap[i] == '\\') {
			char *p;
			switch (termcap[i + 1]) {
				case '\n':
					while ((termcap[i + 1] == ' ') ||
					       (termcap[i + 1] == '\t')) {
						i++;
					}
					continue;
				case 'E':
				case 'e':
					i++;
					ret[o - 1] = 27;
					continue;
				case 'n':
					i++;
					ret[o - 1] = 10;
					continue;
				case 'r':
					i++;
					ret[o - 1] = 13;
					continue;
				case 't':
					i++;
					ret[o - 1] = 8;
					continue;
				case 'b':
					i++;
					ret[o - 1] = 9;
					continue;
				case 'f':
					i++;
					ret[o - 1] = 12;
					continue;
				case '0':
				case '1':
					i++;
					ret[o - 1] = strtol(termcap + i, &p, 8);
					p--;
					i = p - termcap;
					continue;
			}
		} else
		if (termcap[i] == '^') {
			switch (termcap[i + 1]) {
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				case 'G':
				case 'H':
				case 'I':
				case 'J':
				case 'K':
				case 'L':
				case 'M':
				case 'N':
				case 'O':
				case 'P':
				case 'Q':
				case 'R':
				case 'S':
				case 'T':
				case 'U':
				case 'V':
				case 'W':
				case 'X':
				case 'Y':
				case 'Z':
					i++;
					ret[o - 1] = termcap[i] - ('A' - 1);
					continue;
				default:
					break;
			}
		} else {
			if (termcap[i] == ':') {
				while ((termcap[i + 1] == ' ') ||
				       (termcap[i + 1] == '\t')) {
					i++;
				}
				continue;
			}
		}
	}
	ret[o] = ':';
	o++;
	ret[o] = '\0';
	*stripped = ret;
	*len = o;
}

/**
 * _vte_termcap_strip:
 * @termcap: a termcap structure
 * @stripped: a location to store the new stripped version of the string
 * @len: a location to store the length of the new string
 *
 * Converts various types of sequences used to represent non-printable data
 * into the data they represent.  Specifically, this resolves ^{char} sequences,
 * octal escapes. and the \r, \n, \E, \t, \b, and \f sequences.
 *
 */
void
_vte_termcap_strip(const char *termcap, char **stripped, gssize *len)
{
	_vte_termcap_strip_with_pad(termcap, stripped, len);
	while (((*len) > 0) && ((*stripped)[(*len) - 1] == ':')) {
		(*len)--;
		(*stripped)[*len] = '\0';
	}
}

static gint
_vte_direct_compare(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

/* Allocates and initializes new termcap instance. */
static gpointer
_vte_termcap_create(gpointer key)
{
	const char *filename = key;
	struct _vte_termcap *ret = NULL;
	FILE *fp;
	char *s, *stripped, *comment = NULL;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
		fprintf(stderr, "_vte_termcap_create()\n");
	}
#endif
	fp = fopen(filename, "r");
	if (fp != NULL) {
		while ((s = nextline_with_continuation(fp)) != NULL) {
			gssize slen;
			if ((s[0] != '#') && (isprint(s[0]))) {
				if (ret == NULL) {
					ret = g_slice_new0(struct _vte_termcap);
					ret->nametree = g_tree_new(_vte_direct_compare);
				}
				stripped = NULL;
				_vte_termcap_strip_with_pad(s, &stripped, &slen);
				if (stripped) {
					_vte_termcap_add_entry(ret, stripped,
							       slen, comment);
					comment = NULL;
					g_free(stripped);
				}
			} else {
				slen = strlen(s);
				if (comment == NULL) {
					comment = g_malloc(slen + 2);
					memcpy(comment, s, slen);
					comment[slen] = '\n';
					comment[slen + 1] = '\0';
				} else {
					char *tmp;
					gssize clen;
					clen = strlen(comment);
					tmp = g_malloc(slen + clen + 2);
					if (tmp == NULL) {
						return NULL;
					}
					memcpy(tmp, comment, clen);
					memcpy(tmp + clen, s, slen);
					tmp[clen + slen] = '\n';
					tmp[clen + slen + 1] = '\0';
					g_free(comment);
					comment = tmp;
				}
			}
			g_free(s);
		}
		ret->comment = comment;
		fclose(fp);
	}
	return ret;
}

/* Noone uses termcap, destroy it. */
static void
_vte_termcap_destroy(gpointer key)
{
	struct _vte_termcap *termcap = key;
	struct _vte_termcap_entry *entry, *nextentry;
	struct _vte_termcap_alias *alias, *nextalias;

	if (termcap == NULL)
		return;

#ifdef VTE_DEBUG
	if (_vte_debug_on(VTE_DEBUG_LIFECYCLE)) {
		fprintf(stderr, "_vte_termcap_destroy()\n");
	}
#endif
	for (entry = termcap->entries; entry != NULL; entry = nextentry) {
		nextentry = entry->next;
		g_free(entry->comment);
		entry->comment = NULL;
		g_free(entry->string);
		entry->string = NULL;
		g_slice_free(struct _vte_termcap_entry, entry);
	}
	for (alias = termcap->names; alias != NULL; alias = nextalias) {
		nextalias = alias->next;
		g_free(alias->name);
		alias->name = NULL;
		g_slice_free(struct _vte_termcap_alias, alias);
	}
	g_tree_destroy(termcap->nametree);
	termcap->nametree = NULL;
	g_free(termcap->comment);
	termcap->comment = NULL;
	g_slice_free(struct _vte_termcap, termcap);
}

TERMCAP_MAYBE_STATIC struct _vte_termcap *
_vte_termcap_new(char *filename)
{
	struct _vte_termcap *ret = NULL;
	g_static_mutex_lock(&_vte_termcap_mutex);

	if (_vte_termcap_cache == NULL) {
		_vte_termcap_cache = g_cache_new(_vte_termcap_create,
				_vte_termcap_destroy, g_strdup, g_free,
				g_str_hash, g_direct_hash, g_str_equal);
	}
	ret = g_cache_insert(_vte_termcap_cache, filename);

	g_static_mutex_unlock(&_vte_termcap_mutex);
	return ret;
}

/**
 * _vte_termcap_free:
 * @termcap: the structure to be freed
 *
 * Frees the indicated structure.
 *
 */
TERMCAP_MAYBE_STATIC void
_vte_termcap_free(struct _vte_termcap *termcap)
{
	g_assert(_vte_termcap_cache != NULL);
	g_static_mutex_lock(&_vte_termcap_mutex);
	g_cache_remove(_vte_termcap_cache, termcap);
	g_static_mutex_unlock(&_vte_termcap_mutex);
}

static const char *
_vte_termcap_find_l(struct _vte_termcap *termcap, const char *tname, gssize len,
		    const char *cap)
{
	const char *ret;
	struct _vte_termcap_alias *alias;
	char *ttname;
	gssize clen;

	g_assert(termcap != NULL);
	g_assert(tname != NULL);
	g_assert(len > 0);
	g_assert(cap != NULL);
	g_assert(strlen(cap) > 0);

	/* Find the entry by this name. */
	ttname = g_strndup(tname, len);
	alias = g_tree_lookup(termcap->nametree,
			      GINT_TO_POINTER(g_quark_from_string(ttname)));
	g_free(ttname);

	/* If we found the entry, poke around in it. */
	if (alias != NULL) {
		char *str = alias->entry->string;
		const char *nextcap = "tc";
		gssize len = alias->entry->length;

		clen = strlen(cap);
		ret = str;

		/* Search for the capability in this entry. */
		for (ret = str - 1;
		     ret != NULL;
		     ret = memchr(ret, ':', str + len - ret - clen)) {
			/* We've hit the first separator, or are before the
			 * very first part of the entry, so hit the next
			 * capability. */
			ret++;
			/* If the end of the entry's name isn't the end of the
			 * string, and it isn't a boolean/string/numeric, or
			 * its name is wrong, keep looking. */
			if (((ret[clen] != '\0') &&
			     (ret[clen] != ':') &&
			     (ret[clen] != '=') &&
			     (ret[clen] != '#')) ||
			    (memcmp(ret, cap, clen) != 0)) {
				continue;
			}
			/* Found it. */
			return ret;
		}

		/* Now find the "tc=" entries, and scan those entries. */
		clen = strlen(nextcap);
		ret = str - 1;

		while (ret != NULL) {
			for (;
			     ret != NULL;
			     ret = memchr(ret, ':', str + len - ret - clen)) {
				ret++;
				if (((ret[clen] != '\0') &&
				     (ret[clen] != ':') &&
				     (ret[clen] != '=') &&
				     (ret[clen] != '#')) ||
				    (memcmp(ret, nextcap, clen) != 0)) {
					continue;
				}
				break;
			}

			if (ret != NULL) {
				const char *t;
				char *end;
				end = strchr(ret + clen + 1, ':');
				if (end != NULL) {
					t = _vte_termcap_find_l(termcap,
								ret + clen + 1,
								end -
								(ret + clen + 1),
								cap);
				} else {
					t = _vte_termcap_find_l(termcap,
								ret + clen + 1,
								strlen(ret +
								       clen + 1),
								cap);
				}
				if ((t != NULL) && (t[0] != '\0')) {
					return t;
				}
				ret++;
			}
		}
	}
	return "";
}

static const char *
_vte_termcap_find(struct _vte_termcap *termcap,
		  const char *tname, const char *cap)
{
	g_assert(termcap != NULL);
	return _vte_termcap_find_l(termcap, tname, strlen(tname), cap);
}

/**
 * _vte_termcap_find_boolean:
 * @termcap: a termcap structure
 * @tname: the name of the terminal type being queried
 * @cap: the name of the capability being queried
 *
 * Checks if the given boolean capability is defined.
 *
 * Returns: %TRUE if the terminal type is known and the capability is defined
 * for it
 */
TERMCAP_MAYBE_STATIC gboolean
_vte_termcap_find_boolean(struct _vte_termcap *termcap, const char *tname,
			  const char *cap)
{
	const char *val;
	g_return_val_if_fail(termcap != NULL, FALSE);
	val = _vte_termcap_find(termcap, tname, cap);
	if ((val != NULL) && (val[0] != '\0')) {
		return TRUE;
	}
	return FALSE;
}

/**
 * _vte_termcap_find_numeric:
 * @termcap: a termcap structure
 * @tname: the name of the terminal type being queried
 * @cap: the name of the capability being queried
 *
 * Checks if the given numeric capability is defined.
 *
 * Returns: non-zero if the terminal type is known and the capability is defined
 * to a non-zero value for it
 */
TERMCAP_MAYBE_STATIC long
_vte_termcap_find_numeric(struct _vte_termcap *termcap, const char *tname,
			  const char *cap)
{
	const char *val;
	char *p;
	gssize l;
	long ret;
	g_return_val_if_fail(termcap != NULL, 0);
	val = _vte_termcap_find(termcap, tname, cap);
	if ((val != NULL) && (val[0] != '\0')) {
		l = strlen(cap);
		ret = strtol(val + l + 1, &p, 0);
		if ((p != NULL) && ((*p == '\0') || (*p == ':'))) {
			return ret;
		}
	}
	return 0;
}

/**
 * _vte_termcap_find_string:
 * @termcap: a termcap structure
 * @tname: the name of the terminal type being queried
 * @cap: the name of the capability being queried
 *
 * Checks if the given string capability is defined.
 *
 * Returns: the value of the capability if the terminal type is known and the
 * capability is defined for it, else an empty string.  The return value must
 * always be freed by the caller.
 */
TERMCAP_MAYBE_STATIC char *
_vte_termcap_find_string(struct _vte_termcap *termcap, const char *tname,
			 const char *cap)
{
	const char *val, *p;
	gssize l;
	val = _vte_termcap_find(termcap, tname, cap);
	if ((val != NULL) && (val[0] != '\0')) {
		l = strlen(cap);
		val += (l + 1);
		p = strchr(val, ':');
		if (p) {
			return g_strndup(val, p - val);
		} else {
			return g_strdup(val);
		}
	}
	return g_strdup("");
}

/**
 * _vte_termcap_find_string_length:
 * @termcap: a termcap structure
 * @tname: the name of the terminal type being queried
 * @cap: the name of the capability being queried
 * @length: the location to store the length of the returned string
 *
 * Checks if the given string capability is defined.  This version of
 * _vte_termcap_find_string() properly handles zero bytes in the result.
 *
 * Returns: the value of the capability if the terminal type is known and the
 * capability is defined for it, else an empty string.  The return value must
 * always be freed by the caller.
 */
TERMCAP_MAYBE_STATIC char *
_vte_termcap_find_string_length(struct _vte_termcap *termcap, const char *tname,
				const char *cap, gssize *length)
{
	const char *val, *p;
	char *ret;
	gssize l;
	val = _vte_termcap_find(termcap, tname, cap);
	if ((val != NULL) && (val[0] != '\0')) {
		l = strlen(cap);
		val += (l + 1);
		p = val;
		while (*p != ':') p++;
		l = p - val;
		if (length) {
			*length = l;
		}
		ret = g_malloc(l + 1);
		if (l > 0) {
			memcpy(ret, val, l);
		}
		ret[l] = '\0';
		return ret;
	}
	return g_strdup("");
}

#if 0
TERMCAP_MAYBE_STATIC const char *
vte_termcap_comment(struct vte_termcap *termcap, const char *tname)
{
	struct vte_termcap_alias *alias;
	gssize len;
	if ((tname == NULL) || (tname[0] == '\0')) {
		return termcap->comment;
	}
	len = strlen(tname);
	for (alias = termcap->names; alias != NULL; alias = alias->next) {
		if (strncmp(tname, alias->name, len) == 0) {
			if (alias->name[len] == '\0') {
				break;
			}
		}
	}
	if (alias && (alias->entry != NULL)) {
		return alias->entry->comment;
	}
	return NULL;
}

/* FIXME: should escape characters we've previously decoded. */
TERMCAP_MAYBE_STATIC char *
vte_termcap_generate(struct vte_termcap *termcap)
{
	gssize size;
	char *ret = NULL;
	struct vte_termcap_entry *entry;
	size = strlen(termcap->comment ? termcap->comment: "");
	for (entry = termcap->entries; entry != NULL; entry = entry->next) {
		size += (entry->comment ? strlen(entry->comment) : 0);
		size += (entry->string ? strlen(entry->string) : 0) + 1;
	}
	ret = g_malloc(size + 1);
	if (ret == NULL) {
		return NULL;
	}
	memset(ret, '\0', size);
	size = 0;
	for (entry = termcap->entries; entry != NULL; entry = entry->next) {
		if (entry->comment) {
			memcpy(ret + size, entry->comment,
			       strlen(entry->comment));
			size += strlen(entry->comment);
		}
		if (entry->string) {
			memcpy(ret + size, entry->string,
			       strlen(entry->string));
			size += strlen(entry->string);
			ret[size] = '\n';
			size++;
		}
	}
	if (termcap->comment) {
		memcpy(ret + size, termcap->comment,
		       strlen(termcap->comment));
		size += strlen(termcap->comment);
	}
	return ret;
}
#endif

#ifdef TERMCAP_MAIN
int
main(int argc, char **argv)
{
	const char *tc = (argc > 1) ? argv[1] : "linux";
	const char *cap = (argc > 2) ? argv[2] : "so";
	char *value;
	struct _vte_termcap *termcap;
	_vte_debug_parse_string(getenv("VTE_DEBUG_FLAGS"));
	termcap = _vte_termcap_new("/etc/termcap");
	value = _vte_termcap_find_string(termcap, tc, cap);
	printf("%s\n", value);
	g_free(value);
	_vte_termcap_free(termcap);
	return 0;
}
#endif
