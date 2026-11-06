#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"
#include "util.h"

static short int magical;
static short int rmagical;
static struct magic magic_pattern[NPAT];
static struct magic reversed_magic_pattern[NPAT];
static struct magic_replacement magic_replace_pattern[NPAT];

static int amatch(struct magic *mcptr, int direct, struct line **pcwline, int *pcwoff);
static int readpattern(char *prompt, char *apat, int srch);
static int replaces(int kind, int f, int n);
static int nextch(struct line **pcurline, int *pcuroff, int dir);
static int mcstr(void);
static int rmcstr(void);
static int mceq(int bc, struct magic *mt);
static int cclmake(char **ppatptr, struct magic *mcptr);
static int biteq(int bc, char *cclmap);
static char *clearbits(void);
static void setbit(int bc, char *cclmap);

int cmd_search_forward(int f, int n)
{
	int status = TRUE;

	if (n < 0)
		return cmd_search_reverse(f, -n);

	if ((status = readpattern("Search", &search_pattern[0], TRUE)) == TRUE) {
		do {
			if ((magical && curwp->w_bufp->b_mode & MDMAGIC) != 0)
				status = mcscanner(&magic_pattern[0], FORWARD, PTEND);
			else
				status = scanner(&search_pattern[0], FORWARD, PTEND);
		} while ((--n > 0) && status);

		if (status == TRUE)
			savematch();
		else
			msg_printf("Not found");
	}
	return status;
}

int cmd_hunt_forward(int f, int n)
{
	int status = TRUE;

	if (n < 0)
		return cmd_hunt_backward(f, -n);

	if (search_pattern[0] == '\0') {
		msg_printf("No pattern set");
		return FALSE;
	}
	if ((curwp->w_bufp->b_mode & MDMAGIC) != 0 && magic_pattern[0].mc_type == MCNIL) {
		if (!mcstr())
			return FALSE;
	}

	do {
		if ((magical && curwp->w_bufp->b_mode & MDMAGIC) != 0)
			status = mcscanner(&magic_pattern[0], FORWARD, PTEND);
		else
			status = scanner(&search_pattern[0], FORWARD, PTEND);
	} while ((--n > 0) && status);

	if (status == TRUE)
		savematch();
	else
		msg_printf("Not found");

	return status;
}

int cmd_search_reverse(int f, int n)
{
	int status = TRUE;

	if (n < 0)
		return cmd_search_forward(f, -n);

	if ((status = readpattern("Reverse search", &search_pattern[0], TRUE)) == TRUE) {
		do {
			if ((magical && curwp->w_bufp->b_mode & MDMAGIC) != 0)
				status = mcscanner(&reversed_magic_pattern[0], REVERSE, PTBEG);
			else
				status = scanner(&reversed_pattern[0], REVERSE, PTBEG);
		} while ((--n > 0) && status);

		if (status == TRUE)
			savematch();
		else
			msg_printf("Not found");
	}
	return status;
}

int cmd_hunt_backward(int f, int n)
{
	int status = TRUE;

	if (n < 0)
		return cmd_hunt_forward(f, -n);

	if (reversed_pattern[0] == '\0') {
		msg_printf("No pattern set");
		return FALSE;
	}

	if ((curwp->w_bufp->b_mode & MDMAGIC) != 0 && reversed_magic_pattern[0].mc_type == MCNIL) {
		if (!mcstr())
			return FALSE;
	}

	do {
		if ((magical && curwp->w_bufp->b_mode & MDMAGIC) != 0)
			status = mcscanner(&reversed_magic_pattern[0], REVERSE, PTBEG);
		else
			status = scanner(&reversed_pattern[0], REVERSE, PTBEG);
	} while ((--n > 0) && status);

	if (status == TRUE)
		savematch();
	else
		msg_printf("Not found");

	return status;
}

int mcscanner(struct magic *mcpatrn, int direct, int beg_or_end)
{
	struct line *curline;
	int curoff;

	beg_or_end ^= direct;

	previous_match_bytes = match_bytes;

	curline = curwp->w_dotp;
	curoff = curwp->w_doto;

	while (!at_buffer_end(curline, curoff, direct)) {
		match_line = curline;
		match_offset = curoff;
		match_bytes = 0;

		if (amatch(mcpatrn, direct, &curline, &curoff)) {
			if (beg_or_end == PTEND) {
				curwp->w_dotp = curline;
				curwp->w_doto = curoff;
			} else {
				curwp->w_dotp = match_line;
				curwp->w_doto = match_offset;
			}

			curwp->w_flag |= WFMOVE;
			return TRUE;
		}

		nextch(&curline, &curoff, direct);
	}

	return FALSE;
}

static int amatch(struct magic *mcptr, int direct, struct line **pcwline, int *pcwoff)
{
	int c;
	struct line *curline;
	int curoff;
	int nchars;

	curline = *pcwline;
	curoff = *pcwoff;

	if (mcptr->mc_type == BOL) {
		if (curoff != 0)
			return FALSE;
		mcptr++;
	}

	if (mcptr->mc_type == EOL) {
		if (curoff != line_length(curline))
			return FALSE;
		mcptr++;
	}

	while (mcptr->mc_type != MCNIL) {
		c = nextch(&curline, &curoff, direct);

		if (mcptr->mc_type & CLOSURE) {
			nchars = 0;
			while (c != '\n' && mceq(c, mcptr)) {
				c = nextch(&curline, &curoff, direct);
				nchars++;
			}

			mcptr++;

			for (;;) {
				c = nextch(&curline, &curoff, direct ^ REVERSE);

				if (amatch(mcptr, direct, &curline, &curoff)) {
					match_bytes += nchars;
					goto success;
				}

				if (nchars-- == 0)
					return FALSE;
			}
		} else {
			if (mcptr->mc_type == BOL) {
				if (curoff == line_length(curline)) {
					c = nextch(&curline, &curoff, direct ^ REVERSE);
					goto success;
				} else
					return FALSE;
			}

			if (mcptr->mc_type == EOL) {
				if (curoff == 0) {
					c = nextch(&curline, &curoff, direct ^ REVERSE);
					goto success;
				} else
					return FALSE;
			}

			if (!mceq(c, mcptr))
				return FALSE;
		}

		match_bytes++;
		mcptr++;
	}

 success:
	*pcwline = curline;
	*pcwoff = curoff;

	return TRUE;
}

int scanner(const char *patrn, int direct, int beg_or_end)
{
	int c;
	const char *patptr;
	struct line *curline;
	int curoff;
	struct line *scanline;
	int scanoff;

	beg_or_end ^= direct;

	curline = curwp->w_dotp;
	curoff = curwp->w_doto;

	while (!at_buffer_end(curline, curoff, direct)) {
		match_line = curline;
		match_offset = curoff;

		c = nextch(&curline, &curoff, direct);

		if (eq(c, patrn[0])) {
			scanline = curline;
			scanoff = curoff;
			patptr = &patrn[0];

			while (*++patptr != '\0') {
				c = nextch(&scanline, &scanoff, direct);

				if (!eq(c, *patptr))
					goto fail;
			}

			if (beg_or_end == PTEND) {
				curwp->w_dotp = scanline;
				curwp->w_doto = scanoff;
			} else {
				curwp->w_dotp = match_line;
				curwp->w_doto = match_offset;
			}

			curwp->w_flag |= WFMOVE;
			return TRUE;
		}
 fail:		;
	}

	return FALSE;
}

int search_has_upper = FALSE;

void search_case_update(void)
{
	char *p;

	search_has_upper = FALSE;
	for (p = search_pattern; *p; p++)
		if (*p >= 'A' && *p <= 'Z') {
			search_has_upper = TRUE;
			break;
		}
}

int eq(unsigned char bc, unsigned char pc)
{
	if ((curwp->w_bufp->b_mode & MDEXACT) == 0 && !search_has_upper) {
		if (islower(bc))
			bc ^= DIFCASE;

		if (islower(pc))
			pc ^= DIFCASE;
	}

	return bc == pc;
}

static int readpattern(char *prompt, char *apat, int srch)
{
	int status;
	char tpat[NPAT + 20];

	mystrscpy(tpat, prompt, sizeof(tpat));
	if (apat[0]) {
		mystrscat(tpat, " (default ", sizeof(tpat));
		expandp(&apat[0], tpat + strlen(tpat), NPAT / 2);
		mystrscat(tpat, ")", sizeof(tpat));
	}
	mystrscat(tpat, ": ", sizeof(tpat));
	complete_kind = CMPL_SEARCH;

	if ((status = ask_string(tpat, tpat, NPAT)) == TRUE) {
		strcpy(apat, tpat);
		if (srch) {
			rvstrcpy(reversed_pattern, apat);
			previous_match_bytes = match_bytes = strlen(apat);
		}

		if ((curwp->w_bufp->b_mode & MDMAGIC) == 0) {
			mcclear();
			rmcclear();
		} else
			status = srch ? mcstr() : rmcstr();
	} else if (status == FALSE && apat[0] != 0)
		status = TRUE;
	if (srch)
		search_case_update();

	return status;
}

void savematch(void)
{
	char *ptr;
	int j;
	struct line *curline;
	int curoff;

	if (matched_text != NULL)
		free(matched_text);

	ptr = matched_text = malloc(match_bytes + 1);

	if (ptr != NULL) {
		curoff = match_offset;
		curline = match_line;

		for (j = 0; j < match_bytes; j++)
			*ptr++ = nextch(&curline, &curoff, FORWARD);

		*ptr = '\0';
	}
}

void rvstrcpy(char *rvstr, char *str)
{
	int i;

	str += (i = strlen(str));

	while (i-- > 0)
		*rvstr++ = *--str;

	*rvstr = '\0';
}

int cmd_replace_string(int f, int n)
{
	return replaces(FALSE, f, n);
}

int cmd_query_replace_string(int f, int n)
{
	return replaces(TRUE, f, n);
}

static int replaces(int kind, int f, int n)
{
	int status;
	int rlength;
	int numsub;
	int nummatch;
	int nlflag;
	int nlrepl;
	char c;
	char tpat[NPAT];
	struct line *origline;
	int origoff;
	struct line *lastline;
	int lastoff;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();

	if (f && n < 0)
		return FALSE;

	if ((status = readpattern((kind == FALSE ? "Replace" : "Query replace"), &search_pattern[0], TRUE))
	    != TRUE)
		return status;

	if ((status = readpattern("with", &replace_pattern[0], FALSE)) == ABORT)
		return status;

	rlength = strlen(&replace_pattern[0]);

	nlflag = (search_pattern[match_bytes - 1] == '\n');
	nlrepl = FALSE;

	if (kind) {
		mystrscpy(tpat, "Replace '", sizeof(tpat));
		expandp(&search_pattern[0], tpat + strlen(tpat), NPAT / 3);
		mystrscat(tpat, "' with '", sizeof(tpat));
		expandp(&replace_pattern[0], tpat + strlen(tpat), NPAT / 3);
		mystrscat(tpat, "'? ", sizeof(tpat));

		lastline = NULL;
		lastoff = 0;
	}

	origline = curwp->w_dotp;
	origoff = curwp->w_doto;
	numsub = 0;
	nummatch = 0;

	while ((f == FALSE || n > nummatch) && (nlflag == FALSE || nlrepl == FALSE)) {
		if ((magical && curwp->w_bufp->b_mode & MDMAGIC) != 0) {
			if (!mcscanner(&magic_pattern[0], FORWARD, PTBEG))
				break;
		} else if (!scanner(&search_pattern[0], FORWARD, PTBEG))
			break;

		++nummatch;

		nlrepl = (line_next(curwp->w_dotp) == curwp->w_bufp->b_linep);

		if (kind) {
 pprompt:		msg_puts(tpat);
 qprompt:
			update_now();
			c = tgetc();
			msg_printf("");

			switch (c) {
			case 'Y':
			case 'y':
			case ' ':
				savematch();
				break;

			case 'N':
			case 'n':
			case 0x7F:
				cmd_forward_character(FALSE, 1);
				continue;

			case 'q':
			case 'Q':
			case '\r':
			case '\n':
				msg_printf("Replaced %d occurrence%s", numsub, numsub == 1 ? "" : "s");
				return TRUE;

			case '!':
				kind = FALSE;
				break;

			case 'U':
			case 'u':

				if (lastline == NULL) {
					tcapbeep();
					goto pprompt;
				}
				curwp->w_dotp = lastline;
				curwp->w_doto = lastoff;
				lastline = NULL;
				lastoff = 0;

				cmd_backward_character(FALSE, rlength);
				match_line = curwp->w_dotp;
				match_offset = curwp->w_doto;
				status = delins(rlength, matched_text, FALSE);
				if (status != TRUE)
					return status;

				--numsub;
				cmd_backward_character(FALSE, previous_match_bytes);
				match_line = curwp->w_dotp;
				match_offset = curwp->w_doto;
				goto pprompt;

			case '.':

				curwp->w_dotp = origline;
				curwp->w_doto = origoff;
				curwp->w_flag |= WFMOVE;

			case BELL:
				msg_printf("Aborted!");
				return FALSE;

			default:
				tcapbeep();

			case '?':
				msg_printf
				    ("y/SPC replace, n/DEL skip, ! replace all, u undo last, q/RET exit, . exit back, C-g abort");
				goto qprompt;
			}
		}

		status = delins(match_bytes, &replace_pattern[0], TRUE);
		if (status != TRUE)
			return status;

		if (kind) {
			lastline = curwp->w_dotp;
			lastoff = curwp->w_doto;
		}

		numsub++;
	}

	msg_printf("%d substitutions", numsub);
	return TRUE;
}

int delins(int dlength, char *instr, int use_meta)
{
	int status;
	struct magic_replacement *rmcptr;

	if ((status = delete_bytes((long)dlength, FALSE)) != TRUE)
		msg_printf("%%ERROR while deleting");
	else if ((rmagical && use_meta) && (curwp->w_bufp->b_mode & MDMAGIC) != 0) {
		rmcptr = &magic_replace_pattern[0];
		while (rmcptr->mc_type != MCNIL && status == TRUE) {
			if (rmcptr->mc_type == LITCHAR)
				status = insert_string(rmcptr->rstr);
			else
				status = insert_string(matched_text);
			rmcptr++;
		}
	} else
		status = insert_string(instr);

	return status;
}

int expandp(char *srcstr, char *deststr, int size)
{
	unsigned char c;

	if (size < 2) {
		if (size > 0)
			*deststr = '\0';
		return FALSE;
	}

	size--;

	while ((c = *srcstr++) != 0) {
		char piece[4];
		int len;

		if (c == '\n') {
			memcpy(piece, "<NL>", 4);
			len = 4;
		} else if ((c > 0 && c < 0x20) || c == 0x7f) {
			piece[0] = '^';
			piece[1] = c ^ 0x40;
			len = 2;
		} else {
			piece[0] = c;
			len = 1;
		}

		if (len + 1 > size) {
			*deststr++ = '$';
			*deststr = '\0';
			return FALSE;
		}
		memcpy(deststr, piece, len);
		deststr += len;
		size -= len;
	}
	*deststr = '\0';
	return TRUE;
}

int at_buffer_end(struct line *curline, int curoff, int dir)
{
	int border;

	if (dir == FORWARD) {
		border = (curoff == line_length(curline)) && (line_next(curline) == curbp->b_linep);
	} else {
		border = (curoff == 0) && (line_prev(curline) == curbp->b_linep);
	}
	return border;
}

static int nextch(struct line **pcurline, int *pcuroff, int dir)
{
	struct line *curline;
	int curoff;
	int c;

	curline = *pcurline;
	curoff = *pcuroff;

	if (dir == FORWARD) {
		if (curoff == line_length(curline)) {
			curline = line_next(curline);
			curoff = 0;
			c = '\n';
		} else
			c = lgetc(curline, curoff++);
	} else {
		if (curoff == 0) {
			curline = line_prev(curline);
			curoff = line_length(curline);
			c = '\n';
		} else
			c = lgetc(curline, --curoff);
	}
	*pcurline = curline;
	*pcuroff = curoff;

	return c;
}

static int mcstr(void)
{
	struct magic *mcptr, *rtpcm;
	char *patptr;
	int mj;
	int pchr;
	int status = TRUE;
	int does_closure = FALSE;

	if (magical)
		mcclear();

	magical = FALSE;
	mj = 0;
	mcptr = &magic_pattern[0];
	patptr = &search_pattern[0];

	while ((pchr = *patptr) && status) {
		switch (pchr) {
		case MC_CCL:
			status = cclmake(&patptr, mcptr);
			magical = TRUE;
			does_closure = TRUE;
			break;
		case MC_BOL:
			if (mj != 0)
				goto litcase;

			mcptr->mc_type = BOL;
			magical = TRUE;
			does_closure = FALSE;
			break;
		case MC_EOL:
			if (*(patptr + 1) != '\0')
				goto litcase;

			mcptr->mc_type = EOL;
			magical = TRUE;
			does_closure = FALSE;
			break;
		case MC_ANY:
			mcptr->mc_type = ANY;
			magical = TRUE;
			does_closure = TRUE;
			break;
		case MC_CLOSURE:

			if (!does_closure)
				goto litcase;
			mj--;
			mcptr--;
			mcptr->mc_type |= CLOSURE;
			magical = TRUE;
			does_closure = FALSE;
			break;

		case MC_ESC:
			if (*(patptr + 1) != '\0') {
				pchr = *++patptr;
				magical = TRUE;
			}
		default:
 litcase:		mcptr->mc_type = LITCHAR;
			mcptr->u.lchar = pchr;
			does_closure = (pchr != '\n');
			break;
		}
		mcptr++;
		patptr++;
		mj++;
	}

	mcptr->mc_type = MCNIL;

	if (status) {
		rtpcm = &reversed_magic_pattern[0];
		while (--mj >= 0) {
			*rtpcm++ = *--mcptr;
		}
		rtpcm->mc_type = MCNIL;
	} else {
		(--mcptr)->mc_type = MCNIL;
		mcclear();
	}

	return status;
}

static int rmcstr(void)
{
	struct magic_replacement *rmcptr;
	char *patptr;
	int status = TRUE;
	int mj;

	patptr = &replace_pattern[0];
	rmcptr = &magic_replace_pattern[0];
	mj = 0;
	rmagical = FALSE;

	while (*patptr && status == TRUE) {
		switch (*patptr) {
		case MC_DITTO:

			if (mj != 0) {
				rmcptr->mc_type = LITCHAR;
				if ((rmcptr->rstr = malloc(mj + 1)) == NULL) {
					msg_printf("%%Out of memory");
					status = FALSE;
					break;
				}
				strncpy(rmcptr->rstr, patptr - mj, mj);
				rmcptr++;
				mj = 0;
			}
			rmcptr->mc_type = DITTO;
			rmcptr++;
			rmagical = TRUE;
			break;

		case MC_ESC:
			rmcptr->mc_type = LITCHAR;

			if ((rmcptr->rstr = malloc(mj + 2)) == NULL) {
				msg_printf("%%Out of memory");
				status = FALSE;
				break;
			}

			strncpy(rmcptr->rstr, patptr - mj, mj + 1);

			if (*(patptr + 1) != '\0')
				*((rmcptr->rstr) + mj) = *++patptr;

			rmcptr++;
			mj = 0;
			rmagical = TRUE;
			break;

		default:
			mj++;
		}
		patptr++;
	}

	if (rmagical && mj > 0) {
		rmcptr->mc_type = LITCHAR;
		if ((rmcptr->rstr = malloc(mj + 1)) == NULL) {
			msg_printf("%%Out of memory.");
			status = FALSE;
		}
		strncpy(rmcptr->rstr, patptr - mj, mj);
		rmcptr++;
	}

	rmcptr->mc_type = MCNIL;
	return status;
}

void mcclear(void)
{
	struct magic *mcptr;

	mcptr = &magic_pattern[0];

	while (mcptr->mc_type != MCNIL) {
		if ((mcptr->mc_type & MASKCL) == CCL || (mcptr->mc_type & MASKCL) == NCCL)
			free(mcptr->u.cclmap);
		mcptr++;
	}
	magic_pattern[0].mc_type = reversed_magic_pattern[0].mc_type = MCNIL;
}

void rmcclear(void)
{
	struct magic_replacement *rmcptr;

	rmcptr = &magic_replace_pattern[0];

	while (rmcptr->mc_type != MCNIL) {
		if (rmcptr->mc_type == LITCHAR)
			free(rmcptr->rstr);
		rmcptr++;
	}

	magic_replace_pattern[0].mc_type = MCNIL;
}

static int mceq(int bc, struct magic *mt)
{
	int result;

	bc = bc & 0xFF;
	switch (mt->mc_type & MASKCL) {
	case LITCHAR:
		result = eq(bc, mt->u.lchar);
		break;

	case ANY:
		result = (bc != '\n');
		break;

	case CCL:
		if (!(result = biteq(bc, mt->u.cclmap))) {
			if ((curwp->w_bufp->b_mode & MDEXACT) == 0 && (isletter(bc))) {
				result = biteq(CHCASE(bc), mt->u.cclmap);
			}
		}
		break;

	case NCCL:
		result = !biteq(bc, mt->u.cclmap);

		if ((curwp->w_bufp->b_mode & MDEXACT) == 0 && (isletter(bc))) {
			result &= !biteq(CHCASE(bc), mt->u.cclmap);
		}
		break;

	default:
		msg_printf("mceq: what is %d?", mt->mc_type);
		result = FALSE;
		break;
	}

	return result;
}

extern char *clearbits(void);

static int cclmake(char **ppatptr, struct magic *mcptr)
{
	char *bmap;
	char *patptr;
	int pchr, ochr;

	if ((bmap = clearbits()) == NULL) {
		msg_printf("%%Out of memory");
		return FALSE;
	}

	mcptr->u.cclmap = bmap;
	patptr = *ppatptr;

	if (*++patptr == MC_NCCL) {
		patptr++;
		mcptr->mc_type = NCCL;
	} else
		mcptr->mc_type = CCL;

	if ((ochr = *patptr) == MC_ECCL) {
		msg_printf("%%No characters in character class");
		return FALSE;
	} else {
		if (ochr == MC_ESC)
			ochr = *++patptr;

		setbit(ochr, bmap);
		patptr++;
	}

	while (ochr != '\0' && (pchr = *patptr) != MC_ECCL) {
		switch (pchr) {
		case MC_RCCL:
			if (*(patptr + 1) == MC_ECCL)
				setbit(pchr, bmap);
			else {
				pchr = *++patptr;
				while (++ochr <= pchr)
					setbit(ochr, bmap);
			}
			break;

		case MC_ESC:
			pchr = *++patptr;
		default:
			setbit(pchr, bmap);
			break;
		}
		patptr++;
		ochr = pchr;
	}

	*ppatptr = patptr;

	if (ochr == '\0') {
		msg_printf("%%Character class not ended");
		free(bmap);
		return FALSE;
	}
	return TRUE;
}

static int biteq(int bc, char *cclmap)
{
	bc = bc & 0xFF;
	if (bc >= HICHAR)
		return FALSE;

	return (*(cclmap + (bc >> 3)) & BIT(bc & 7)) ? TRUE : FALSE;
}

static char *clearbits(void)
{
	char *cclstart;
	char *cclmap;
	int i;

	if ((cclmap = cclstart = (char *)malloc(HIBYTE)) != NULL) {
		for (i = 0; i < HIBYTE; i++)
			*cclmap++ = 0;
	}
	return cclstart;
}

static void setbit(int bc, char *cclmap)
{
	bc = bc & 0xFF;
	if (bc < HICHAR)
		*(cclmap + (bc >> 3)) |= BIT(bc & 7);
}
