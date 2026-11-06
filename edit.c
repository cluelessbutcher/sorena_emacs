#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "util.h"

enum {
	LANG_TEXT = 0,
	LANG_C,
	LANG_CLIKE,
	LANG_PY,
	LANG_HASH,
	LANG_YAML,
	LANG_MAKE,
	LANG_LISP,
	LANG_DASH,
	LANG_TEX,
	LANG_HTML,
	LANG_MD
};

struct pos {
	long ln;
	int off;
};

static int has_ext(const char *base, const char *const *list)
{
	const char *dot = strrchr(base, '.');
	int i;

	if (dot == NULL)
		return FALSE;
	dot++;
	for (i = 0; list[i]; i++)
		if (strcasecmp(dot, list[i]) == 0)
			return TRUE;
	return FALSE;
}

int detect_language(const char *fname)
{
	static const char *const c_ext[] = { "c", "h", "cc", "cpp", "cxx", "hpp", "hh", "hxx", "c++", "h++", "ino", "m", "mm", "cu", "cuh", NULL };
	static const char *const clike_ext[] = { "java", "js", "mjs", "cjs", "ts", "tsx", "jsx", "cs", "go", "rs", "swift", "kt", "kts",
		"scala", "php", "dart", "css", "scss", "less", "json", "jsonc", "groovy", "gradle", "zig", "sol", "proto", "v",
		"glsl", "hlsl", "vert", "frag", "d", "vala", "hx", NULL };
	static const char *const py_ext[] = { "py", "pyw", "pyi", "nim", "coffee", "gd", NULL };
	static const char *const yaml_ext[] = { "yaml", "yml", NULL };
	static const char *const hash_ext[] = { "sh", "bash", "zsh", "fish", "ksh", "rb", "pl", "pm", "r", "toml", "conf", "cfg",
		"cmake", "tcl", "ps1", "ex", "exs", "jl", "awk", "sed", "properties", "gitignore", "dockerignore", "env", "nix", "cr", NULL };
	static const char *const make_ext[] = { "mk", "mak", NULL };
	static const char *const lisp_ext[] = { "el", "lisp", "lsp", "cl", "clj", "cljs", "cljc", "edn", "scm", "ss", "rkt", "fnl", "asm", "s", "ini", NULL };
	static const char *const dash_ext[] = { "sql", "lua", "hs", "lhs", "elm", "ada", "adb", "ads", "vhd", "vhdl", NULL };
	static const char *const tex_ext[] = { "tex", "sty", "cls", "bib", "erl", "hrl", "m4", "mat", NULL };
	static const char *const html_ext[] = { "html", "htm", "xml", "xhtml", "svg", "vue", "svelte", "xsl", "plist", NULL };
	static const char *const md_ext[] = { "md", "markdown", "txt", "rst", "org", "text", NULL };
	const char *base = strrchr(fname, '/');

	base = base ? base + 1 : fname;
	if (strcasecmp(base, "Makefile") == 0 || strcasecmp(base, "GNUmakefile") == 0 || has_ext(base, make_ext))
		return LANG_MAKE;
	if (strcmp(base, "Dockerfile") == 0 || strcmp(base, "CMakeLists.txt") == 0 || strcmp(base, ".bashrc") == 0
	    || strcmp(base, ".zshrc") == 0 || strcmp(base, ".profile") == 0 || strcmp(base, ".bash_profile") == 0)
		return LANG_HASH;
	if (strcmp(base, ".emacsrc") == 0 || strcmp(base, ".emacs") == 0)
		return LANG_LISP;
	if (has_ext(base, c_ext))
		return LANG_C;
	if (has_ext(base, clike_ext))
		return LANG_CLIKE;
	if (has_ext(base, py_ext))
		return LANG_PY;
	if (has_ext(base, yaml_ext))
		return LANG_YAML;
	if (has_ext(base, hash_ext))
		return LANG_HASH;
	if (has_ext(base, lisp_ext))
		return LANG_LISP;
	if (has_ext(base, dash_ext))
		return LANG_DASH;
	if (has_ext(base, tex_ext))
		return LANG_TEX;
	if (has_ext(base, html_ext))
		return LANG_HTML;
	if (has_ext(base, md_ext))
		return LANG_MD;
	return LANG_TEXT;
}

static const char *comment_start(void)
{
	switch (curbp->b_lang) {
	case LANG_C:
	case LANG_CLIKE:
		return "//";
	case LANG_LISP:
		return ";;";
	case LANG_DASH:
		return "--";
	case LANG_TEX:
		return "%";
	case LANG_HTML:
	case LANG_MD:
		return "<!--";
	default:
		return "#";
	}
}

static const char *comment_end(void)
{
	if (curbp->b_lang == LANG_HTML || curbp->b_lang == LANG_MD)
		return "-->";
	return "";
}

static int colon_indents(void)
{
	return curbp->b_lang == LANG_PY || curbp->b_lang == LANG_YAML || curbp->b_lang == LANG_MAKE;
}

static int is_c_like(void)
{
	return curbp->b_lang == LANG_C || curbp->b_lang == LANG_CLIKE;
}

int indent_unit(void)
{
	if (curbp->b_indent > 0)
		return curbp->b_indent;
	return tabmask + 1;
}

static int is_blank_char(int c)
{
	return c == ' ' || c == '\t';
}

static int is_word_char(int c)
{
	return isalnum(c) || c == '_' || c >= 0x80;
}

static int closer_of(int c)
{
	switch (c) {
	case '(':
		return ')';
	case '[':
		return ']';
	case '{':
		return '}';
	case '"':
	case '\'':
	case '`':
		return c;
	}
	return 0;
}

static int opener_of(int c)
{
	switch (c) {
	case ')':
		return '(';
	case ']':
		return '[';
	case '}':
		return '{';
	}
	return 0;
}

int line_indent_bytes(struct line *lp)
{
	int i = 0, len = line_length(lp);

	while (i < len && is_blank_char(lgetc(lp, i)))
		i++;
	return i;
}

int line_indent_col(struct line *lp)
{
	int i, col = 0, len = line_length(lp);

	for (i = 0; i < len; i++) {
		int c = lgetc(lp, i);
		if (c == '\t')
			col = (col | tabmask) + 1;
		else if (c == ' ')
			col++;
		else
			break;
	}
	return col;
}

static int line_is_blank(struct line *lp)
{
	return line_indent_bytes(lp) == line_length(lp);
}

static int last_nonblank(struct line *lp)
{
	int i = line_length(lp);

	while (i > 0 && is_blank_char(lgetc(lp, i - 1)))
		i--;
	return i > 0 ? lgetc(lp, i - 1) : 0;
}

static int insert_indent(int col)
{
	if (col <= 0)
		return TRUE;
	if (curbp->b_indent == 0) {
		int w = tabmask + 1;
		if (col / w && insert_char(col / w, '\t') != TRUE)
			return FALSE;
		if (col % w && insert_char(col % w, ' ') != TRUE)
			return FALSE;
		return TRUE;
	}
	return insert_char(col, ' ');
}

static struct pos pos_of(struct line *lp, int off)
{
	struct pos p;

	p.ln = line_number(lp);
	p.off = off;
	return p;
}

static void pos_goto(struct pos p)
{
	struct line *lp = line_at(p.ln);

	curwp->w_dotp = lp;
	curwp->w_doto = p.off < 0 ? 0 : (p.off > line_length(lp) ? line_length(lp) : p.off);
}

static void pos_mark(struct pos p)
{
	struct line *lp = line_at(p.ln);

	curwp->w_markp = lp;
	curwp->w_marko = p.off < 0 ? 0 : (p.off > line_length(lp) ? line_length(lp) : p.off);
}

static int indent_string(int col, char *buf, int size)
{
	int n = 0;

	if (col < 0)
		col = 0;
	if (curbp->b_indent == 0) {
		int w = tabmask + 1, i;
		for (i = 0; i < col / w && n < size - 1; i++)
			buf[n++] = '\t';
		for (i = 0; i < col % w && n < size - 1; i++)
			buf[n++] = ' ';
	} else {
		while (n < col && n < size - 1)
			buf[n++] = ' ';
	}
	buf[n] = 0;
	return n;
}

static int set_indent_here(int col)
{
	struct line *lp = curwp->w_dotp;
	int oldib, off, newib, el;
	char exp[1024];

	if (lp == curbp->b_linep)
		return TRUE;
	oldib = line_indent_bytes(lp);
	off = curwp->w_doto;
	el = indent_string(col, exp, sizeof(exp));
	if (el == oldib && memcmp(lp->l_text, exp, el) == 0)
		return TRUE;
	curwp->w_doto = 0;
	if (oldib && delete_bytes(oldib, FALSE) != TRUE)
		return FALSE;
	if (el && insert_bytes(exp, el) != TRUE)
		return FALSE;
	newib = line_indent_bytes(curwp->w_dotp);
	if (off <= oldib)
		curwp->w_doto = newib;
	else
		curwp->w_doto = off - oldib + newib;
	return TRUE;
}

static int find_open(struct line *lp, int off, int closer, struct line **olp, int *ooff)
{
	int open = opener_of(closer);
	int depth = 0;
	long lines = 0;
	struct line *hdr = curbp->b_linep;

	if (!open)
		return FALSE;
	for (;;) {
		while (off > 0) {
			int c = lgetc(lp, --off);
			if (c == closer)
				depth++;
			else if (c == open) {
				if (depth == 0) {
					*olp = lp;
					*ooff = off;
					return TRUE;
				}
				depth--;
			}
		}
		lp = line_prev(lp);
		if (lp == hdr || ++lines > 20000)
			return FALSE;
		off = line_length(lp);
	}
}

static int find_close(struct line *lp, int off, int open, struct line **clp, int *coff)
{
	int closer = closer_of(open);
	int depth = 0;
	long lines = 0;
	struct line *hdr = curbp->b_linep;

	if (!closer || closer == open)
		return FALSE;
	off++;
	for (;;) {
		while (off < line_length(lp)) {
			int c = lgetc(lp, off);
			if (c == open)
				depth++;
			else if (c == closer) {
				if (depth == 0) {
					*clp = lp;
					*coff = off;
					return TRUE;
				}
				depth--;
			}
			off++;
		}
		lp = line_next(lp);
		if (lp == hdr || ++lines > 20000)
			return FALSE;
		off = 0;
	}
}

int find_matching_bracket(struct line *lp, int off, struct line **mlp, int *moff)
{
	int c;

	if (lp == curbp->b_linep || off >= line_length(lp))
		return FALSE;
	c = lgetc(lp, off);
	if (c == '(' || c == '[' || c == '{')
		return find_close(lp, off, c, mlp, moff);
	if (c == ')' || c == ']' || c == '}')
		return find_open(lp, off, c, mlp, moff);
	return FALSE;
}

static int compute_indent(struct line *lp)
{
	struct line *p = line_prev(lp);
	struct line *hdr = curbp->b_linep;
	int ind, last, first, ib;

	ib = (lp == hdr) ? 0 : line_indent_bytes(lp);
	first = (lp != hdr && ib < line_length(lp)) ? lgetc(lp, ib) : 0;
	if (first == ')' || first == ']' || first == '}') {
		struct line *olp;
		int ooff;
		if (find_open(lp, ib, first, &olp, &ooff))
			return line_indent_col(olp);
	}
	while (p != hdr && line_is_blank(p))
		p = line_prev(p);
	if (p == hdr)
		return 0;
	ind = line_indent_col(p);
	last = last_nonblank(p);
	if (last == '{' || last == '[' || last == '(')
		ind += indent_unit();
	else if (last == ':' && colon_indents())
		ind += indent_unit();
	return ind;
}

int region_bounds(struct pos *start, struct pos *end)
{
	struct pos d, m;

	if (curwp->w_markp == NULL)
		return FALSE;
	d = pos_of(curwp->w_dotp, curwp->w_doto);
	m = pos_of(curwp->w_markp, curwp->w_marko);
	if (d.ln < m.ln || (d.ln == m.ln && d.off <= m.off)) {
		*start = d;
		*end = m;
	} else {
		*start = m;
		*end = d;
	}
	return TRUE;
}

int delete_active_region(void)
{
	struct region r;

	if (!region_active || curwp->w_markp == NULL)
		return FALSE;
	region_active = FALSE;
	if (getregion(&r) != TRUE || r.r_size == 0)
		return FALSE;
	curwp->w_dotp = r.r_linep;
	curwp->w_doto = r.r_offset;
	delete_bytes(r.r_size, FALSE);
	curwp->w_markp = NULL;
	return TRUE;
}

static void line_range(long *l1, long *l2)
{
	struct pos s, e;

	if (region_active && region_bounds(&s, &e)) {
		*l1 = s.ln;
		*l2 = e.ln;
		if (e.off == 0 && e.ln > s.ln)
			(*l2)--;
	} else {
		*l1 = *l2 = line_number(curwp->w_dotp);
	}
}

static long buffer_lines(void)
{
	return buffer_line_number(curbp, curbp->b_linep);
}

static int view_check(void)
{
	if (curbp->b_mode & MDVIEW) {
		readonly_error();
		return FALSE;
	}
	return TRUE;
}

static int typing_fast(void)
{
	return keyboard_macro_mode != PLAY && typahead() > 0;
}

int self_insert(int c, int n)
{
	struct line *lp;
	int off, len, next, prev, status;
	int pairing, indenting;

	if ((curbp->b_mode & MDVIEW) && (curbp->b_flag & BFINVS) && c == 'q' && n == 1) {
		struct buffer *bp = last_buffer;
		if (bp == NULL || bp == curbp) {
			for (bp = buffer_head; bp != NULL; bp = bp->b_bufp)
				if (bp != curbp && !(bp->b_flag & BFINVS))
					break;
		}
		if (bp == NULL)
			bp = find_buffer("*scratch*", TRUE, 0);
		return bp ? swbuffer(bp) : FALSE;
	}
	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	pairing = (curbp->b_mode & MDPAIR) && n == 1 && !(curbp->b_mode & MDOVER) && !typing_fast();
	indenting = (curbp->b_mode & MDINDENT) && n == 1 && !typing_fast();

	if (region_active && curwp->w_markp != NULL) {
		int cl = closer_of(c);
		if (pairing && cl) {
			struct pos s, e;
			if (region_bounds(&s, &e) && (s.ln != e.ln || s.off != e.off)) {
				pos_goto(e);
				insert_char(1, cl);
				pos_goto(s);
				insert_char(1, c);
				if (s.ln == e.ln)
					e.off++;
				e.off++;
				pos_goto(e);
				region_active = FALSE;
				return TRUE;
			}
		}
		delete_active_region();
	}

	if (curwp->w_bufp->b_mode & MDOVER && curwp->w_doto < curwp->w_dotp->l_used &&
	    (lgetc(curwp->w_dotp, curwp->w_doto) != '\t' || (curwp->w_doto) % 8 == 7))
		delete_characters(1, FALSE);

	lp = curwp->w_dotp;
	off = curwp->w_doto;
	len = lp == curbp->b_linep ? 0 : line_length(lp);
	next = (off >= 0 && off < len) ? (unsigned char)lp->l_text[off] : '\n';
	prev = off > 0 ? lgetc(lp, off - 1) : '\n';

	if (pairing && (c == ')' || c == ']' || c == '}' || c == '"' || c == '\'' || c == '`') && next == c) {
		return cmd_forward_character(FALSE, 1);
	}

	if (indenting && (c == '}' || c == ']' || c == ')') && lp != curbp->b_linep && line_indent_bytes(lp) == off && off == len) {
		struct line *olp;
		int ooff;
		if (find_open(lp, off, c, &olp, &ooff))
			set_indent_here(line_indent_col(olp));
	} else if (c == '#' && (curbp->b_mode & MDCMOD) && curbp->b_lang == LANG_C && lp != curbp->b_linep && line_indent_bytes(lp) == off) {
		curwp->w_doto = 0;
		if (off)
			delete_bytes(off, FALSE);
	}

	if (pairing && closer_of(c)) {
		static const char before_ok[] = " \t\n;:.,=}])>";
		int cl = closer_of(c);
		int ok = strchr(before_ok, next) != NULL;
		if (c == '"' || c == '\'' || c == '`') {
			if (prev == '\\')
				ok = FALSE;
			if (c == '\'' && is_word_char(prev))
				ok = FALSE;
			if (c == '`' && curbp->b_lang == LANG_C)
				ok = FALSE;
			if (c == '\'' && (curbp->b_lang == LANG_MD || curbp->b_lang == LANG_TEXT || curbp->b_lang == LANG_LISP))
				ok = FALSE;
			if (is_word_char(prev) && c != '\'')
				ok = ok && (prev == 'f' || prev == 'r' || prev == 'b' || prev == 'u' || prev == 'L' || prev == 'R');
		}
		if (ok) {
			if ((status = insert_char(1, c)) != TRUE)
				return status;
			if ((status = insert_char(1, cl)) != TRUE)
				return status;
			return cmd_backward_character(FALSE, 1);
		}
	}

	status = insert_char(n, c);
	return status;
}

int cmd_smart_newline(int f, int n)
{
	struct line *lp;
	char *t;
	int off, len, b, a, ib, base, newcol, extra, pair, prevch, nextch;
	const char *cont = NULL;

	if (!view_check())
		return FALSE;
	if (n < 0)
		return FALSE;
	if (region_active)
		delete_active_region();
	if ((curwp->w_bufp->b_mode & MDWRAP) && fill_column > 0 && getccol(FALSE) > fill_column)
		execute(HOOK_WRAP, FALSE, 1);
	if (n != 1 || !(curbp->b_mode & MDINDENT) || typing_fast()) {
		while (n--)
			if (insert_newline() != TRUE)
				return FALSE;
		return TRUE;
	}
	lp = curwp->w_dotp;
	if (lp == curbp->b_linep)
		return insert_newline();
	t = lp->l_text;
	off = curwp->w_doto;
	len = line_length(lp);
	b = off;
	while (b > 0 && is_blank_char((unsigned char)t[b - 1]))
		b--;
	a = off;
	while (a < len && is_blank_char((unsigned char)t[a]))
		a++;
	ib = line_indent_bytes(lp);
	if (b == 0) {
		base = getccol(FALSE);
		if (a == len && off < len) {
			curwp->w_doto = len;
			base = getccol(FALSE);
		}
	} else
		base = line_indent_col(lp);
	prevch = b > 0 ? (unsigned char)t[b - 1] : 0;
	nextch = a < len ? (unsigned char)t[a] : 0;
	extra = (prevch == '{' || prevch == '[' || prevch == '(') || (prevch == ':' && colon_indents());
	pair = extra && nextch && nextch == closer_of(prevch);

	if (is_c_like() && b > 0 && !extra) {
		char *s = t + ib;
		int sl = b - ib;
		if (sl >= 2 && s[0] == '/' && s[1] == '*') {
			int i, closed = FALSE;
			for (i = 2; i + 1 < sl; i++)
				if (s[i] == '*' && s[i + 1] == '/')
					closed = TRUE;
			if (!closed)
				cont = " * ";
		} else if (sl >= 1 && s[0] == '*' && !(sl >= 2 && s[1] == '/')) {
			if (!(sl >= 2 && s[sl - 1] == '/' && s[sl - 2] == '*'))
				cont = "* ";
		}
	}

	curwp->w_doto = b;
	if (a > b && delete_bytes(a - b, FALSE) != TRUE)
		return FALSE;
	if (insert_newline() != TRUE)
		return FALSE;
	newcol = base + (extra ? indent_unit() : 0);
	if (!extra && curbp->b_lang == LANG_PY && b > 0) {
		static const char *const kw[] = { "return", "pass", "break", "continue", "raise", NULL };
		int k;
		for (k = 0; kw[k]; k++) {
			int kl = strlen(kw[k]);
			if (b - ib >= kl && memcmp(t + ib, kw[k], kl) == 0 && (b - ib == kl || !is_word_char((unsigned char)t[ib + kl]))) {
				newcol -= indent_unit();
				if (newcol < 0)
					newcol = 0;
				break;
			}
		}
	}
	if (!extra && (nextch == ')' || nextch == ']' || nextch == '}')) {
		struct line *olp;
		int ooff;
		if (find_open(curwp->w_dotp, 0, nextch, &olp, &ooff))
			newcol = line_indent_col(olp);
	}
	if (insert_indent(newcol) != TRUE)
		return FALSE;
	if (cont && !extra)
		insert_string((char *)cont);
	if (pair) {
		if (insert_newline() != TRUE)
			return FALSE;
		insert_indent(base);
		curwp->w_dotp = line_prev(curwp->w_dotp);
		curwp->w_doto = line_length(curwp->w_dotp);
	}
	return TRUE;
}

int cmd_newline_and_indent(int f, int n)
{
	return cmd_smart_newline(f, n);
}

static int insert_unit_at_dot(void)
{
	if (curbp->b_indent == 0)
		return insert_char(1, '\t');
	return insert_char(curbp->b_indent - (getccol(FALSE) % curbp->b_indent), ' ');
}

static int shift_lines(long l1, long l2, int delta, int stops)
{
	struct pos d = pos_of(curwp->w_dotp, curwp->w_doto);
	struct pos m = { -1, 0 };
	long ln;
	int mark = curwp->w_markp != NULL;

	if (mark)
		m = pos_of(curwp->w_markp, curwp->w_marko);
	for (ln = l1; ln <= l2; ln++) {
		struct line *lp = line_at(ln);
		int oldib, col, newib;
		if (lp == curbp->b_linep)
			break;
		if (line_is_blank(lp) && delta > 0)
			continue;
		oldib = line_indent_bytes(lp);
		col = line_indent_col(lp) + delta;
		if (delta < 0 && stops) {
			int u = -delta;
			int cur = line_indent_col(lp);
			col = cur - (cur % u ? cur % u : u);
		}
		if (col < 0)
			col = 0;
		curwp->w_dotp = lp;
		curwp->w_doto = 0;
		set_indent_here(col);
		newib = line_indent_bytes(curwp->w_dotp);
		if (d.ln == ln)
			d.off = d.off <= oldib ? (d.off == 0 && region_active ? 0 : newib) : d.off - oldib + newib;
		if (m.ln == ln)
			m.off = m.off <= oldib ? (m.off == 0 ? 0 : newib) : m.off - oldib + newib;
	}
	if (mark)
		pos_mark(m);
	pos_goto(d);
	return TRUE;
}

int cmd_indent_for_tab(int f, int n)
{
	int off, want;

	if (!view_check())
		return FALSE;
	if (f && n != 1) {
		if (n < 0)
			return FALSE;
		curbp->b_indent = n;
		if (n == 0)
			msg_printf("(Indenting with tabs)");
		else
			msg_printf("(Indenting with %d spaces)", n);
		update_modeline();
		return TRUE;
	}
	if (region_active && curwp->w_markp) {
		long l1, l2;
		line_range(&l1, &l2);
		shift_lines(l1, l2, indent_unit(), FALSE);
		thisflag |= CFKEEPR;
		return TRUE;
	}
	if (!(curbp->b_mode & MDINDENT) || typing_fast())
		return insert_unit_at_dot();
	off = curwp->w_doto;
	if (curwp->w_dotp != curbp->b_linep && off <= line_indent_bytes(curwp->w_dotp)) {
		want = compute_indent(curwp->w_dotp);
		if (line_indent_col(curwp->w_dotp) < want) {
			set_indent_here(want);
			curwp->w_doto = line_indent_bytes(curwp->w_dotp);
			return TRUE;
		}
		if (off < line_indent_bytes(curwp->w_dotp)) {
			curwp->w_doto = line_indent_bytes(curwp->w_dotp);
			return TRUE;
		}
	}
	return insert_unit_at_dot();
}

int cmd_outdent(int f, int n)
{
	long l1, l2;

	if (!view_check())
		return FALSE;
	line_range(&l1, &l2);
	shift_lines(l1, l2, -indent_unit(), TRUE);
	if (region_active)
		thisflag |= CFKEEPR;
	return TRUE;
}

int cmd_indent_rigidly(int f, int n)
{
	long l1, l2;
	int delta = f ? n : indent_unit();

	if (!view_check())
		return FALSE;
	line_range(&l1, &l2);
	shift_lines(l1, l2, delta, FALSE);
	thisflag |= CFKEEPR;
	return TRUE;
}

int cmd_indent_region(int f, int n)
{
	long l1, l2, ln;
	struct pos d;

	if (!view_check())
		return FALSE;
	line_range(&l1, &l2);
	d = pos_of(curwp->w_dotp, curwp->w_doto);
	for (ln = l1; ln <= l2; ln++) {
		struct line *lp = line_at(ln);
		if (lp == curbp->b_linep)
			break;
		curwp->w_dotp = lp;
		curwp->w_doto = 0;
		if (line_is_blank(lp)) {
			if (line_length(lp))
				line_truncate(lp, 0);
			continue;
		}
		set_indent_here(compute_indent(lp));
	}
	pos_goto(d);
	if (curwp->w_doto < line_indent_bytes(curwp->w_dotp))
		curwp->w_doto = line_indent_bytes(curwp->w_dotp);
	msg_printf("(Indented %ld line%s)", l2 - l1 + 1, l2 == l1 ? "" : "s");
	return TRUE;
}

int cmd_smart_backspace(int f, int n)
{
	struct line *lp = curwp->w_dotp;
	int off = curwp->w_doto;
	int len;

	if (!view_check())
		return FALSE;
	if (region_active && curwp->w_markp) {
		if (delete_active_region())
			return TRUE;
	}
	if (f || n != 1)
		return cmd_delete_previous_character(f, n);
	len = lp == curbp->b_linep ? 0 : line_length(lp);
	if (off > 0 && off < len && (curbp->b_mode & MDPAIR)) {
		int p = lgetc(lp, off - 1), q = lgetc(lp, off);
		if (closer_of(p) && closer_of(p) == q) {
			cmd_backward_character(FALSE, 1);
			return delete_bytes(2, FALSE);
		}
	}
	if (off > 0 && (curbp->b_mode & MDINDENT) && curbp->b_indent > 0) {
		int i, allsp = TRUE;
		for (i = 0; i < off; i++)
			if (lgetc(lp, i) != ' ') {
				allsp = FALSE;
				break;
			}
		if (allsp) {
			int u = curbp->b_indent;
			int del = off % u ? off % u : u;
			if (del > off)
				del = off;
			curwp->w_doto = off - del;
			return delete_bytes(del, FALSE);
		}
	}
	return cmd_delete_previous_character(FALSE, 1);
}

int cmd_smart_delete(int f, int n)
{
	if (!view_check())
		return FALSE;
	if (region_active && curwp->w_markp && delete_active_region())
		return TRUE;
	return cmd_delete_next_character(f, n);
}

static char *lines_text(long l1, long l2, int *outlen)
{
	long ln;
	int total = 0, pos = 0;
	char *buf;
	struct line *lp = line_at(l1);
	struct line *p = lp;

	for (ln = l1; ln <= l2 && p != curbp->b_linep; ln++, p = line_next(p))
		total += line_length(p) + 1;
	buf = malloc(total + 1);
	if (buf == NULL)
		return NULL;
	p = lp;
	for (ln = l1; ln <= l2 && p != curbp->b_linep; ln++, p = line_next(p)) {
		memcpy(buf + pos, p->l_text, line_length(p));
		pos += line_length(p);
		buf[pos++] = '\n';
	}
	buf[pos] = 0;
	*outlen = pos;
	return buf;
}

static int duplicate(int up)
{
	long l1, l2, cnt;
	int len, mark = curwp->w_markp != NULL;
	char *text;
	struct pos d, m = { 0, 0 };

	if (!view_check())
		return FALSE;
	if (curwp->w_dotp == curbp->b_linep) {
		tcapbeep();
		return FALSE;
	}
	line_range(&l1, &l2);
	cnt = l2 - l1 + 1;
	d = pos_of(curwp->w_dotp, curwp->w_doto);
	if (mark)
		m = pos_of(curwp->w_markp, curwp->w_marko);
	text = lines_text(l1, l2, &len);
	if (text == NULL)
		return FALSE;
	curwp->w_dotp = line_at(l1);
	curwp->w_doto = 0;
	insert_bytes(text, len);
	free(text);
	if (!up) {
		d.ln += cnt;
		m.ln += cnt;
	}
	if (mark)
		pos_mark(m);
	pos_goto(d);
	thisflag |= CFKEEPR;
	return TRUE;
}

int cmd_duplicate_line(int f, int n)
{
	int s = TRUE;

	if (n < 1)
		n = 1;
	while (n-- && s == TRUE)
		s = duplicate(FALSE);
	return s;
}

int cmd_duplicate_line_up(int f, int n)
{
	return duplicate(TRUE);
}

static int move_lines(int down)
{
	long l1, l2, total;
	int len, mark = curwp->w_markp != NULL;
	struct pos d, m = { 0, 0 };
	char *text;
	struct line *lp;

	if (!view_check())
		return FALSE;
	if (curwp->w_dotp == curbp->b_linep)
		return FALSE;
	line_range(&l1, &l2);
	total = buffer_lines();
	if ((down && l2 + 1 >= total) || (!down && l1 == 0)) {
		tcapbeep();
		return FALSE;
	}
	d = pos_of(curwp->w_dotp, curwp->w_doto);
	if (mark)
		m = pos_of(curwp->w_markp, curwp->w_marko);
	if (down) {
		text = lines_text(l2 + 1, l2 + 1, &len);
		if (text == NULL)
			return FALSE;
		lp = line_at(l2 + 1);
		curwp->w_dotp = lp;
		curwp->w_doto = 0;
		delete_bytes(len, FALSE);
		curwp->w_dotp = line_at(l1);
		curwp->w_doto = 0;
		insert_bytes(text, len);
		d.ln++;
		m.ln++;
	} else {
		text = lines_text(l1 - 1, l1 - 1, &len);
		if (text == NULL)
			return FALSE;
		lp = line_at(l1 - 1);
		curwp->w_dotp = lp;
		curwp->w_doto = 0;
		delete_bytes(len, FALSE);
		curwp->w_dotp = line_at(l2);
		curwp->w_doto = 0;
		insert_text(text, len);
		d.ln--;
		m.ln--;
	}
	free(text);
	if (mark)
		pos_mark(m);
	pos_goto(d);
	thisflag |= CFKEEPR;
	return TRUE;
}

int cmd_move_line_up(int f, int n)
{
	int s = TRUE;

	if (n < 1)
		n = 1;
	while (n-- && s == TRUE)
		s = move_lines(FALSE);
	return s;
}

int cmd_move_line_down(int f, int n)
{
	int s = TRUE;

	if (n < 1)
		n = 1;
	while (n-- && s == TRUE)
		s = move_lines(TRUE);
	return s;
}

int cmd_transpose_lines(int f, int n)
{
	long ln;

	if (!view_check())
		return FALSE;
	region_active = FALSE;
	if (move_lines(FALSE) != TRUE)
		return FALSE;
	ln = line_number(curwp->w_dotp) + 2;
	curwp->w_dotp = line_at(ln);
	curwp->w_doto = 0;
	return TRUE;
}

int cmd_comment_toggle(int f, int n)
{
	long l1, l2, ln;
	const char *cs = comment_start(), *ce = comment_end();
	int csl = strlen(cs), cel = strlen(ce);
	int all = TRUE, any = FALSE, mincol = 1 << 30;
	struct pos d;

	if (!view_check())
		return FALSE;
	line_range(&l1, &l2);
	for (ln = l1; ln <= l2; ln++) {
		struct line *lp = line_at(ln);
		int ib;
		if (lp == curbp->b_linep)
			break;
		if (line_is_blank(lp))
			continue;
		any = TRUE;
		ib = line_indent_bytes(lp);
		if (line_length(lp) - ib < csl || memcmp(lp->l_text + ib, cs, csl) != 0)
			all = FALSE;
		if (line_indent_col(lp) < mincol)
			mincol = line_indent_col(lp);
	}
	if (!any) {
		if (curwp->w_dotp == curbp->b_linep)
			return FALSE;
		insert_string((char *)cs);
		insert_char(1, ' ');
		if (cel) {
			insert_char(1, ' ');
			insert_string((char *)ce);
			cmd_backward_character(FALSE, cel + 1);
		}
		return TRUE;
	}
	d = pos_of(curwp->w_dotp, curwp->w_doto);
	for (ln = l1; ln <= l2; ln++) {
		struct line *lp = line_at(ln);
		int ib, delta = 0;
		if (lp == curbp->b_linep)
			break;
		if (line_is_blank(lp))
			continue;
		ib = line_indent_bytes(lp);
		curwp->w_dotp = lp;
		if (all) {
			int k = csl;
			if (ib + k < line_length(lp) && lgetc(lp, ib + k) == ' ')
				k++;
			curwp->w_doto = ib;
			delete_bytes(k, FALSE);
			delta = -k;
			if (cel) {
				struct line *cl = curwp->w_dotp;
				int e = line_length(cl);
				while (e > 0 && is_blank_char(lgetc(cl, e - 1)))
					e--;
				if (e >= cel && memcmp(cl->l_text + e - cel, ce, cel) == 0) {
					int st = e - cel;
					if (st > 0 && lgetc(cl, st - 1) == ' ')
						st--;
					curwp->w_doto = st;
					delete_bytes(line_length(cl) - st, FALSE);
				}
			}
		} else {
			int i, col = 0;
			for (i = 0; i < line_length(lp) && col < mincol; i++) {
				int c = lgetc(lp, i);
				col = c == '\t' ? (col | tabmask) + 1 : col + 1;
			}
			curwp->w_doto = i;
			insert_string((char *)cs);
			insert_char(1, ' ');
			delta = csl + 1;
			if (cel) {
				curwp->w_doto = line_length(curwp->w_dotp);
				insert_char(1, ' ');
				insert_string((char *)ce);
			}
		}
		if (d.ln == ln) {
			d.off += delta;
			if (d.off < 0)
				d.off = 0;
		}
	}
	pos_goto(d);
	if (region_active)
		thisflag |= CFKEEPR;
	return TRUE;
}

int cmd_kill_whole_line(int f, int n)
{
	struct line *lp = curwp->w_dotp;
	long chunk;

	if (!view_check())
		return FALSE;
	if (lp == curbp->b_linep)
		return FALSE;
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	if (n < 1)
		n = 1;
	curwp->w_doto = 0;
	chunk = 0;
	while (n-- && lp != curbp->b_linep) {
		chunk += line_length(lp) + 1;
		lp = line_next(lp);
	}
	return delete_bytes(chunk, TRUE);
}

int cmd_back_to_indentation(int f, int n)
{
	if (curwp->w_dotp != curbp->b_linep)
		curwp->w_doto = line_indent_bytes(curwp->w_dotp);
	return TRUE;
}

int cmd_smart_home(int f, int n)
{
	int ib;

	if (curwp->w_dotp == curbp->b_linep)
		return TRUE;
	ib = line_indent_bytes(curwp->w_dotp);
	curwp->w_doto = (curwp->w_doto == ib) ? 0 : ib;
	return TRUE;
}

int cmd_delete_horizontal_space(int f, int n)
{
	struct line *lp = curwp->w_dotp;
	int a, b;

	if (!view_check())
		return FALSE;
	if (lp == curbp->b_linep)
		return TRUE;
	a = b = curwp->w_doto;
	while (a > 0 && is_blank_char(lgetc(lp, a - 1)))
		a--;
	while (b < line_length(lp) && is_blank_char(lgetc(lp, b)))
		b++;
	curwp->w_doto = a;
	if (b > a)
		return delete_bytes(b - a, FALSE);
	return TRUE;
}

int cmd_just_one_space(int f, int n)
{
	if (cmd_delete_horizontal_space(FALSE, 1) != TRUE)
		return FALSE;
	return insert_char(n < 1 ? 1 : n, ' ');
}

int cmd_delete_indentation(int f, int n)
{
	struct line *lp;
	int prev, next;

	if (!view_check())
		return FALSE;
	if (f)
		cmd_next_line(FALSE, 1);
	lp = curwp->w_dotp;
	if (lp == curbp->b_linep || line_prev(lp) == curbp->b_linep)
		return FALSE;
	curwp->w_doto = 0;
	delete_bytes(line_indent_bytes(lp), FALSE);
	cmd_backward_character(FALSE, 1);
	delete_bytes(1, FALSE);
	cmd_delete_horizontal_space(FALSE, 1);
	lp = curwp->w_dotp;
	prev = curwp->w_doto > 0 ? lgetc(lp, curwp->w_doto - 1) : 0;
	next = curwp->w_doto < line_length(lp) ? lgetc(lp, curwp->w_doto) : 0;
	if (prev && next && prev != '(' && next != ')' && prev != '[' && next != ']')
		insert_char(1, ' ');
	else if (prev && next)
		return TRUE;
	return TRUE;
}

int cmd_zap_to_char(int f, int n)
{
	int c;
	struct line *lp;
	int off;
	long count = 0;

	if (!view_check())
		return FALSE;
	msg_printf("Zap to char: ");
	c = tgetc();
	if (c == keycode_to_char(abort_char))
		return cmd_abort_command(FALSE, 0);
	if (n < 1)
		n = 1;
	lp = curwp->w_dotp;
	off = curwp->w_doto;
	while (lp != curbp->b_linep) {
		int ch;
		if (off >= line_length(lp)) {
			ch = '\n';
			lp = line_next(lp);
			off = 0;
		} else
			ch = lgetc(lp, off++);
		count++;
		if (ch == c && --n == 0)
			break;
	}
	if (n != 0) {
		msg_printf("Search failed: \"%c\"", c);
		return FALSE;
	}
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	msg_erase();
	return delete_bytes(count, TRUE);
}

int cmd_mark_whole_buffer(int f, int n)
{
	curwp->w_markp = curbp->b_linep;
	curwp->w_marko = 0;
	curwp->w_dotp = line_next(curbp->b_linep);
	curwp->w_doto = 0;
	region_active = TRUE;
	region_shift = FALSE;
	curwp->w_flag |= WFMOVE;
	thisflag |= CFKEEPR;
	msg_printf("(Mark set)");
	return TRUE;
}

int cmd_mark_paragraph(int f, int n)
{
	cmd_next_paragraph(FALSE, 1);
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;
	cmd_previous_paragraph(FALSE, 1);
	curwp->w_doto = 0;
	region_active = TRUE;
	region_shift = FALSE;
	thisflag |= CFKEEPR;
	return TRUE;
}

int cmd_toggle_read_only(int f, int n)
{
	curbp->b_mode ^= MDVIEW;
	update_modeline();
	msg_printf((curbp->b_mode & MDVIEW) ? "Read-only mode enabled in current buffer" : "Read-only mode disabled in current buffer");
	return TRUE;
}

int cmd_toggle_overwrite(int f, int n)
{
	curbp->b_mode ^= MDOVER;
	update_modeline();
	msg_printf((curbp->b_mode & MDOVER) ? "Overwrite mode enabled" : "Overwrite mode disabled");
	return TRUE;
}

int cmd_toggle_auto_pair(int f, int n)
{
	curbp->b_mode ^= MDPAIR;
	update_modeline();
	msg_printf((curbp->b_mode & MDPAIR) ? "Auto-pairing of brackets and quotes enabled" : "Auto-pairing of brackets and quotes disabled");
	return TRUE;
}

int cmd_toggle_auto_indent(int f, int n)
{
	curbp->b_mode ^= MDINDENT;
	update_modeline();
	msg_printf((curbp->b_mode & MDINDENT) ? "Auto-indent enabled" : "Auto-indent disabled");
	return TRUE;
}

int cmd_toggle_line_numbers(int f, int n)
{
	struct window *wp;

	show_line_numbers = !show_line_numbers;
	for (wp = window_head; wp != NULL; wp = wp->w_wndp)
		wp->w_flag |= WFHARD | WFMODE;
	msg_printf(show_line_numbers ? "Line numbers shown" : "Line numbers hidden");
	return TRUE;
}

int cmd_set_indentation(int f, int n)
{
	char buf[NSTRING];
	int s, v;

	if (f) {
		v = n;
	} else {
		s = ask_string("Indent with (t = tabs, or number of spaces): ", buf, sizeof(buf));
		if (s != TRUE)
			return s;
		if (buf[0] == 't' || buf[0] == 'T')
			v = 0;
		else
			v = atoi(buf);
	}
	if (v < 0 || v > 16) {
		msg_printf("Invalid indentation");
		return FALSE;
	}
	curbp->b_indent = v;
	update_modeline();
	if (v == 0)
		msg_printf("(Indenting with tabs)");
	else
		msg_printf("(Indenting with %d spaces)", v);
	return TRUE;
}

int cmd_delete_trailing_whitespace(int f, int n)
{
	struct line *lp;
	long count = 0;
	struct pos d;

	if (!view_check())
		return FALSE;
	d = pos_of(curwp->w_dotp, curwp->w_doto);
	for (lp = line_next(curbp->b_linep); lp != curbp->b_linep; lp = line_next(lp)) {
		int e = line_length(lp);
		while (e > 0 && is_blank_char(lgetc(lp, e - 1)))
			e--;
		if (e < line_length(lp)) {
			line_truncate(lp, e);
			count++;
		}
	}
	pos_goto(d);
	msg_printf("(Cleaned %ld line%s)", count, count == 1 ? "" : "s");
	return TRUE;
}

int cmd_open_line_below(int f, int n)
{
	if (!view_check())
		return FALSE;
	region_active = FALSE;
	if (curwp->w_dotp != curbp->b_linep)
		curwp->w_doto = line_length(curwp->w_dotp);
	return cmd_smart_newline(FALSE, 1);
}

int cmd_open_line_above(int f, int n)
{
	int col;

	if (!view_check())
		return FALSE;
	region_active = FALSE;
	col = curwp->w_dotp == curbp->b_linep ? 0 : line_indent_col(curwp->w_dotp);
	curwp->w_doto = 0;
	if (insert_newline() != TRUE)
		return FALSE;
	cmd_backward_character(FALSE, 1);
	return insert_indent(col);
}

int cmd_goto_bracket(int f, int n)
{
	struct line *lp = curwp->w_dotp, *mlp;
	int off = curwp->w_doto, moff;

	if (find_matching_bracket(lp, off, &mlp, &moff) ||
	    (off > 0 && find_matching_bracket(lp, off - 1, &mlp, &moff))) {
		curwp->w_dotp = mlp;
		curwp->w_doto = moff;
		curwp->w_flag |= WFMOVE;
		return TRUE;
	}
	tcapbeep();
	return FALSE;
}

int cmd_forward_sexp(int f, int n)
{
	struct line *mlp;
	int moff;

	if (n < 0)
		return cmd_backward_sexp(f, -n);
	while (n--) {
		struct line *lp;
		int off, c;
		while (curwp->w_dotp != curbp->b_linep) {
			lp = curwp->w_dotp;
			off = curwp->w_doto;
			c = off < line_length(lp) ? lgetc(lp, off) : '\n';
			if (c != ' ' && c != '\t' && c != '\n')
				break;
			cmd_forward_character(FALSE, 1);
		}
		lp = curwp->w_dotp;
		off = curwp->w_doto;
		if (lp == curbp->b_linep)
			return FALSE;
		c = lgetc(lp, off);
		if ((c == '(' || c == '[' || c == '{') && find_close(lp, off, c, &mlp, &moff)) {
			curwp->w_dotp = mlp;
			curwp->w_doto = moff + 1;
		} else if (c == '"' || c == '\'' || c == '`') {
			int i = off + 1;
			while (i < line_length(lp) && !(lgetc(lp, i) == c && lgetc(lp, i - 1) != '\\'))
				i++;
			curwp->w_doto = i < line_length(lp) ? i + 1 : i;
		} else if (is_word_char(c)) {
			while (curwp->w_doto < line_length(lp) && is_word_char(lgetc(lp, curwp->w_doto)))
				curwp->w_doto++;
		} else
			curwp->w_doto++;
	}
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

int cmd_backward_sexp(int f, int n)
{
	struct line *mlp;
	int moff;

	if (n < 0)
		return cmd_forward_sexp(f, -n);
	while (n--) {
		struct line *lp;
		int off, c;
		for (;;) {
			lp = curwp->w_dotp;
			off = curwp->w_doto;
			if (off == 0) {
				if (line_prev(lp) == curbp->b_linep)
					return FALSE;
				cmd_backward_character(FALSE, 1);
				continue;
			}
			c = lgetc(lp, off - 1);
			if (c != ' ' && c != '\t')
				break;
			curwp->w_doto--;
		}
		c = lgetc(lp, off - 1);
		if ((c == ')' || c == ']' || c == '}') && find_open(lp, off - 1, c, &mlp, &moff)) {
			curwp->w_dotp = mlp;
			curwp->w_doto = moff;
		} else if (c == '"' || c == '\'' || c == '`') {
			int i = off - 2;
			while (i >= 0 && !(lgetc(lp, i) == c && (i == 0 || lgetc(lp, i - 1) != '\\')))
				i--;
			curwp->w_doto = i >= 0 ? i : 0;
		} else if (is_word_char(c)) {
			while (curwp->w_doto > 0 && is_word_char(lgetc(lp, curwp->w_doto - 1)))
				curwp->w_doto--;
		} else
			curwp->w_doto--;
	}
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

static int is_defun_start(struct line *lp)
{
	int c, last;
	struct line *nx;

	if (line_length(lp) == 0)
		return FALSE;
	c = lgetc(lp, 0);
	if (curbp->b_lang == LANG_PY) {
		return (line_length(lp) > 4 && memcmp(lp->l_text, "def ", 4) == 0) ||
		    (line_length(lp) > 6 && memcmp(lp->l_text, "class ", 6) == 0) ||
		    (line_length(lp) > 10 && memcmp(lp->l_text, "async def ", 10) == 0);
	}
	if (c == '(' && curbp->b_lang == LANG_LISP)
		return TRUE;
	if (c == '{')
		return TRUE;
	if (is_blank_char(c) || c == '}' || c == '#' || c == '/' || c == '*')
		return FALSE;
	last = last_nonblank(lp);
	if (last == '{')
		return TRUE;
	if (last == ';' || last == ',')
		return FALSE;
	nx = line_next(lp);
	if (nx != curbp->b_linep && line_length(nx) > 0 && lgetc(nx, 0) == '{')
		return TRUE;
	return FALSE;
}

int cmd_beginning_of_defun(int f, int n)
{
	struct line *lp = curwp->w_dotp;

	if (n < 1)
		n = 1;
	while (n--) {
		if (lp == line_next(curbp->b_linep) && curwp->w_doto == 0)
			return FALSE;
		lp = line_prev(lp);
		while (lp != curbp->b_linep && !is_defun_start(lp))
			lp = line_prev(lp);
		if (lp == curbp->b_linep) {
			lp = line_next(curbp->b_linep);
			break;
		}
		if (line_length(lp) && lgetc(lp, 0) == '{' && line_prev(lp) != curbp->b_linep && is_defun_start(line_prev(lp)))
			lp = line_prev(lp);
	}
	curwp->w_dotp = lp;
	curwp->w_doto = 0;
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

int cmd_end_of_defun(int f, int n)
{
	struct line *lp = curwp->w_dotp;

	if (n < 1)
		n = 1;
	while (n--) {
		if (lp == curbp->b_linep)
			return FALSE;
		if (curbp->b_lang == LANG_PY) {
			lp = line_next(lp);
			while (lp != curbp->b_linep && !is_defun_start(lp))
				lp = line_next(lp);
			continue;
		}
		while (lp != curbp->b_linep && !(line_length(lp) && lgetc(lp, 0) == '}'))
			lp = line_next(lp);
		if (lp != curbp->b_linep)
			lp = line_next(lp);
	}
	curwp->w_dotp = lp;
	curwp->w_doto = 0;
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

static int word_bounds_before(struct line *lp, int off, int *s, int *e)
{
	while (off > 0 && !is_word_char(lgetc(lp, off - 1)))
		off--;
	if (off == 0)
		return FALSE;
	*e = off;
	while (off > 0 && is_word_char(lgetc(lp, off - 1)))
		off--;
	*s = off;
	return TRUE;
}

static int word_bounds_after(struct line *lp, int off, int *s, int *e)
{
	int len = line_length(lp);

	while (off < len && !is_word_char(lgetc(lp, off)))
		off++;
	if (off >= len)
		return FALSE;
	*s = off;
	while (off < len && is_word_char(lgetc(lp, off)))
		off++;
	*e = off;
	return TRUE;
}

int cmd_transpose_words(int f, int n)
{
	struct line *lp = curwp->w_dotp;
	int off = curwp->w_doto, s1, e1, s2, e2, len;
	char *buf;

	if (!view_check())
		return FALSE;
	if (lp == curbp->b_linep)
		return FALSE;
	len = line_length(lp);
	if (off < len && is_word_char(lgetc(lp, off))) {
		while (off < len && is_word_char(lgetc(lp, off)))
			off++;
		if (!word_bounds_after(lp, off, &s2, &e2)) {
			if (!word_bounds_before(lp, off, &s2, &e2) || !word_bounds_before(lp, s2, &s1, &e1)) {
				tcapbeep();
				return FALSE;
			}
		} else if (!word_bounds_before(lp, off, &s1, &e1)) {
			tcapbeep();
			return FALSE;
		}
	} else {
		if (!word_bounds_before(lp, off, &s1, &e1) || !word_bounds_after(lp, off, &s2, &e2)) {
			if (!word_bounds_before(lp, off, &s2, &e2) || !word_bounds_before(lp, s2, &s1, &e1)) {
				tcapbeep();
				return FALSE;
			}
		}
	}
	buf = malloc(e2 - s1 + 1);
	if (buf == NULL)
		return FALSE;
	memcpy(buf, lp->l_text + s2, e2 - s2);
	memcpy(buf + (e2 - s2), lp->l_text + e1, s2 - e1);
	memcpy(buf + (e2 - s2) + (s2 - e1), lp->l_text + s1, e1 - s1);
	curwp->w_doto = s1;
	delete_bytes(e2 - s1, FALSE);
	insert_bytes(buf, e2 - s1);
	free(buf);
	return TRUE;
}

int cmd_recenter(int f, int n)
{
	static int state = 0;

	if (f) {
		curwp->w_force = n >= 0 ? n + 1 : n;
		curwp->w_flag |= WFFORCE;
		return TRUE;
	}
	if ((lastflag & CFRECN) == 0)
		state = 0;
	else
		state = (state + 1) % 3;
	thisflag |= CFRECN;
	curwp->w_force = state == 0 ? 0 : (state == 1 ? 1 : -1);
	curwp->w_flag |= WFFORCE | WFHARD;
	if (state == 0)
		screen_garbage = TRUE;
	return TRUE;
}

int cmd_move_to_window_line(int f, int n)
{
	static int state = 0;
	struct line *lp = curwp->w_linep;
	int row, i;

	if ((lastflag & CFRECN) == 0)
		state = 0;
	else
		state = (state + 1) % 3;
	thisflag |= CFRECN;
	if (f)
		row = n >= 0 ? n : curwp->w_ntrows + n;
	else if (state == 0)
		row = curwp->w_ntrows / 2;
	else if (state == 1)
		row = 0;
	else
		row = curwp->w_ntrows - 1;
	for (i = 0; i < row && lp != curbp->b_linep; i++)
		lp = line_next(lp);
	curwp->w_dotp = lp;
	curwp->w_doto = 0;
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

#define KRING 64
static char *kring[KRING];
static int kring_len[KRING];
static int kring_n = 0;
static int kring_head = 0;
static int yank_index = 0;
static long yank_bytes = 0;

void kill_ring_push(void)
{
	struct kill *kp;
	long total = 0;
	char *buf;
	int slot;

	if (kill_head == NULL)
		return;
	for (kp = kill_head; kp != NULL; kp = kp->d_next)
		total += kp->d_next == NULL ? kill_used : KBLOCK;
	if (total <= 0)
		return;
	buf = malloc(total);
	if (buf == NULL)
		return;
	total = 0;
	for (kp = kill_head; kp != NULL; kp = kp->d_next) {
		int nb = kp->d_next == NULL ? kill_used : KBLOCK;
		memcpy(buf + total, kp->d_chunk, nb);
		total += nb;
	}
	slot = (kring_head + KRING - 1) % KRING;
	free(kring[slot]);
	kring[slot] = buf;
	kring_len[slot] = total;
	kring_head = slot;
	if (kring_n < KRING)
		kring_n++;
}

void yank_noted(long bytes)
{
	yank_index = 0;
	yank_bytes = bytes;
}

int cmd_yank_pop(int f, int n)
{
	int idx;

	if (!view_check())
		return FALSE;
	if ((lastflag & CFYANK) == 0) {
		msg_printf("Previous command was not a yank");
		tcapbeep();
		return FALSE;
	}
	if (kring_n == 0) {
		msg_printf("Kill ring is empty");
		return FALSE;
	}
	if (curwp->w_markp == NULL)
		return FALSE;
	{
		struct region r;
		if (getregion(&r) != TRUE)
			return FALSE;
		curwp->w_dotp = r.r_linep;
		curwp->w_doto = r.r_offset;
		delete_bytes(r.r_size, FALSE);
	}
	yank_index++;
	if (yank_index > kring_n)
		yank_index = 1;
	idx = (kring_head + yank_index - 1) % KRING;
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;
	insert_bytes(kring[idx], kring_len[idx]);
	thisflag |= CFYANK;
	return TRUE;
}

#define DABBREV_MAX 256
static char *dab_seen[DABBREV_MAX];
static int dab_nseen = 0;
static long dab_ln;
static int dab_off;
static int dab_dir;
static int dab_inserted;
static long dab_start_ln;
static int dab_start_off;
static char dab_prefix[NSTRING];

static void dab_reset(void)
{
	int i;

	for (i = 0; i < dab_nseen; i++)
		free(dab_seen[i]);
	dab_nseen = 0;
}

static int dab_was_seen(const char *w, int len)
{
	int i;

	for (i = 0; i < dab_nseen; i++)
		if ((int)strlen(dab_seen[i]) == len && memcmp(dab_seen[i], w, len) == 0)
			return TRUE;
	return FALSE;
}

static int dab_try(struct line *lp, int i, int plen, char *out, int outsz, int *endp)
{
	int e = i, len = line_length(lp);

	while (e < len && is_word_char(lgetc(lp, e)))
		e++;
	*endp = e;
	if (e - i > plen && e - i < outsz && memcmp(lp->l_text + i, dab_prefix, plen) == 0
	    && !dab_was_seen(lp->l_text + i, e - i)) {
		memcpy(out, lp->l_text + i, e - i);
		out[e - i] = 0;
		return TRUE;
	}
	return FALSE;
}

static int dab_scan(char *out, int outsz)
{
	int plen = strlen(dab_prefix);
	struct line *hdr = curbp->b_linep;
	struct line *lp = line_at(dab_ln);
	int e;

	for (;;) {
		if (lp == hdr) {
			if (dab_dir < 0) {
				dab_dir = 1;
				dab_ln = dab_start_ln;
				dab_off = dab_start_off;
				lp = line_at(dab_ln);
				continue;
			}
			return FALSE;
		}
		if (dab_dir < 0) {
			int i = dab_off - 1;
			while (i >= 0) {
				if (is_word_char(lgetc(lp, i)) && (i == 0 || !is_word_char(lgetc(lp, i - 1)))) {
					if (dab_try(lp, i, plen, out, outsz, &e)) {
						dab_off = i;
						return TRUE;
					}
				}
				i--;
			}
			lp = line_prev(lp);
			dab_ln--;
			dab_off = lp == hdr ? 0 : line_length(lp);
		} else {
			int i = dab_off, len = line_length(lp);
			while (i < len) {
				if (is_word_char(lgetc(lp, i)) && (i == 0 || !is_word_char(lgetc(lp, i - 1)))) {
					if (dab_try(lp, i, plen, out, outsz, &e)) {
						dab_off = e;
						return TRUE;
					}
					i = e;
				} else
					i++;
			}
			lp = line_next(lp);
			dab_ln++;
			dab_off = 0;
		}
	}
}

int cmd_dabbrev_expand(int f, int n)
{
	struct line *lp = curwp->w_dotp;
	int off = curwp->w_doto, s;
	char word[NSTRING];

	if (!view_check())
		return FALSE;
	if (lp == curbp->b_linep)
		return FALSE;
	if ((lastflag & CFDABB) == 0) {
		dab_reset();
		s = off;
		while (s > 0 && is_word_char(lgetc(lp, s - 1)))
			s--;
		if (s == off || off - s >= NSTRING - 1) {
			msg_printf("No dynamic expansion for this position");
			tcapbeep();
			return FALSE;
		}
		memcpy(dab_prefix, lp->l_text + s, off - s);
		dab_prefix[off - s] = 0;
		dab_seen[dab_nseen++] = strdup(dab_prefix);
		dab_ln = line_number(lp);
		dab_off = s;
		dab_dir = -1;
		dab_inserted = 0;
		dab_start_ln = dab_ln;
		dab_start_off = off;
	} else if (dab_inserted) {
		curwp->w_doto -= dab_inserted;
		delete_bytes(dab_inserted, FALSE);
		dab_inserted = 0;
	}
	thisflag |= CFDABB;
	if (!dab_scan(word, sizeof(word))) {
		msg_printf("No further dynamic expansion for \"%s\"", dab_prefix);
		tcapbeep();
		return FALSE;
	}
	if (dab_nseen < DABBREV_MAX)
		dab_seen[dab_nseen++] = strdup(word);
	dab_inserted = strlen(word) - strlen(dab_prefix);
	insert_bytes(word + strlen(dab_prefix), dab_inserted);
	return TRUE;
}

int cmd_goto_prefix(int f, int n)
{
	int c;

	msg_printf("M-g-");
	c = getcmd();
	msg_erase();
	switch (c) {
	case 'g':
	case 'G':
	case META | 'G':
	case 'l':
		return cmd_goto_line(f, n);
	case CONTROL | 'I':
	case 'c':
	case 'C':
		return cmd_move_to_column(f, n);
	}
	if (c == abort_char)
		return cmd_abort_command(FALSE, 0);
	tcapbeep();
	msg_printf("M-g: key not bound");
	return FALSE;
}

int cmd_move_to_column(int f, int n)
{
	char buf[NSTRING];
	int s;

	if (!f) {
		if ((s = ask_string("Move to column: ", buf, sizeof(buf))) != TRUE)
			return s;
		n = atoi(buf);
	}
	if (n < 0)
		n = 0;
	setccol(n);
	return TRUE;
}

int cmd_help_prefix(int f, int n)
{
	int c;

	msg_printf("C-h- (k = key, b = bindings, a = apropos, m = modes, ? = help)");
	c = getcmd();
	msg_erase();
	switch (c) {
	case 'k':
	case 'K':
	case 'c':
	case 'C':
		return cmd_describe_key(f, n);
	case 'b':
	case 'B':
		return cmd_describe_bindings(f, n);
	case 'a':
	case 'A':
	case 'f':
	case 'F':
	case 'x':
	case 'X':
		return cmd_apropos(f, n);
	case 'm':
	case 'M':
		return cmd_describe_mode(f, n);
	case '?':
	case 'h':
	case 'H':
	case 't':
	case 'T':
	case CONTROL | 'H':
		return cmd_help(f, n);
	}
	if (c == abort_char)
		return cmd_abort_command(FALSE, 0);
	tcapbeep();
	return FALSE;
}

int cmd_describe_mode(int f, int n)
{
	char buf[NSTRING];
	int i;

	buf[0] = 0;
	for (i = 0; i < NUMMODES; i++)
		if (curbp->b_mode & (1 << i)) {
			if (buf[0])
				mystrscat(buf, " ", sizeof(buf));
			mystrscat(buf, mode2name[i], sizeof(buf));
		}
	if (curbp->b_indent)
		msg_printf("Modes: %s | indent: %d spaces | fill column: %d", buf[0] ? buf : "none", curbp->b_indent, fill_column);
	else
		msg_printf("Modes: %s | indent: tabs | fill column: %d", buf[0] ? buf : "none", fill_column);
	return TRUE;
}

int cmd_kmacro_start(int f, int n)
{
	if (keyboard_macro_mode == RECORD)
		return TRUE;
	return cmd_begin_macro(f, n);
}

int cmd_kmacro_end_or_call(int f, int n)
{
	if (keyboard_macro_mode == RECORD)
		return cmd_end_macro(f, n);
	return cmd_execute_macro(f, n);
}

int cmd_save_some_buffers(int f, int n)
{
	struct buffer *bp, *oldbp = curbp;
	int saved = 0, s;
	char prompt[NSTRING + 32];

	for (bp = buffer_head; bp != NULL; bp = bp->b_bufp) {
		if ((bp->b_flag & BFCHG) == 0 || (bp->b_flag & BFINVS) || bp->b_fname[0] == 0 || (bp->b_mode & MDVIEW))
			continue;
		if (!f) {
			snprintf(prompt, sizeof(prompt), "Save file %s", bp->b_fname);
			s = ask_yesno(prompt);
			if (s == ABORT) {
				curbp = oldbp;
				return ABORT;
			}
			if (s != TRUE)
				continue;
		}
		curbp = bp;
		if (cmd_save_file(FALSE, 1) == TRUE)
			saved++;
		curbp = oldbp;
	}
	curbp = oldbp;
	update_modeline();
	if (saved == 0)
		msg_printf("(No files need saving)");
	else
		msg_printf("(Saved %d file%s)", saved, saved == 1 ? "" : "s");
	return TRUE;
}

int cmd_save_buffers_kill_emacs(int f, int n)
{
	if (f)
		return cmd_exit_emacs(f, n);
	if (any_changed_buffers()) {
		int s = cmd_save_some_buffers(FALSE, 1);
		if (s == ABORT)
			return s;
	}
	return cmd_exit_emacs(FALSE, n);
}

int cmd_revert_buffer(int f, int n)
{
	long ln;
	int off, s;
	char fname[NFILEN];

	if (curbp->b_fname[0] == 0) {
		msg_printf("Buffer is not visiting a file");
		return FALSE;
	}
	if (!f) {
		char prompt[NFILEN + 32];
		snprintf(prompt, sizeof(prompt), "Revert buffer from file %s", curbp->b_fname);
		if ((s = ask_yesno(prompt)) != TRUE)
			return s;
	}
	ln = line_number(curwp->w_dotp);
	off = curwp->w_doto;
	curbp->b_flag &= ~BFCHG;
	mystrscpy(fname, curbp->b_fname, sizeof(fname));
	s = readin(fname, FALSE);
	curwp->w_dotp = line_at(ln);
	curwp->w_doto = off > line_length(curwp->w_dotp) ? line_length(curwp->w_dotp) : off;
	curwp->w_flag |= WFHARD | WFMODE | WFFORCE;
	return s;
}

void detect_indentation(struct buffer *bp)
{
	struct line *lp;
	int tabs = 0, spaces = 0, n = 0;
	int hist[9] = { 0 };
	int prev = 0;

	bp->b_lang = detect_language(bp->b_fname);
	bp->b_indent = default_indent;
	if (bp->b_lang == LANG_MAKE) {
		bp->b_indent = 0;
		return;
	}
	if (bp->b_lang == LANG_C && default_indent == 4)
		bp->b_indent = 4;
	for (lp = line_next(bp->b_linep); lp != bp->b_linep && n < 5000; lp = line_next(lp), n++) {
		int ib, i, sp = 0;
		if (line_length(lp) == 0)
			continue;
		ib = line_indent_bytes(lp);
		if (ib == line_length(lp))
			continue;
		if (ib == 0) {
			prev = 0;
			continue;
		}
		if (lgetc(lp, 0) == '\t') {
			tabs++;
			prev = -1;
			continue;
		}
		for (i = 0; i < ib && lgetc(lp, i) == ' '; i++)
			sp++;
		if (sp == 1 && lgetc(lp, ib) == '*')
			continue;
		spaces++;
		if (prev >= 0 && sp > prev) {
			int d = sp - prev;
			if (d <= 8)
				hist[d]++;
		}
		prev = sp;
	}
	if (tabs == 0 && spaces == 0)
		return;
	if (tabs > spaces) {
		bp->b_indent = 0;
		return;
	}
	{
		int best = 0, d;
		for (d = 2; d <= 8; d++)
			if (hist[d] > hist[best] || (best && hist[d] == hist[best] && d < best))
				best = d;
		if (best && hist[best] > 0)
			bp->b_indent = best;
	}
}

void setup_buffer_modes(struct buffer *bp)
{
	detect_indentation(bp);
	if (bp->b_lang == LANG_C || bp->b_lang == LANG_CLIKE)
		bp->b_mode |= MDCMOD;
	if (bp->b_lang == LANG_MD)
		bp->b_mode &= ~MDCMOD;
}
