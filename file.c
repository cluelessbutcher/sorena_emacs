#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "util.h"

#define	MAXNLINE 10000000

static void record_fstate(struct buffer *bp, char *fn)
{
	struct stat st;

	if (stat(fn, &st) < 0) {
		bp->b_fstate.fs_what = errno == ENOENT ? FSTATE_ABSENT : FSTATE_UNKNOWN;
		return;
	}

	bp->b_fstate.fs_what = FSTATE_PRESENT;
	bp->b_fstate.fs_dev = st.st_dev;
	bp->b_fstate.fs_ino = st.st_ino;
	bp->b_fstate.fs_size = st.st_size;
	bp->b_fstate.fs_mtim = st.st_mtim;
}

int file_changed(struct buffer *bp, char *fn)
{
	struct stat st;
	int there = stat(fn, &st) == 0;

	switch (bp->b_fstate.fs_what) {
	case FSTATE_ABSENT:
		return there;
	case FSTATE_PRESENT:
		if (!there)
			return TRUE;
		return st.st_dev != bp->b_fstate.fs_dev ||
		       st.st_ino != bp->b_fstate.fs_ino ||
		       st.st_size != bp->b_fstate.fs_size ||
		       st.st_mtim.tv_sec != bp->b_fstate.fs_mtim.tv_sec ||
		       st.st_mtim.tv_nsec != bp->b_fstate.fs_mtim.tv_nsec;
	default:
		return FALSE;
	}
}

static char prefill_buf[NFILEN];

static void file_prompt_setup(int withname)
{
	char *slash;

	complete_kind = CMPL_FILE;
	prefill_buf[0] = 0;
	if (curbp->b_fname[0]) {
		mystrscpy(prefill_buf, curbp->b_fname, sizeof(prefill_buf));
		if (!withname) {
			slash = strrchr(prefill_buf, '/');
			if (slash)
				slash[1] = 0;
			else
				prefill_buf[0] = 0;
		}
	}
	prefill_text = prefill_buf[0] ? prefill_buf : NULL;
}

int cmd_read_file(int f, int n)
{
	int s;
	char fname[NFILEN];

	if (restflag)
		return restricted_error();
	file_prompt_setup(TRUE);
	if ((s = ask_string("Read file: ", fname, NFILEN)) != TRUE)
		return s;
	return readin(fname, TRUE);
}

int cmd_find_alternate_file(int f, int n)
{
	int s;
	char fname[NFILEN];
	char bname[NBUFN];

	if (restflag)
		return restricted_error();
	file_prompt_setup(TRUE);
	if ((s = ask_string("Find alternate file: ", fname, NFILEN)) != TRUE)
		return s;
	if ((s = readin(fname, TRUE)) == ABORT)
		return s;
	makename(bname, fname);
	if (strcmp(bname, curbp->b_bname) != 0 && find_buffer(bname, FALSE, 0) == NULL)
		mystrscpy(curbp->b_bname, bname, NBUFN);
	update_modeline();
	return s;
}

int cmd_insert_file(int f, int n)
{
	int s;
	char fname[NFILEN];

	if (restflag)
		return restricted_error();
	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	file_prompt_setup(FALSE);
	if ((s = ask_string("Insert file: ", fname, NFILEN)) != TRUE)
		return s;
	if ((s = insert_file(fname)) != TRUE)
		return s;
	return cmd_redraw_display(TRUE, -1);
}

int cmd_find_file(int f, int n)
{
	char fname[NFILEN];
	int s;

	if (restflag)
		return restricted_error();
	file_prompt_setup(FALSE);
	if ((s = ask_string("Find file: ", fname, NFILEN)) != TRUE)
		return s;
	return getfile(fname, TRUE);
}

int cmd_view_file(int f, int n)
{
	char fname[NFILEN];
	int s;

	if (restflag)
		return restricted_error();
	file_prompt_setup(FALSE);
	if ((s = ask_string("Find file read-only: ", fname, NFILEN)) != TRUE)
		return s;
	s = getfile(fname, FALSE);
	if (s) {
		struct window *wp;

		curwp->w_bufp->b_mode |= MDVIEW;

		wp = window_head;
		while (wp != NULL) {
			wp->w_flag |= WFMODE;
			wp = wp->w_wndp;
		}
	}
	return s;
}

int getfile(char *fname, int lockfl)
{
	struct buffer *bp;
	struct line *lp;
	int i;
	int s;
	char bname[NBUFN];

	for (bp = buffer_head; bp != NULL; bp = bp->b_bufp) {
		if ((bp->b_flag & BFINVS) == 0 && strcmp(bp->b_fname, fname) == 0) {
			swbuffer(bp);
			lp = curwp->w_dotp;
			i = curwp->w_ntrows / 2;
			while (i-- && line_prev(lp) != curbp->b_linep)
				lp = line_prev(lp);
			curwp->w_linep = lp;
			curwp->w_flag |= WFMODE | WFHARD;
			shown_buffer_changed();
			msg_printf("(Old buffer)");
			return TRUE;
		}
	}
	makename(bname, fname);
	while ((bp = find_buffer(bname, FALSE, 0)) != NULL) {
		s = ask_string("Buffer name: ", bname, NBUFN);
		if (s == ABORT)
			return s;
		if (s == FALSE) {
			makename(bname, fname);
			break;
		}
	}
	if (bp == NULL && (bp = find_buffer(bname, TRUE, 0)) == NULL) {
		msg_printf("Cannot create buffer");
		return FALSE;
	}
	if (--curbp->b_nwnd == 0) {
		curbp->b_dotp = curwp->w_dotp;
		curbp->b_doto = curwp->w_doto;
		curbp->b_markp = curwp->w_markp;
		curbp->b_marko = curwp->w_marko;
	}
	curbp = bp;
	curwp->w_bufp = bp;
	curbp->b_nwnd++;
	s = readin(fname, lockfl);
	shown_buffer_changed();
	return s;
}

int readin(char *fname, int lockfl)
{
	struct line *lp1;
	struct line *lp2;
	int i;
	struct window *wp;
	struct buffer *bp;
	int s;
	int nbytes;
	int nline;
	char mesg[NSTRING];

	if (lockfl && lockchk(fname) == ABORT) {
		s = FIOFNF;
		bp = curbp;
		strcpy(bp->b_fname, "");
		goto out;
	}
	bp = curbp;
	if ((s = clear_buffer(bp)) != TRUE)
		return s;
	bp->b_flag &= ~(BFINVS | BFCHG);
	mystrscpy(bp->b_fname, fname, NFILEN);

	execute(HOOK_READ, FALSE, 1);

	if ((s = file_open_read(fname)) == FIOERR)
		goto out;

	if (s == FIOFNF) {
		msg_printf("(New file)");
		goto out;
	}

	nline = 0;
	while ((s = file_get_line()) == FIOSUC) {
		nbytes = file_line_len;
		if ((lp1 = line_alloc(nbytes)) == NULL) {
			s = FIOMEM;
			break;
		}
		if (nline > MAXNLINE) {
			s = FIOMEM;
			break;
		}
		lp2 = line_prev(curbp->b_linep);
		lp2->l_fp = lp1;
		lp1->l_fp = curbp->b_linep;
		lp1->l_bp = lp2;
		curbp->b_linep->l_bp = lp1;
		memcpy(lp1->l_text, file_line, nbytes);
		++nline;
	}
	file_close();
	(void)i;
	strcpy(mesg, "(");
	if (s == FIOERR) {
		strcat(mesg, "I/O ERROR, ");
		curbp->b_flag |= BFTRUNC;
	}
	if (s == FIOMEM) {
		strcat(mesg, "OUT OF MEMORY, ");
		curbp->b_flag |= BFTRUNC;
	}
	sprintf(&mesg[strlen(mesg)], "Read %d line", nline);
	if (nline != 1)
		strcat(mesg, "s");
	strcat(mesg, ")");
	msg_puts(mesg);

 out:

	if (bp->b_fname[0])
		record_fstate(bp, bp->b_fname);
	setup_buffer_modes(bp);
	undo_reset(bp);

	wp = window_head;
	while (wp != NULL) {
		if (wp->w_bufp == curbp) {
			wp->w_linep = line_next(curbp->b_linep);
			wp->w_dotp = line_next(curbp->b_linep);
			wp->w_doto = 0;
			wp->w_markp = NULL;
			wp->w_marko = 0;
			wp->w_flag |= WFMODE | WFHARD;
		}
		wp = wp->w_wndp;
	}
	if (s == FIOERR || s == FIOFNF)
		return FALSE;
	return TRUE;
}

void makename(char *bname, char *fname)
{
	char *cp1;
	char *cp2;

	cp1 = &fname[0];
	while (*cp1 != 0)
		++cp1;

	while (cp1 != &fname[0] && cp1[-1] != '/')
		--cp1;
	cp2 = &bname[0];
	while (cp2 != &bname[NBUFN - 1] && *cp1 != 0 && *cp1 != ';')
		*cp2++ = *cp1++;
	*cp2 = 0;
}

void unique_buffer_name(char *name)
{
	char *sp;

	while (find_buffer(name, 0, FALSE) != NULL) {
		sp = name;
		while (*sp)
			++sp;
		if (sp == name || (*(sp - 1) < '0' || *(sp - 1) > '8')) {
			*sp++ = '0';
			*sp = 0;
		} else
			*(--sp) += 1;
	}
}

int cmd_write_file(int f, int n)
{
	struct window *wp;
	int s;
	char fname[NFILEN];

	if (restflag)
		return restricted_error();
	file_prompt_setup(TRUE);
	if ((s = ask_string("Write file: ", fname, NFILEN)) != TRUE)
		return s;
	if (fname[strlen(fname) - 1] == '/') {
		char *base = strrchr(curbp->b_fname, '/');
		mystrscat(fname, base ? base + 1 : (curbp->b_fname[0] ? curbp->b_fname : curbp->b_bname), NFILEN);
	}
	if (file_exists(fname) && strcmp(fname, curbp->b_fname) != 0) {
		char prompt[NFILEN + 40];
		snprintf(prompt, sizeof(prompt), "File %s exists; overwrite", fname);
		if ((s = ask_yesno(prompt)) != TRUE)
			return s;
	}
	if ((s = writeout(fname)) == TRUE) {
		char bname[NBUFN];
		strcpy(curbp->b_fname, fname);
		record_fstate(curbp, fname);
		curbp->b_flag &= ~BFCHG;
		undo_mark_saved(curbp);
		makename(bname, fname);
		if (find_buffer(bname, FALSE, 0) == NULL)
			mystrscpy(curbp->b_bname, bname, NBUFN);
		setup_buffer_modes(curbp);
		wp = window_head;
		while (wp != NULL) {
			if (wp->w_bufp == curbp)
				wp->w_flag |= WFMODE;
			wp = wp->w_wndp;
		}
	}
	return s;
}

int cmd_save_file(int f, int n)
{
	int s;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if ((curbp->b_flag & BFCHG) == 0)
		return TRUE;
	if (curbp->b_fname[0] == 0) {
		msg_printf("No file name");
		return FALSE;
	}

	if ((curbp->b_flag & BFTRUNC) != 0) {
		if (ask_yesno("Truncated file ... write it out") == FALSE) {
			msg_printf("(Aborted)");
			return FALSE;
		}
	}

	if (file_changed(curbp, curbp->b_fname)) {
		if (ask_yesno("File changed on disk, overwrite") != TRUE) {
			msg_printf("(Aborted)");
			return FALSE;
		}
	}

	if ((s = writeout(curbp->b_fname)) == TRUE) {
		record_fstate(curbp, curbp->b_fname);
		curbp->b_flag &= ~BFCHG;
		undo_mark_saved(curbp);
		curwp->w_flag |= WFMODE;
	}
	return s;
}

int writeout(char *fn)
{
	int s;
	struct line *lp;
	int nline;

	if ((s = file_open_write(fn)) != FIOSUC) {
		return FALSE;
	}
	lp = line_next(curbp->b_linep);
	nline = 0;
	while (lp != curbp->b_linep) {
		if ((s = file_put_line(&lp->l_text[0], line_length(lp))) != FIOSUC)
			break;
		++nline;
		lp = line_next(lp);
	}
	if (s == FIOSUC) {
		s = file_close();
		if (s == FIOSUC) {
			msg_printf("Wrote %s (%d line%s)", fn, nline, nline == 1 ? "" : "s");
		}
	} else
		file_close();
	if (s != FIOSUC)
		return FALSE;
	return TRUE;
}

int cmd_change_file_name(int f, int n)
{
	int s;
	char fname[NFILEN];

	if (restflag)
		return restricted_error();
	if ((s = ask_string("Name: ", fname, NFILEN)) == ABORT)
		return s;
	if (s == FALSE)
		strcpy(curbp->b_fname, "");
	else
		strcpy(curbp->b_fname, fname);
	curwp->w_flag |= WFMODE;
	curbp->b_mode &= ~MDVIEW;
	return TRUE;
}

int insert_file(char *fname)
{
	struct line *lp0;
	struct line *lp1;
	struct line *lp2;
	int i;
	struct buffer *bp;
	int s;
	int nbytes;
	int nline;
	char mesg[NSTRING];

	bp = curbp;
	bp->b_flag |= BFCHG;
	bp->b_flag &= ~BFINVS;
	if ((s = file_open_read(fname)) == FIOERR)
		goto out;
	if (s == FIOFNF) {
		msg_printf("(No such file)");
		return FALSE;
	}
	msg_printf("(Inserting file)");

	{
		long startln;
		char *acc = NULL;
		long acclen = 0, acccap = 0;
		int ok = TRUE;

		curwp->w_doto = 0;
		startln = line_number(curwp->w_dotp);
		nline = 0;
		while ((s = file_get_line()) == FIOSUC) {
			nbytes = file_line_len;
			if (acclen + nbytes + 1 > acccap) {
				char *na;
				acccap = (acclen + nbytes + 1) * 2;
				na = realloc(acc, acccap);
				if (na == NULL) {
					s = FIOMEM;
					ok = FALSE;
					break;
				}
				acc = na;
			}
			memcpy(acc + acclen, file_line, nbytes);
			acclen += nbytes;
			acc[acclen++] = '\n';
			++nline;
		}
		file_close();
		if (ok && acclen > 0)
			insert_text(acc, acclen);
		free(acc);
		curwp->w_markp = line_at(startln);
		curwp->w_marko = 0;
		goto inserted;
	}

	nline = 0;
	while ((s = file_get_line()) == FIOSUC) {
		nbytes = strlen(file_line);
		if ((lp1 = line_alloc(nbytes)) == NULL) {
			s = FIOMEM;
			break;
		}
		lp0 = curwp->w_dotp;
		lp2 = lp0->l_fp;

		lp2->l_bp = lp1;
		lp0->l_fp = lp1;
		lp1->l_bp = lp0;
		lp1->l_fp = lp2;

		curwp->w_dotp = lp1;
		for (i = 0; i < nbytes; ++i)
			lputc(lp1, i, file_line[i]);
		++nline;
	}
	file_close();
	curwp->w_markp = line_next(curwp->w_markp);
 inserted:
	strcpy(mesg, "(");
	if (s == FIOERR) {
		strcat(mesg, "I/O ERROR, ");
		curbp->b_flag |= BFTRUNC;
	}
	if (s == FIOMEM) {
		strcat(mesg, "OUT OF MEMORY, ");
		curbp->b_flag |= BFTRUNC;
	}
	sprintf(&mesg[strlen(mesg)], "Inserted %d line", nline);
	if (nline > 1)
		strcat(mesg, "s");
	strcat(mesg, ")");
	msg_puts(mesg);

 out:
	curwp->w_flag |= WFHARD | WFMODE;

	curbp->b_dotp = curwp->w_dotp;
	curbp->b_doto = curwp->w_doto;
	curbp->b_markp = curwp->w_markp;
	curbp->b_marko = curwp->w_marko;

	if (s == FIOERR)
		return FALSE;
	return TRUE;
}
