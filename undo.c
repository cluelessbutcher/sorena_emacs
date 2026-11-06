#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"

enum { U_BOUND = 0, U_INS, U_DEL, U_REP };

struct urec {
	int type;
	int eob;
	long line;
	int off;
	long eline;
	int eoff;
	int len;
	int cap;
	char *text;
	char *text2;
	long cline;
	int coff;
	unsigned long gid;
};

struct ustack {
	struct urec *r;
	int n;
	int cap;
};

struct undo {
	struct ustack u;
	struct ustack r;
	unsigned long saved_gid;
};

#define UNDO_MAX_RECORDS 4000000

int buffer_was_modified = FALSE;
int undo_disabled = FALSE;
struct line *undo_cmd_lp = NULL;
int undo_cmd_off = 0;
struct buffer *undo_cmd_bp = NULL;

static int undo_state = 0;
static unsigned long gid_counter = 0;
static unsigned long pending_gid = 0;

static struct undo *get_undo(struct buffer *bp)
{
	if (bp->b_undo == NULL)
		bp->b_undo = calloc(1, sizeof(struct undo));
	return bp->b_undo;
}

static void rec_free(struct urec *r)
{
	free(r->text);
	free(r->text2);
	r->text = r->text2 = NULL;
}

static void stack_clear(struct ustack *s)
{
	int i;

	for (i = 0; i < s->n; i++)
		rec_free(&s->r[i]);
	s->n = 0;
}

static void stack_release(struct ustack *s)
{
	stack_clear(s);
	free(s->r);
	s->r = NULL;
	s->cap = 0;
}

void undo_free(struct buffer *bp)
{
	if (bp->b_undo == NULL)
		return;
	stack_release(&bp->b_undo->u);
	stack_release(&bp->b_undo->r);
	free(bp->b_undo);
	bp->b_undo = NULL;
}

void undo_reset(struct buffer *bp)
{
	if (bp->b_undo == NULL)
		return;
	stack_clear(&bp->b_undo->u);
	stack_clear(&bp->b_undo->r);
	bp->b_undo->saved_gid = 0;
}

static void stack_trim(struct ustack *s)
{
	int drop = s->n / 4, i;

	while (drop < s->n && s->r[drop].type != U_BOUND)
		drop++;
	if (drop >= s->n)
		return;
	for (i = 0; i < drop; i++)
		rec_free(&s->r[i]);
	memmove(s->r, s->r + drop, (s->n - drop) * sizeof(struct urec));
	s->n -= drop;
}

static struct urec *push(struct ustack *s)
{
	struct urec *r;

	if (s->n >= UNDO_MAX_RECORDS)
		stack_trim(s);
	if (s->n == s->cap) {
		int ncap = s->cap ? s->cap * 2 : 64;
		struct urec *nr = realloc(s->r, ncap * sizeof(struct urec));
		if (nr == NULL)
			return NULL;
		s->r = nr;
		s->cap = ncap;
	}
	r = &s->r[s->n++];
	memset(r, 0, sizeof(*r));
	return r;
}

static struct urec *top_rec(struct ustack *s)
{
	if (s->n && s->r[s->n - 1].type != U_BOUND)
		return &s->r[s->n - 1];
	return NULL;
}

static int text_reserve(struct urec *r, int extra)
{
	if (r->len + extra > r->cap) {
		int ncap = r->cap ? r->cap : 16;
		char *nt;
		while (ncap < r->len + extra)
			ncap *= 2;
		nt = realloc(r->text, ncap);
		if (nt == NULL)
			return FALSE;
		r->text = nt;
		if (r->type == U_REP) {
			nt = realloc(r->text2, ncap);
			if (nt == NULL)
				return FALSE;
			r->text2 = nt;
		}
		r->cap = ncap;
	}
	return TRUE;
}

static void advance_pos(long *line, int *off, const char *s, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (s[i] == '\n') {
			(*line)++;
			*off = 0;
		} else
			(*off)++;
	}
}

static struct ustack *target(struct undo *u)
{
	return undo_state == 1 ? &u->r : &u->u;
}

static struct urec *new_rec(struct undo *u, struct ustack *st, int type)
{
	int newgroup = (st->n == 0 || st->r[st->n - 1].type == U_BOUND);
	unsigned long gid = newgroup ? 0 : st->r[st->n - 1].gid;
	long cl = -1;
	struct urec *r;

	if (newgroup) {
		gid = undo_state ? pending_gid : ++gid_counter;
		if (undo_cmd_bp == curbp && undo_cmd_lp != NULL)
			cl = line_number(undo_cmd_lp);
	}
	r = push(st);
	if (r == NULL)
		return NULL;
	r->type = type;
	r->gid = gid;
	r->cline = cl;
	r->coff = undo_cmd_off;
	return r;
}

static int recording(void)
{
	return !undo_disabled && curbp != NULL && !(curbp->b_flag & BFINVS);
}

static long record_insert(struct line *lp, int off, const char *s, int c, int len, int eob)
{
	struct undo *u;
	struct ustack *st;
	struct urec *t;
	long L;

	if (!recording())
		return -1;
	u = get_undo(curbp);
	if (u == NULL)
		return -1;
	if (undo_state == 0)
		stack_clear(&u->r);
	st = target(u);
	L = line_number(lp);
	t = top_rec(st);
	if (!(t && t->type == U_INS && !eob && t->eline == L && t->eoff == off)) {
		t = new_rec(u, st, U_INS);
		if (t == NULL)
			return L;
		t->eob = eob;
		t->line = t->eline = L;
		t->off = t->eoff = off;
	}
	if (!text_reserve(t, len))
		return L;
	if (s)
		memcpy(t->text + t->len, s, len);
	else
		memset(t->text + t->len, c, len);
	advance_pos(&t->eline, &t->eoff, t->text + t->len, len);
	t->len += len;
	return L;
}

long undo_record_insert(struct line *lp, int off, const char *s, int len)
{
	return record_insert(lp, off, s, 0, len, 0);
}

long undo_record_insert_buf(struct line *lp, int off, const char *s, int len)
{
	return record_insert(lp, off, s, 0, len, lp == curbp->b_linep);
}

long undo_record_insert_fill(struct line *lp, int off, int c, int n)
{
	return record_insert(lp, off, NULL, c, n, lp == curbp->b_linep);
}

long undo_record_delete(struct line *lp, int off, const char *s, int len)
{
	struct undo *u;
	struct ustack *st;
	struct urec *t;
	long L;

	if (!recording())
		return -1;
	u = get_undo(curbp);
	if (u == NULL)
		return -1;
	if (undo_state == 0)
		stack_clear(&u->r);
	st = target(u);
	L = line_number(lp);
	t = top_rec(st);
	if (t && t->type == U_DEL) {
		if (t->line == L && t->off == off) {
			if (text_reserve(t, len)) {
				memcpy(t->text + t->len, s, len);
				t->len += len;
			}
			return L;
		} else {
			long el = L;
			int eo = off;
			advance_pos(&el, &eo, s, len);
			if (el == t->line && eo == t->off) {
				if (text_reserve(t, len)) {
					memmove(t->text + len, t->text, t->len);
					memcpy(t->text, s, len);
					t->len += len;
					t->line = L;
					t->off = off;
				}
				return L;
			}
		}
	}
	t = new_rec(u, st, U_DEL);
	if (t == NULL)
		return L;
	t->line = L;
	t->off = off;
	if (text_reserve(t, len)) {
		memcpy(t->text, s, len);
		t->len = len;
	}
	return L;
}

long undo_record_replace(struct line *lp, int off, const char *olds, const char *news, int len)
{
	struct undo *u;
	struct ustack *st;
	struct urec *t;
	long L;

	if (!recording())
		return -1;
	u = get_undo(curbp);
	if (u == NULL)
		return -1;
	if (undo_state == 0)
		stack_clear(&u->r);
	st = target(u);
	L = line_number(lp);
	t = top_rec(st);
	if (!(t && t->type == U_REP && t->line == L && t->off + t->len == off)) {
		t = new_rec(u, st, U_REP);
		if (t == NULL)
			return L;
		t->line = L;
		t->off = off;
	}
	if (text_reserve(t, len)) {
		memcpy(t->text + t->len, olds, len);
		memcpy(t->text2 + t->len, news, len);
		t->len += len;
	}
	return L;
}

void undo_boundary(void)
{
	struct buffer *bp;

	for (bp = buffer_head; bp != NULL; bp = bp->b_bufp) {
		struct undo *u = bp->b_undo;
		if (u && u->u.n && u->u.r[u->u.n - 1].type != U_BOUND) {
			struct urec *r = push(&u->u);
			if (r)
				r->type = U_BOUND;
		}
	}
}

static unsigned long state_gid(struct undo *u)
{
	int i;

	for (i = u->u.n - 1; i >= 0; i--)
		if (u->u.r[i].type != U_BOUND)
			return u->u.r[i].gid;
	return 0;
}

void undo_mark_saved(struct buffer *bp)
{
	if (bp->b_undo)
		bp->b_undo->saved_gid = state_gid(bp->b_undo);
}

struct line *line_at(long L)
{
	struct line *hdr = curbp->b_linep;
	struct line *lp = line_next(hdr);
	long i = 0;
	long cur;
	struct line *clp = curwp->w_dotp;

	cur = line_number(clp);
	if (labs(cur - L) < L) {
		lp = clp;
		i = cur;
		while (i < L && lp != hdr) {
			lp = line_next(lp);
			i++;
		}
		while (i > L) {
			lp = line_prev(lp);
			i--;
		}
	} else {
		while (i < L && lp != hdr) {
			lp = line_next(lp);
			i++;
		}
	}
	line_number_hint(lp, i);
	return lp;
}

static void goto_pos(long L, int O)
{
	struct line *lp = line_at(L);

	curwp->w_dotp = lp;
	curwp->w_doto = O > line_length(lp) ? line_length(lp) : O;
}

static int apply_inverse(struct urec *r)
{
	int i;

	switch (r->type) {
	case U_INS:
		goto_pos(r->line, r->off);
		return delete_bytes((long)r->len + r->eob, FALSE);
	case U_DEL:
		goto_pos(r->line, r->off);
		return insert_text(r->text, r->len);
	case U_REP:
		goto_pos(r->line, r->off);
		for (i = 0; i < r->len && r->off + i < line_length(curwp->w_dotp); i++)
			line_put_byte(curwp->w_dotp, r->off + i, (unsigned char)r->text[i]);
		curwp->w_doto = r->off;
		return TRUE;
	}
	return TRUE;
}

static int apply_group(int dir)
{
	struct undo *u;
	struct ustack *from, *to;
	struct urec *rp;
	int g, i, status = TRUE;
	long cl;
	int co;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	u = get_undo(curbp);
	if (u == NULL)
		return FALSE;
	from = dir == 1 ? &u->u : &u->r;
	to = dir == 1 ? &u->r : &u->u;
	while (from->n && from->r[from->n - 1].type == U_BOUND)
		from->n--;
	if (from->n == 0) {
		msg_printf(dir == 1 ? "No further undo information" : "No further redo information");
		return FALSE;
	}
	if (to->n && to->r[to->n - 1].type != U_BOUND) {
		rp = push(to);
		if (rp)
			rp->type = U_BOUND;
	}
	g = from->n - 1;
	while (g > 0 && from->r[g - 1].type != U_BOUND)
		g--;
	cl = from->r[g].cline;
	co = from->r[g].coff;
	pending_gid = from->r[g].gid;
	undo_cmd_lp = curwp->w_dotp;
	undo_cmd_off = curwp->w_doto;
	undo_cmd_bp = curbp;
	undo_state = dir;
	for (i = from->n - 1; i >= g; i--) {
		struct urec r = from->r[i];
		from->n = i;
		if (apply_inverse(&r) != TRUE)
			status = FALSE;
		rec_free(&r);
	}
	undo_state = 0;
	if (to->n && to->r[to->n - 1].type != U_BOUND) {
		rp = push(to);
		if (rp)
			rp->type = U_BOUND;
	}
	undo_cmd_lp = NULL;
	undo_cmd_bp = NULL;
	if (cl >= 0)
		goto_pos(cl, co);
	if (state_gid(u) == u->saved_gid)
		curbp->b_flag &= ~BFCHG;
	else
		curbp->b_flag |= BFCHG;
	region_active = FALSE;
	update_modeline();
	curwp->w_flag |= WFHARD | WFMOVE;
	return status;
}

int cmd_undo(int f, int n)
{
	int s = TRUE;

	if (n < 1)
		n = 1;
	while (n-- && s == TRUE)
		s = apply_group(1);
	if (s == TRUE)
		msg_printf("Undo");
	thisflag |= CFKEEPR;
	return s;
}

int cmd_redo(int f, int n)
{
	int s = TRUE;

	if (n < 1)
		n = 1;
	while (n-- && s == TRUE)
		s = apply_group(2);
	if (s == TRUE)
		msg_printf("Redo");
	thisflag |= CFKEEPR;
	return s;
}
