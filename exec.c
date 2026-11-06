#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"

int cmd_execute_named_command(int f, int n)
{
	fn_t kfunc;

	kfunc = getname(f ? "C-u M-x " : "M-x ");
	if (kfunc == NULL) {
		msg_printf("(No such function)");
		return FALSE;
	}

	return kfunc(f, n);
}

int cmd_execute_command_line(int f, int n)
{
	int status;
	char cmdstr[NSTRING];

	if ((status = ask_string(": ", cmdstr, NSTRING)) != TRUE)
		return status;

	if_level = 0;
	return docmd(cmdstr);
}

int docmd(char *cline)
{
	int f;
	int n;
	fn_t fnc;
	int status;
	int oldcle;
	char *oldestr;
	char tkn[NSTRING];

	if (if_level)
		return TRUE;

	oldestr = command_string;
	command_string = cline;

	f = FALSE;
	n = 1;
	lastflag = thisflag;
	thisflag = 0;

	if ((status = macarg(tkn)) != TRUE) {
		command_string = oldestr;
		return status;
	}

	if (token_type(tkn) != TKCMD) {
		f = TRUE;
		getval(tkn, tkn, sizeof(tkn));
		n = atoi(tkn);

		if ((status = macarg(tkn)) != TRUE) {
			command_string = oldestr;
			return status;
		}
	}

	if ((fnc = fncmatch(tkn)) == NULL) {
		msg_printf("(No such Function)");
		command_string = oldestr;
		return FALSE;
	}

	oldcle = executing_command_line;
	executing_command_line = TRUE;
	status = (*fnc) (f, n);
	command_status = status;
	executing_command_line = oldcle;
	command_string = oldestr;
	return status;
}

char *token(char *src, char *tok, int size)
{
	int quotef;
	char c;

	while (*src == ' ' || *src == '\t')
		++src;

	quotef = FALSE;
	while (*src) {
		if (*src == '~') {
			++src;
			if (*src == 0)
				break;
			switch (*src++) {
			case 'r':
				c = 13;
				break;
			case 'n':
				c = 10;
				break;
			case 't':
				c = 9;
				break;
			case 'b':
				c = 8;
				break;
			case 'f':
				c = 12;
				break;
			default:
				c = *(src - 1);
			}
			if (--size > 0) {
				*tok++ = c;
			}
		} else {
			if (quotef) {
				if (*src == '"')
					break;
			} else {
				if (*src == ' ' || *src == '\t')
					break;
			}

			if (*src == '"')
				quotef = TRUE;

			c = *src++;
			if (--size > 0)
				*tok++ = c;
		}
	}

	if (*src)
		++src;
	*tok = 0;
	return src;
}

int macarg(char *tok)
{
	int savcle;
	int status;

	savcle = executing_command_line;
	executing_command_line = TRUE;
	status = nextarg("", tok, NSTRING, char_to_keycode('\n'));
	executing_command_line = savcle;
	return status;
}

int nextarg(char *prompt, char *buffer, int size, int terminator)
{
	if (executing_command_line == FALSE)
		return getstring(prompt, buffer, size, terminator);

	command_string = token(command_string, buffer, size);

	getval(buffer, buffer, size);
	return TRUE;
}

int cmd_store_macro(int f, int n)
{
	struct buffer *bp;
	char bname[NBUFN];

	if (f == FALSE) {
		msg_printf("No macro specified");
		return FALSE;
	}

	if (n < 1 || n > 40) {
		msg_printf("Macro number out of range");
		return FALSE;
	}

	strcpy(bname, "*Macro xx*");
	bname[7] = '0' + (n / 10);
	bname[8] = '0' + (n % 10);

	if ((bp = find_buffer(bname, TRUE, BFINVS)) == NULL) {
		msg_printf("Can not create macro");
		return FALSE;
	}

	clear_buffer(bp);

	storing_macro = TRUE;
	store_buffer = bp;
	return TRUE;
}

int cmd_store_procedure(int f, int n)
{
	struct buffer *bp;
	int status;
	char bname[NBUFN];

	if (f == TRUE)
		return cmd_store_macro(f, n);

	if ((status = ask_string("Procedure name: ", &bname[1], NBUFN - 2)) != TRUE)
		return status;

	bname[0] = '*';
	strcat(bname, "*");

	if ((bp = find_buffer(bname, TRUE, BFINVS)) == NULL) {
		msg_printf("Can not create macro");
		return FALSE;
	}

	clear_buffer(bp);

	storing_macro = TRUE;
	store_buffer = bp;
	return TRUE;
}

int cmd_execute_procedure(int f, int n)
{
	struct buffer *bp;
	int status;
	char bufn[NBUFN + 2];

	if ((status = ask_string("Execute procedure: ", &bufn[1], NBUFN)) != TRUE)
		return status;

	bufn[0] = '*';
	strcat(bufn, "*");

	if ((bp = find_buffer(bufn, FALSE, 0)) == NULL) {
		msg_printf("No such procedure");
		return FALSE;
	}

	while (n-- > 0)
		if ((status = dobuf(bp)) != TRUE)
			return status;
	return TRUE;
}

int cmd_execute_buffer(int f, int n)
{
	struct buffer *bp;
	int status;
	char bufn[NSTRING];

	if ((status = ask_string("Execute buffer: ", bufn, NBUFN)) != TRUE)
		return status;

	if ((bp = find_buffer(bufn, FALSE, 0)) == NULL) {
		msg_printf("No such buffer");
		return FALSE;
	}

	while (n-- > 0)
		if ((status = dobuf(bp)) != TRUE)
			return status;
	return TRUE;
}

int dobuf(struct buffer *bp)
{
	int status;
	struct line *lp;
	struct line *hlp;
	struct line *glp;
	struct line *mp;
	int dirnum;
	int linlen;
	int i;
	int force;
	struct window *wp;
	struct while_block *whlist;
	struct while_block *scanner;
	struct while_block *whtemp;
	char *einit;
	char *eline;
	char tkn[NSTRING];

	if_level = 0;
	whlist = NULL;
	scanner = NULL;

	hlp = bp->b_linep;
	lp = hlp->l_fp;
	while (lp != hlp) {
		eline = lp->l_text;
		i = lp->l_used;

		while (i-- > 0 && (*eline == ' ' || *eline == '\t'))
			++eline;

		if (i <= 0)
			goto nxtscan;

		if (eline[0] == '!' && eline[1] == 'w' && eline[2] == 'h') {
			whtemp = (struct while_block *)
			    malloc(sizeof(struct while_block));
			if (whtemp == NULL) {
 noram:			msg_printf("%%Out of memory during while scan");
 failexit:			freewhile(scanner);
				freewhile(whlist);
				return FALSE;
			}
			whtemp->w_begin = lp;
			whtemp->w_type = BTWHILE;
			whtemp->w_next = scanner;
			scanner = whtemp;
		}

		if (eline[0] == '!' && eline[1] == 'b' && eline[2] == 'r') {
			if (scanner == NULL) {
				msg_printf("%%!BREAK outside of any !WHILE loop");
				goto failexit;
			}
			whtemp = (struct while_block *)
			    malloc(sizeof(struct while_block));
			if (whtemp == NULL)
				goto noram;
			whtemp->w_begin = lp;
			whtemp->w_type = BTBREAK;
			whtemp->w_next = scanner;
			scanner = whtemp;
		}

		if (eline[0] == '!' && strncmp(&eline[1], "endw", 4) == 0) {
			if (scanner == NULL) {
				msg_printf
				    ("%%!ENDWHILE with no preceding !WHILE in '%s'", bp->b_bname);
				goto failexit;
			}

			do {
				scanner->w_end = lp;
				whtemp = whlist;
				whlist = scanner;
				scanner = scanner->w_next;
				whlist->w_next = whtemp;
			} while (whlist->w_type == BTBREAK);
		}

 nxtscan:
		lp = lp->l_fp;
	}

	if (scanner != NULL) {
		msg_printf("%%!WHILE with no matching !ENDWHILE in '%s'", bp->b_bname);
		goto failexit;
	}

	thisflag = lastflag;

	hlp = bp->b_linep;
	lp = hlp->l_fp;
	while (lp != hlp) {
		linlen = lp->l_used;
		if ((einit = eline = malloc(linlen + 1)) == NULL) {
			msg_printf("%%Out of Memory during macro execution");
			freewhile(whlist);
			return FALSE;
		}
		strncpy(eline, lp->l_text, linlen);
		eline[linlen] = 0;

		while (*eline == ' ' || *eline == '\t')
			++eline;

		if (*eline == ';' || *eline == 0)
			goto onward;

		dirnum = -1;
		if (*eline == '!') {
			++eline;
			for (dirnum = 0; dirnum < NUMDIRS; dirnum++)
				if (strncmp(eline, directive_names[dirnum], strlen(directive_names[dirnum])) == 0)
					break;

			if (dirnum == NUMDIRS) {
				msg_printf("%%Unknown Directive");
				freewhile(whlist);
				return FALSE;
			}

			if (dirnum == DENDM) {
				storing_macro = FALSE;
				store_buffer = NULL;
				goto onward;
			}

			--eline;
		}

		if (storing_macro) {
			linlen = strlen(eline);
			if ((mp = line_alloc(linlen)) == NULL) {
				msg_printf("Out of memory while storing macro");
				return FALSE;
			}

			for (i = 0; i < linlen; ++i)
				lputc(mp, i, eline[i]);

			store_buffer->b_linep->l_bp->l_fp = mp;
			mp->l_bp = store_buffer->b_linep->l_bp;
			store_buffer->b_linep->l_bp = mp;
			mp->l_fp = store_buffer->b_linep;
			goto onward;
		}

		force = FALSE;

		if (*eline == '*')
			goto onward;

		if (dirnum != -1) {
			while (*eline && *eline != ' ' && *eline != '\t')
				++eline;
			command_string = eline;

			switch (dirnum) {
			case DIF:

				if (if_level == 0) {
					if (macarg(tkn) != TRUE)
						goto eexec;
					if (truth_value(tkn) == FALSE)
						++if_level;
				} else
					++if_level;
				goto onward;

			case DWHILE:

				if (if_level == 0) {
					if (macarg(tkn) != TRUE)
						goto eexec;
					if (truth_value(tkn) == TRUE)
						goto onward;
				}

			case DBREAK:
				if (dirnum == DBREAK && if_level)
					goto onward;

				whtemp = whlist;
				while (whtemp) {
					if (whtemp->w_begin == lp)
						break;
					whtemp = whtemp->w_next;
				}

				if (whtemp == NULL) {
					msg_printf("%%Internal While loop error");
					freewhile(whlist);
					return FALSE;
				}

				lp = whtemp->w_end;
				goto onward;

			case DELSE:
				if (if_level == 1)
					--if_level;
				else if (if_level == 0)
					++if_level;
				goto onward;

			case DENDIF:
				if (if_level)
					--if_level;
				goto onward;

			case DGOTO:

				if (if_level == 0) {
					eline = token(eline, goto_label, NPAT);
					linlen = strlen(goto_label);
					glp = hlp->l_fp;
					while (glp != hlp) {
						if (*glp->l_text == '*' &&
						    (strncmp
						     (&glp->l_text[1], goto_label, linlen) == 0)) {
							lp = glp;
							goto onward;
						}
						glp = glp->l_fp;
					}
					msg_printf("%%No such label");
					freewhile(whlist);
					return FALSE;
				}
				goto onward;

			case DRETURN:
				if (if_level == 0)
					goto eexec;
				goto onward;

			case DENDWHILE:
				if (if_level) {
					--if_level;
					goto onward;
				} else {
					whtemp = whlist;
					while (whtemp) {
						if (whtemp->w_type ==
						    BTWHILE && whtemp->w_end == lp)
							break;
						whtemp = whtemp->w_next;
					}

					if (whtemp == NULL) {
						msg_printf("%%Internal While loop error");
						freewhile(whlist);
						return FALSE;
					}

					lp = whtemp->w_begin->l_bp;
					goto onward;
				}

			case DFORCE:
				force = TRUE;
			}
		}

		status = docmd(eline);
		if (force)
			status = TRUE;

		if (status != TRUE) {
			wp = window_head;
			while (wp != NULL) {
				if (wp->w_bufp == bp) {
					wp->w_dotp = lp;
					wp->w_doto = 0;
					wp->w_flag |= WFHARD;
				}
				wp = wp->w_wndp;
			}

			bp->b_dotp = lp;
			bp->b_doto = 0;
			free(einit);
			if_level = 0;
			freewhile(whlist);
			return status;
		}

 onward:
		free(einit);
		lp = lp->l_fp;
	}

 eexec:
	if_level = 0;
	freewhile(whlist);
	return TRUE;
}

void freewhile(struct while_block *wp)
{
	if (wp == NULL)
		return;
	if (wp->w_next)
		freewhile(wp->w_next);
	free(wp);
}

int cmd_execute_file(int f, int n)
{
	int status;
	char fname[NSTRING];
	char *fspec;

	if ((status = ask_string("File to execute: ", fname, NSTRING - 1)) != TRUE)
		return status;

	fspec = lookup_file(fname, FALSE);

	if (fspec == NULL)
		return FALSE;

	while (n-- > 0)
		if ((status = dofile(fspec)) != TRUE)
			return status;

	return TRUE;
}

int dofile(char *fname)
{
	struct buffer *bp;
	struct buffer *cb;
	int status;
	char bname[NBUFN];

	makename(bname, fname);
	unique_buffer_name(bname);
	if ((bp = find_buffer(bname, TRUE, 0)) == NULL)
		return FALSE;

	bp->b_mode = MDVIEW;
	cb = curbp;
	curbp = bp;

	if ((status = readin(fname, FALSE)) != TRUE) {
		curbp = cb;
		return status;
	}

	curbp = cb;
	if ((status = dobuf(bp)) != TRUE)
		return status;

	if (bp->b_nwnd == 0)
		destroy_buffer(bp);
	return TRUE;
}

int execute_numbered_macro(int f, int n, int number)
{
	struct buffer *bp;
	int status;
	static char bufname[] = "*Macro xx*";

	bufname[7] = '0' + (number / 10);
	bufname[8] = '0' + (number % 10);

	if ((bp = find_buffer(bufname, FALSE, 0)) == NULL) {
		msg_printf("Macro not defined");
		return FALSE;
	}

	while (n-- > 0)
		if ((status = dobuf(bp)) != TRUE)
			return status;
	return TRUE;
}

int cmd_execute_macro_1(int f, int n)
{
	return execute_numbered_macro(f, n, 1);
}

int cmd_execute_macro_2(int f, int n)
{
	return execute_numbered_macro(f, n, 2);
}

int cmd_execute_macro_3(int f, int n)
{
	return execute_numbered_macro(f, n, 3);
}

int cmd_execute_macro_4(int f, int n)
{
	return execute_numbered_macro(f, n, 4);
}

int cmd_execute_macro_5(int f, int n)
{
	return execute_numbered_macro(f, n, 5);
}

int cmd_execute_macro_6(int f, int n)
{
	return execute_numbered_macro(f, n, 6);
}

int cmd_execute_macro_7(int f, int n)
{
	return execute_numbered_macro(f, n, 7);
}

int cmd_execute_macro_8(int f, int n)
{
	return execute_numbered_macro(f, n, 8);
}

int cmd_execute_macro_9(int f, int n)
{
	return execute_numbered_macro(f, n, 9);
}

int cmd_execute_macro_10(int f, int n)
{
	return execute_numbered_macro(f, n, 10);
}

int cmd_execute_macro_11(int f, int n)
{
	return execute_numbered_macro(f, n, 11);
}

int cmd_execute_macro_12(int f, int n)
{
	return execute_numbered_macro(f, n, 12);
}

int cmd_execute_macro_13(int f, int n)
{
	return execute_numbered_macro(f, n, 13);
}

int cmd_execute_macro_14(int f, int n)
{
	return execute_numbered_macro(f, n, 14);
}

int cmd_execute_macro_15(int f, int n)
{
	return execute_numbered_macro(f, n, 15);
}

int cmd_execute_macro_16(int f, int n)
{
	return execute_numbered_macro(f, n, 16);
}

int cmd_execute_macro_17(int f, int n)
{
	return execute_numbered_macro(f, n, 17);
}

int cmd_execute_macro_18(int f, int n)
{
	return execute_numbered_macro(f, n, 18);
}

int cmd_execute_macro_19(int f, int n)
{
	return execute_numbered_macro(f, n, 19);
}

int cmd_execute_macro_20(int f, int n)
{
	return execute_numbered_macro(f, n, 20);
}

int cmd_execute_macro_21(int f, int n)
{
	return execute_numbered_macro(f, n, 21);
}

int cmd_execute_macro_22(int f, int n)
{
	return execute_numbered_macro(f, n, 22);
}

int cmd_execute_macro_23(int f, int n)
{
	return execute_numbered_macro(f, n, 23);
}

int cmd_execute_macro_24(int f, int n)
{
	return execute_numbered_macro(f, n, 24);
}

int cmd_execute_macro_25(int f, int n)
{
	return execute_numbered_macro(f, n, 25);
}

int cmd_execute_macro_26(int f, int n)
{
	return execute_numbered_macro(f, n, 26);
}

int cmd_execute_macro_27(int f, int n)
{
	return execute_numbered_macro(f, n, 27);
}

int cmd_execute_macro_28(int f, int n)
{
	return execute_numbered_macro(f, n, 28);
}

int cmd_execute_macro_29(int f, int n)
{
	return execute_numbered_macro(f, n, 29);
}

int cmd_execute_macro_30(int f, int n)
{
	return execute_numbered_macro(f, n, 30);
}

int cmd_execute_macro_31(int f, int n)
{
	return execute_numbered_macro(f, n, 31);
}

int cmd_execute_macro_32(int f, int n)
{
	return execute_numbered_macro(f, n, 32);
}

int cmd_execute_macro_33(int f, int n)
{
	return execute_numbered_macro(f, n, 33);
}

int cmd_execute_macro_34(int f, int n)
{
	return execute_numbered_macro(f, n, 34);
}

int cmd_execute_macro_35(int f, int n)
{
	return execute_numbered_macro(f, n, 35);
}

int cmd_execute_macro_36(int f, int n)
{
	return execute_numbered_macro(f, n, 36);
}

int cmd_execute_macro_37(int f, int n)
{
	return execute_numbered_macro(f, n, 37);
}

int cmd_execute_macro_38(int f, int n)
{
	return execute_numbered_macro(f, n, 38);
}

int cmd_execute_macro_39(int f, int n)
{
	return execute_numbered_macro(f, n, 39);
}

int cmd_execute_macro_40(int f, int n)
{
	return execute_numbered_macro(f, n, 40);
}
