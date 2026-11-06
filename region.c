#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "line.h"

int cmd_kill_region(int f, int n)
{
	int s;
	struct region region;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if ((s = getregion(&region)) != TRUE)
		return s;
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	curwp->w_dotp = region.r_linep;
	curwp->w_doto = region.r_offset;
	return delete_bytes(region.r_size, TRUE);
}

int cmd_copy_region(int f, int n)
{
	struct line *linep;
	int loffs;
	int s;
	struct region region;

	if ((s = getregion(&region)) != TRUE)
		return s;
	if ((lastflag & CFKILL) == 0)
		kdelete();
	thisflag |= CFKILL;
	linep = region.r_linep;
	loffs = region.r_offset;
	while (region.r_size--) {
		if (loffs == line_length(linep)) {
			if ((s = kinsert('\n')) != TRUE)
				return s;
			linep = line_next(linep);
			loffs = 0;
		} else {
			if ((s = kinsert(lgetc(linep, loffs))) != TRUE)
				return s;
			++loffs;
		}
	}
	region_active = FALSE;
	curwp->w_flag |= WFHARD;
	return TRUE;
}

int cmd_case_region_lower(int f, int n)
{
	struct line *linep;
	int loffs;
	int c;
	int s;
	struct region region;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if ((s = getregion(&region)) != TRUE)
		return s;
	buffer_changed(WFHARD);
	linep = region.r_linep;
	loffs = region.r_offset;
	while (region.r_size--) {
		if (loffs == line_length(linep)) {
			linep = line_next(linep);
			loffs = 0;
		} else {
			c = lgetc(linep, loffs);
			if (c >= 'A' && c <= 'Z')
				line_put_byte(linep, loffs, c + 'a' - 'A');
			++loffs;
		}
	}
	return TRUE;
}

int cmd_case_region_upper(int f, int n)
{
	struct line *linep;
	int loffs;
	int c;
	int s;
	struct region region;

	if (curbp->b_mode & MDVIEW)
		return readonly_error();
	if ((s = getregion(&region)) != TRUE)
		return s;
	buffer_changed(WFHARD);
	linep = region.r_linep;
	loffs = region.r_offset;
	while (region.r_size--) {
		if (loffs == line_length(linep)) {
			linep = line_next(linep);
			loffs = 0;
		} else {
			c = lgetc(linep, loffs);
			if (c >= 'a' && c <= 'z')
				line_put_byte(linep, loffs, c - 'a' + 'A');
			++loffs;
		}
	}
	return TRUE;
}

int getregion(struct region *rp)
{
	struct line *flp;
	struct line *blp;
	long fsize;
	long bsize;

	if (curwp->w_markp == NULL) {
		msg_printf("No mark set in this window");
		return FALSE;
	}
	if (curwp->w_dotp == curwp->w_markp) {
		rp->r_linep = curwp->w_dotp;
		if (curwp->w_doto < curwp->w_marko) {
			rp->r_offset = curwp->w_doto;
			rp->r_size = (long)(curwp->w_marko - curwp->w_doto);
		} else {
			rp->r_offset = curwp->w_marko;
			rp->r_size = (long)(curwp->w_doto - curwp->w_marko);
		}
		return TRUE;
	}
	blp = curwp->w_dotp;
	bsize = (long)curwp->w_doto;
	flp = curwp->w_dotp;
	fsize = (long)(line_length(flp) - curwp->w_doto + 1);
	while (flp != curbp->b_linep || line_prev(blp) != curbp->b_linep) {
		if (flp != curbp->b_linep) {
			flp = line_next(flp);
			if (flp == curwp->w_markp) {
				rp->r_linep = curwp->w_dotp;
				rp->r_offset = curwp->w_doto;
				rp->r_size = fsize + curwp->w_marko;
				return TRUE;
			}
			fsize += line_length(flp) + 1;
		}
		if (line_prev(blp) != curbp->b_linep) {
			blp = line_prev(blp);
			bsize += line_length(blp) + 1;
			if (blp == curwp->w_markp) {
				rp->r_linep = blp;
				rp->r_offset = curwp->w_marko;
				rp->r_size = bsize - curwp->w_marko;
				return TRUE;
			}
		}
	}
	msg_printf("Bug: lost mark");
	return FALSE;
}
