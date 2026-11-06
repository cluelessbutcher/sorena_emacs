#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "version.h"
#include "wrapper.h"
#include "utf8.h"
#include "util.h"

#include <signal.h>
#include <sys/ioctl.h>

volatile sig_atomic_t chg_width, chg_height;

static int reframe(struct window *wp);
static void update_cursor_position(void);
static void paint_window(struct window *wp, bool check);
static void modeline(struct window *wp);

static unsigned char shown_modeline[MAXCOL];
static int shown_modeline_row = -1;

void display_open(void)
{
	tcapopen();
	tcapkopen();
	tcaprev(FALSE);
}

void display_close(void)
{
	msg_erase();
	movecursor(term.t_nrow, 0);
	ttflush();
	tcapclose();
	tcapkclose();
	if (write(1, "\r", 1) < 0)
		return;
}

static void ttputs(const char *s)
{
	fputs(s, stdout);
}

struct paint {
	int col;
	int offset;
	int out;
	int width;
	bool overflow;
};

struct hlinfo {
	int region;
	long rs_ln;
	int rs_off;
	long re_ln;
	int re_off;
	struct line *b1lp;
	int b1off;
	struct line *b2lp;
	int b2off;
};

static struct hlinfo hl;
static struct hlinfo shown_hl;
static int shown_cursor_row = -1;
static struct window *shown_cursor_wp = NULL;
static long shown_cursor_ln = -1;

#define ATTR_SEL_ON "\033[7m"
#define ATTR_SEL_OFF "\033[27m"
#define ATTR_BR_ON "\033[1;4m"
#define ATTR_BR_OFF "\033[22;24m"
#define ATTR_DIM_ON "\033[2m"
#define ATTR_DIM_OFF "\033[22m"

static void paint_raw(struct paint *p, unicode_t c)
{
	if (p->col >= p->offset + p->width) {
		p->overflow = true;
		p->col++;
		return;
	}
	if (p->col >= p->offset) {
		ttputc(c);
		p->out++;
	}
	p->col++;
}

static void paint_char(struct paint *p, unicode_t c)
{
	if (c == '\t') {
		do {
			paint_raw(p, ' ');
		} while ((p->col & tabmask) != 0);
		return;
	}

	if (c < 0x20) {
		paint_raw(p, '^');
		paint_raw(p, c ^ 0x40);
		return;
	}

	if (c == 0x7f) {
		paint_raw(p, '^');
		paint_raw(p, '?');
		return;
	}

	if (c >= 0x80 && c <= 0xA0) {
		static const char hex[] = "0123456789abcdef";
		paint_raw(p, '\\');
		paint_raw(p, hex[c >> 4]);
		paint_raw(p, hex[c & 15]);
		return;
	}

	paint_raw(p, c);
}

static void paint_bytes(struct paint *p, char *text, int from, int to)
{
	while (from < to) {
		unicode_t c;

		from += utf8_to_unicode(text, from, to, &c);
		paint_char(p, c);
	}
}

static bool word_ok(char *text, int from, int to)
{
	char word[128];
	int len = to - from;

	if (len >= sizeof(word))
		return true;

	memcpy(word, text + from, len);
	word[len] = 0;
	return spellcheck(word);
}

#define SPELLSTART "\033[1m"
#define SPELLSTOP "\033[22m"

static void paint_word(struct paint *p, char *text, int from, int to, bool check)
{
	bool bad = check && !word_ok(text, from, to);

	if (bad)
		ttputs(SPELLSTART);
	paint_bytes(p, text, from, to);
	if (bad)
		ttputs(SPELLSTOP);
}

static bool is_letter(unsigned char c)
{
	return c >= 0x80 || isalpha(c);
}

static bool is_notaword(unsigned char c)
{
	return c == '_' || (c >= '0' && c <= '9');
}

static bool is_token(unsigned char c)
{
	return is_letter(c) || is_notaword(c);
}

static int gutter_width(struct window *wp, long topln)
{
	long last;
	int digits = 1;

	if (!show_line_numbers || (wp->w_bufp->b_flag & BFINVS))
		return 0;
	last = topln + wp->w_ntrows;
	while (last >= 10) {
		last /= 10;
		digits++;
	}
	if (digits < 3)
		digits = 3;
	if (digits + 2 > term.t_ncol / 2)
		return 0;
	return digits + 1;
}

static void paint_gutter(int row, long lineno, int gw, bool current)
{
	char num[32];
	int i, n;

	if (gw <= 0)
		return;
	movecursor(row, 0);
	if (lineno < 0) {
		for (i = 0; i < gw; i++)
			ttputc(' ');
	} else {
		n = snprintf(num, sizeof(num), "%*ld ", gw - 1, lineno + 1);
		if (!current)
			ttputs(ATTR_DIM_ON);
		for (i = 0; i < n && i < gw; i++)
			ttputc(num[i]);
		if (!current)
			ttputs(ATTR_DIM_OFF);
	}
	shown_col = gw;
}

static void paint_line(int row, struct line *lp, int offset, bool check, int gw, long lineno,
		       bool current, int hs, int he, int b1, int b2)
{
	struct paint p = { .offset = offset, .width = term.t_ncol - gw };
	char *text = lp ? lp->l_text : NULL;
	int len = lp ? line_length(lp) : 0;
	int i = 0;

	if (gw > 0)
		paint_gutter(row, lp ? lineno : -1, gw, current);
	else
		movecursor(row, 0);

	if (check && hs < 0 && b1 < 0 && b2 < 0) {
		while (i < len) {
			if (!is_token(text[i])) {
				paint_bytes(&p, text, i, i + 1);
				i++;
				continue;
			}

			int start = i;
			bool word = true;

			while (i < len) {
				unsigned char c = text[i];

				if (is_letter(c)) {
					i++;
				} else if (is_notaword(c)) {
					word = false;
					i++;
				} else if (c == '\'' && word && i + 1 < len &&
					   isalpha((unsigned char)text[i + 1])) {
					i++;
				} else
					break;
			}

			paint_word(&p, text, start, i, check && word);
		}
	} else if (hs < 0 && b1 < 0 && b2 < 0) {
		while (i < len) {
			unsigned char c = text[i];
			if (c >= 0x20 && c < 0x7F) {
				if (p.col >= p.offset + p.width) {
					p.overflow = true;
					break;
				}
				if (p.col >= p.offset) {
					putc_unlocked(c, stdout);
					p.out++;
				}
				p.col++;
				i++;
			} else {
				unicode_t uc;
				i += utf8_to_unicode(text, i, len, &uc);
				paint_char(&p, uc);
			}
		}
	} else {
		bool sel = false, br = false;
		while (i < len) {
			unicode_t uc;
			int nb;
			bool wsel = (hs >= 0 && i >= hs && i < he);
			bool wbr = (i == b1 || i == b2);
			if (wsel != sel) {
				ttputs(wsel ? ATTR_SEL_ON : ATTR_SEL_OFF);
				sel = wsel;
			}
			if (wbr != br) {
				ttputs(wbr ? ATTR_BR_ON : ATTR_BR_OFF);
				br = wbr;
			}
			nb = utf8_to_unicode(text, i, len, &uc);
			paint_char(&p, uc);
			i += nb;
		}
		if (br)
			ttputs(ATTR_BR_OFF);
		if (hs >= 0 && he > len && p.col >= p.offset && p.col < p.offset + p.width) {
			if (!sel)
				ttputs(ATTR_SEL_ON);
			sel = true;
			ttputc(' ');
			p.out++;
		}
		if (sel)
			ttputs(ATTR_SEL_OFF);
	}

	shown_col = gw + p.out;
	tcapeeol();

	if (p.overflow) {
		movecursor(row, term.t_ncol - 1);
		ttputc('$');
		shown_col = term.t_ncol;
	}
	if (offset) {
		movecursor(row, gw);
		ttputc('$');
		shown_col = gw + 1;
	}
}

static void line_hl(struct window *wp, struct line *lp, long ln, int *hs, int *he, int *b1, int *b2)
{
	*hs = *he = *b1 = *b2 = -1;
	if (wp != curwp || lp == NULL)
		return;
	if (hl.region && ln >= hl.rs_ln && ln <= hl.re_ln) {
		*hs = ln == hl.rs_ln ? hl.rs_off : 0;
		*he = ln == hl.re_ln ? hl.re_off : line_length(lp) + 1;
		if (*hs >= *he) {
			*hs = *he = -1;
		}
	}
	if (hl.b1lp == lp)
		*b1 = hl.b1off;
	if (hl.b2lp == lp)
		*b2 = hl.b2off;
}

static void paint_window(struct window *wp, bool check)
{
	struct line *end = wp->w_bufp->b_linep;
	struct line *lp = wp->w_linep;
	long ln = show_line_numbers || (wp == curwp && hl.region) ? buffer_line_number(wp->w_bufp, lp) : 0;
	int gw = gutter_width(wp, ln);
	long dotln = -1;

	if (gw && wp == curwp)
		dotln = buffer_line_number(wp->w_bufp, wp->w_dotp);
	for (int i = 0; i < wp->w_ntrows; i++) {
		int row = wp->w_toprow + i;
		bool eob = lp == end;
		int hs, he, b1, b2;

		line_hl(wp, eob ? NULL : lp, ln, &hs, &he, &b1, &b2);
		paint_line(row, eob ? NULL : lp,
			   wp == curwp && row == cursor_row ? left_column : 0,
			   check, gw, ln, wp == curwp ? ln == dotln : false, hs, he, b1, b2);
		if (!eob) {
			lp = line_next(lp);
			ln++;
		}
	}
	if (wp == curwp) {
		shown_cursor_row = cursor_row;
		shown_cursor_wp = wp;
		shown_cursor_ln = dotln;
	}
	modeline(wp);
}

int cmd_update_screen(int f, int n)
{
	update_now();
	return TRUE;
}

void update(void)
{
	if (keyboard_macro_mode == PLAY)
		return;
	update_now();
}

void display_invalidate(void)
{
	struct window *wp;

	for (wp = window_head; wp != NULL; wp = wp->w_wndp)
		wp->w_flag |= WFHARD | WFMODE;
	shown_modeline_row = -1;
}

static int window_gutter(struct window *wp)
{
	if (!show_line_numbers)
		return 0;
	return gutter_width(wp, buffer_line_number(wp->w_bufp, wp->w_linep));
}

static void update_window(struct window *wp, int oldbound)
{
	bool check = (wp->w_bufp->b_mode & MDSPELL) != 0;
	int bound = wp == curwp ? left_column : 0;
	int oldb = wp == curwp ? oldbound : 0;

	if (wp == curwp && (wp->w_flag & ~WFMODE) == WFEDIT
	    && !bound && !oldb) {
		long ln = (show_line_numbers || hl.region) ? buffer_line_number(wp->w_bufp, wp->w_dotp) : 0;
		long topln = show_line_numbers ? buffer_line_number(wp->w_bufp, wp->w_linep) : 0;
		int gw = gutter_width(wp, topln);
		int hs, he, b1, b2;
		line_hl(wp, wp->w_dotp, ln, &hs, &he, &b1, &b2);
		paint_line(cursor_row, wp->w_dotp, 0, check, gw, ln, true, hs, he, b1, b2);
		shown_cursor_row = cursor_row;
		shown_cursor_wp = wp;
		shown_cursor_ln = ln;
		modeline(wp);
	} else if (!(wp->w_flag & ~(WFMOVE | WFMODE)) && !bound && !oldb) {
		if (wp == curwp && show_line_numbers) {
			long topln = buffer_line_number(wp->w_bufp, wp->w_linep);
			int gw = gutter_width(wp, topln);
			long ln = buffer_line_number(wp->w_bufp, wp->w_dotp);
			if (gw && (shown_cursor_wp != wp || shown_cursor_row != cursor_row || shown_cursor_ln != ln)) {
				if (shown_cursor_wp == wp && shown_cursor_row >= wp->w_toprow
				    && shown_cursor_row < wp->w_toprow + wp->w_ntrows && shown_cursor_ln >= 0) {
					long oln = topln + (shown_cursor_row - wp->w_toprow);
					struct line *lp = wp->w_linep;
					int k;
					for (k = 0; k < shown_cursor_row - wp->w_toprow && lp != wp->w_bufp->b_linep; k++)
						lp = line_next(lp);
					if (lp != wp->w_bufp->b_linep)
						paint_gutter(shown_cursor_row, oln, gw, false);
				}
				paint_gutter(cursor_row, ln, gw, true);
				shown_cursor_row = cursor_row;
				shown_cursor_wp = wp;
				shown_cursor_ln = ln;
			}
		}
		modeline(wp);
	} else if (wp->w_flag || bound != oldb)
		paint_window(wp, check);
}

static void compute_highlights(void)
{
	struct line *mlp;
	int moff;

	memset(&hl, 0, sizeof(hl));
	hl.b1off = hl.b2off = -1;
	if (region_active && curwp->w_markp != NULL) {
		long dl = buffer_line_number(curbp, curwp->w_dotp);
		long ml = buffer_line_number(curbp, curwp->w_markp);
		int doff = curwp->w_doto, moff2 = curwp->w_marko;
		hl.region = 1;
		if (dl < ml || (dl == ml && doff <= moff2)) {
			hl.rs_ln = dl;
			hl.rs_off = doff;
			hl.re_ln = ml;
			hl.re_off = moff2;
		} else {
			hl.rs_ln = ml;
			hl.rs_off = moff2;
			hl.re_ln = dl;
			hl.re_off = doff;
		}
	}
	if (!(curbp->b_flag & BFINVS) && curwp->w_dotp != curbp->b_linep) {
		struct line *lp = curwp->w_dotp;
		int off = curwp->w_doto;
		if (find_matching_bracket(lp, off, &mlp, &moff)) {
			hl.b1lp = lp;
			hl.b1off = off;
			hl.b2lp = mlp;
			hl.b2off = moff;
		} else if (off > 0 && find_matching_bracket(lp, off - 1, &mlp, &moff)) {
			int c = lgetc(lp, off - 1);
			if (c == ')' || c == ']' || c == '}') {
				hl.b1lp = lp;
				hl.b1off = off - 1;
				hl.b2lp = mlp;
				hl.b2off = moff;
			}
		}
	}
	if (hl.region || shown_hl.region || hl.b1lp != shown_hl.b1lp || hl.b1off != shown_hl.b1off
	    || hl.b2lp != shown_hl.b2lp || hl.b2off != shown_hl.b2off)
		curwp->w_flag |= WFHARD;
	shown_hl = hl;
}

void update_now(void)
{
	struct window *wp;
	int oldbound = left_column;

	for (wp = window_head; wp != NULL; wp = wp->w_wndp)
		if (wp->w_flag)
			reframe(wp);

	compute_highlights();
	for (wp = window_head; wp != NULL; wp = wp->w_wndp)
		if (wp->w_flag & WFFORCE)
			reframe(wp);

	update_cursor_position();

	if (screen_garbage != FALSE) {
		movecursor(0, 0);
		tcapeeop();
		screen_garbage = FALSE;
		message_present = FALSE;
		shown_modeline_row = -1;
		for (wp = window_head; wp != NULL; wp = wp->w_wndp)
			paint_window(wp, (wp->w_bufp->b_mode & MDSPELL) != 0);
	} else {
		for (wp = window_head; wp != NULL; wp = wp->w_wndp)
			if (wp == curwp || wp->w_flag)
				update_window(wp, oldbound);
	}

	for (wp = window_head; wp != NULL; wp = wp->w_wndp) {
		wp->w_flag = 0;
		wp->w_force = 0;
	}

	movecursor(cursor_row, window_gutter(curwp) + cursor_col - left_column);
	ttflush();

	while (chg_width || chg_height)
		checkwinsize();
}

static int reframe(struct window *wp)
{
	struct line *lp, *lp0;
	int i = 0;

	if ((wp->w_flag & WFFORCE) == 0) {
		lp = wp->w_linep;
		lp0 = line_prev(lp);
		if (lp0 == wp->w_bufp->b_linep)
			i = 0;
		else {
			i = -1;
			lp = lp0;
		}
		for (; i <= wp->w_ntrows; i++) {
			if (lp == wp->w_dotp) {
				if (i < 0 || i == wp->w_ntrows) {
					break;
				}
				return TRUE;
			}

			if (lp == wp->w_bufp->b_linep)
				break;

			lp = line_next(lp);
		}
	}
	if (i == -1) {
		i = scroll_lines;
	} else if (i == wp->w_ntrows) {
		i = -scroll_lines;
	} else
		i = wp->w_force;

	wp->w_flag |= WFMODE;

	if (i > 0) {
		if (--i >= wp->w_ntrows)
			i = wp->w_ntrows - 1;
	} else if (i < 0) {
		i += wp->w_ntrows;
		if (i < 0)
			i = 0;
	} else
		i = wp->w_ntrows / 2;

	lp = wp->w_dotp;
	while (i != 0 && line_prev(lp) != wp->w_bufp->b_linep) {
		--i;
		lp = line_prev(lp);
	}

	wp->w_linep = lp;
	wp->w_flag |= WFHARD;
	wp->w_flag &= ~WFFORCE;
	return TRUE;
}

static void update_cursor_position(void)
{
	struct line *lp;
	int i, tw;

	lp = curwp->w_linep;
	cursor_row = curwp->w_toprow;
	while (lp != curwp->w_dotp) {
		++cursor_row;
		lp = line_next(lp);
	}

	cursor_col = 0;
	i = 0;
	while (i < curwp->w_doto) {
		unicode_t c;
		int bytes;
		unsigned char b = lp->l_text[i];

		if (b >= 0x20 && b < 0x7F) {
			cursor_col++;
			i++;
			continue;
		}
		bytes = utf8_to_unicode(lp->l_text, i, curwp->w_doto, &c);
		i += bytes;
		cursor_col = next_column(cursor_col, c);
	}

	tw = term.t_ncol - window_gutter(curwp);
	if (cursor_col >= tw - 1) {
		int rcursor = ((cursor_col - tw) % term.t_scrsiz)
			      + term.t_margin;
		left_column = cursor_col - rcursor + 1;
	} else
		left_column = 0;
}

static void modeline_putc(unsigned char *mline, int *np, int c)
{
	if (c < 0) {
		c += 256;
		if (c < 0)
			return;
	}

	if (*np >= term.t_ncol) {
		mline[term.t_ncol - 1] = '$';
		(*np)++;
		return;
	}

	if (c == '\t') {
		do {
			modeline_putc(mline, np, ' ');
		} while ((*np & tabmask) != 0);
		return;
	}

	if (c < 0x20) {
		modeline_putc(mline, np, '^');
		modeline_putc(mline, np, c ^ 0x40);
		return;
	}

	if (c == 0x7f) {
		modeline_putc(mline, np, '^');
		modeline_putc(mline, np, '?');
		return;
	}

	mline[(*np)++] = c;
}

static void modeline_puts(unsigned char *mline, int *np, const char *s)
{
	while (*s)
		modeline_putc(mline, np, (unsigned char)*s++);
}

static void modeline(struct window *wp)
{
	int n, i, lchar, firstm, col;
	struct buffer *bp = wp->w_bufp;
	char tline[NLINE];
	unsigned char mline[MAXCOL + 8];
	long ln, top, total;
	struct line *lp;

	memset(mline, ' ', sizeof(mline));
	n = 0;
	if (wp == curwp)
		lchar = '-';
	else if (can_reverse_video)
		lchar = ' ';
	else
		lchar = '-';

	modeline_putc(mline, &n, lchar);
	if (bp->b_mode & MDVIEW)
		modeline_puts(mline, &n, (bp->b_flag & BFCHG) ? "%*" : "%%");
	else
		modeline_puts(mline, &n, (bp->b_flag & BFCHG) ? "**" : "--");
	modeline_putc(mline, &n, lchar);
	modeline_puts(mline, &n, "  ");
	modeline_puts(mline, &n, bp->b_bname);
	modeline_puts(mline, &n, "   ");

	ln = buffer_line_number(bp, wp->w_dotp);
	if (wp == curwp)
		col = cursor_col;
	else {
		col = 0;
		lp = wp->w_dotp;
		for (i = 0; i < wp->w_doto && lp != bp->b_linep; i++) {
			int c = lgetc(lp, i);
			col = c == '\t' ? (col | tabmask) + 1 : col + 1;
		}
	}
	snprintf(tline, sizeof(tline), "L%ld C%d", ln + 1, col);
	modeline_puts(mline, &n, tline);
	modeline_puts(mline, &n, "   ");

	top = buffer_line_number(bp, wp->w_linep);
	total = buffer_line_number(bp, bp->b_linep);
	if (total == 0)
		strcpy(tline, "Emp");
	else if (top == 0 && top + wp->w_ntrows >= total)
		strcpy(tline, "All");
	else if (top == 0)
		strcpy(tline, "Top");
	else if (top + wp->w_ntrows >= total)
		strcpy(tline, "Bot");
	else
		snprintf(tline, sizeof(tline), "%ld%%", (100L * top) / total);
	modeline_puts(mline, &n, tline);
	modeline_puts(mline, &n, "   (");

	firstm = TRUE;
	tline[0] = 0;
	if ((bp->b_flag & BFTRUNC) != 0) {
		firstm = FALSE;
		strcat(tline, "Truncated");
	}
	for (i = 0; i < NUMMODES; i++)
		if (bp->b_mode & (1 << i)) {
			if ((1 << i) == 0x0100)
				continue;
			if (firstm != TRUE)
				strcat(tline, " ");
			firstm = FALSE;
			strcat(tline, mode2name[i]);
		}
	strcat(tline, ")  ");
	modeline_puts(mline, &n, tline);
	if (bp->b_indent)
		snprintf(tline, sizeof(tline), "Spaces:%d  ", bp->b_indent);
	else
		snprintf(tline, sizeof(tline), "Tabs  ");
	modeline_puts(mline, &n, tline);
	if (bp->b_fname[0] != 0 && strcmp(bp->b_bname, bp->b_fname) != 0) {
		modeline_puts(mline, &n, bp->b_fname);
		modeline_puts(mline, &n, "  ");
	}

	{
		const char *tag = " " PROGRAM_NAME_LONG " ";
		int tl = strlen(tag);
		if (n + tl + 4 <= term.t_ncol) {
			while (n < term.t_ncol - tl - 2)
				modeline_putc(mline, &n, lchar);
			modeline_puts(mline, &n, tag);
		}
	}
	while (n < term.t_ncol)
		modeline_putc(mline, &n, lchar);

	i = wp->w_toprow + wp->w_ntrows;

	if (shown_modeline_row == i &&
	    memcmp(shown_modeline, mline, term.t_ncol) == 0)
		return;
	memcpy(shown_modeline, mline, term.t_ncol);
	shown_modeline_row = i;

	movecursor(i, 0);
	tcaprev(TRUE);
	for (i = 0; i < term.t_ncol; i++)
		ttputc(mline[i]);
	tcaprev(FALSE);
	shown_col = term.t_ncol;
}

void update_modeline(void)
{
	struct window *wp;

	for (wp = window_head; wp != NULL; wp = wp->w_wndp)
		wp->w_flag |= WFMODE;
}

void movecursor(int row, int col)
{
	if (row != shown_row || col != shown_col) {
		shown_row = row;
		shown_col = col;
		tcapmove(row, col);
	}
}

void msg_erase(void)
{
	int i;

	movecursor(term.t_nrow, 0);
	if (display_commands == FALSE)
		return;

	if (can_erase_to_eol == TRUE)
		tcapeeol();
	else {
		for (i = 0; i < term.t_ncol - 1; i++)
			ttputc(' ');
		movecursor(term.t_nrow, 1);
		movecursor(term.t_nrow, 0);
	}
	ttflush();
	message_present = FALSE;
}

static int msg_begin(void)
{
	if (display_commands == FALSE) {
		movecursor(term.t_nrow, 0);
		return FALSE;
	}

	if (can_erase_to_eol == FALSE) {
		msg_erase();
		ttflush();
	}

	movecursor(term.t_nrow, 0);
	return TRUE;
}

static void msg_end(void)
{
	if (can_erase_to_eol == TRUE)
		tcapeeol();
	ttflush();
	message_present = TRUE;
}

void msg_puts(const char *s)
{
	if (!msg_begin())
		return;
	msg_append(s);
	msg_end();
}

void msg_printf(const char *fmt, ...)
{
	char buf[MAXCOL];
	va_list ap;

	if (!msg_begin())
		return;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	msg_append(buf);
	msg_end();
}

void msg_force(char *s)
{
	int oldcmd;

	oldcmd = display_commands;
	display_commands = TRUE;
	msg_puts(s);
	display_commands = oldcmd;
}

void msg_append(const char *s)
{
	int c;

	while ((c = *s++) != 0) {
		ttputc(c);
		++shown_col;
	}
}

void getscreensize(int *widthp, int *heightp)
{
	struct winsize size;
	*widthp = 0;
	*heightp = 0;
	if (ioctl(0, TIOCGWINSZ, &size) < 0)
		return;
	*widthp = size.ws_col;
	*heightp = size.ws_row;
}

void sizesignal(int signr)
{
	int w, h;
	int old_errno = errno;

	getscreensize(&w, &h);

	if (h && w && (h - 1 != term.t_nrow || w != term.t_ncol)) {
		chg_width = w;
		chg_height = h;
	}

	errno = old_errno;
}

void checkwinsize(void)
{
	int w = chg_width, h = chg_height;

	if (!w && !h)
		return;

	chg_width = chg_height = 0;
	if (h - 1 < term.t_mrow)
		cmd_change_screen_size(TRUE, h);
	if (w < term.t_mcol)
		cmd_change_screen_width(TRUE, w);

	update_now();
}
