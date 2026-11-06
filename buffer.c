#include        <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "util.h"

struct buffer *last_buffer = NULL;

static struct buffer *other_buffer(struct buffer *notthis)
{
	struct buffer *bp;

	if (last_buffer && last_buffer != notthis && !(last_buffer->b_flag & BFINVS)) {
		for (bp = buffer_head; bp != NULL; bp = bp->b_bufp)
			if (bp == last_buffer)
				return bp;
	}
	for (bp = buffer_head; bp != NULL; bp = bp->b_bufp)
		if (bp != notthis && !(bp->b_flag & BFINVS))
			return bp;
	return NULL;
}

int cmd_select_buffer(int f, int n)
{
	struct buffer *bp, *def;
	int s;
	char bufn[NBUFN];
	char prompt[NBUFN + 48];

	def = other_buffer(curbp);
	if (def)
		snprintf(prompt, sizeof(prompt), "Switch to buffer (default %s): ", def->b_bname);
	else
		snprintf(prompt, sizeof(prompt), "Switch to buffer: ");
	complete_kind = CMPL_BUFFER;
	s = ask_string(prompt, bufn, NBUFN);
	if (s == ABORT)
		return s;
	if (s == FALSE) {
		if (def == NULL)
			return FALSE;
		return swbuffer(def);
	}
	if ((bp = find_buffer(bufn, TRUE, 0)) == NULL)
		return FALSE;
	return swbuffer(bp);
}

int cmd_previous_buffer(int f, int n)
{
	struct buffer *bp, *prev = NULL, *cand;

	if (f == FALSE)
		n = 1;
	while (n-- > 0) {
		prev = NULL;
		for (cand = buffer_head; cand != NULL; cand = cand->b_bufp) {
			if (cand == curbp)
				break;
			if (!(cand->b_flag & BFINVS))
				prev = cand;
		}
		if (prev == NULL) {
			for (bp = curbp->b_bufp; bp != NULL; bp = bp->b_bufp)
				if (!(bp->b_flag & BFINVS))
					prev = bp;
		}
		if (prev == NULL || prev == curbp)
			return FALSE;
		swbuffer(prev);
	}
	return TRUE;
}

int cmd_next_buffer(int f, int n)
{
	struct buffer *bp = NULL;
	struct buffer *bbp;

	if (f == FALSE)
		n = 1;
	if (n < 1)
		return FALSE;

	bbp = curbp;
	while (n-- > 0) {
		bp = bbp->b_bufp;

		while (bp == NULL || bp->b_flag & BFINVS) {
			if (bp == NULL)
				bp = buffer_head;
			else
				bp = bp->b_bufp;

			if (bp == bbp)
				return FALSE;
		}

		bbp = bp;
	}

	return swbuffer(bp);
}

int swbuffer(struct buffer *bp)
{
	if (bp != curbp && !(curbp->b_flag & BFINVS))
		last_buffer = curbp;
	if (--curbp->b_nwnd == 0) {
		curbp->b_dotp = curwp->w_dotp;
		curbp->b_doto = curwp->w_doto;
		curbp->b_markp = curwp->w_markp;
		curbp->b_marko = curwp->w_marko;
	}
	curbp = bp;
	if (curbp->b_active != TRUE) {
		curbp->b_mode |= global_mode;
		readin(curbp->b_fname, TRUE);
		curbp->b_dotp = line_next(curbp->b_linep);
		curbp->b_doto = 0;
		curbp->b_active = TRUE;
	}
	curwp->w_bufp = bp;
	curwp->w_linep = bp->b_linep;
	curwp->w_flag |= WFMODE | WFFORCE | WFHARD;
	if (bp->b_nwnd++ == 0) {
		curwp->w_dotp = bp->b_dotp;
		curwp->w_doto = bp->b_doto;
		curwp->w_markp = bp->b_markp;
		curwp->w_marko = bp->b_marko;
	} else {
		struct window *wp;

		wp = window_head;
		while (wp != NULL) {
			if (wp != curwp && wp->w_bufp == bp) {
				curwp->w_dotp = wp->w_dotp;
				curwp->w_doto = wp->w_doto;
				curwp->w_markp = wp->w_markp;
				curwp->w_marko = wp->w_marko;
				break;
			}
			wp = wp->w_wndp;
		}
	}
	shown_buffer_changed();
	return TRUE;
}

void shown_buffer_changed(void)
{
	execute(HOOK_CHANGE, FALSE, 1);
}

int cmd_delete_buffer(int f, int n)
{
	struct buffer *bp, *alt;
	struct window *wp;
	int s;
	char bufn[NBUFN];
	char prompt[NBUFN + 40];

	snprintf(prompt, sizeof(prompt), "Kill buffer (default %s): ", curbp->b_bname);
	complete_kind = CMPL_BUFFER;
	s = ask_string(prompt, bufn, NBUFN);
	if (s == ABORT)
		return s;
	if (s == FALSE)
		mystrscpy(bufn, curbp->b_bname, NBUFN);
	if ((bp = find_buffer(bufn, FALSE, 0)) == NULL) {
		msg_printf("No such buffer: %s", bufn);
		return FALSE;
	}
	if ((bp->b_flag & BFCHG) && !(bp->b_flag & BFINVS) && bp->b_fname[0]) {
		snprintf(prompt, sizeof(prompt), "Buffer %s modified; kill anyway", bp->b_bname);
		if ((s = ask_yesno(prompt)) != TRUE)
			return s;
		bp->b_flag &= ~BFCHG;
	}
	if (bp->b_nwnd != 0) {
		alt = other_buffer(bp);
		if (alt == NULL) {
			alt = find_buffer("*scratch*", TRUE, 0);
			if (alt == NULL || alt == bp)
				return FALSE;
		}
		{
			struct window *save = curwp;
			for (wp = window_head; wp != NULL; wp = wp->w_wndp) {
				if (wp->w_bufp == bp) {
					curwp = wp;
					curbp = bp;
					swbuffer(alt);
				}
			}
			curwp = save;
			curbp = curwp->w_bufp;
		}
	}
	if (last_buffer == bp)
		last_buffer = NULL;
	bp->b_flag &= ~BFCHG;
	s = destroy_buffer(bp);
	if (s == TRUE)
		msg_printf("(Killed buffer %s)", bufn);
	return s;
}

int destroy_buffer(struct buffer *bp)
{
	struct buffer *bp1;
	struct buffer *bp2;
	int s;

	if (bp->b_nwnd != 0) {
		msg_printf("Buffer is being displayed");
		return FALSE;
	}
	if ((s = clear_buffer(bp)) != TRUE)
		return s;
	undo_free(bp);
	if (last_buffer == bp)
		last_buffer = NULL;
	free((char *)bp->b_linep);
	line_gen++;
	bp1 = NULL;
	bp2 = buffer_head;
	while (bp2 != bp) {
		bp1 = bp2;
		bp2 = bp2->b_bufp;
	}
	bp2 = bp2->b_bufp;
	if (bp1 == NULL)
		buffer_head = bp2;
	else
		bp1->b_bufp = bp2;
	free((char *)bp);
	return TRUE;
}

int cmd_name_buffer(int f, int n)
{
	struct buffer *bp;
	char bufn[NBUFN];

 ask:	if (ask_string("Change buffer name to: ", bufn, NBUFN) != TRUE)
		return FALSE;

	bp = buffer_head;
	while (bp != NULL) {
		if (bp != curbp) {
			if (strcmp(bufn, bp->b_bname) == 0)
				goto ask;
		}
		bp = bp->b_bufp;
	}

	strcpy(curbp->b_bname, bufn);
	curwp->w_flag |= WFMODE;
	msg_erase();
	return TRUE;
}

void ltoa(char *buf, int width, long num)
{
	buf[width] = 0;
	while (num >= 10) {
		buf[--width] = (int)(num % 10L) + '0';
		num /= 10L;
	}
	buf[--width] = (int)num + '0';
	while (width != 0)
		buf[--width] = ' ';
}

int addline(struct buffer *bp, char *text)
{
	struct line *lp;
	int i;
	int ntext;

	ntext = strlen(text);
	if ((lp = line_alloc(ntext)) == NULL)
		return FALSE;
	for (i = 0; i < ntext; ++i)
		lputc(lp, i, text[i]);
	bp->b_linep->l_bp->l_fp = lp;
	lp->l_bp = bp->b_linep->l_bp;
	bp->b_linep->l_bp = lp;
	lp->l_fp = bp->b_linep;
	if (bp->b_dotp == bp->b_linep)
		bp->b_dotp = lp;
	return TRUE;
}

int makelist(int iflag)
{
	char *cp1;
	char *cp2;
	int c;
	struct buffer *bp;
	struct line *lp;
	int s;
	int i;
	long nbytes;
	char b[7 + 1];
	char line[MAXCOL];

	list_buffer->b_flag &= ~BFCHG;
	if ((s = clear_buffer(list_buffer)) != TRUE)
		return s;
	strcpy(list_buffer->b_fname, "");
	if (addline(list_buffer, "ACT MODES        Size Buffer        File") == FALSE
	    || addline(list_buffer, "--- -----        ---- ------        ----") == FALSE)
		return FALSE;
	bp = buffer_head;

	cp1 = &line[0];
	*cp1++ = ' ';
	*cp1++ = ' ';
	*cp1++ = ' ';
	*cp1++ = ' ';

	for (i = 0; i < NUMMODES; i++)
		if (global_mode & (1 << i))
			*cp1++ = modecode[i];
		else
			*cp1++ = '.';
	strcpy(cp1, "         Global Modes");
	if (addline(list_buffer, line) == FALSE)
		return FALSE;

	while (bp != NULL) {
		if (((bp->b_flag & BFINVS) != 0) && (iflag != TRUE)) {
			bp = bp->b_bufp;
			continue;
		}
		cp1 = &line[0];

		if (bp->b_active == TRUE)
			*cp1++ = '@';
		else
			*cp1++ = ' ';

		if ((bp->b_flag & BFCHG) != 0)
			*cp1++ = '*';
		else
			*cp1++ = ' ';

		if ((bp->b_flag & BFTRUNC) != 0)
			*cp1++ = '#';
		else
			*cp1++ = ' ';

		*cp1++ = ' ';

		for (i = 0; i < NUMMODES; i++) {
			if (bp->b_mode & (1 << i))
				*cp1++ = modecode[i];
			else
				*cp1++ = '.';
		}
		*cp1++ = ' ';
		nbytes = 0L;
		lp = line_next(bp->b_linep);
		while (lp != bp->b_linep) {
			nbytes += (long)line_length(lp) + 1L;
			lp = line_next(lp);
		}
		ltoa(b, 7, nbytes);
		cp2 = &b[0];
		while ((c = *cp2++) != 0)
			*cp1++ = c;
		*cp1++ = ' ';
		cp2 = &bp->b_bname[0];
		while ((c = *cp2++) != 0)
			*cp1++ = c;
		cp2 = &bp->b_fname[0];
		if (*cp2 != 0) {
			while (cp1 < &line[3 + 1 + 5 + 1 + 6 + 4 + NBUFN])
				*cp1++ = ' ';
			while ((c = *cp2++) != 0) {
				if (cp1 < &line[MAXCOL - 1])
					*cp1++ = c;
			}
		}
		*cp1 = 0;
		if (addline(list_buffer, line) == FALSE)
			return FALSE;
		bp = bp->b_bufp;
	}
	return TRUE;
}

int cmd_list_buffers(int f, int n)
{
	int s;

	if ((s = makelist(f)) != TRUE)
		return s;

	if (curwp->w_bufp != list_buffer && (s = swbuffer(list_buffer)) != TRUE)
		return s;

	curwp->w_linep = line_next(list_buffer->b_linep);
	curwp->w_dotp = line_next(list_buffer->b_linep);
	curwp->w_doto = 0;
	curwp->w_markp = NULL;
	curwp->w_marko = 0;
	curwp->w_flag |= WFMODE | WFHARD;
	return TRUE;
}

int any_changed_buffers(void)
{
	struct buffer *bp;

	bp = buffer_head;
	while (bp != NULL) {
		if ((bp->b_flag & BFINVS) == 0 && (bp->b_flag & BFCHG) != 0)
			return TRUE;
		bp = bp->b_bufp;
	}
	return FALSE;
}

struct buffer *find_buffer(char *bname, int cflag, int bflag)
{
	struct buffer *bp;
	struct buffer *sb;
	struct line *lp;

	bp = buffer_head;
	while (bp != NULL) {
		if (strcmp(bname, bp->b_bname) == 0)
			return bp;
		bp = bp->b_bufp;
	}
	if (cflag != FALSE) {
		if ((bp = (struct buffer *)malloc(sizeof(struct buffer))) == NULL)
			return NULL;
		if ((lp = line_alloc(0)) == NULL) {
			free((char *)bp);
			return NULL;
		}

		if (buffer_head == NULL || strcmp(buffer_head->b_bname, bname) > 0) {
			bp->b_bufp = buffer_head;
			buffer_head = bp;
		} else {
			sb = buffer_head;
			while (sb->b_bufp != NULL) {
				if (strcmp(sb->b_bufp->b_bname, bname) > 0)
					break;
				sb = sb->b_bufp;
			}

			bp->b_bufp = sb->b_bufp;
			sb->b_bufp = bp;
		}

		bp->b_active = TRUE;
		bp->b_dotp = lp;
		bp->b_doto = 0;
		bp->b_markp = NULL;
		bp->b_marko = 0;
		bp->b_flag = bflag;
		bp->b_mode = global_mode;
		bp->b_nwnd = 0;
		bp->b_linep = lp;
		bp->b_fstate.fs_what = FSTATE_UNKNOWN;
		bp->b_undo = NULL;
		bp->b_indent = default_indent;
		bp->b_lang = 0;
		strcpy(bp->b_fname, "");
		strcpy(bp->b_bname, bname);
		lp->l_fp = lp;
		lp->l_bp = lp;
	}
	return bp;
}

int clear_buffer(struct buffer *bp)
{
	struct line *lp;
	int s;

	if ((bp->b_flag & BFINVS) == 0
	    && (bp->b_flag & BFCHG) != 0
	    && (s = ask_yesno("Discard changes")) != TRUE)
		return s;
	bp->b_flag &= ~BFCHG;
	undo_reset(bp);
	{
		struct line *hdr = bp->b_linep, *nx;
		struct window *wp;
		lp = line_next(hdr);
		while (lp != hdr) {
			nx = line_next(lp);
			free(lp);
			lp = nx;
		}
		hdr->l_fp = hdr->l_bp = hdr;
		line_gen++;
		for (wp = window_head; wp != NULL; wp = wp->w_wndp)
			if (wp->w_bufp == bp) {
				wp->w_linep = wp->w_dotp = hdr;
				wp->w_doto = 0;
				wp->w_markp = NULL;
				wp->w_marko = 0;
			}
	}
	bp->b_dotp = bp->b_linep;
	bp->b_doto = 0;
	bp->b_markp = NULL;
	bp->b_marko = 0;
	return TRUE;
}

int cmd_unmark_buffer(int f, int n)
{
	curbp->b_flag &= ~BFCHG;
	curwp->w_flag |= WFMODE;
	return TRUE;
}
