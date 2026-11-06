#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "utf8.h"
#include "util.h"

static int getgoal(struct line *dlp)
{
	int col;
	int dbo;
	int len = line_length(dlp);

	col = 0;
	dbo = 0;
	while (dbo != len) {
		unicode_t c;
		int width = utf8_to_unicode(dlp->l_text, dbo, len, &c);

		col = next_column(col, c);
		if (col > goal_column)
			break;
		dbo += width;
	}
	return dbo;
}

int cmd_beginning_of_line(int f, int n)
{
	curwp->w_doto = 0;
	return TRUE;
}

int cmd_backward_character(int f, int n)
{
	struct line *lp;

	if (n < 0)
		return cmd_forward_character(f, -n);
	while (n--) {
		if (curwp->w_doto == 0) {
			if ((lp = line_prev(curwp->w_dotp)) == curbp->b_linep)
				return FALSE;
			curwp->w_dotp = lp;
			curwp->w_doto = line_length(lp);
			curwp->w_flag |= WFMOVE;
		} else {
			do {
				unsigned char c;
				curwp->w_doto--;
				c = lgetc(curwp->w_dotp, curwp->w_doto);
				if (is_beginning_utf8(c))
					break;
			} while (curwp->w_doto);
		}
	}
	return TRUE;
}

int cmd_end_of_line(int f, int n)
{
	curwp->w_doto = line_length(curwp->w_dotp);
	return TRUE;
}

int cmd_forward_character(int f, int n)
{
	if (n < 0)
		return cmd_backward_character(f, -n);
	while (n--) {
		int len = line_length(curwp->w_dotp);
		if (curwp->w_doto == len) {
			if (curwp->w_dotp == curbp->b_linep)
				return FALSE;
			curwp->w_dotp = line_next(curwp->w_dotp);
			curwp->w_doto = 0;
			curwp->w_flag |= WFMOVE;
		} else {
			do {
				unsigned char c;
				curwp->w_doto++;
				c = lgetc(curwp->w_dotp, curwp->w_doto);
				if (is_beginning_utf8(c))
					break;
			} while (curwp->w_doto < len);
		}
	}
	return TRUE;
}

int cmd_goto_line(int f, int n)
{
	int status;
	char arg[NSTRING];

	if (f == FALSE) {
		if ((status = ask_string("Goto line: ", arg, NSTRING)) != TRUE) {
			msg_printf("(Aborted)");
			return status;
		}
		n = atoi(arg);
	}

	if (n == 0)
		return cmd_end_of_file(f, n);

	if (n < 0)
		return FALSE;

	cmd_beginning_of_file(f, n);
	return cmd_next_line(f, n - 1);
}

int cmd_beginning_of_file(int f, int n)
{
	curwp->w_dotp = line_next(curbp->b_linep);
	curwp->w_doto = 0;
	curwp->w_flag |= WFHARD;
	return TRUE;
}

int cmd_end_of_file(int f, int n)
{
	curwp->w_dotp = curbp->b_linep;
	curwp->w_doto = 0;
	curwp->w_flag |= WFHARD;
	return TRUE;
}

int cmd_next_line(int f, int n)
{
	struct line *dlp;

	if (n < 0)
		return cmd_previous_line(f, -n);

	if (curwp->w_dotp == curbp->b_linep)
		return FALSE;

	if ((lastflag & CFCPCN) == 0)
		goal_column = getccol(FALSE);

	thisflag |= CFCPCN;

	dlp = curwp->w_dotp;
	while (n-- && dlp != curbp->b_linep)
		dlp = line_next(dlp);

	curwp->w_dotp = dlp;
	curwp->w_doto = getgoal(dlp);
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

int cmd_previous_line(int f, int n)
{
	struct line *dlp;

	if (n < 0)
		return cmd_next_line(f, -n);

	if (line_prev(curwp->w_dotp) == curbp->b_linep)
		return FALSE;

	if ((lastflag & CFCPCN) == 0)
		goal_column = getccol(FALSE);

	thisflag |= CFCPCN;

	dlp = curwp->w_dotp;
	while (n-- && line_prev(dlp) != curbp->b_linep)
		dlp = line_prev(dlp);

	curwp->w_dotp = dlp;
	curwp->w_doto = getgoal(dlp);
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

static int is_new_para(void)
{
	int i, len;

	len = line_length(curwp->w_dotp);

	for (i = 0; i < len; i++) {
		int c = lgetc(curwp->w_dotp, i);
		if (c == ' ' || c == TAB) {
			if (justflag)
				continue;
			return 1;
		}
		if (!isletter(c))
			return 1;
		return 0;
	}
	return 1;
}

int cmd_previous_paragraph(int f, int n)
{
	int suc;

	if (n < 0)
		return cmd_next_paragraph(f, -n);

	while (n-- > 0) {
		suc = cmd_backward_character(FALSE, 1);
		while (!inword() && suc)
			suc = cmd_backward_character(FALSE, 1);
		curwp->w_doto = 0;

		while (line_prev(curwp->w_dotp) != curbp->b_linep) {
			if (is_new_para())
				break;
			curwp->w_dotp = line_prev(curwp->w_dotp);
		}

		suc = cmd_forward_character(FALSE, 1);
		while (suc && !inword())
			suc = cmd_forward_character(FALSE, 1);
	}
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

int cmd_next_paragraph(int f, int n)
{
	int suc;

	if (n < 0)
		return cmd_previous_paragraph(f, -n);

	while (n-- > 0) {
		suc = cmd_forward_character(FALSE, 1);
		while (!inword() && suc)
			suc = cmd_forward_character(FALSE, 1);
		curwp->w_doto = 0;
		if (suc)
			curwp->w_dotp = line_next(curwp->w_dotp);

		while (curwp->w_dotp != curbp->b_linep) {
			if (is_new_para())
				break;
			curwp->w_dotp = line_next(curwp->w_dotp);
		}

		suc = cmd_backward_character(FALSE, 1);
		while (suc && !inword()) {
			suc = cmd_backward_character(FALSE, 1);
		}
		curwp->w_doto = line_length(curwp->w_dotp);
	}
	curwp->w_flag |= WFMOVE;
	return TRUE;
}

static int page_lines(void)
{
	int n = curwp->w_ntrows - 2;

	if (n < 1)
		n = 1;
	return n;
}

int cmd_next_page(int f, int n)
{
	struct line *lp;
	int i, row;

	if (f == FALSE)
		n = page_lines();
	else if (n < 0)
		return cmd_previous_page(f, -n);
	lp = curwp->w_linep;
	for (i = 0; i < n && lp != curbp->b_linep; i++)
		lp = line_next(lp);
	if (i == 0 || lp == curbp->b_linep) {
		if (curwp->w_dotp == curbp->b_linep) {
			msg_printf("End of buffer");
			return FALSE;
		}
		curwp->w_dotp = curbp->b_linep;
		curwp->w_doto = 0;
		curwp->w_flag |= WFMOVE;
		return TRUE;
	}
	curwp->w_linep = lp;
	row = 0;
	{
		struct line *p = lp;
		int found = FALSE;
		for (row = 0; row < curwp->w_ntrows && p != curbp->b_linep; row++, p = line_next(p))
			if (p == curwp->w_dotp) {
				found = TRUE;
				break;
			}
		if (!found) {
			curwp->w_dotp = lp;
			curwp->w_doto = 0;
		}
	}
	curwp->w_flag |= WFHARD;
	return TRUE;
}

int cmd_previous_page(int f, int n)
{
	struct line *lp;
	int i;

	if (f == FALSE)
		n = page_lines();
	else if (n < 0)
		return cmd_next_page(f, -n);
	lp = curwp->w_linep;
	if (line_prev(lp) == curbp->b_linep) {
		if (curwp->w_dotp == line_next(curbp->b_linep) && curwp->w_doto == 0) {
			msg_printf("Beginning of buffer");
			return FALSE;
		}
		curwp->w_dotp = lp;
		curwp->w_doto = 0;
		curwp->w_flag |= WFMOVE;
		return TRUE;
	}
	for (i = 0; i < n && line_prev(lp) != curbp->b_linep; i++)
		lp = line_prev(lp);
	curwp->w_linep = lp;
	{
		struct line *p = lp;
		int found = FALSE, row;
		struct line *last = lp;
		for (row = 0; row < curwp->w_ntrows && p != curbp->b_linep; row++, p = line_next(p)) {
			last = p;
			if (p == curwp->w_dotp) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			curwp->w_dotp = last;
			curwp->w_doto = 0;
		}
	}
	curwp->w_flag |= WFHARD;
	return TRUE;
}

int cmd_set_mark(int f, int n)
{
	if (f && curwp->w_markp != NULL) {
		curwp->w_dotp = curwp->w_markp;
		curwp->w_doto = curwp->w_marko;
		curwp->w_flag |= WFMOVE;
		region_active = FALSE;
		return TRUE;
	}
	if ((lastflag & CFMARK) && region_active && curwp->w_markp == curwp->w_dotp && curwp->w_marko == curwp->w_doto) {
		region_active = FALSE;
		curwp->w_flag |= WFHARD;
		msg_printf("Mark deactivated");
		thisflag |= CFMARK;
		return TRUE;
	}
	curwp->w_markp = curwp->w_dotp;
	curwp->w_marko = curwp->w_doto;
	region_active = TRUE;
	region_shift = FALSE;
	thisflag |= CFMARK | CFKEEPR;
	msg_printf("Mark set");
	return TRUE;
}

int cmd_exchange_point_and_mark(int f, int n)
{
	struct line *odotp;
	int odoto;

	if (curwp->w_markp == NULL) {
		msg_printf("No mark in this window");
		return FALSE;
	}
	odotp = curwp->w_dotp;
	odoto = curwp->w_doto;
	curwp->w_dotp = curwp->w_markp;
	curwp->w_doto = curwp->w_marko;
	curwp->w_markp = odotp;
	curwp->w_marko = odoto;
	curwp->w_flag |= WFMOVE;
	if (!f)
		region_active = TRUE;
	thisflag |= CFKEEPR;
	return TRUE;
}
