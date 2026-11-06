#include <stdio.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "evar.h"
#include "line.h"
#include "util.h"
#include "version.h"

#define	MAXVARS	255

static struct user_variable uv[MAXVARS + 1];

void varinit(void)
{
	int i;
	for (i = 0; i < MAXVARS; i++)
		uv[i].u_name[0] = 0;
}

char *eval_function(char *fname)
{
	int fnum;
	int status;
	char *tsp;
	char arg1[NSTRING];
	char arg2[NSTRING];
	char arg3[NSTRING];
	static char result[2 * NSTRING];

	fname[3] = 0;
	mklower(fname, fname);
	for (fnum = 0; fnum < ARRAY_SIZE(funcs); fnum++)
		if (strcmp(fname, funcs[fnum].f_name) == 0)
			break;

	if (fnum == ARRAY_SIZE(funcs))
		return error_text;

	if (funcs[fnum].f_type >= MONAMIC) {
		if ((status = macarg(arg1)) != TRUE)
			return error_text;

		if (funcs[fnum].f_type >= DYNAMIC) {
			if ((status = macarg(arg2)) != TRUE)
				return error_text;

			if (funcs[fnum].f_type >= TRINAMIC)
				if ((status = macarg(arg3)) != TRUE)
					return error_text;
		}
	}

	switch (fnum) {
	case UFADD:
		return itoa(atoi(arg1) + atoi(arg2));
	case UFSUB:
		return itoa(atoi(arg1) - atoi(arg2));
	case UFTIMES:
		return itoa(atoi(arg1) * atoi(arg2));
	case UFDIV:
		return itoa(atoi(arg1) / atoi(arg2));
	case UFMOD:
		return itoa(atoi(arg1) % atoi(arg2));
	case UFNEG:
		return itoa(-atoi(arg1));
	case UFCAT:
		strcpy(result, arg1);
		return strcat(result, arg2);
	case UFLEFT:
		return strncpy(result, arg1, atoi(arg2));
	case UFRIGHT:
		return (strcpy(result, &arg1[(strlen(arg1) - atoi(arg2))]));
	case UFMID:
		return (strncpy(result, &arg1[atoi(arg2) - 1], atoi(arg3)));
	case UFNOT:
		return truth_text(truth_value(arg1) == FALSE);
	case UFEQUAL:
		return truth_text(atoi(arg1) == atoi(arg2));
	case UFLESS:
		return truth_text(atoi(arg1) < atoi(arg2));
	case UFGREATER:
		return truth_text(atoi(arg1) > atoi(arg2));
	case UFSEQUAL:
		return truth_text(strcmp(arg1, arg2) == 0);
	case UFSLESS:
		return truth_text(strcmp(arg1, arg2) < 0);
	case UFSGREAT:
		return truth_text(strcmp(arg1, arg2) > 0);
	case UFIND:
		return getval(arg1, result, sizeof(result));
	case UFAND:
		return truth_text(truth_value(arg1) && truth_value(arg2));
	case UFOR:
		return truth_text(truth_value(arg1) || truth_value(arg2));
	case UFLENGTH:
		return itoa(strlen(arg1));
	case UFUPPER:
		return mkupper(arg1, result);
	case UFLOWER:
		return mklower(arg1, result);
	case UFTRUTH:
		return truth_text(atoi(arg1) == 42);
	case UFASCII:
		return itoa((int)arg1[0]);
	case UFCHR:
		result[0] = atoi(arg1);
		result[1] = 0;
		return result;
	case UFGTKEY:
		result[0] = tgetc();
		result[1] = 0;
		return result;
	case UFRND:
		return itoa((next_random() % abs(atoi(arg1))) + 1);
	case UFABS:
		return itoa(abs(atoi(arg1)));
	case UFSINDEX:
		return itoa(sindex(arg1, arg2));
	case UFENV:
		tsp = getenv(arg1);
		return tsp == NULL ? "" : tsp;
	case UFBIND:
		return transbind(arg1);
	case UFEXIST:
		return truth_text(file_exists(arg1));
	case UFFIND:
		tsp = lookup_file(arg1, TRUE);
		return tsp == NULL ? "" : tsp;
	case UFBAND:
		return itoa(atoi(arg1) & atoi(arg2));
	case UFBOR:
		return itoa(atoi(arg1) | atoi(arg2));
	case UFBXOR:
		return itoa(atoi(arg1) ^ atoi(arg2));
	case UFBNOT:
		return itoa(~atoi(arg1));
	case UFXLATE:
		return xlat(arg1, arg2, arg3);
	}

	exit(-11);
}

char *user_variable(char *vname)
{
	int vnum;

	for (vnum = 0; vnum < MAXVARS; vnum++) {
		if (uv[vnum].u_name[0] == 0)
			return error_text;
		if (strcmp(vname, uv[vnum].u_name) == 0)
			return uv[vnum].u_value;
	}

	return error_text;
}

static char *getkill(void);

char *environment_variable(char *vname)
{
	int vnum;

	for (vnum = 0; vnum < ARRAY_SIZE(envars); vnum++)
		if (strcmp(vname, envars[vnum]) == 0)
			break;

	if (vnum == ARRAY_SIZE(envars)) {
		char *ename = getenv(vname);

		if (ename != NULL)
			return ename;
		else
			return error_text;
	}

	switch (vnum) {
	case EVFILLCOL:
		return itoa(fill_column);
	case EVPAGELEN:
		return itoa(term.t_nrow + 1);
	case EVCURCOL:
		return itoa(getccol(FALSE));
	case EVCURLINE:
		return itoa(getcline());
	case EVRAM:
		return itoa(0);
	case EVFLICKER:
		return truth_text(FALSE);
	case EVCURWIDTH:
		return itoa(term.t_ncol);
	case EVCBUFNAME:
		return curbp->b_bname;
	case EVCFNAME:
		return curbp->b_fname;
	case EVSRES:
		return "NORMAL";
	case EVDEBUG:
		return truth_text(macro_debug);
	case EVSTATUS:
		return truth_text(command_status);
	case EVPALETTE:
		return "";
	case EVASAVE:
		return itoa(autosave_interval);
	case EVACOUNT:
		return itoa(autosave_countdown);
	case EVLASTKEY:
		return itoa(last_key);
	case EVCURCHAR:
		return (curwp->w_dotp->l_used ==
			curwp->w_doto ? itoa('\n') : itoa(lgetc(curwp->w_dotp, curwp->w_doto)));
	case EVDISCMD:
		return truth_text(display_commands);
	case EVVERSION:
		return VERSION;
	case EVPROGNAME:
		return PROGRAM_NAME_LONG;
	case EVSEED:
		return itoa(random_seed);
	case EVDISINP:
		return truth_text(display_input);
	case EVTARGET:
		saveflag = lastflag;
		return itoa(goal_column);
	case EVSEARCH:
		return search_pattern;
	case EVREPLACE:
		return replace_pattern;
	case EVMATCH:
		return (matched_text == NULL) ? "" : matched_text;
	case EVKILL:
		return getkill();
	case EVCMODE:
		return itoa(curbp->b_mode);
	case EVGMODE:
		return itoa(global_mode);
	case EVTPAUSE:
		return itoa(term.t_pause);
	case EVPENDING:
		return truth_text(typahead());
	case EVLWIDTH:
		return itoa(line_length(curwp->w_dotp));
	case EVLINE:
		return getctext();
	case EVGFLAGS:
		return itoa(global_flags);
	case EVRVAL:
		return itoa(subprocess_status);
	case EVTAB:
		return itoa(tabmask + 1);
	case EVOVERLAP:
		return itoa(overlap);
	case EVSCROLLCOUNT:
		return itoa(scroll_lines);
	case EVSCROLL:
		return truth_text(0);
	}
	exit(-12);
}

static char *getkill(void)
{
	int size;
	static char value[NSTRING];

	if (kill_head == NULL)

		value[0] = 0;
	else {
		if (kill_used < NSTRING)
			size = kill_used;
		else
			size = NSTRING - 1;
		memcpy(value, kill_head->d_chunk, size);
		value[size] = 0;
	}

	return value;
}

int cmd_set(int f, int n)
{
	int status;
	struct variable_description vd;
	char var[NVSIZE + 1];
	char value[NSTRING];

	if (executing_command_line == FALSE) {
		status = ask_string("Variable to set: ", &var[0], NVSIZE);
		if (status != TRUE)
			return status;
	} else {
		command_string = token(command_string, var, NVSIZE + 1);
	}

	findvar(var, &vd, NVSIZE + 1);

	if (vd.v_type == -1) {
		msg_printf("%%No such variable as '%s'", var);
		return FALSE;
	}

	if (f == TRUE)
		strcpy(value, itoa(n));
	else {
		status = ask_string("Value: ", &value[0], NSTRING);
		if (status != TRUE)
			return status;
	}

	status = svar(&vd, value);

	return status;
}

void findvar(char *var, struct variable_description *vd, int size)
{
	int vnum;
	int vtype;

	vnum = -1;
 fvar:
	vtype = -1;
	switch (var[0]) {
	case '$':
		for (vnum = 0; vnum < ARRAY_SIZE(envars); vnum++)
			if (strcmp(&var[1], envars[vnum]) == 0) {
				vtype = TKENV;
				break;
			}
		break;

	case '%':
		for (vnum = 0; vnum < MAXVARS; vnum++)
			if (strcmp(&var[1], uv[vnum].u_name) == 0) {
				vtype = TKVAR;
				break;
			}
		if (vnum < MAXVARS)
			break;

		for (vnum = 0; vnum < MAXVARS; vnum++)
			if (uv[vnum].u_name[0] == 0) {
				vtype = TKVAR;
				strcpy(uv[vnum].u_name, &var[1]);
				break;
			}
		break;

	case '&':
		var[4] = 0;
		if (strcmp(&var[1], "ind") == 0) {
			command_string = token(command_string, var, size);
			getval(var, var, size);
			goto fvar;
		}
	}

	vd->v_num = vnum;
	vd->v_type = vtype;
	return;
}

int svar(struct variable_description *var, char *value)
{
	int vnum;
	int vtype;
	int status;
	int c;
	char *sp;

	vnum = var->v_num;
	vtype = var->v_type;

	status = TRUE;
	switch (vtype) {
	case TKVAR:
		if (uv[vnum].u_value != NULL)
			free(uv[vnum].u_value);
		sp = malloc(strlen(value) + 1);
		if (sp == NULL)
			return FALSE;
		strcpy(sp, value);
		uv[vnum].u_value = sp;
		break;

	case TKENV:
		status = TRUE;
		switch (vnum) {
		case EVFILLCOL:
			fill_column = atoi(value);
			break;
		case EVPAGELEN:
			status = cmd_change_screen_size(TRUE, atoi(value));
			break;
		case EVCURCOL:
			status = setccol(atoi(value));
			break;
		case EVCURLINE:
			status = cmd_goto_line(TRUE, atoi(value));
			break;
		case EVRAM:
			break;
		case EVFLICKER:
			break;
		case EVCURWIDTH:
			status = cmd_change_screen_width(TRUE, atoi(value));
			break;
		case EVCBUFNAME:
			strcpy(curbp->b_bname, value);
			curwp->w_flag |= WFMODE;
			break;
		case EVCFNAME:
			strcpy(curbp->b_fname, value);
			curwp->w_flag |= WFMODE;
			break;
		case EVSRES:
			break;
		case EVDEBUG:
			macro_debug = truth_value(value);
			break;
		case EVSTATUS:
			command_status = truth_value(value);
			break;
		case EVASAVE:
			autosave_interval = atoi(value);
			break;
		case EVACOUNT:
			autosave_countdown = atoi(value);
			break;
		case EVLASTKEY:
			last_key = atoi(value);
			break;
		case EVCURCHAR:
			delete_characters(1, FALSE);
			c = atoi(value);
			if (c == '\n')
				insert_newline();
			else
				insert_char(1, c);
			cmd_backward_character(FALSE, 1);
			break;
		case EVDISCMD:
			display_commands = truth_value(value);
			break;
		case EVVERSION:
			break;
		case EVPROGNAME:
			break;
		case EVSEED:
			random_seed = atoi(value);
			break;
		case EVDISINP:
			display_input = truth_value(value);
			break;
		case EVTARGET:
			goal_column = atoi(value);
			thisflag = saveflag;
			break;
		case EVSEARCH:
			strcpy(search_pattern, value);
			rvstrcpy(reversed_pattern, search_pattern);
			mcclear();
			break;
		case EVREPLACE:
			strcpy(replace_pattern, value);
			break;
		case EVMATCH:
			break;
		case EVKILL:
			break;
		case EVCMODE:
			curbp->b_mode = atoi(value);
			curwp->w_flag |= WFMODE;
			break;
		case EVGMODE:
			global_mode = atoi(value);
			break;
		case EVTPAUSE:
			term.t_pause = atoi(value);
			break;
		case EVPENDING:
			break;
		case EVLWIDTH:
			break;
		case EVLINE:
			putctext(value);
		case EVGFLAGS:
			global_flags = atoi(value);
			break;
		case EVRVAL:
			break;
		case EVTAB:
			tabmask = atoi(value) - 1;
			if (tabmask != 0x07 && tabmask != 0x03)
				tabmask = 0x07;
			curwp->w_flag |= WFHARD;
			break;
		case EVOVERLAP:
			overlap = atoi(value);
			break;
		case EVSCROLLCOUNT:
			scroll_lines = atoi(value);
			break;
		case EVSCROLL:
			break;
		}
		break;
	}
	return status;
}

char *itoa(int i)
{
	int digit;
	char *sp;
	int sign;
	static char result[INTWIDTH + 1];

	sign = 1;
	if (i < 0) {
		sign = -1;
		i = -i;
	}

	sp = result + INTWIDTH;
	*sp = 0;
	do {
		digit = i % 10;
		*(--sp) = '0' + digit;
		i = i / 10;
	} while (i);

	if (sign == -1) {
		*(--sp) = '-';
	}

	return sp;
}

int token_type(char *token)
{
	char c;

	c = *token;

	if (c == 0)
		return TKNUL;

	if (c >= '0' && c <= '9')
		return TKLIT;

	switch (c) {
	case '"':
		return TKSTR;

	case '!':
		return TKDIR;
	case '@':
		return TKARG;
	case '#':
		return TKBUF;
	case '$':
		return TKENV;
	case '%':
		return TKVAR;
	case '&':
		return TKFUN;
	case '*':
		return TKLBL;

	default:
		return TKCMD;
	}
}

static char *internal_getval(char *token)
{
	int status;
	struct buffer *bp;
	int blen;
	int distmp;
	static char buf[NSTRING];

	switch (token_type(token)) {
	case TKNUL:
		return "";

	case TKARG:
		getval(token + 1, token, -1);
		distmp = display_commands;
		display_commands = TRUE;
		status = getstring(token, buf, NSTRING, char_to_keycode('\n'));
		display_commands = distmp;
		if (status == ABORT)
			return error_text;
		return buf;

	case TKBUF:

		getval(token + 1, token, -1);
		bp = find_buffer(token, FALSE, 0);
		if (bp == NULL)
			return error_text;

		if (bp->b_nwnd > 0) {
			curbp->b_dotp = curwp->w_dotp;
			curbp->b_doto = curwp->w_doto;
		}

		if (bp->b_linep == bp->b_dotp)
			return error_text;

		blen = bp->b_dotp->l_used - bp->b_doto;
		if (blen >= NSTRING)
			blen = NSTRING - 1;
		strncpy(buf, bp->b_dotp->l_text + bp->b_doto, blen);
		buf[blen] = 0;

		bp->b_dotp = bp->b_dotp->l_fp;
		bp->b_doto = 0;

		if (bp->b_nwnd > 0) {
			curwp->w_dotp = curbp->b_dotp;
			curwp->w_doto = 0;
			curwp->w_flag |= WFMOVE;
		}

		return buf;

	case TKVAR:
		return user_variable(token + 1);
	case TKENV:
		return environment_variable(token + 1);
	case TKFUN:
		return eval_function(token + 1);
	case TKDIR:
		return error_text;
	case TKLBL:
		return error_text;
	case TKLIT:
		return token;
	case TKSTR:
		return token + 1;
	case TKCMD:
		return token;
	}
	return error_text;
}

char *getval(char *token, char *dst, int size)
{
	char *res = internal_getval(token);
	mystrscpy(dst, res, size);
	return dst;
}

int truth_value(char *val)
{
	if (val[0] == 'F')
		return FALSE;
	if (val[0] == 'T')
		return TRUE;

	return (atoi(val) != 0);
}

char *truth_text(int val)
{
	if (val)
		return true_text;
	else
		return false_text;
}

char *mkupper(const char *str, char *res)
{
	char *out = res, c;

	do {
		c = *str++;
		if ('a' <= c && c <= 'z')
			c += 'A' - 'a';
		*out++ = c;
	} while (c);
	return res;
}

char *mklower(const char *str, char *res)
{
	char *out = res, c;

	out = res;
	do {
		c = *str++;
		if ('A' <= c && c <= 'Z')
			c += 'a' - 'A';
		*out++ = c;
	} while (c);
	return res;
}

int abs(int x)
{
	return x < 0 ? -x : x;
}

int next_random(void)
{
	random_seed = abs(random_seed * 1721 + 10007);
	return random_seed;
}

int sindex(char *source, char *pattern)
{
	char *sp;
	char *csp;
	char *cp;

	sp = source;
	while (*sp) {
		cp = pattern;
		csp = sp;
		while (*cp) {
			if (!eq(*cp, *csp))
				break;
			++cp;
			++csp;
		}

		if (*cp == 0)
			return (int)(sp - source) + 1;
		++sp;
	}

	return 0;
}

char *xlat(char *source, char *lookup, char *trans)
{
	char *sp;
	char *lp;
	char *rp;
	static char result[NSTRING];

	sp = source;
	rp = result;
	while (*sp) {
		lp = lookup;
		while (*lp) {
			if (*sp == *lp) {
				*rp++ = trans[lp - lookup];
				goto xnext;
			}
			++lp;
		}

		*rp++ = *sp;

 xnext:	++sp;
	}

	*rp = 0;
	return result;
}
