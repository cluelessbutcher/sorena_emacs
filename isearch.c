#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "util.h"

static int echo_char(int c, int col);

static int cmd_buff[CMDBUFLEN];
static int cmd_offset;
static int cmd_reexecute = -1;

int cmd_reverse_incremental_search(int f, int n)
{
	struct line *curline;
	int curoff;

	curline = curwp->w_dotp;
	curoff = curwp->w_doto;

	cmd_backward_character(TRUE, 1);

	if (!(isearch(f, -n))) {
		curwp->w_dotp = curline;
		curwp->w_doto = curoff;
		curwp->w_flag |= WFMOVE;
		update();
		msg_printf("(search failed)");
		match_bytes = strlen(search_pattern);
	} else
		msg_erase();
	match_bytes = strlen(search_pattern);
	return TRUE;
}

int cmd_incremental_search(int f, int n)
{
	struct line *curline;
	int curoff;

	curline = curwp->w_dotp;
	curoff = curwp->w_doto;

	if (!(isearch(f, n))) {
		curwp->w_dotp = curline;
		curwp->w_doto = curoff;
		curwp->w_flag |= WFMOVE;
		update();
		msg_printf("(search failed)");
		match_bytes = strlen(search_pattern);
	} else
		msg_erase();
	match_bytes = strlen(search_pattern);
	return TRUE;
}

int isearch(int f, int n)
{
	int status;
	int col;
	int cpos;
	int c;
	int expc;
	char pat_save[NPAT];
	struct line *curline;
	int curoff;
	int init_direction;

	cmd_reexecute = -1;
	cmd_offset = 0;
	cmd_buff[0] = '\0';
	mystrscpy(pat_save, search_pattern, NPAT);
	curline = curwp->w_dotp;
	curoff = curwp->w_doto;
	init_direction = n;

 start_over:

	search_case_update();
	col = promptpattern(n < 0 ? "I-search backward: " : "I-search: ");

	cpos = 0;
	status = TRUE;

	c = keycode_to_char(expc = get_char());
	if ((c == IS_FORWARD) || (c == IS_REVERSE)) {
		for (cpos = 0; search_pattern[cpos] != 0; cpos++)
			col = echo_char(search_pattern[cpos], col);
		if (c == IS_REVERSE) {
			n = -1;
			cmd_backward_character(TRUE, 1);
		} else
			n = 1;
		status = scanmore(search_pattern, n);
		c = keycode_to_char(expc = get_char());
	}

	for (;;) {
		if (expc == meta_char)
			return TRUE;

		switch (c) {
		case IS_ABORT:
			return FALSE;

		case IS_REVERSE:
		case IS_FORWARD:
			if (c == IS_REVERSE)
				n = -1;
			else
				n = 1;
			status = scanmore(search_pattern, n);
			c = keycode_to_char(expc = get_char());
			continue;

		case IS_NEWLINE:
			return TRUE;

		case IS_QUOTE:
			c = keycode_to_char(expc = get_char());

		case IS_TAB:
		case '\n':
			break;

		case IS_BACKSP:
		case IS_RUBOUT:
			if (cmd_offset <= 1)
				return TRUE;
			--cmd_offset;
			cmd_buff[--cmd_offset] = '\0';
			curwp->w_dotp = curline;
			curwp->w_doto = curoff;
			n = init_direction;
			mystrscpy(search_pattern, pat_save, NPAT);
			cmd_reexecute = 0;
			goto start_over;

		default:
			if (c == 128 + 27 || (expc & (SPEC | META | CTLX | CTLC))) {
				ttungetc(c);
				return TRUE;
			}
			if (c < ' ') {
				ttungetc(c);
				return TRUE;
			}
		}

		search_pattern[cpos++] = c;
		if (cpos >= NPAT) {
			msg_printf("? Search string too long");
			return TRUE;
		}
		search_pattern[cpos] = 0;
		search_case_update();
		col = echo_char(c, col);
		if (!status) {
			ttputc(BELL);
			ttflush();
		} else   if (!(status = checknext(c, search_pattern, n)))
			status = scanmore(search_pattern, n);
		c = keycode_to_char(expc = get_char());
	}
}

int checknext(char chr, char *patrn, int dir)
{
	struct line *curline;
	int curoff;
	int buffchar;
	int status;

	curline = curwp->w_dotp;
	curoff = curwp->w_doto;

	if (dir > 0) {
		if (curoff == line_length(curline)) {
			curline = line_next(curline);
			if (curline == curbp->b_linep)
				return FALSE;
			curoff = 0;
			buffchar = '\n';
		} else
			buffchar = lgetc(curline, curoff++);
		if ((status = eq(buffchar, chr)) != 0) {
			curwp->w_dotp = curline;
			curwp->w_doto = curoff;
			curwp->w_flag |= WFMOVE;
		}
		return status;
	} else
		return match_pat(patrn);
}

int scanmore(char *patrn, int dir)
{
	int sts;

	if (dir < 0) {
		rvstrcpy(reversed_pattern, patrn);
		sts = scanner(reversed_pattern, REVERSE, PTBEG);
	} else
		sts = scanner(patrn, FORWARD, PTEND);

	if (!sts) {
		ttputc(BELL);
		ttflush();
	}

	return sts;
}

int match_pat(char *patrn)
{
	int i;
	int buffchar;
	struct line *curline;
	int curoff;

	curline = curwp->w_dotp;
	curoff = curwp->w_doto;

	for (i = 0; i < strlen(patrn); i++) {
		if (curoff == line_length(curline)) {
			curline = line_next(curline);
			curoff = 0;
			if (curline == curbp->b_linep)
				return FALSE;
			buffchar = '\n';
		} else
			buffchar = lgetc(curline, curoff++);
		if (!eq(buffchar, patrn[i]))
			return FALSE;
	}
	return TRUE;
}

int promptpattern(char *prompt)
{
	char tpat[NPAT + 20];

	mystrscpy(tpat, prompt, sizeof(tpat));

	if (!executing_command_line) {
		msg_puts(tpat);
	}
	return strlen(tpat);
}

static int echo_char(int c, int col)
{
	movecursor(term.t_nrow, col);
	if ((c < ' ') || (c == 0x7F)) {
		switch (c) {
		case '\n':
			ttputc('<');
			ttputc('N');
			ttputc('L');
			ttputc('>');
			col += 3;
			break;

		case '\t':
			ttputc('<');
			ttputc('T');
			ttputc('A');
			ttputc('B');
			ttputc('>');
			col += 4;
			break;

		case 0x7F:
			ttputc('^');
			ttputc('?');
			col++;
			break;

		default:
			ttputc('^');
			ttputc(c + 0x40);
			col++;
		}
	} else
		ttputc(c);
	ttflush();
	return ++col;
}

int get_char(void)
{
	int c;

	if (cmd_reexecute >= 0)
		if ((c = cmd_buff[cmd_reexecute++]) != 0)
			return c;

	cmd_reexecute = -1;
	update();
	if (cmd_offset >= CMDBUFLEN - 1) {
		msg_printf("? command too long");
		return meta_char;
	}
	c = get1key();
	cmd_buff[cmd_offset++] = c;
	cmd_buff[cmd_offset] = '\0';
	return c;
}
