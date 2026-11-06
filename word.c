#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"

int cmd_wrap_word(int f, int n)
{
	int cnt;
	int c;

	if (!cmd_backward_character(0, 1))
		return FALSE;

	cnt = 0;
	while (((c = lgetc(curwp->w_dotp, curwp->w_doto)) != ' ')
	       && (c != '\t')) {
		cnt++;
		if (!cmd_backward_character(0, 1))
			return FALSE;

		if (curwp->w_doto == 0) {
			cmd_end_of_line(FALSE, 0);
			return insert_newline();
		}
	}

	if (!cmd_delete_next_character(0, 1))
		return FALSE;

	if (!insert_newline())
		return FALSE;

	while (cnt-- > 0) {
		if (cmd_forward_character(FALSE, 1) == FALSE)
			return FALSE;
	}
	return TRUE;
}

int cmd_previous_word(int f, int n)
{
	if (n < 0)
		return cmd_next_word(f, -n);
	if (cmd_backward_character(FALSE, 1) == FALSE)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (cmd_backward_character(FALSE, 1) == FALSE)
				return FALSE;
		}
		while (inword() != FALSE) {
			if (cmd_backward_character(FALSE, 1) == FALSE)
				return FALSE;
		}
	}
	return cmd_forward_character(FALSE, 1);
}

int cmd_next_word(int f, int n)
{
	if (n < 0)
		return cmd_previous_word(f, -n);
	while (n--) {
		while (inword() == TRUE) {
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
		}

		while (inword() == FALSE) {
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
		}
	}
	return TRUE;
}

int cmd_case_word_upper(int f, int n)
{
	int c;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (n < 0)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
		}
		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (islower(c)) {
				c -= 'a' - 'A';
				line_put_byte(curwp->w_dotp, curwp->w_doto, c);
				buffer_changed(WFHARD);
			}
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
		}
	}
	return TRUE;
}

int cmd_case_word_lower(int f, int n)
{
	int c;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (n < 0)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
		}
		while (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (isupper(c)) {
				c += 'a' - 'A';
				line_put_byte(curwp->w_dotp, curwp->w_doto, c);
				buffer_changed(WFHARD);
			}
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
		}
	}
	return TRUE;
}

int cmd_case_word_capitalize(int f, int n)
{
	int c;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (n < 0)
		return FALSE;
	while (n--) {
		while (inword() == FALSE) {
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
		}
		if (inword() != FALSE) {
			c = lgetc(curwp->w_dotp, curwp->w_doto);
			if (islower(c)) {
				c -= 'a' - 'A';
				line_put_byte(curwp->w_dotp, curwp->w_doto, c);
				buffer_changed(WFHARD);
			}
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
			while (inword() != FALSE) {
				c = lgetc(curwp->w_dotp, curwp->w_doto);
				if (isupper(c)) {
					c += 'a' - 'A';
					line_put_byte(curwp->w_dotp, curwp->w_doto, c);
					buffer_changed(WFHARD);
				}
				if (cmd_forward_character(FALSE, 1) == FALSE)
					return FALSE;
			}
		}
	}
	return TRUE;
}

int cmd_delete_next_word(int f, int n)
{
	struct line *dotp;
	int doto;
	int c;
	long size;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();

	if (n < 0)
		return FALSE;

	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;

	dotp = curwp->w_dotp;
	doto = curwp->w_doto;

	size = 0;

	while (inword() == FALSE) {
		if (cmd_forward_character(FALSE, 1) == FALSE)
			return FALSE;
		++size;
	}

	if (n == 0) {
		while (inword() == TRUE) {
			if (cmd_forward_character(FALSE, 1) == FALSE)
				return FALSE;
			++size;
		}
	} else {
		while (n--) {
			while (curwp->w_doto == line_length(curwp->w_dotp)) {
				if (cmd_forward_character(FALSE, 1) == FALSE)
					return FALSE;
				++size;
			}

			while (inword() == TRUE) {
				if (cmd_forward_character(FALSE, 1) == FALSE)
					return FALSE;
				++size;
			}

			if (n != 0)
				while (inword() == FALSE) {
					if (cmd_forward_character(FALSE, 1) == FALSE)
						return FALSE;
					++size;
				}
		}

		while ((curwp->w_doto == line_length(curwp->w_dotp)) ||
		       ((c = lgetc(curwp->w_dotp, curwp->w_doto)) == ' ')
		       || (c == '\t')) {
			if (cmd_forward_character(FALSE, 1) == FALSE)
				break;
			++size;
		}
	}

	curwp->w_dotp = dotp;
	curwp->w_doto = doto;
	return delete_bytes(size, TRUE);
}

int cmd_delete_previous_word(int f, int n)
{
	long size;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();

	if (n <= 0)
		return FALSE;

	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;

	if (cmd_backward_character(FALSE, 1) == FALSE)
		return FALSE;
	size = 0;
	while (n--) {
		while (inword() == FALSE) {
			if (cmd_backward_character(FALSE, 1) == FALSE)
				return FALSE;
			++size;
		}
		while (inword() != FALSE) {
			++size;
			if (cmd_backward_character(FALSE, 1) == FALSE)
				goto bckdel;
		}
	}
	if (cmd_forward_character(FALSE, 1) == FALSE)
		return FALSE;
 bckdel:return delete_characters(size, TRUE);
}

int inword(void)
{
	int c;

	if (curwp->w_doto == line_length(curwp->w_dotp))
		return FALSE;
	c = lgetc(curwp->w_dotp, curwp->w_doto);
	if (isletter(c))
		return TRUE;
	if (c >= '0' && c <= '9')
		return TRUE;
	return FALSE;
}

int cmd_fill_paragraph(int f, int n)
{
	unicode_t c;
	unicode_t wbuf[NSTRING];
	int wordlen;
	int clength;
	int i;
	int newlength;
	int eopflag;
	int firstflag;
	struct line *eopline;
	int dotflag;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (fill_column == 0) {
		msg_printf("No fill column set");
		return FALSE;
	}
	justflag = FALSE;

	cmd_next_paragraph(FALSE, 1);
	eopline = line_next(curwp->w_dotp);

	cmd_previous_paragraph(FALSE, 1);

	clength = curwp->w_doto;
	if (clength && curwp->w_dotp->l_text[0] == TAB)
		clength = 8;
	wordlen = 0;
	dotflag = FALSE;

	firstflag = TRUE;
	eopflag = FALSE;
	while (!eopflag) {
		int bytes = 1;

		if (curwp->w_dotp == curbp->b_linep)
			break;
		if (curwp->w_doto == line_length(curwp->w_dotp)) {
			c = ' ';
			if (line_next(curwp->w_dotp) == eopline || line_next(curwp->w_dotp) == curbp->b_linep)
				eopflag = TRUE;
		} else
			bytes = char_at_dot(&c);

		if (delete_bytes(bytes, FALSE) != TRUE)
			eopflag = TRUE;

		if (c != ' ' && c != '\t') {
			dotflag = (c == '.');
			if (wordlen < NSTRING - 1)
				wbuf[wordlen++] = c;
		} else if (wordlen) {
			newlength = clength + 1 + wordlen;
			if (newlength <= fill_column) {
				if (!firstflag) {
					insert_char(1, ' ');
					++clength;
				}
				firstflag = FALSE;
			} else {
				insert_newline();
				clength = 0;
			}

			for (i = 0; i < wordlen; i++) {
				insert_char(1, wbuf[i]);
				++clength;
			}
			if (dotflag) {
				insert_char(1, ' ');
				++clength;
			}
			wordlen = 0;
		}
	}

	insert_newline();
	return TRUE;
}

int cmd_justify_paragraph(int f, int n)
{
	unicode_t c;
	unicode_t wbuf[NSTRING];
	int wordlen;
	int clength;
	int i;
	int newlength;
	int eopflag;
	int firstflag;
	struct line *eopline;
	int leftmarg;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (fill_column == 0) {
		msg_printf("No fill column set");
		return FALSE;
	}
	justflag = TRUE;
	leftmarg = curwp->w_doto;
	if (leftmarg + 10 > fill_column) {
		leftmarg = 0;
		msg_printf("Column too narrow");
		return FALSE;
	}

	cmd_next_paragraph(FALSE, 1);
	eopline = line_next(curwp->w_dotp);

	cmd_previous_paragraph(FALSE, 1);

	if (leftmarg < line_length(curwp->w_dotp))
		curwp->w_doto = leftmarg;

	clength = curwp->w_doto;
	if (clength && curwp->w_dotp->l_text[0] == TAB)
		clength = 8;

	wordlen = 0;

	firstflag = TRUE;
	eopflag = FALSE;
	while (!eopflag) {
		int bytes = 1;

		if (curwp->w_dotp == curbp->b_linep)
			break;
		if (curwp->w_doto == line_length(curwp->w_dotp)) {
			c = ' ';
			if (line_next(curwp->w_dotp) == eopline || line_next(curwp->w_dotp) == curbp->b_linep)
				eopflag = TRUE;
		} else
			bytes = char_at_dot(&c);

		if (delete_bytes(bytes, FALSE) != TRUE)
			eopflag = TRUE;

		if (c != ' ' && c != '\t') {
			if (wordlen < NSTRING - 1)
				wbuf[wordlen++] = c;
		} else if (wordlen) {
			newlength = clength + 1 + wordlen;
			if (newlength <= fill_column) {
				if (!firstflag) {
					insert_char(1, ' ');
					++clength;
				}
				firstflag = FALSE;
			} else {
				insert_newline();
				for (i = 0; i < leftmarg; i++)
					insert_char(1, ' ');
				clength = leftmarg;
			}

			for (i = 0; i < wordlen; i++) {
				insert_char(1, wbuf[i]);
				++clength;
			}
			wordlen = 0;
		}
	}

	insert_newline();

	cmd_next_word(FALSE, 1);
	if (line_length(curwp->w_dotp) > leftmarg)
		curwp->w_doto = leftmarg;
	else
		curwp->w_doto = line_length(curwp->w_dotp);

	justflag = FALSE;
	return TRUE;
}

int cmd_kill_paragraph(int f, int n)
{
	int status;

	while (n--) {
		cmd_next_paragraph(FALSE, 1);

		curwp->w_markp = curwp->w_dotp;
		curwp->w_marko = curwp->w_doto;

		cmd_previous_paragraph(FALSE, 1);
		curwp->w_doto = 0;

		if ((status = cmd_kill_region(FALSE, 1)) != TRUE)
			return status;

		delete_bytes(2L, TRUE);
	}
	return TRUE;
}

int cmd_count_words(int f, int n)
{
	struct line *lp;
	int offset;
	long size;
	int ch;
	int wordflag;
	int lastword;
	long nwords;
	long nchars;
	int nlines;
	int avgch;
	int status;
	struct region region;

	if ((status = getregion(&region)) != TRUE)
		return status;
	lp = region.r_linep;
	offset = region.r_offset;
	size = region.r_size;

	lastword = FALSE;
	nchars = 0L;
	nwords = 0L;
	nlines = 0;
	while (size--) {
		if (offset == line_length(lp)) {
			ch = '\n';
			lp = line_next(lp);
			offset = 0;
			++nlines;
		} else {
			ch = lgetc(lp, offset);
			++offset;
		}

		wordflag = ((isletter(ch)) || (ch >= '0' && ch <= '9'));
		if (wordflag == TRUE && lastword == FALSE)
			++nwords;
		lastword = wordflag;
		++nchars;
	}

	if (nwords > 0L)
		avgch = (int)((100L * nchars) / nwords);
	else
		avgch = 0;

	msg_printf("Words %ld Chars %ld Lines %d Avg chars/word %d.%02d",
		nwords, nchars, nlines + 1, avgch / 100, avgch % 100);
	return TRUE;
}
