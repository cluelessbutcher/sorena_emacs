#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "epath.h"
#include "line.h"
#include "util.h"
#include "utf8.h"
#include <stdlib.h>
#include <strings.h>

int show_text_buffer(char *bname, const char *const *text)
{
	struct buffer *bp;
	int i;

	bp = find_buffer(bname, TRUE, BFINVS);
	if (bp == NULL)
		return FALSE;
	bp->b_mode &= ~MDVIEW;
	if (clear_buffer(bp) == FALSE)
		return FALSE;
	for (i = 0; text[i] != NULL; i++)
		if (addline(bp, (char *)text[i]) == FALSE)
			return FALSE;
	bp->b_mode |= MDVIEW;
	bp->b_flag &= ~BFCHG;
	if (curwp->w_bufp != bp && swbuffer(bp) != TRUE)
		return FALSE;
	curwp->w_linep = line_next(bp->b_linep);
	curwp->w_dotp = line_next(bp->b_linep);
	curwp->w_doto = 0;
	curwp->w_markp = NULL;
	curwp->w_marko = 0;
	curwp->w_flag |= WFMODE | WFHARD;
	return TRUE;
}

int cmd_help(int f, int n)
{
	return show_text_buffer("*Help*", help_text);
}

static int build_binding_list(char *match)
{
	struct name_bind *nptr;
	struct key_tab *ktp;
	struct buffer *bp;
	char outseq[NSTRING];
	int cpos;

	bp = find_buffer("*Binding list*", TRUE, BFINVS);
	if (bp != NULL)
		bp->b_mode &= ~MDVIEW;
	if (bp == NULL || clear_buffer(bp) == FALSE) {
		msg_printf("Can not display binding list");
		return FALSE;
	}

	for (nptr = &names[0]; nptr->n_func != NULL; ++nptr) {
		if (match && strstr(nptr->n_name, match) == NULL)
			continue;

		strcpy(outseq, nptr->n_name);
		cpos = strlen(outseq);

		for (ktp = &keytab[0]; ktp->k_fp != NULL; ++ktp) {
			if (ktp->k_fp != nptr->n_func)
				continue;
			if ((ktp->k_code & (CTLX | CTLC)) == (CTLX | CTLC))
				continue;
			while (cpos < 36)
				outseq[cpos++] = ' ';
			cmdstr(ktp->k_code, &outseq[cpos]);
			if (addline(bp, outseq) == FALSE)
				return FALSE;
			cpos = 0;
		}

		if (cpos > 0) {
			outseq[cpos] = 0;
			if (addline(bp, outseq) == FALSE)
				return FALSE;
		}
	}

	bp->b_mode |= MDVIEW;
	bp->b_flag &= ~BFCHG;

	if (curwp->w_bufp != bp && swbuffer(bp) != TRUE)
		return FALSE;
	curwp->w_linep = line_next(bp->b_linep);
	curwp->w_dotp = line_next(bp->b_linep);
	curwp->w_doto = 0;
	curwp->w_markp = NULL;
	curwp->w_marko = 0;
	curwp->w_flag |= WFMODE | WFHARD;
	return TRUE;
}

int cmd_describe_bindings(int f, int n)
{
	return build_binding_list(NULL);
}

int cmd_apropos(int f, int n)
{
	char match[NSTRING];
	int status;

	status = ask_string("Apropos string: ", match, NSTRING - 1);
	if (status != TRUE)
		return status;
	return build_binding_list(match);
}

int cmd_describe_key(int f, int n)
{
	int c;
	char *ptr;
	char outseq[NSTRING];

	msg_printf("Describe key: ");
	c = getckey(FALSE);
	cmdstr(c, &outseq[0]);
	ptr = getfname(getbind(c));
	if (ptr == NULL && (c & SHIFT) && getbind(c & ~SHIFT) != NULL) {
		ptr = getfname(getbind(c & ~SHIFT));
		msg_printf("%s runs %s (extending the selection)", outseq, ptr ? ptr : "?");
		return TRUE;
	}
	if (ptr == NULL) {
		if (!(c & (CONTROL | META | SPEC | CTLX | CTLC)) && c >= 0x20)
			ptr = "self-insert";
		else
			ptr = "nothing (key is not bound)";
	}
	msg_printf("%s runs %s", outseq, ptr);
	return TRUE;
}

int cmd_bind_to_key(int f, int n)
{
	unsigned int c;
	fn_t kfunc;
	struct key_tab *ktp;
	int found;
	char outseq[80];

	kfunc = getname("Bind function: ");
	if (kfunc == NULL) {
		msg_printf("(No such function)");
		return FALSE;
	}
	msg_printf("Bind %s to key: ", getfname(kfunc));

	c = getckey((kfunc == cmd_meta_prefix) || (kfunc == cmd_ctlx_prefix) || (kfunc == cmd_universal_argument) || (kfunc == cmd_abort_command));

	cmdstr(c, &outseq[0]);

	ostring(outseq);

	if (kfunc == cmd_meta_prefix || kfunc == cmd_ctlx_prefix || kfunc == cmd_universal_argument || kfunc == cmd_abort_command) {
		ktp = &keytab[0];
		found = FALSE;
		while (ktp->k_fp != NULL) {
			if (ktp->k_fp == kfunc)
				unbindchar(ktp->k_code);
			++ktp;
		}

		if (kfunc == cmd_meta_prefix)
			meta_char = c;
		if (kfunc == cmd_ctlx_prefix)
			ctlx_char = c;
		if (kfunc == cmd_universal_argument)
			repeat_key = c;
		if (kfunc == cmd_abort_command)
			abort_char = c;
	}

	ktp = &keytab[0];
	found = FALSE;
	while (ktp->k_fp != NULL) {
		if (ktp->k_code == c) {
			found = TRUE;
			break;
		}
		++ktp;
	}

	if (found) {
		ktp->k_fp = kfunc;
	} else {
		if (ktp >= &keytab[NBINDS]) {
			msg_printf("Binding table FULL!");
			return FALSE;
		}

		ktp->k_code = c;
		ktp->k_fp = kfunc;
		++ktp;
		ktp->k_code = 0;
		ktp->k_fp = NULL;
	}
	return TRUE;
}

int cmd_unbind_key(int f, int n)
{
	int c;
	char outseq[80];

	msg_printf("Unbind key: ");

	c = getckey(FALSE);

	cmdstr(c, &outseq[0]);

	ostring(outseq);

	if (unbindchar(c) == FALSE) {
		msg_printf("(Key not bound)");
		return FALSE;
	}
	return TRUE;
}

int unbindchar(int c)
{
	struct key_tab *ktp;
	struct key_tab *sktp;
	int found;

	ktp = &keytab[0];
	found = FALSE;
	while (ktp->k_fp != NULL) {
		if (ktp->k_code == c) {
			found = TRUE;
			break;
		}
		++ktp;
	}

	if (!found)
		return FALSE;

	sktp = ktp;
	while (ktp->k_fp != NULL)
		++ktp;
	--ktp;

	sktp->k_code = ktp->k_code;
	sktp->k_fp = ktp->k_fp;

	ktp->k_code = 0;
	ktp->k_fp = NULL;
	return TRUE;
}

unsigned int getckey(int mflag)
{
	unsigned int c;
	char tok[NSTRING];

	if (executing_command_line) {
		macarg(tok);
		return stock(tok);
	}

	if (mflag)
		c = get1key();
	else
		c = getcmd();
	return c;
}

int startup(char *sfname)
{
	char *fname;

	if (*sfname != 0)
		fname = lookup_file(sfname, TRUE);
	else
		fname = lookup_file(pathname[0], TRUE);

	if (fname == NULL)
		return TRUE;

	return dofile(fname);
}

char *lookup_file(char *fname, int try_home)
{
	char *home;
	char *path;
	char *sp;
	int i;
	static char fspec[NSTRING];

	if (try_home) {
		home = getenv("HOME");
		if (home != NULL) {
			snprintf(fspec, sizeof(fspec), "%s/%s", home, fname);

			if (file_open_read(fspec) == FIOSUC) {
				file_close();
				return fspec;
			}

			snprintf(fspec, sizeof(fspec), "%s/lib/%s", home, fname);
			if (file_open_read(fspec) == FIOSUC) {
				file_close();
				return fspec;
			}
		}
	}

	if (file_open_read(fname) == FIOSUC) {
		file_close();
		return fname;
	}

	path = getenv("PATH");
	if (path != NULL)
		while (*path) {
			sp = fspec;
			while (*path && (*path != PATHCHR))
				*sp++ = *path++;

			if (sp != fspec)
				*sp++ = '/';
			*sp = 0;
			strcat(fspec, fname);

			if (file_open_read(fspec) == FIOSUC) {
				file_close();
				return fspec;
			}

			if (*path == PATHCHR)
				++path;
		}

	for (i = 2; i < ARRAY_SIZE(pathname); i++) {
		strcpy(fspec, pathname[i]);
		strcat(fspec, fname);

		if (file_open_read(fspec) == FIOSUC) {
			file_close();
			return fspec;
		}
	}

	return NULL;
}

static const char *spec_name(int code, char *buf)
{
	switch (code) {
	case 'A':
		return "up";
	case 'B':
		return "down";
	case 'C':
		return "right";
	case 'D':
		return "left";
	case 'H':
		return "home";
	case 'F':
		return "end";
	case '2':
		return "insert";
	case '3':
		return "delete";
	case '5':
		return "prior";
	case '6':
		return "next";
	case '~':
		return "paste";
	}
	if (code >= 'a' && code <= 'l') {
		sprintf(buf, "f%d", code - 'a' + 1);
		return buf;
	}
	return NULL;
}

static int spec_code(const char *name)
{
	static const char *const names[] = { "up", "down", "right", "left", "home", "end", "insert", "delete",
		"prior", "next", "pageup", "pagedown", "backtab", NULL };
	static const char codes[] = "ABCDHF2356560";
	int i;

	for (i = 0; names[i]; i++)
		if (strcasecmp(name, names[i]) == 0)
			return codes[i];
	if ((name[0] == 'f' || name[0] == 'F') && name[1] >= '1' && name[1] <= '9') {
		int k = atoi(name + 1);
		if (k >= 1 && k <= 12)
			return 'a' + k - 1;
	}
	return 0;
}

void cmdstr(int c, char *seq)
{
	char *p = seq;
	int ch = c & 0x1FFFFF;
	char nb[8];

	if (c & CTLX)
		p += sprintf(p, "C-x ");
	if (c & CTLC)
		p += sprintf(p, "C-c ");
	if (c & SPEC) {
		const char *nm = spec_name(ch, nb);
		if (c & CONTROL)
			p += sprintf(p, "C-");
		if (c & META)
			p += sprintf(p, "M-");
		if (c & SHIFT)
			p += sprintf(p, "S-");
		if (nm)
			p += sprintf(p, "<%s>", nm);
		else
			p += sprintf(p, "<internal-%c>", ch);
		*p = 0;
		return;
	}
	if (c & CONTROL) {
		if (ch == 'I' && (c & SHIFT)) {
			p += sprintf(p, "%s<backtab>", (c & META) ? "M-" : "");
		} else if (ch == 'I') {
			p += sprintf(p, "%sTAB", (c & META) ? "M-" : "");
		} else if (ch == 'M') {
			p += sprintf(p, "%sRET", (c & META) ? "M-" : "");
		} else if (ch == '[') {
			p += sprintf(p, "%sESC", (c & META) ? "M-" : "");
		} else if (ch == '@') {
			p += sprintf(p, "%sC-SPC", (c & META) ? "M-" : "");
		} else {
			int l = (ch >= 'A' && ch <= 'Z') ? ch + 0x20 : ch;
			p += sprintf(p, "%s", (c & META) ? "C-M-" : "C-");
			p += unicode_to_utf8(l, p);
		}
		*p = 0;
		return;
	}
	if (c & META)
		p += sprintf(p, "M-");
	if (ch == 0x7F)
		p += sprintf(p, "DEL");
	else if (ch == ' ')
		p += sprintf(p, "SPC");
	else {
		int l = ch;
		if ((c & (META | CTLX | CTLC)) && l >= 'A' && l <= 'Z')
			l += 0x20;
		p += unicode_to_utf8(l, p);
	}
	*p = 0;
}

int (*getbind(int c))(int, int) {
	struct key_tab *ktp;

	ktp = &keytab[0];
	while (ktp->k_fp != NULL) {
		if (ktp->k_code == c)
			return ktp->k_fp;
		++ktp;
	}

	return NULL;
}

char *getfname(fn_t func)
{
	struct name_bind *nptr;

	nptr = &names[0];
	while (nptr->n_func != NULL) {
		if (nptr->n_func == func)
			return nptr->n_name;
		++nptr;
	}
	return NULL;
}

int (*fncmatch(char *fname))(int, int) {
	struct name_bind *ffp;

	ffp = &names[0];
	while (ffp->n_func != NULL) {
		if (strcmp(fname, ffp->n_name) == 0)
			return ffp->n_func;
		++ffp;
	}
	ffp = &legacy_names[0];
	while (ffp->n_func != NULL) {
		if (strcmp(fname, ffp->n_name) == 0)
			return ffp->n_func;
		++ffp;
	}
	return NULL;
}

static unsigned int stock_legacy(char *keyname)
{
	unsigned int c = 0;

	if (*keyname == 'M' && *(keyname + 1) == '-') {
		c = META;
		keyname += 2;
	}
	if (*keyname == 'F' && *(keyname + 1) == 'N') {
		c |= SPEC;
		keyname += 2;
	}
	if (*keyname == '^' && *(keyname + 1) == 'X' && *(keyname + 2) != 0 && !(c & SPEC)) {
		c |= CTLX;
		keyname += 2;
	} else if (*keyname == '^' && *(keyname + 1) == 'C' && *(keyname + 2) != 0 && !(c & SPEC)) {
		c |= CTLC;
		keyname += 2;
	}
	if (*keyname == '^' && *(keyname + 1) != 0) {
		c |= CONTROL;
		++keyname;
	}
	if (*keyname < 32) {
		c |= CONTROL;
		*keyname += 'A';
	}
	if (*keyname >= 'a' && *keyname <= 'z' && !(c & SPEC))
		*keyname -= 32;
	c |= (unsigned char)*keyname;
	return c;
}

unsigned int stock(char *keyname)
{
	unsigned int c = 0;
	int ctrl = 0, ch;
	char *s = keyname;

	while (*s == ' ')
		s++;
	if (strstr(s, "FN") != NULL)
		return stock_legacy(keyname);
	if (strchr(s, '-') == NULL && strchr(s, '<') == NULL && strcmp(s, "SPC") && strcmp(s, "TAB")
	    && strcmp(s, "RET") && strcmp(s, "DEL") && strcmp(s, "ESC"))
		return stock_legacy(keyname);
	if (s[0] == '-' || (s[0] && s[1] == 0))
		return stock_legacy(keyname);
	if (strncmp(s, "C-x ", 4) == 0) {
		c |= CTLX;
		s += 4;
	} else if (strncmp(s, "C-c ", 4) == 0) {
		c |= CTLC;
		s += 4;
	}
	for (;;) {
		if (s[0] == 'C' && s[1] == '-' && s[2]) {
			ctrl = 1;
			s += 2;
		} else if (s[0] == 'M' && s[1] == '-' && s[2]) {
			c |= META;
			s += 2;
		} else if (s[0] == 'S' && s[1] == '-' && s[2]) {
			c |= SHIFT;
			s += 2;
		} else
			break;
	}
	if (*s == '<') {
		char name[32];
		int i = 0;
		s++;
		while (*s && *s != '>' && i < 31)
			name[i++] = *s++;
		name[i] = 0;
		if (strcasecmp(name, "backtab") == 0)
			return c | SHIFT | CONTROL | 'I';
		if (strcasecmp(name, "tab") == 0)
			return c | (ctrl ? CONTROL : 0) | CONTROL | 'I';
		ch = spec_code(name);
		if (!ch)
			return 0;
		return c | SPEC | (ctrl ? CONTROL : 0) | ch;
	}
	if (strcmp(s, "SPC") == 0)
		ch = ' ';
	else if (strcmp(s, "TAB") == 0)
		return c | CONTROL | 'I';
	else if (strcmp(s, "RET") == 0)
		return c | CONTROL | 'M';
	else if (strcmp(s, "DEL") == 0)
		return c | 0x7F;
	else if (strcmp(s, "ESC") == 0)
		return c | CONTROL | '[';
	else {
		unicode_t u;
		utf8_to_unicode(s, 0, strlen(s), &u);
		ch = u;
	}
	if (ctrl) {
		if (ch >= 'a' && ch <= 'z')
			ch -= 0x20;
		if (ch == ' ')
			ch = '@';
		if (ch == '/' || ch == '-')
			ch = '_';
		c |= CONTROL;
	}
	if ((c & (META | CTLX | CTLC)) && ch >= 'a' && ch <= 'z')
		ch -= 0x20;
	return c | ch;
}

char *transbind(char *skey)
{
	char *bindname;

	bindname = getfname(getbind(stock(skey)));
	if (bindname == NULL)
		bindname = "ERROR";

	return bindname;
}
