#include "line.h"

#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "utf8.h"

#define	BLOCK_SIZE 16

struct line *line_alloc(int used)
{
	struct line *lp;
	int size;

	size = (used + BLOCK_SIZE - 1) & ~(BLOCK_SIZE - 1);
	if (size == 0)
		size = BLOCK_SIZE;
	if ((lp = (struct line *)malloc(sizeof(struct line) + size)) == NULL) {
		msg_printf("(OUT OF MEMORY)");
		return NULL;
	}
	lp->l_size = size;
	lp->l_used = used;
	line_gen++;
	return lp;
}

void line_free(struct line *lp)
{
	struct buffer *bp;
	struct window *wp;

	wp = window_head;
	while (wp != NULL) {
		if (wp->w_linep == lp)
			wp->w_linep = lp->l_fp;
		if (wp->w_dotp == lp) {
			wp->w_dotp = lp->l_fp;
			wp->w_doto = 0;
		}
		if (wp->w_markp == lp) {
			wp->w_markp = lp->l_fp;
			wp->w_marko = 0;
		}
		wp = wp->w_wndp;
	}

	bp = buffer_head;
	while (bp != NULL) {
		if (bp->b_nwnd == 0) {
			if (bp->b_dotp == lp) {
				bp->b_dotp = lp->l_fp;
				bp->b_doto = 0;
			}
			if (bp->b_markp == lp) {
				bp->b_markp = lp->l_fp;
				bp->b_marko = 0;
			}
		}
		bp = bp->b_bufp;
	}
	lp->l_bp->l_fp = lp->l_fp;
	lp->l_fp->l_bp = lp->l_bp;
	free((char *)lp);
	line_gen++;
}

void buffer_changed(int flag)
{
	struct window *wp;

	buffer_was_modified = TRUE;
	if (curbp->b_nwnd != 1)
		flag = WFHARD;
	if ((curbp->b_flag & BFCHG) == 0) {
		flag |= WFMODE;
		curbp->b_flag |= BFCHG;
	}
	wp = window_head;
	while (wp != NULL) {
		if (wp->w_bufp == curbp)
			wp->w_flag |= flag;
		wp = wp->w_wndp;
	}
}

int cmd_insert_space(int f, int n)
{
	insert_char(n, ' ');
	cmd_backward_character(f, n);
	return TRUE;
}

int insert_string(char *instr)
{
	int status = TRUE;
	char tmpc;

	if (instr != NULL)
		while ((tmpc = *instr) && status == TRUE) {
			status = (tmpc == '\n' ? insert_newline() : insert_char(1, tmpc));

			if (status != TRUE) {
				msg_printf("%%Out of memory while inserting");
				break;
			}
			instr++;
		}
	return status;
}

static long hint_line = -1;

static int insert_mem(const char *src, int n, int c)
{
	struct line *lp1;
	struct line *lp2;
	struct line *lp3;
	int doto;
	struct window *wp;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	buffer_changed(WFEDIT);
	lp1 = curwp->w_dotp;
	if (lp1 == curbp->b_linep) {
		long ln;
		if (curwp->w_doto != 0) {
			msg_printf("bug: insert_char");
			return FALSE;
		}
		ln = src ? undo_record_insert_buf(lp1, 0, src, n) : undo_record_insert_fill(lp1, 0, c, n);
		if ((lp2 = line_alloc(n)) == NULL)
			return FALSE;
		lp3 = lp1->l_bp;
		lp3->l_fp = lp2;
		lp2->l_fp = lp1;
		lp1->l_bp = lp2;
		lp2->l_bp = lp3;
		if (src)
			memcpy(lp2->l_text, src, n);
		else
			memset(lp2->l_text, c, n);
		curwp->w_dotp = lp2;
		curwp->w_doto = n;
		for (wp = window_head; wp != NULL; wp = wp->w_wndp)
			if (wp != curwp && wp->w_bufp == curbp && wp->w_dotp == lp1 && wp->w_linep == lp1)
				wp->w_linep = lp2;
		if (ln >= 0)
			line_number_hint(lp2, ln);
		return TRUE;
	}
	doto = curwp->w_doto;
	{
		long ln = src ? undo_record_insert_buf(lp1, doto, src, n) : undo_record_insert_fill(lp1, doto, c, n);
		if (ln >= 0)
			hint_line = ln;
		else
			hint_line = -1;
	}
	if (lp1->l_used + n > lp1->l_size) {
		if ((lp2 = line_alloc(lp1->l_used + n)) == NULL)
			return FALSE;
		memcpy(lp2->l_text, lp1->l_text, doto);
		memcpy(lp2->l_text + doto + n, lp1->l_text + doto,
		       lp1->l_used - doto);
		lp1->l_bp->l_fp = lp2;
		lp2->l_fp = lp1->l_fp;
		lp1->l_fp->l_bp = lp2;
		lp2->l_bp = lp1->l_bp;
		free((char *)lp1);
		line_gen++;
	} else {
		lp2 = lp1;
		lp2->l_used += n;
		memmove(lp1->l_text + doto + n, lp1->l_text + doto,
			lp1->l_used - doto - n);
	}
	if (src)
		memcpy(lp2->l_text + doto, src, n);
	else
		memset(lp2->l_text + doto, c, n);
	wp = window_head;
	while (wp != NULL) {
		if (wp->w_linep == lp1)
			wp->w_linep = lp2;
		if (wp->w_dotp == lp1) {
			wp->w_dotp = lp2;
			wp->w_doto += n;
		}
		if (wp->w_markp == lp1) {
			wp->w_markp = lp2;
			if (wp->w_marko > doto)
				wp->w_marko += n;
		}
		wp = wp->w_wndp;
	}
	if (hint_line >= 0)
		line_number_hint(lp2, hint_line);
	return TRUE;
}

int insert_byte(int n, int c)
{
	return insert_mem(NULL, n, c);
}

int insert_char(int n, int c)
{
	char utf8[6];
	int bytes = unicode_to_utf8(c, utf8), i;

	if (bytes == 1)
		return insert_byte(n, (unsigned char)utf8[0]);
	for (i = 0; i < n; i++) {
		int j;
		for (j = 0; j < bytes; j++) {
			unsigned char c = utf8[j];
			if (!insert_byte(1, c))
				return FALSE;
		}
	}
	return TRUE;
}

int overwrite_char(int c)
{
	if (curwp->w_doto < curwp->w_dotp->l_used &&
	    (lgetc(curwp->w_dotp, curwp->w_doto) != '\t' || ((curwp->w_doto) & tabmask) == tabmask))
		delete_characters(1, FALSE);
	return insert_char(1, c);
}

int overwrite_string(char *ostr)
{
	int status = TRUE;
	char tmpc;

	if (ostr != NULL)
		while ((tmpc = *ostr) && status == TRUE) {
			status = (tmpc == '\n' ? insert_newline() : overwrite_char(tmpc));

			if (status != TRUE) {
				msg_printf("%%Out of memory while overwriting");
				break;
			}
			ostr++;
		}
	return status;
}

int insert_newline(void)
{
	char *cp1;
	char *cp2;
	struct line *lp1;
	struct line *lp2;
	int doto;
	struct window *wp;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	buffer_changed(WFHARD);
	lp1 = curwp->w_dotp;
	doto = curwp->w_doto;
	{
		long ln = undo_record_insert(lp1, doto, "\n", 1);
		hint_line = ln;
	}
	if ((lp2 = line_alloc(doto)) == NULL)
		return FALSE;
	cp1 = &lp1->l_text[0];
	cp2 = &lp2->l_text[0];
	while (cp1 != &lp1->l_text[doto])
		*cp2++ = *cp1++;
	cp2 = &lp1->l_text[0];
	while (cp1 != &lp1->l_text[lp1->l_used])
		*cp2++ = *cp1++;
	lp1->l_used -= doto;
	lp2->l_bp = lp1->l_bp;
	lp1->l_bp = lp2;
	lp2->l_bp->l_fp = lp2;
	lp2->l_fp = lp1;
	wp = window_head;
	while (wp != NULL) {
		if (wp->w_linep == lp1)
			wp->w_linep = lp2;
		if (wp->w_dotp == lp1) {
			if (wp->w_doto < doto)
				wp->w_dotp = lp2;
			else
				wp->w_doto -= doto;
		}
		if (wp->w_markp == lp1) {
			if (wp->w_marko < doto)
				wp->w_markp = lp2;
			else
				wp->w_marko -= doto;
		}
		wp = wp->w_wndp;
	}
	if (hint_line >= 0)
		line_number_hint(lp1, hint_line + 1);
	return TRUE;
}

int char_at_dot(unicode_t *c)
{
	int len = line_length(curwp->w_dotp);
	char *buf = curwp->w_dotp->l_text;
	return utf8_to_unicode(buf, curwp->w_doto, len, c);
}

int delete_characters(long n, int kflag)
{
	while (n-- > 0) {
		unicode_t c;
		if (!delete_bytes(char_at_dot(&c), kflag))
			return FALSE;
	}
	return TRUE;
}

int delete_bytes(long n, int kflag)
{
	char *cp1;
	char *cp2;
	struct line *dotp;
	int doto;
	int chunk;
	struct window *wp;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	while (n != 0) {
		dotp = curwp->w_dotp;
		doto = curwp->w_doto;
		if (dotp == curbp->b_linep)
			return FALSE;
		chunk = dotp->l_used - doto;
		if (chunk > n)
			chunk = n;
		if (chunk == 0) {
			buffer_changed(WFHARD);
			if (delete_newline() == FALSE || (kflag != FALSE && kinsert('\n') == FALSE))
				return FALSE;
			--n;
			continue;
		}
		buffer_changed(WFEDIT);
		{
			long ln = undo_record_delete(dotp, doto, &dotp->l_text[doto], chunk);
			if (ln >= 0)
				line_number_hint(dotp, ln);
		}
		cp1 = &dotp->l_text[doto];
		cp2 = cp1 + chunk;
		if (kflag != FALSE) {
			while (cp1 != cp2) {
				if (kinsert(*cp1) == FALSE)
					return FALSE;
				++cp1;
			}
			cp1 = &dotp->l_text[doto];
		}
		while (cp2 != &dotp->l_text[dotp->l_used])
			*cp1++ = *cp2++;
		dotp->l_used -= chunk;
		wp = window_head;
		while (wp != NULL) {
			if (wp->w_dotp == dotp && wp->w_doto >= doto) {
				wp->w_doto -= chunk;
				if (wp->w_doto < doto)
					wp->w_doto = doto;
			}
			if (wp->w_markp == dotp && wp->w_marko >= doto) {
				wp->w_marko -= chunk;
				if (wp->w_marko < doto)
					wp->w_marko = doto;
			}
			wp = wp->w_wndp;
		}
		n -= chunk;
	}
	return TRUE;
}

char *getctext(void)
{
	struct line *lp;
	int size;
	char *sp;
	char *dp;
	static char rline[NSTRING];

	lp = curwp->w_dotp;
	sp = lp->l_text;
	size = lp->l_used;
	if (size >= NSTRING)
		size = NSTRING - 1;

	dp = rline;
	while (size--)
		*dp++ = *sp++;
	*dp = 0;
	return rline;
}

int putctext(char *iline)
{
	int status;

	curwp->w_doto = 0;
	if ((status = cmd_kill_to_end_of_line(TRUE, 1)) != TRUE)
		return status;

	if ((status = insert_string(iline)) != TRUE)
		return status;
	status = insert_newline();
	cmd_previous_line(TRUE, 1);
	return status;
}

int delete_newline(void)
{
	char *cp1;
	char *cp2;
	struct line *lp1;
	struct line *lp2;
	struct line *lp3;
	struct window *wp;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	lp1 = curwp->w_dotp;
	lp2 = lp1->l_fp;
	if (lp2 == curbp->b_linep) {
		if (lp1->l_used == 0) {
			undo_record_delete(lp1, 0, "\n", 1);
			line_free(lp1);
		}
		return TRUE;
	}
	{
		long ln = undo_record_delete(lp1, lp1->l_used, "\n", 1);
		hint_line = ln;
	}
	if (lp2->l_used <= lp1->l_size - lp1->l_used) {
		cp1 = &lp1->l_text[lp1->l_used];
		cp2 = &lp2->l_text[0];
		while (cp2 != &lp2->l_text[lp2->l_used])
			*cp1++ = *cp2++;
		wp = window_head;
		while (wp != NULL) {
			if (wp->w_linep == lp2)
				wp->w_linep = lp1;
			if (wp->w_dotp == lp2) {
				wp->w_dotp = lp1;
				wp->w_doto += lp1->l_used;
			}
			if (wp->w_markp == lp2) {
				wp->w_markp = lp1;
				wp->w_marko += lp1->l_used;
			}
			wp = wp->w_wndp;
		}
		lp1->l_used += lp2->l_used;
		lp1->l_fp = lp2->l_fp;
		lp2->l_fp->l_bp = lp1;
		free((char *)lp2);
		line_gen++;
		if (hint_line >= 0)
			line_number_hint(lp1, hint_line);
		return TRUE;
	}
	if ((lp3 = line_alloc(lp1->l_used + lp2->l_used)) == NULL)
		return FALSE;
	cp1 = &lp1->l_text[0];
	cp2 = &lp3->l_text[0];
	while (cp1 != &lp1->l_text[lp1->l_used])
		*cp2++ = *cp1++;
	cp1 = &lp2->l_text[0];
	while (cp1 != &lp2->l_text[lp2->l_used])
		*cp2++ = *cp1++;
	lp1->l_bp->l_fp = lp3;
	lp3->l_fp = lp2->l_fp;
	lp2->l_fp->l_bp = lp3;
	lp3->l_bp = lp1->l_bp;
	wp = window_head;
	while (wp != NULL) {
		if (wp->w_linep == lp1 || wp->w_linep == lp2)
			wp->w_linep = lp3;
		if (wp->w_dotp == lp1)
			wp->w_dotp = lp3;
		else if (wp->w_dotp == lp2) {
			wp->w_dotp = lp3;
			wp->w_doto += lp1->l_used;
		}
		if (wp->w_markp == lp1)
			wp->w_markp = lp3;
		else if (wp->w_markp == lp2) {
			wp->w_markp = lp3;
			wp->w_marko += lp1->l_used;
		}
		wp = wp->w_wndp;
	}
	free((char *)lp1);
	free((char *)lp2);
	line_gen++;
	if (hint_line >= 0)
		line_number_hint(lp3, hint_line);
	return TRUE;
}

void kdelete(void)
{
	struct kill *kp;

	if (kill_head != NULL) {
		kill_ring_push();

		kill_last = kill_head;
		while (kill_last != NULL) {
			kp = kill_last->d_next;
			free(kill_last);
			kill_last = kp;
		}

		kill_head = kill_last = NULL;
		kill_used = KBLOCK;
	}
}

int kinsert(int c)
{
	struct kill *nchunk;

	if (kill_used >= KBLOCK) {
		if ((nchunk = (struct kill *)malloc(sizeof(struct kill))) == NULL)
			return FALSE;
		if (kill_head == NULL)
			kill_head = nchunk;
		if (kill_last != NULL)
			kill_last->d_next = nchunk;
		kill_last = nchunk;
		kill_last->d_next = NULL;
		kill_used = 0;
	}

	kill_last->d_chunk[kill_used++] = c;
	return TRUE;
}

int cmd_yank(int f, int n)
{
	int c;
	int i;
	char *sp;
	struct kill *kp;

	long sl;
	int so;
	long total = 0;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if (n < 0)
		return FALSE;
	if (region_active)
		delete_active_region();

	if (kill_head == NULL)
		return TRUE;

	sl = line_number(curwp->w_dotp);
	so = curwp->w_doto;

	while (n--) {
		kp = kill_head;
		while (kp != NULL) {
			if (kp->d_next == NULL)
				i = kill_used;
			else
				i = KBLOCK;
			sp = kp->d_chunk;
			total += i;
			if (insert_bytes(sp, i) != TRUE)
				return FALSE;
			(void)c;
			kp = kp->d_next;
		}
	}
	curwp->w_markp = line_at(sl);
	curwp->w_marko = so > line_length(curwp->w_markp) ? line_length(curwp->w_markp) : so;
	yank_noted(total);
	thisflag |= CFYANK;
	return TRUE;
}

#define LCACHE 4
static struct {
	struct buffer *bp;
	struct line *lp;
	long n;
	unsigned long gen;
} lcache[LCACHE];
static int lcache_next;

void line_number_hint(struct line *lp, long n)
{
	int i;

	for (i = 0; i < LCACHE; i++)
		if (lcache[i].bp == curbp && lcache[i].lp == lp) {
			lcache[i].n = n;
			lcache[i].gen = line_gen;
			return;
		}
	lcache[lcache_next].bp = curbp;
	lcache[lcache_next].lp = lp;
	lcache[lcache_next].n = n;
	lcache[lcache_next].gen = line_gen;
	lcache_next = (lcache_next + 1) % LCACHE;
}

void line_number_flush(void)
{
	int i;

	for (i = 0; i < LCACHE; i++)
		lcache[i].bp = NULL;
}

long buffer_line_number(struct buffer *bp, struct line *lp)
{
	struct line *hdr = bp->b_linep;
	struct line *f, *b, *p;
	long nf, nb, n;
	int i, best = -1;

	for (i = 0; i < LCACHE; i++) {
		if (lcache[i].bp != bp || lcache[i].gen != line_gen)
			continue;
		if (lcache[i].lp == lp)
			return lcache[i].n;
		if (best < 0)
			best = i;
	}
	if (best >= 0) {
		f = b = lcache[best].lp;
		nf = nb = lcache[best].n;
		int bstop = 0;
		for (i = 0; i < 8192; i++) {
			int moved = 0;
			if (f != hdr) {
				f = f->l_fp;
				nf++;
				moved = 1;
				if (f == lp) {
					n = nf;
					goto found;
				}
			}
			if (!bstop) {
				p = b->l_bp;
				if (p == hdr)
					bstop = 1;
				else {
					b = p;
					nb--;
					moved = 1;
					if (b == lp) {
						n = nb;
						goto found;
					}
				}
			}
			if (!moved)
				break;
		}
	}
	n = 0;
	p = hdr->l_fp;
	while (p != lp && p != hdr) {
		p = p->l_fp;
		n++;
	}
found:
	lcache[lcache_next].bp = bp;
	lcache[lcache_next].lp = lp;
	lcache[lcache_next].n = n;
	lcache[lcache_next].gen = line_gen;
	lcache_next = (lcache_next + 1) % LCACHE;
	return n;
}

long line_number(struct line *lp)
{
	return buffer_line_number(curbp, lp);
}

int line_put_byte(struct line *lp, int off, int c)
{
	char nb = c;

	if (lgetc(lp, off) == (c & 0xFF))
		return TRUE;
	undo_record_replace(lp, off, &lp->l_text[off], &nb, 1);
	lputc(lp, off, c);
	buffer_changed(WFHARD);
	return TRUE;
}

int insert_bytes(const char *s, int len)
{
	int i, start = 0;

	for (i = 0; i <= len; i++) {
		if (i == len || s[i] == '\n') {
			if (i > start && insert_mem(s + start, i - start, 0) != TRUE)
				return FALSE;
			if (i < len && insert_newline() != TRUE)
				return FALSE;
			start = i + 1;
		}
	}
	return TRUE;
}

int line_truncate(struct line *lp, int len)
{
	struct window *wp;

	if (len >= lp->l_used)
		return TRUE;
	undo_record_delete(lp, len, &lp->l_text[len], lp->l_used - len);
	lp->l_used = len;
	for (wp = window_head; wp != NULL; wp = wp->w_wndp) {
		if (wp->w_dotp == lp && wp->w_doto > len)
			wp->w_doto = len;
		if (wp->w_markp == lp && wp->w_marko > len)
			wp->w_marko = len;
	}
	buffer_changed(WFHARD);
	return TRUE;
}

int insert_text(const char *s, int len)
{
	if (len > 0 && curwp->w_dotp == curbp->b_linep && s[len - 1] == '\n') {
		if (insert_newline() != TRUE)
			return FALSE;
		curwp->w_dotp = line_prev(curbp->b_linep);
		curwp->w_doto = 0;
		return insert_bytes(s, len - 1);
	}
	return insert_bytes(s, len);
}
