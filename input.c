#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "utf8.h"
#include "util.h"
#include "wrapper.h"

int complete_kind = CMPL_NONE;
char *prefill_text = NULL;

int ask_yesno(char *prompt)
{
	int c;
	char buf[NSTRING];

	for (;;) {
		snprintf(buf, sizeof(buf), "%s (y or n)? ", prompt);
		msg_puts(buf);
		c = tgetc();
		if (c == keycode_to_char(abort_char))
			return ABORT;
		if (c == 'y' || c == 'Y' || c == ' ')
			return TRUE;
		if (c == 'n' || c == 'N' || c == 0x7F)
			return FALSE;
	}
}

int ask_string(char *prompt, char *buf, int nbuf)
{
	int s = nextarg(prompt, buf, nbuf, char_to_keycode('\n'));
	complete_kind = CMPL_NONE;
	prefill_text = NULL;
	return s;
}

int ask_string_until(char *prompt, char *buf, int nbuf, int eolchar)
{
	int s = nextarg(prompt, buf, nbuf, eolchar);
	complete_kind = CMPL_NONE;
	prefill_text = NULL;
	return s;
}

int keycode_to_char(int c)
{
	if (c & CONTROL)
		c = c & ~(CONTROL | 0x40);
	if (c & SPEC)
		c = c & 255;
	return c;
}

int char_to_keycode(int c)
{
	if (c >= 0x00 && c <= 0x1F)
		c = CONTROL | (c + '@');
	return c;
}

fn_t getname(char *prompt)
{
	char buf[NSTRING];
	int s;

	if (executing_command_line) {
		if (macarg(buf) != TRUE)
			return NULL;
		return fncmatch(&buf[0]);
	}
	complete_kind = CMPL_COMMAND;
	s = ask_string(prompt, buf, sizeof(buf));
	if (s != TRUE)
		return NULL;
	return fncmatch(buf);
}

int tgetc(void)
{
	int c;

	if (keyboard_macro_mode == PLAY) {
		if (keyboard_macro_pos < keyboard_macro_end)
			return (int)*keyboard_macro_pos++;
		if (--keyboard_macro_repeat < 1) {
			keyboard_macro_mode = STOP;
			update();
		} else {
			keyboard_macro_pos = &keyboard_macro[0];
			return (int)*keyboard_macro_pos++;
		}
	}
	c = ttgetc();
	last_key = c;
	if (keyboard_macro_mode == RECORD) {
		*keyboard_macro_pos++ = c;
		keyboard_macro_end = keyboard_macro_pos;
		if (keyboard_macro_pos == &keyboard_macro[NKBDM - 1]) {
			keyboard_macro_mode = STOP;
			tcapbeep();
		}
	}
	return c;
}

int get1key(void)
{
	int c = tgetc();

	if (c >= 0x00 && c <= 0x1F)
		c = CONTROL | (c + '@');
	return c;
}

static int csi_key(int prefix)
{
	int p[4] = { 0, 0, 0, 0 };
	int np = 0, c, m, mods = 0, code;

	c = tgetc();
	if (c == '[') {
		c = tgetc();
		if (c >= 'A' && c <= 'E')
			return prefix | SPEC | ('a' + c - 'A');
		return 0;
	}
	if (c == '<' || c == '?' || c == '>' || c == '=') {
		do
			c = tgetc();
		while ((c >= '0' && c <= '9') || c == ';' || c == ':');
		return 0;
	}
	while ((c >= '0' && c <= '9') || c == ';' || c == ':') {
		if (c == ';' || c == ':') {
			if (np < 3)
				np++;
		} else
			p[np] = p[np] * 10 + (c - '0');
		c = tgetc();
	}
	m = p[1] > 0 ? p[1] - 1 : 0;
	if (m & 1)
		mods |= SHIFT;
	if (m & 2)
		mods |= META;
	if (m & 4)
		mods |= CONTROL;
	if (m & 8)
		mods |= META;
	switch (c) {
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'H':
	case 'F':
		code = c;
		break;
	case 'Z':
		return prefix | SHIFT | CONTROL | 'I';
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
		code = 'a' + (c - 'P');
		break;
	case '~':
		switch (p[0]) {
		case 1:
		case 7:
			code = 'H';
			break;
		case 2:
			code = '2';
			break;
		case 3:
			code = '3';
			break;
		case 4:
		case 8:
			code = 'F';
			break;
		case 5:
			code = '5';
			break;
		case 6:
			code = '6';
			break;
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			code = 'a' + (p[0] - 11);
			break;
		case 17:
		case 18:
		case 19:
		case 20:
		case 21:
			code = 'f' + (p[0] - 17);
			break;
		case 23:
		case 24:
			code = 'k' + (p[0] - 23);
			break;
		case 200:
			return SPEC | '~';
		case 201:
			return 0;
		default:
			return 0;
		}
		break;
	case 'u':
		c = p[0];
		if (c == 9)
			c = CONTROL | 'I';
		else if (c == 13)
			c = CONTROL | 'M';
		else if (c == 27)
			c = CONTROL | '[';
		else if (c == 127)
			c = 0x7F;
		else if (mods & CONTROL) {
			if (c >= 'a' && c <= 'z')
				c -= 0x20;
			if (c == '/' || c == '-')
				c = '_';
			if (c == ' ')
				c = '@';
			c |= CONTROL;
		}
		if ((mods & META) && c >= 'a' && c <= 'z')
			c -= 0x20;
		return prefix | (mods & META) | c;
	default:
		return 0;
	}
	return prefix | SPEC | mods | code;
}

static int ss3_key(int prefix)
{
	int c = tgetc();

	switch (c) {
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'H':
	case 'F':
		return prefix | SPEC | c;
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
		return prefix | SPEC | ('a' + c - 'P');
	case 'M':
		return prefix | CONTROL | 'M';
	}
	return 0;
}

static int finish_key(int c, int prefix)
{
	if (prefix & (META | CTLX | CTLC)) {
		if (c >= 'a' && c <= 'z')
			c -= 0x20;
	}
	return prefix | c;
}

static int decode_key(int c, int prefix)
{
	if (c == 128 + 27)
		return csi_key(prefix);
	if (c == meta_char) {
		c = get1key();
		if (c == 128 + 27)
			return csi_key(prefix | META);
		if (c == '[')
			return csi_key(prefix);
		if (c == 'O')
			return ss3_key(prefix);
		if (c == meta_char) {
			c = get1key();
			if (c == 128 + 27 || c == '[')
				return csi_key(prefix | META);
			if (c == 'O')
				return ss3_key(prefix | META);
			if (c == meta_char)
				return abort_char;
			ttungetc(keycode_to_char(c));
			return prefix | META | CONTROL | '[';
		}
		return finish_key(c, prefix | META);
	}
	if (!(prefix & (CTLX | CTLC))) {
		if (c == ctlx_char)
			return decode_key(get1key(), prefix | CTLX);
		if (c == ctlc_char)
			return decode_key(get1key(), prefix | CTLC);
	}
	return finish_key(c, prefix);
}

int getcmd(void)
{
	return decode_key(get1key(), 0);
}

int paste_read_char(void)
{
	int c = tgetc();

	if (c == 128 + 27 || c == 27) {
		char seq[16];
		int n = 0, d;
		if (c == 27) {
			d = tgetc();
			if (d != '[') {
				ttungetc(d);
				return 27;
			}
		}
		for (;;) {
			d = tgetc();
			if (n < (int)sizeof(seq) - 1)
				seq[n++] = d;
			if (!((d >= '0' && d <= '9') || d == ';'))
				break;
		}
		seq[n] = 0;
		if (strcmp(seq, "201~") == 0)
			return -1;
		return 0;
	}
	return c;
}

#define NKINDS 8
#define NHIST 64
static char *history[NKINDS][NHIST];
static int hist_count[NKINDS];

static void hist_add(int kind, const char *s)
{
	int i;

	if (kind < 0 || kind >= NKINDS || !*s)
		return;
	for (i = 0; i < hist_count[kind]; i++)
		if (strcmp(history[kind][i], s) == 0) {
			char *t = history[kind][i];
			memmove(&history[kind][1], &history[kind][0], i * sizeof(char *));
			history[kind][0] = t;
			return;
		}
	if (hist_count[kind] == NHIST) {
		free(history[kind][NHIST - 1]);
		hist_count[kind]--;
	}
	memmove(&history[kind][1], &history[kind][0], hist_count[kind] * sizeof(char *));
	history[kind][0] = strdup(s);
	hist_count[kind]++;
}

struct cands {
	char **v;
	int n;
	int cap;
	int icase;
};

static void cands_free(struct cands *c)
{
	int i;

	for (i = 0; i < c->n; i++)
		free(c->v[i]);
	free(c->v);
	c->v = NULL;
	c->n = c->cap = 0;
}

static void cands_add(struct cands *c, const char *s, int len)
{
	char *t;

	if (c->n == c->cap) {
		int nc = c->cap ? c->cap * 2 : 64;
		char **nv = realloc(c->v, nc * sizeof(char *));
		if (nv == NULL)
			return;
		c->v = nv;
		c->cap = nc;
	}
	t = malloc(len + 1);
	if (t == NULL)
		return;
	memcpy(t, s, len);
	t[len] = 0;
	c->v[c->n++] = t;
}

static int cmpstr(const void *a, const void *b)
{
	return strcmp(*(char *const *)a, *(char *const *)b);
}

struct dircache {
	char path[NFILEN];
	struct timespec mtime;
	dev_t dev;
	ino_t ino;
	char **names;
	int n;
	int valid;
};
static struct dircache dcache;

static void dircache_clear(void)
{
	int i;

	for (i = 0; i < dcache.n; i++)
		free(dcache.names[i]);
	free(dcache.names);
	dcache.names = NULL;
	dcache.n = 0;
	dcache.valid = FALSE;
}

static int dircache_load(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *de;
	int cap = 0;

	if (stat(path, &st) != 0)
		return FALSE;
	if (dcache.valid && strcmp(dcache.path, path) == 0 && st.st_mtim.tv_sec == dcache.mtime.tv_sec
	    && st.st_mtim.tv_nsec == dcache.mtime.tv_nsec && st.st_ino == dcache.ino && st.st_dev == dcache.dev)
		return TRUE;
	dircache_clear();
	d = opendir(path);
	if (d == NULL)
		return FALSE;
	while ((de = readdir(d)) != NULL) {
		int isdir = 0, len;
		char *nm;
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;
		if (de->d_type == DT_DIR)
			isdir = 1;
		else if (de->d_type == DT_LNK || de->d_type == DT_UNKNOWN) {
			char full[NFILEN * 2];
			struct stat s2;
			snprintf(full, sizeof(full), "%s/%s", path, de->d_name);
			if (stat(full, &s2) == 0 && S_ISDIR(s2.st_mode))
				isdir = 1;
		}
		len = strlen(de->d_name);
		nm = malloc(len + 2);
		if (nm == NULL)
			continue;
		memcpy(nm, de->d_name, len);
		if (isdir)
			nm[len++] = '/';
		nm[len] = 0;
		if (dcache.n == cap) {
			char **nn;
			cap = cap ? cap * 2 : 256;
			nn = realloc(dcache.names, cap * sizeof(char *));
			if (nn == NULL) {
				free(nm);
				break;
			}
			dcache.names = nn;
		}
		dcache.names[dcache.n++] = nm;
	}
	closedir(d);
	qsort(dcache.names, dcache.n, sizeof(char *), cmpstr);
	mystrscpy(dcache.path, path, sizeof(dcache.path));
	dcache.mtime = st.st_mtim;
	dcache.dev = st.st_dev;
	dcache.ino = st.st_ino;
	dcache.valid = TRUE;
	return TRUE;
}

static void expand_home(const char *in, char *out, int size)
{
	const char *home = getenv("HOME");

	if (in[0] == '~' && (in[1] == '/' || in[1] == 0) && home)
		snprintf(out, size, "%s%s", home, in + 1);
	else
		mystrscpy(out, in, size);
}

static int prefix_match(const char *s, const char *p, int plen, int icase)
{
	if (icase)
		return strncasecmp(s, p, plen) == 0;
	return strncmp(s, p, plen) == 0;
}

static int gather(int kind, const char *text, struct cands *c, int *basepos)
{
	int i, pass;

	cands_free(c);
	c->icase = FALSE;
	*basepos = 0;
	if (kind == CMPL_FILE) {
		char dir[NFILEN], edir[NFILEN];
		const char *slash = strrchr(text, '/');
		const char *base;
		int blen;
		if (slash) {
			int dl = slash - text + 1;
			if (dl >= NFILEN)
				return 0;
			memcpy(dir, text, dl);
			dir[dl] = 0;
			base = slash + 1;
		} else {
			strcpy(dir, "");
			base = text;
		}
		*basepos = base - text;
		blen = strlen(base);
		expand_home(dir[0] ? dir : "./", edir, sizeof(edir));
		if (!dircache_load(edir))
			return 0;
		for (pass = 0; pass < 2 && c->n == 0; pass++) {
			for (i = 0; i < dcache.n; i++) {
				const char *nm = dcache.names[i];
				if (nm[0] == '.' && base[0] != '.')
					continue;
				if (prefix_match(nm, base, blen, pass))
					cands_add(c, nm, strlen(nm));
			}
			c->icase = pass;
		}
		return c->n;
	}
	if (kind == CMPL_BUFFER) {
		struct buffer *bp;
		int tl = strlen(text);
		for (pass = 0; pass < 2 && c->n == 0; pass++) {
			for (bp = buffer_head; bp != NULL; bp = bp->b_bufp)
				if (!(bp->b_flag & BFINVS) && prefix_match(bp->b_bname, text, tl, pass))
					cands_add(c, bp->b_bname, strlen(bp->b_bname));
			c->icase = pass;
		}
		return c->n;
	}
	if (kind == CMPL_COMMAND) {
		int tl = strlen(text);
		struct name_bind *nb;
		for (nb = &names[0]; nb->n_func != NULL; nb++)
			if (strncmp(nb->n_name, text, tl) == 0)
				cands_add(c, nb->n_name, strlen(nb->n_name));
		if (c->n == 0 && tl > 0) {
			for (nb = &names[0]; nb->n_func != NULL; nb++)
				if (strstr(nb->n_name, text) != NULL)
					cands_add(c, nb->n_name, strlen(nb->n_name));
			c->icase = 2;
		}
		return c->n;
	}
	if (kind == CMPL_MODE) {
		int tl = strlen(text);
		for (i = 0; i < NUMMODES; i++)
			if (strncasecmp(modename[i], text, tl) == 0) {
				char low[32];
				int k;
				for (k = 0; modename[i][k] && k < 31; k++)
					low[k] = tolower((unsigned char)modename[i][k]);
				low[k] = 0;
				cands_add(c, low, k);
			}
		c->icase = 1;
		return c->n;
	}
	return 0;
}

static int common_prefix(struct cands *c)
{
	int n, i;

	if (c->n == 0)
		return 0;
	n = strlen(c->v[0]);
	for (i = 1; i < c->n; i++) {
		int k = 0;
		while (k < n && c->v[i][k] && (c->icase ? tolower((unsigned char)c->v[i][k]) == tolower((unsigned char)c->v[0][k]) : c->v[i][k] == c->v[0][k]))
			k++;
		n = k;
	}
	return n;
}

static int popup_rows_used = 0;

static void popup_draw(struct cands *c, int kind)
{
	int maxw = 0, i, cols, rows, avail, top, r, shown;
	char *nomatch[1] = { "[No match]" };
	struct cands empty = { nomatch, 1, 1, 0 };

	if (c->n == 0)
		c = &empty;

	avail = term.t_nrow / 2;
	if (avail > 12)
		avail = 12;
	if (avail < 1)
		return;
	for (i = 0; i < c->n; i++) {
		int w = strlen(c->v[i]);
		if (w > maxw)
			maxw = w;
	}
	maxw += 2;
	if (maxw > term.t_ncol)
		maxw = term.t_ncol;
	cols = term.t_ncol / maxw;
	if (cols < 1)
		cols = 1;
	rows = (c->n + cols - 1) / cols;
	if (rows > avail)
		rows = avail;
	if (rows < 1)
		rows = 1;
	if (popup_rows_used > rows) {
		for (r = term.t_nrow - popup_rows_used; r < term.t_nrow - rows; r++) {
			movecursor(r, 0);
			tcapeeol();
		}
	}
	top = term.t_nrow - rows;
	shown = rows * cols;
	if (shown < c->n)
		shown--;
	for (r = 0; r < rows; r++) {
		int col = 0, k;
		movecursor(top + r, 0);
		tcaprev(TRUE);
		for (k = 0; k < cols; k++) {
			int idx = k * rows + r, w;
			const char *s;
			char more[48];
			if (idx >= shown || idx >= c->n) {
				if (idx == shown && shown < c->n) {
					snprintf(more, sizeof(more), "... %d more", c->n - shown);
					s = more;
				} else
					break;
			} else
				s = c->v[idx];
			for (w = 0; s[w] && col < term.t_ncol - 1 && w < maxw - 1; w++, col++)
				ttputc((unsigned char)s[w]);
			while (col < (k + 1) * maxw && col < term.t_ncol - 1) {
				ttputc(' ');
				col++;
			}
		}
		while (col < term.t_ncol) {
			ttputc(' ');
			col++;
		}
		tcaprev(FALSE);
		shown_col = term.t_ncol;
	}
	popup_rows_used = rows;
	(void)kind;
}

static int utf8_prev(const char *s, int pos)
{
	if (pos <= 0)
		return 0;
	pos--;
	while (pos > 0 && (s[pos] & 0xC0) == 0x80)
		pos--;
	return pos;
}

static int utf8_next(const char *s, int pos, int len)
{
	if (pos >= len)
		return len;
	pos++;
	while (pos < len && (s[pos] & 0xC0) == 0x80)
		pos++;
	return pos;
}

static int disp_width(const char *s, int from, int to)
{
	int w = 0;

	while (from < to) {
		unsigned char c = s[from];
		if (c < 0x20 || c == 0x7F)
			w += 2;
		else if ((c & 0xC0) != 0x80)
			w++;
		from++;
	}
	return w;
}

static void render(const char *prompt, const char *ed, int len, int cur)
{
	int pw, avail, start, i, col;

	if (!display_commands)
		return;
	movecursor(term.t_nrow, 0);
	pw = 0;
	for (i = 0; prompt[i]; i++) {
		ttputc((unsigned char)prompt[i]);
		pw++;
	}
	avail = term.t_ncol - 1 - pw;
	if (avail < 4)
		avail = 4;
	start = 0;
	while (disp_width(ed, start, cur) > avail - 1)
		start = utf8_next(ed, start, len);
	col = pw;
	for (i = start; i < len; i++) {
		unsigned char c = ed[i];
		int w = (c < 0x20 || c == 0x7F) ? 2 : ((c & 0xC0) == 0x80 ? 0 : 1);
		if (col + w > term.t_ncol - 1)
			break;
		if (!display_input)
			continue;
		if (c < 0x20 || c == 0x7F) {
			ttputc('^');
			ttputc(c == 0x7F ? '?' : (c ^ 0x40));
		} else
			fputc(c, stdout);
		col += w;
	}
	shown_col = col;
	tcapeeol();
	message_present = TRUE;
	movecursor(term.t_nrow, pw + disp_width(ed, start, cur));
	ttflush();
}

static void ed_insert(char *ed, int *len, int *cur, int cap, const char *s, int n)
{
	if (*len + n > cap)
		n = cap - *len;
	if (n <= 0)
		return;
	memmove(ed + *cur + n, ed + *cur, *len - *cur);
	memcpy(ed + *cur, s, n);
	*len += n;
	*cur += n;
	ed[*len] = 0;
}

static void ed_delete(char *ed, int *len, int from, int to)
{
	if (to <= from)
		return;
	memmove(ed + from, ed + to, *len - to);
	*len -= to - from;
	ed[*len] = 0;
}

static int is_wordc(int c)
{
	return isalnum(c) || c == '_' || c >= 0x80;
}

int getstring(char *prompt, char *buf, int nbuf, int eolchar)
{
	int kind = complete_kind;
	char *pre = prefill_text;
	char ed[NFILEN + NSTRING];
	int cap, len = 0, cur = 0, c, quotef = FALSE, hidx = -1, popup = FALSE, use_cmd;
	int hkind;
	char saved[NFILEN + NSTRING];
	struct cands cs = { NULL, 0, 0, 0 };

	complete_kind = CMPL_NONE;
	prefill_text = NULL;
	cap = nbuf - 1;
	if (cap > (int)sizeof(ed) - 1)
		cap = sizeof(ed) - 1;
	if (cap < 0)
		cap = 0;
	ed[0] = 0;
	if (pre) {
		mystrscpy(ed, pre, cap + 1);
		len = cur = strlen(ed);
	}
	hkind = kind;
	saved[0] = 0;
	use_cmd = (eolchar != meta_char);
	popup_rows_used = 0;

	for (;;) {
		if (popup) {
			int bp;
			gather(kind, ed, &cs, &bp);
			popup_draw(&cs, kind);
		}
		render(prompt, ed, len, cur);
		c = use_cmd ? getcmd() : get1key();
		if (c == 0)
			continue;
		if (quotef) {
			char u[8];
			int n;
			quotef = FALSE;
			c = keycode_to_char(c);
			n = unicode_to_utf8(c, u);
			ed_insert(ed, &len, &cur, cap, u, n);
			continue;
		}
		if (c == (CONTROL | 'M'))
			c = CONTROL | 'J';
		if (c == eolchar) {
			if (kind == CMPL_COMMAND && len > 0 && fncmatch(ed) == NULL) {
				int bp;
				gather(kind, ed, &cs, &bp);
				if (cs.n == 1) {
					mystrscpy(ed, cs.v[0], cap + 1);
					len = cur = strlen(ed);
				} else if (cs.n > 1) {
					int k = common_prefix(&cs);
					if (cs.icase != 2 && k > len) {
						mystrscpy(ed, cs.v[0], k + 1 > cap + 1 ? cap + 1 : k + 1);
						len = cur = strlen(ed);
					}
					popup = TRUE;
					tcapbeep();
					continue;
				}
			}
			if (kind == CMPL_FILE && ed[0] == '~') {
				char tmp[NFILEN + NSTRING];
				expand_home(ed, tmp, sizeof(tmp));
				mystrscpy(ed, tmp, cap + 1);
				len = strlen(ed);
			}
			mystrscpy(buf, ed, nbuf);
			hist_add(hkind, ed);
			cands_free(&cs);
			if (popup_rows_used)
				display_invalidate();
			msg_printf("");
			ttflush();
			return buf[0] ? TRUE : FALSE;
		}
		if (c == abort_char || c == (META | CONTROL | '[')) {
			cands_free(&cs);
			if (popup_rows_used)
				display_invalidate();
			cmd_abort_command(FALSE, 0);
			ttflush();
			return ABORT;
		}
		if ((c == quote_char || c == (CONTROL | 'Q') || c == (CONTROL | 'V')) && use_cmd) {
			quotef = TRUE;
			continue;
		}
		if (!use_cmd && c == (CONTROL | 'Q')) {
			quotef = TRUE;
			continue;
		}
		switch (c) {
		case 0x7F:
		case CONTROL | 'H':
			if (cur > 0) {
				int p = utf8_prev(ed, cur);
				ed_delete(ed, &len, p, cur);
				cur = p;
			}
			continue;
		case CONTROL | 'D':
		case SPEC | '3':
			if (cur < len)
				ed_delete(ed, &len, cur, utf8_next(ed, cur, len));
			continue;
		case CONTROL | 'A':
		case SPEC | 'H':
			cur = 0;
			continue;
		case CONTROL | 'E':
		case SPEC | 'F':
			cur = len;
			continue;
		case CONTROL | 'B':
		case SPEC | 'D':
			cur = utf8_prev(ed, cur);
			continue;
		case CONTROL | 'F':
		case SPEC | 'C':
			cur = utf8_next(ed, cur, len);
			continue;
		case CONTROL | 'K':
			len = cur;
			ed[len] = 0;
			continue;
		case CONTROL | 'U':
			ed_delete(ed, &len, 0, cur);
			cur = 0;
			continue;
		case META | 'B':
		case SPEC | CONTROL | 'D':
			while (cur > 0 && !is_wordc((unsigned char)ed[cur - 1]))
				cur--;
			while (cur > 0 && is_wordc((unsigned char)ed[cur - 1]))
				cur--;
			continue;
		case META | 'F':
		case SPEC | CONTROL | 'C':
			while (cur < len && !is_wordc((unsigned char)ed[cur]))
				cur++;
			while (cur < len && is_wordc((unsigned char)ed[cur]))
				cur++;
			continue;
		case META | 0x7F:
		case CONTROL | 'W':
		case META | CONTROL | 'H':
			{
				int p = cur;
				if (kind == CMPL_FILE) {
					if (p > 0 && ed[p - 1] == '/')
						p--;
					while (p > 0 && ed[p - 1] != '/')
						p--;
				} else {
					while (p > 0 && !is_wordc((unsigned char)ed[p - 1]))
						p--;
					while (p > 0 && is_wordc((unsigned char)ed[p - 1]))
						p--;
				}
				ed_delete(ed, &len, p, cur);
				cur = p;
			}
			continue;
		case META | 'D':
			{
				int p = cur;
				while (p < len && !is_wordc((unsigned char)ed[p]))
					p++;
				while (p < len && is_wordc((unsigned char)ed[p]))
					p++;
				ed_delete(ed, &len, cur, p);
			}
			continue;
		case CONTROL | 'Y':
			if (kill_head) {
				struct kill *kp;
				for (kp = kill_head; kp != NULL; kp = kp->d_next) {
					int nb = kp->d_next == NULL ? kill_used : KBLOCK, k;
					for (k = 0; k < nb && kp->d_chunk[k] != '\n'; k++)
						ed_insert(ed, &len, &cur, cap, &kp->d_chunk[k], 1);
					if (k < nb)
						break;
				}
			}
			continue;
		case META | 'P':
		case SPEC | 'A':
			if (hkind >= 0 && hkind < NKINDS && hidx + 1 < hist_count[hkind]) {
				if (hidx < 0)
					mystrscpy(saved, ed, sizeof(saved));
				hidx++;
				mystrscpy(ed, history[hkind][hidx], cap + 1);
				len = cur = strlen(ed);
			} else
				tcapbeep();
			continue;
		case META | 'N':
		case SPEC | 'B':
			if (hidx > 0) {
				hidx--;
				mystrscpy(ed, history[hkind][hidx], cap + 1);
				len = cur = strlen(ed);
			} else if (hidx == 0) {
				hidx = -1;
				mystrscpy(ed, saved, cap + 1);
				len = cur = strlen(ed);
			} else
				tcapbeep();
			continue;
		case SPEC | '~':
			{
				int pc;
				while ((pc = paste_read_char()) >= 0) {
					char u[8];
					int n;
					if (pc == '\r' || pc == '\n' || pc == 0)
						continue;
					n = unicode_to_utf8(pc, u);
					ed_insert(ed, &len, &cur, cap, u, n);
				}
			}
			continue;
		}
		if ((c == (CONTROL | 'I') || (c == ' ' && kind == CMPL_COMMAND) || (c == '?' && kind == CMPL_COMMAND)) && kind != CMPL_NONE && kind != CMPL_SEARCH) {
			int bp, k, baselen, tl;
			cur = len;
			gather(kind, ed, &cs, &bp);
			if (cs.n == 0) {
				tcapbeep();
				popup = TRUE;
				continue;
			}
			baselen = len - bp;
			if (cs.icase == 2) {
				if (cs.n == 1) {
					mystrscpy(ed, cs.v[0], cap + 1);
					len = cur = strlen(ed);
					popup = FALSE;
				} else
					popup = TRUE;
				continue;
			}
			k = cs.n == 1 ? (int)strlen(cs.v[0]) : common_prefix(&cs);
			if (cs.n == 1 || k > baselen || (cs.icase && k == baselen && memcmp(ed + bp, cs.v[0], k) != 0)) {
				tl = bp + k;
				if (tl > cap)
					tl = cap;
				if (tl > bp)
					memcpy(ed + bp, cs.v[0], tl - bp);
				len = cur = tl;
				ed[len] = 0;
				if (cs.n == 1 && !(kind == CMPL_FILE && ed[len - 1] == '/'))
					popup = FALSE;
			} else
				popup = TRUE;
			continue;
		}
		if (c & (CONTROL | META | SPEC | CTLX | CTLC)) {
			if (c == (CONTROL | 'I'))
				c = '\t';
			else if (c == (CONTROL | 'J') && !use_cmd)
				c = '\n';
			else {
				tcapbeep();
				continue;
			}
		}
		{
			char u[8];
			int n = unicode_to_utf8(c, u);
			ed_insert(ed, &len, &cur, cap, u, n);
		}
	}
}

void outstring(char *s)
{
	if (display_input)
		while (*s)
			ttputc(*s++);
}

void ostring(char *s)
{
	if (display_commands)
		while (*s)
			ttputc(*s++);
}
