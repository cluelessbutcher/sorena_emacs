#include <stdio.h>

#define	maindef

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "ebind.h"
#include "version.h"
#include "util.h"
#include "utf8.h"
#include "line.h"

#include <signal.h>
static void emergencyexit(int);
extern void sizesignal(int);

void usage(int status)
{
	printf("Usage: %s [options] [file ...]\n\n", PROGRAM_NAME);
	fputs("      +<n>        start at line <n>\n", stdout);
	fputs("      -g<n>       go to line <n>\n", stdout);
	fputs("      -s<text>    search for <text> at startup\n", stdout);
	fputs("      -v          open files read-only (view mode)\n", stdout);
	fputs("      -e          open files for editing (default)\n", stdout);
	fputs("      -n          accept NUL characters in files\n", stdout);
	fputs("      -r          restricted mode (no shell, no file switching)\n", stdout);
	fputs("      @<file>     run <file> as the startup file\n", stdout);
	fputs("      --help      display this help and exit\n", stdout);
	fputs("      --version   output version information and exit\n\n", stdout);
	fputs("Inside the editor press C-h ? (or F1) for help, C-h b for all key bindings.\n", stdout);

	exit(status);
}

static int is_motion(fn_t fn)
{
	static const fn_t motions[] = {
		cmd_forward_character, cmd_backward_character, cmd_next_line, cmd_previous_line,
		cmd_beginning_of_line, cmd_end_of_line, cmd_next_word, cmd_previous_word,
		cmd_next_page, cmd_previous_page, cmd_beginning_of_file, cmd_end_of_file,
		cmd_next_paragraph, cmd_previous_paragraph, cmd_smart_home, cmd_back_to_indentation,
		cmd_forward_sexp, cmd_backward_sexp, cmd_beginning_of_defun, cmd_end_of_defun,
		cmd_goto_bracket, cmd_goto_matching_fence, NULL
	};
	int i;

	for (i = 0; motions[i]; i++)
		if (motions[i] == fn)
			return TRUE;
	return FALSE;
}

static int do_paste(void)
{
	int c, prev = 0, view = (curbp->b_mode & MDVIEW) != 0;
	char *buf = NULL;
	long len = 0, cap = 0;

	while ((c = paste_read_char()) >= 0) {
		char u[8];
		int n;
		if (view || c == 0)
			continue;
		if (c == '\n' && prev == '\r') {
			prev = c;
			continue;
		}
		prev = c;
		if (c == '\r')
			c = '\n';
		n = unicode_to_utf8(c, u);
		if (len + n > cap) {
			char *nb;
			cap = cap ? cap * 2 : 4096;
			nb = realloc(buf, cap);
			if (nb == NULL)
				break;
			buf = nb;
		}
		memcpy(buf + len, u, n);
		len += n;
	}
	if (view) {
		free(buf);
		return readonly_error();
	}
	if (region_active)
		delete_active_region();
	if (len > 0)
		insert_bytes(buf, len);
	free(buf);
	return TRUE;
}

int main(int argc, char **argv)
{
	int c = -1;
	int f;
	int n;
	int mflag;
	struct buffer *bp;
	int firstfile;
	int carg;
	int startflag;
	struct buffer *firstbp = NULL;
	int basec;
	int viewflag;
	int gotoflag;
	int gline = 0;
	int searchflag;
	int saveflag;
	int errflag;
	char bname[NBUFN];

	spell_init();

	struct sigaction winch = { .sa_handler = sizesignal };
	sigemptyset(&winch.sa_mask);
	sigaction(SIGWINCH, &winch, NULL);
	if (argc == 2) {
		if (strcmp(argv[1], "--help") == 0) {
			usage(EXIT_FAILURE);
		}
		if (strcmp(argv[1], "--version") == 0) {
			version();
			exit(EXIT_SUCCESS);
		}
	}

	display_open();
	edinit("*scratch*");
	varinit();

	viewflag = FALSE;
	gotoflag = FALSE;
	searchflag = FALSE;
	firstfile = TRUE;
	startflag = FALSE;
	errflag = FALSE;

	for (carg = 1; carg < argc; ++carg) {
		if (argv[carg][0] == '+') {
			gotoflag = TRUE;
			gline = atoi(&argv[carg][1]);
		} else if (argv[carg][0] == '-') {
			switch (argv[carg][1]) {
			case 'a':
			case 'A':
				errflag = TRUE;
				break;
			case 'e':
			case 'E':
				viewflag = FALSE;
				break;
			case 'g':
			case 'G':
				gotoflag = TRUE;
				gline = atoi(&argv[carg][2]);
				break;
			case 'n':
			case 'N':
				accept_nulls = TRUE;
				break;
			case 'r':
			case 'R':
				restflag = TRUE;
				break;
			case 's':
			case 'S':
				searchflag = TRUE;
				strncpy(search_pattern, &argv[carg][2], NPAT);
				break;
			case 'v':
			case 'V':
				viewflag = TRUE;
				break;
			default:

				break;
			}
		} else if (argv[carg][0] == '@') {
			if (startup(&argv[carg][1]) == TRUE)

				startflag = TRUE;
		} else {
			makename(bname, argv[carg]);
			unique_buffer_name(bname);

			bp = find_buffer(bname, TRUE, 0);
			strcpy(bp->b_fname, argv[carg]);
			bp->b_active = FALSE;
			if (firstfile) {
				firstbp = bp;
				firstfile = FALSE;
			}

			if (viewflag)
				bp->b_mode |= MDVIEW;
		}
	}

	signal(SIGHUP, emergencyexit);
	signal(SIGTERM, emergencyexit);

	if (errflag) {
		if (startup("error.cmd") == TRUE)
			startflag = TRUE;
	}

	if (startflag == FALSE) {
		startup("");
		startflag = TRUE;
	}
	display_commands = TRUE;

	bp = find_buffer("*scratch*", FALSE, 0);
	if (firstfile == FALSE && (global_flags & GFREAD)) {
		swbuffer(firstbp);
		destroy_buffer(bp);
	} else
		bp->b_mode |= global_mode;

	if (gotoflag && searchflag) {
		update();
		msg_printf("(Can not search and goto at the same time!)");
	} else if (gotoflag) {
		if (cmd_goto_line(TRUE, gline) == FALSE) {
			update();
			msg_printf("(Bogus goto argument)");
		}
	} else if (searchflag) {
		if (cmd_hunt_forward(FALSE, 0) == FALSE)
			update();
	}

	lastflag = 0;

 loop:

	saveflag = lastflag;
	execute(HOOK_CMD, FALSE, 1);
	lastflag = saveflag;

	if (curwp->w_flag || !typahead())
		update();
	c = getcmd();
	if (c == 0)
		goto loop;

	if (message_present != FALSE) {
		msg_erase();
		update();
	}
	f = FALSE;
	n = 1;

	basec = c & ~META;
	if ((c & META) && ((basec >= '0' && basec <= '9') || basec == '-')) {
		f = TRUE;
		n = 0;
		mflag = 1;
		c = basec;
		while ((c >= '0' && c <= '9') || (c == '-')) {
			if (c == '-') {
				if ((mflag == -1) || (n != 0))
					break;
				mflag = -1;
			} else {
				n = n * 10 + (c - '0');
			}
			if ((n == 0) && (mflag == -1))
				msg_printf("Arg:");
			else
				msg_printf("Arg: %d", n * mflag);

			c = getcmd();
		}
		n = n * mflag;
	}

	if (c == repeat_key) {
		f = TRUE;
		n = 4;
		mflag = 0;
		msg_printf("Arg: 4");
		while (((c = getcmd()) >= '0' && c <= '9') || c == repeat_key || c == '-') {
			if (c == repeat_key)
				if ((n > 0) == ((n * 4) > 0))
					n = n * 4;
				else
					n = 1;

			else if (c == '-') {
				if (mflag)
					break;
				n = 0;
				mflag = -1;
			}

			else {
				if (!mflag) {
					n = 0;
					mflag = 1;
				}
				n = 10 * n + c - '0';
			}
			msg_printf("Arg: %d", (mflag >= 0) ? n : (n ? -n : -1));
		}

		if (mflag == -1) {
			if (n == 0)
				n++;
			n = -n;
		}
	}

	{
		static int last_selfins = FALSE;
		static int coalesce = 0;
		fn_t bound = getbind(c);
		int selfins = bound == NULL && !f && ((c >= 0x20 && c <= 0x7E) || (c >= 0xA0 && c <= 0x10FFFF));
		struct window *owp = curwp;
		struct buffer *obp = curbp;

		if (!(selfins && last_selfins && coalesce < 20 && c != ' ')) {
			undo_boundary();
			coalesce = 0;
		}
		if (selfins)
			coalesce++;
		last_selfins = selfins;
		undo_cmd_lp = curwp->w_dotp;
		undo_cmd_off = curwp->w_doto;
		undo_cmd_bp = curbp;
		buffer_was_modified = FALSE;

		execute(c, f, n);

		undo_cmd_bp = NULL;
		undo_cmd_lp = NULL;
		if (region_active) {
			if (curwp != owp || curbp != obp || curwp->w_markp == NULL)
				region_active = FALSE;
			else if (buffer_was_modified && !(lastflag & CFKEEPR))
				region_active = FALSE;
			else if (region_shift && !(c & SHIFT) && bound != NULL && is_motion(bound))
				region_active = FALSE;
		}
		if (!region_active)
			region_shift = FALSE;
	}
	goto loop;
}

void edinit(char *bname)
{
	struct buffer *bp;
	struct window *wp;

	bp = find_buffer(bname, TRUE, 0);
	list_buffer = find_buffer("*List*", TRUE, BFINVS);
	wp = (struct window *)malloc(sizeof(struct window));
	if (bp == NULL || wp == NULL || list_buffer == NULL)
		exit(1);
	curbp = bp;
	window_head = wp;
	curwp = wp;
	wp->w_wndp = NULL;
	wp->w_bufp = bp;
	bp->b_nwnd = 1;
	wp->w_linep = bp->b_linep;
	wp->w_dotp = bp->b_linep;
	wp->w_doto = 0;
	wp->w_markp = NULL;
	wp->w_marko = 0;
	wp->w_toprow = 0;
	wp->w_ntrows = term.t_nrow - 1;
	wp->w_force = 0;
	wp->w_flag = WFMODE | WFHARD;
}

static void autosave(void)
{
	if (curbp->b_mode & MDVIEW)
		return;
	if (curbp->b_fname[0] == 0) {
		msg_printf("(No file name, not autosaving)");
		return;
	}
	if (curbp->b_flag & BFTRUNC) {
		msg_printf("(%s was truncated, not autosaving)", curbp->b_fname);
		return;
	}
	if (file_changed(curbp, curbp->b_fname)) {
		msg_printf("(%s changed on disk, not autosaving)", curbp->b_fname);
		return;
	}
	cmd_update_screen(FALSE, 0);
	cmd_save_file(FALSE, 0);
}

int execute(int c, int f, int n)
{
	int status;
	fn_t execfunc;

	execfunc = getbind(c);
	if (execfunc != NULL) {
		thisflag = 0;
		status = (*execfunc) (f, n);
		lastflag = thisflag;
		return status;
	}

	if (c & SHIFT) {
		fn_t base = getbind(c & ~SHIFT);
		if (base != NULL && is_motion(base)) {
			if (!region_active || curwp->w_markp == NULL) {
				curwp->w_markp = curwp->w_dotp;
				curwp->w_marko = curwp->w_doto;
				region_active = TRUE;
				region_shift = TRUE;
			}
			thisflag = 0;
			status = (*base) (f, n);
			thisflag |= CFKEEPR;
			lastflag = thisflag;
			return status;
		}
		if (base != NULL) {
			thisflag = 0;
			status = (*base) (f, n);
			lastflag = thisflag;
			return status;
		}
	}

	if (c == (SPEC | '~')) {
		thisflag = 0;
		status = do_paste();
		lastflag = thisflag;
		return status;
	}

	if (c == ' ' && (curwp->w_bufp->b_mode & MDWRAP) && fill_column > 0 &&
	    n >= 0 && getccol(FALSE) > fill_column && (curwp->w_bufp->b_mode & MDVIEW) == FALSE)
		execute(HOOK_WRAP, FALSE, 1);

	if ((c >= 0x20 && c <= 0x7E)
	    ||(c >= 0xA0 && c <= 0x10FFFF)) {
		if (n <= 0) {
			lastflag = 0;
			return n < 0 ? FALSE : TRUE;
		}
		thisflag = 0;

		status = self_insert(c, n);

		if (curbp->b_mode & MDASAVE)
			if (--autosave_countdown == 0) {
				autosave();
				autosave_countdown = autosave_interval;
			}

		lastflag = thisflag;
		return status;
	}
	tcapbeep();
	msg_printf("(Key not bound)");
	lastflag = 0;
	return FALSE;
}

int cmd_quick_exit(int f, int n)
{
	struct buffer *bp;
	struct buffer *oldcb;
	int status;

	oldcb = curbp;

	bp = buffer_head;
	while (bp != NULL) {
		if ((bp->b_flag & BFCHG) != 0
		    && (bp->b_flag & BFTRUNC) == 0
		    && (bp->b_flag & BFINVS) == 0) {
			curbp = bp;
			msg_printf("(Saving %s)", bp->b_fname);
			if ((status = cmd_save_file(f, n)) != TRUE) {
				curbp = oldcb;
				return status;
			}
		}
		bp = bp->b_bufp;
	}
	cmd_exit_emacs(f, n);
	return TRUE;
}

void emergency_exit(int signr)
{
	emergencyexit(signr);
}

static void emergencyexit(int signr)
{
	cmd_quick_exit(FALSE, 0);
	cmd_exit_emacs(TRUE, 0);
}

int cmd_exit_emacs(int f, int n)
{
	int s;

	if (f != FALSE
	    || any_changed_buffers() == FALSE

	    || (s = ask_yesno("Modified buffers exist. Leave anyway")) == TRUE) {
		display_close();
		if (f)
			exit(n);
		else
			exit(0);
	}
	msg_printf("");
	return s;
}

int cmd_begin_macro(int f, int n)
{
	if (keyboard_macro_mode != STOP) {
		msg_printf("%%Macro already active");
		return FALSE;
	}
	msg_printf("(Start macro)");
	keyboard_macro_pos = &keyboard_macro[0];
	keyboard_macro_end = keyboard_macro_pos;
	keyboard_macro_mode = RECORD;
	return TRUE;
}

int cmd_end_macro(int f, int n)
{
	if (keyboard_macro_mode == STOP) {
		msg_printf("%%Macro not active");
		return FALSE;
	}
	if (keyboard_macro_mode == RECORD) {
		msg_printf("(End macro)");
		keyboard_macro_mode = STOP;
	}
	return TRUE;
}

int cmd_execute_macro(int f, int n)
{
	if (keyboard_macro_mode != STOP) {
		msg_printf("%%Macro already active");
		return FALSE;
	}
	if (n <= 0)
		return TRUE;
	keyboard_macro_repeat = n;
	keyboard_macro_mode = PLAY;
	keyboard_macro_pos = &keyboard_macro[0];
	return TRUE;
}

int cmd_abort_command(int f, int n)
{
	tcapbeep();
	keyboard_macro_mode = STOP;
	region_active = FALSE;
	region_shift = FALSE;
	msg_printf("Quit");
	return ABORT;
}

int readonly_error(void)
{
	tcapbeep();
	msg_printf("(Key illegal in VIEW mode)");
	return FALSE;
}

int restricted_error(void)
{
	tcapbeep();
	msg_printf("(That command is RESTRICTED)");
	return FALSE;
}

int cmd_nop(int f, int n)
{
	return TRUE;
}

int cmd_meta_prefix(int f, int n)
{
	return TRUE;
}

int cmd_ctlc_prefix(int f, int n)
{
	return TRUE;
}

int cmd_ctlx_prefix(int f, int n)
{
	return TRUE;
}

int cmd_universal_argument(int f, int n)
{
	return TRUE;
}
