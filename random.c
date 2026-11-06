#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "utf8.h"
#include "util.h"

int tabsize;

int cmd_set_fill_column(int f, int n)
{
	fill_column = n;
	msg_printf("(Fill column is %d)", n);
	return TRUE;
}

int cmd_buffer_position(int f, int n)
{
	struct line *lp;
	long numchars;
	int numlines;
	long predchars;
	int predlines;
	int curchar;
	int ratio;
	int col;
	int savepos;
	int ecol;

	lp = line_next(curbp->b_linep);

	numchars = 0;
	numlines = 0;
	predchars = 0;
	predlines = 0;
	curchar = 0;
	while (lp != curbp->b_linep) {
		if (lp == curwp->w_dotp) {
			predlines = numlines;
			predchars = numchars + curwp->w_doto;
			if ((curwp->w_doto) == line_length(lp))
				curchar = '\n';
			else
				curchar = lgetc(lp, curwp->w_doto);
		}

		++numlines;
		numchars += line_length(lp) + 1;
		lp = line_next(lp);
	}

	if (curwp->w_dotp == curbp->b_linep) {
		predlines = numlines;
		predchars = numchars;
		curchar = 0;
	}

	col = getccol(FALSE);
	savepos = curwp->w_doto;
	curwp->w_doto = line_length(curwp->w_dotp);
	ecol = getccol(FALSE);
	curwp->w_doto = savepos;

	ratio = 0;
	if (numchars != 0)
		ratio = (100L * predchars) / numchars;

	msg_printf("Line %d/%d Col %d/%d Char %ld/%ld (%d%%) char = 0x%x",
		predlines + 1, numlines + 1, col, ecol, predchars, numchars, ratio, curchar);
	return TRUE;
}

int getcline(void)
{
	struct line *lp;
	int numlines;

	lp = line_next(curbp->b_linep);

	numlines = 0;
	while (lp != curbp->b_linep) {
		if (lp == curwp->w_dotp)
			break;
		++numlines;
		lp = line_next(lp);
	}

	return numlines + 1;
}

int getccol(int bflg)
{
	int i, col;
	struct line *dlp = curwp->w_dotp;
	int byte_offset = curwp->w_doto;
	int len = line_length(dlp);

	col = i = 0;
	while (i < byte_offset) {
		unicode_t c;

		i += utf8_to_unicode(dlp->l_text, i, len, &c);
		if (c != ' ' && c != '\t' && bflg)
			break;
		col = next_column(col, c);
	}
	return col;
}

int setccol(int pos)
{
	int c;
	int i;
	int col;
	int llen;

	col = 0;
	llen = line_length(curwp->w_dotp);

	for (i = 0; i < llen; ++i) {
		if (col >= pos)
			break;

		c = lgetc(curwp->w_dotp, i);
		if (c == '\t')
			col |= tabmask;
		else if (c < 0x20 || c == 0x7F)
			++col;
		++col;
	}

	curwp->w_doto = i;

	return col >= pos;
}

int cmd_transpose_characters(int f, int n)
{
	struct line *dotp;
	int doto;
	int cl;
	int cr;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	dotp = curwp->w_dotp;
	doto = curwp->w_doto;
	if (doto == line_length(dotp) && --doto < 0)
		return FALSE;
	cr = lgetc(dotp, doto);
	if (--doto < 0)
		return FALSE;
	cl = lgetc(dotp, doto);
	line_put_byte(dotp, doto + 0, cr);
	line_put_byte(dotp, doto + 1, cl);
	if (curwp->w_doto < line_length(dotp))
		curwp->w_doto = doto + 2;
	buffer_changed(WFEDIT);
	return TRUE;
}

int cmd_quote_character(int f, int n)
{
	int s;
	int c;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	c = tgetc();
	if (n < 0)
		return FALSE;
	if (n == 0)
		return TRUE;
	if (c == '\n') {
		do {
			s = insert_newline();
		} while (s == TRUE && --n);
		return s;
	}
	return insert_char(n, c);
}

int cmd_handle_tab(int f, int n)
{
	if (n < 0)
		return FALSE;
	if (n == 0 || n > 1) {
		tabsize = n;
		return TRUE;
	}
	if (!tabsize)
		return insert_char(1, '\t');
	return insert_char(tabsize - (getccol(FALSE) % tabsize), ' ');
}

int cmd_detab_line(int f, int n)
{
	int inc;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();

	if (f == FALSE)
		n = 1;

	inc = ((n > 0) ? 1 : -1);
	while (n) {
		curwp->w_doto = 0;

		while (curwp->w_doto < line_length(curwp->w_dotp)) {
			if (lgetc(curwp->w_dotp, curwp->w_doto) == '\t') {
				delete_characters(1, FALSE);
				cmd_insert_space(TRUE, (tabmask + 1) - (curwp->w_doto & tabmask));
			}
			cmd_forward_character(FALSE, 1);
		}

		cmd_next_line(TRUE, inc);
		n -= inc;
	}
	curwp->w_doto = 0;
	thisflag &= ~CFCPCN;
	buffer_changed(WFEDIT);
	return TRUE;
}

int cmd_entab_line(int f, int n)
{
	int inc;
	int fspace;
	int ccol;
	char cchar;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();

	if (f == FALSE)
		n = 1;

	inc = ((n > 0) ? 1 : -1);
	while (n) {
		curwp->w_doto = 0;

		fspace = -1;
		ccol = 0;
		while (curwp->w_doto < line_length(curwp->w_dotp)) {
			if ((fspace >= 0) && (nextab(fspace) <= ccol)) {
				if (ccol - fspace < 2)
					fspace = -1;
				else {
					cmd_backward_character(TRUE, ccol - fspace);
					delete_bytes((long)(ccol - fspace), FALSE);
					insert_char(1, '\t');
					fspace = -1;
				}
			}

			cchar = lgetc(curwp->w_dotp, curwp->w_doto);

			switch (cchar) {
			case '\t':
				ccol = nextab(ccol);
				break;

			case ' ':
				if (fspace == -1)
					fspace = ccol;
				ccol++;
				break;

			default:
				ccol++;
				fspace = -1;
				break;
			}
			cmd_forward_character(FALSE, 1);
		}

		cmd_next_line(TRUE, inc);
		n -= inc;
	}
	curwp->w_doto = 0;
	thisflag &= ~CFCPCN;
	buffer_changed(WFEDIT);
	return TRUE;
}

int cmd_trim_line(int f, int n)
{
	struct line *lp;
	int offset;
	int length;
	int inc;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();

	if (f == FALSE)
		n = 1;

	inc = ((n > 0) ? 1 : -1);
	while (n) {
		lp = curwp->w_dotp;
		offset = curwp->w_doto;
		length = lp->l_used;

		while (length > offset) {
			if (lgetc(lp, length - 1) != ' ' && lgetc(lp, length - 1) != '\t')
				break;
			length--;
		}
		line_truncate(lp, length);

		cmd_next_line(TRUE, inc);
		n -= inc;
	}
	buffer_changed(WFEDIT);
	thisflag &= ~CFCPCN;
	return TRUE;
}

int cmd_open_line(int f, int n)
{
	int i;
	int s;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (n < 0)
		return FALSE;
	if (n == 0)
		return TRUE;
	i = n;
	do {
		s = insert_newline();
	} while (s == TRUE && --i);
	if (s == TRUE)
		s = cmd_backward_character(f, n);
	return s;
}

int cmd_newline(int f, int n)
{
	int s;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (n < 0)
		return FALSE;

	if (n == 1 && (curbp->b_mode & MDCMOD) && curwp->w_dotp != curbp->b_linep)
		return cinsert();

	if ((curwp->w_bufp->b_mode & MDWRAP) && fill_column > 0 &&
	    getccol(FALSE) > fill_column && (curwp->w_bufp->b_mode & MDVIEW) == FALSE)
		execute(HOOK_WRAP, FALSE, 1);

	while (n--) {
		if ((s = insert_newline()) != TRUE)
			return s;
	}
	return TRUE;
}

int cinsert(void)
{
	char *cptr;
	int tptr;
	int bracef;
	int i;
	char ichar[NSTRING];

	cptr = &curwp->w_dotp->l_text[0];

	tptr = curwp->w_doto - 1;
	bracef = (cptr[tptr] == '{');

	i = 0;
	while ((i < tptr) && (cptr[i] == ' ' || cptr[i] == '\t')
	       && (i < NSTRING - 1)) {
		ichar[i] = cptr[i];
		++i;
	}
	ichar[i] = 0;

	if (insert_newline() == FALSE)
		return FALSE;

	insert_string(ichar);

	if (bracef)
		cmd_handle_tab(FALSE, 1);

	return TRUE;
}

int insbrace(int n, int c)
{
	int ch;
	int oc;
	int i, count;
	int target;
	struct line *oldlp;
	int oldoff;

	if (curwp->w_doto != 0)

		for (i = curwp->w_doto - 1; i >= 0; --i) {
			ch = lgetc(curwp->w_dotp, i);
			if (ch != ' ' && ch != '\t')
				return insert_char(n, c);
		}

	switch (c) {
	case '}':
		oc = '{';
		break;
	case ']':
		oc = '[';
		break;
	case ')':
		oc = '(';
		break;
	default:
		return FALSE;
	}

	oldlp = curwp->w_dotp;
	oldoff = curwp->w_doto;

	count = 1;
	cmd_backward_character(FALSE, 1);

	while (count > 0) {
		if (curwp->w_doto == line_length(curwp->w_dotp))
			ch = '\n';
		else
			ch = lgetc(curwp->w_dotp, curwp->w_doto);

		if (ch == c)
			++count;
		if (ch == oc)
			--count;

		cmd_backward_character(FALSE, 1);
		if (at_buffer_end(curwp->w_dotp, curwp->w_doto, REVERSE))
			break;
	}

	if (count != 0) {
		curwp->w_dotp = oldlp;
		curwp->w_doto = oldoff;
		return insert_char(n, c);
	}

	curwp->w_doto = 0;

	while ((ch = lgetc(curwp->w_dotp, curwp->w_doto)) == ' ' || ch == '\t')
		cmd_forward_character(FALSE, 1);

	target = getccol(FALSE);
	curwp->w_dotp = oldlp;
	curwp->w_doto = oldoff;

	while (target != getccol(FALSE)) {
		if (target < getccol(FALSE))
			while (getccol(FALSE) > target)
				cmd_delete_previous_character(FALSE, 1);
		else {
			while (target - getccol(FALSE) >= 8)
				insert_char(1, '\t');
			insert_char(target - getccol(FALSE), ' ');
		}
	}

	return insert_char(n, c);
}

int inspound(void)
{
	int ch;
	int i;

	if (curwp->w_doto == 0)
		return insert_char(1, '#');

	for (i = curwp->w_doto - 1; i >= 0; --i) {
		ch = lgetc(curwp->w_dotp, i);
		if (ch != ' ' && ch != '\t')
			return insert_char(1, '#');
	}

	while (getccol(FALSE) >= 1)
		cmd_delete_previous_character(FALSE, 1);

	return insert_char(1, '#');
}

int cmd_delete_blank_lines(int f, int n)
{
	struct line *lp1;
	struct line *lp2;
	long nld;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	lp1 = curwp->w_dotp;
	while (line_length(lp1) == 0 && (lp2 = line_prev(lp1)) != curbp->b_linep)
		lp1 = lp2;
	lp2 = lp1;
	nld = 0;
	while ((lp2 = line_next(lp2)) != curbp->b_linep && line_length(lp2) == 0)
		++nld;
	if (nld == 0)
		return TRUE;
	curwp->w_dotp = line_next(lp1);
	curwp->w_doto = 0;
	return delete_bytes(nld, FALSE);
}

int cmd_delete_next_character(int f, int n)
{
	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (n < 0)
		return cmd_delete_previous_character(f, -n);
	if (f != FALSE) {
		if ((lastflag & CFKILL) == 0)
			kdelete();
		thisflag |= CFKILL;
	}
	return delete_characters((long)n, f);
}

int cmd_delete_previous_character(int f, int n)
{
	int s;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (n < 0)
		return cmd_delete_next_character(f, -n);
	if (f != FALSE) {
		if ((lastflag & CFKILL) == 0)
			kdelete();
		thisflag |= CFKILL;
	}
	if ((s = cmd_backward_character(f, n)) == TRUE)
		s = delete_characters(n, f);
	return s;
}

int cmd_kill_to_end_of_line(int f, int n)
{
	struct line *nextp;
	long chunk;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	if (f == FALSE) {
		chunk = line_length(curwp->w_dotp) - curwp->w_doto;
		if (chunk == 0)
			chunk = 1;
	} else if (n == 0) {
		chunk = curwp->w_doto;
		curwp->w_doto = 0;
	} else if (n > 0) {
		chunk = line_length(curwp->w_dotp) - curwp->w_doto + 1;
		nextp = line_next(curwp->w_dotp);
		while (--n) {
			if (nextp == curbp->b_linep)
				return FALSE;
			chunk += line_length(nextp) + 1;
			nextp = line_next(nextp);
		}
	} else {
		msg_printf("neg kill");
		return FALSE;
	}
	return delete_bytes(chunk, TRUE);
}

int cmd_add_mode(int f, int n)
{
	return adjustmode(TRUE, FALSE);
}

int cmd_delete_mode(int f, int n)
{
	return adjustmode(FALSE, FALSE);
}

int cmd_add_global_mode(int f, int n)
{
	return adjustmode(TRUE, TRUE);
}

int cmd_delete_global_mode(int f, int n)
{
	return adjustmode(FALSE, TRUE);
}

int adjustmode(int kind, int global)
{
	char *scan;
	int i;
	int status;
	char prompt[50];
	char mode_name[NPAT];

	if (global)
		strcpy(prompt, "Global mode to ");
	else
		strcpy(prompt, "Mode to ");

	if (kind == TRUE)
		strcat(prompt, "add: ");
	else
		strcat(prompt, "delete: ");

	complete_kind = CMPL_MODE;
	status = ask_string(prompt, mode_name, NPAT - 1);
	if (status != TRUE)
		return status;

	scan = mode_name;
	while (*scan != 0) {
		if (*scan >= 'a' && *scan <= 'z')
			*scan = *scan - 32;
		scan++;
	}

	for (i = 0; i < NUMMODES; i++) {
		if (strcmp(mode_name, modename[i]) == 0) {
			if (kind == TRUE)
				if (global)
					global_mode |= (1 << i);
				else
					curbp->b_mode |= (1 << i);
			else if (global)
				global_mode &= ~(1 << i);
			else
				curbp->b_mode &= ~(1 << i);

			if (global == 0)
				update_modeline();
			msg_erase();
			return TRUE;
		}
	}

	msg_printf("No such mode!");
	return FALSE;
}

int cmd_clear_message_line(int f, int n)
{
	msg_force("");
	return TRUE;
}

int cmd_write_message(int f, int n)
{
	int status;
	char buf[NPAT];

	if ((status = ask_string("Message to write: ", buf, NPAT - 1)) != TRUE)
		return status;

	msg_force(buf);
	return TRUE;
}

int cmd_goto_matching_fence(int f, int n)
{
	struct line *oldlp;
	int oldoff;
	int sdir;
	int count;
	char ch;
	char ofence;
	char c;

	oldlp = curwp->w_dotp;
	oldoff = curwp->w_doto;

	if (oldoff == line_length(oldlp))
		ch = '\n';
	else
		ch = lgetc(oldlp, oldoff);

	switch (ch) {
	case '(':
		ofence = ')';
		sdir = FORWARD;
		break;
	case '{':
		ofence = '}';
		sdir = FORWARD;
		break;
	case '[':
		ofence = ']';
		sdir = FORWARD;
		break;
	case ')':
		ofence = '(';
		sdir = REVERSE;
		break;
	case '}':
		ofence = '{';
		sdir = REVERSE;
		break;
	case ']':
		ofence = '[';
		sdir = REVERSE;
		break;
	default:
		tcapbeep();
		return FALSE;
	}

	count = 1;
	if (sdir == REVERSE)
		cmd_backward_character(FALSE, 1);
	else
		cmd_forward_character(FALSE, 1);

	while (count > 0) {
		if (curwp->w_doto == line_length(curwp->w_dotp))
			c = '\n';
		else
			c = lgetc(curwp->w_dotp, curwp->w_doto);
		if (c == ch)
			++count;
		if (c == ofence)
			--count;
		if (sdir == FORWARD)
			cmd_forward_character(FALSE, 1);
		else
			cmd_backward_character(FALSE, 1);
		if (at_buffer_end(curwp->w_dotp, curwp->w_doto, sdir))
			break;
	}

	if (count == 0) {
		if (sdir == FORWARD)
			cmd_backward_character(FALSE, 1);
		else
			cmd_forward_character(FALSE, 1);
		curwp->w_flag |= WFMOVE;
		return TRUE;
	}

	curwp->w_dotp = oldlp;
	curwp->w_doto = oldoff;
	tcapbeep();
	return FALSE;
}

int fmatch(int ch)
{
	struct line *oldlp;
	int oldoff;
	struct line *toplp;
	int count;
	char opench;
	char c;

	update();

	oldlp = curwp->w_dotp;
	oldoff = curwp->w_doto;

	if (ch == ')')
		opench = '(';
	else if (ch == '}')
		opench = '{';
	else
		opench = '[';

	toplp = curwp->w_linep->l_bp;
	count = 1;
	cmd_backward_character(FALSE, 2);

	while (count > 0 && curwp->w_dotp != toplp) {
		if (curwp->w_doto == line_length(curwp->w_dotp))
			c = '\n';
		else
			c = lgetc(curwp->w_dotp, curwp->w_doto);
		if (c == ch)
			++count;
		if (c == opench)
			--count;
		cmd_backward_character(FALSE, 1);
		if (curwp->w_dotp == curwp->w_bufp->b_linep->l_fp && curwp->w_doto == 0)
			break;
	}

	if (count == 0) {
		cmd_forward_character(FALSE, 1);
		update();
		ttpause();
	}

	curwp->w_dotp = oldlp;
	curwp->w_doto = oldoff;
	return TRUE;
}

int cmd_insert_string(int f, int n)
{
	int status;
	char tstring[NPAT + 1];

	status = ask_string("Insert: ", tstring, NPAT);
	if (status != TRUE)
		return status;

	if (f == FALSE)
		n = 1;

	if (n < 0)
		n = -n;

	while (n-- && (status = insert_string(tstring))) ;
	return status;
}

int cmd_overwrite_string(int f, int n)
{
	int status;
	char tstring[NPAT + 1];

	status = ask_string("Overwrite with: ", tstring, NPAT);
	if (status != TRUE)
		return status;

	if (f == FALSE)
		n = 1;

	if (n < 0)
		n = -n;

	while (n-- && (status = overwrite_string(tstring))) ;
	return status;
}
