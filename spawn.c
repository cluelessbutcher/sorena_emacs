#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"

#include        <signal.h>

static void record_status(int status)
{
	if (status < 0)
		subprocess_status = -1;
	else if (WIFEXITED(status))
		subprocess_status = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
		subprocess_status = 128 + WTERMSIG(status);
}

int cmd_interactive_shell(int f, int n)
{
	char *cp;

	if (restflag)
		return restricted_error();

	movecursor(term.t_nrow, 0);
	ttflush();
	tcapclose();
	tcapkclose();
	if ((cp = getenv("SHELL")) != NULL && *cp != '\0')
		record_status(system(cp));
	else
		record_status(system("exec /bin/sh"));
	screen_garbage = TRUE;
	sleep(2);
	tcapopen();
	tcapkopen();
	return TRUE;
}

int cmd_suspend_emacs(int f, int n)
{
	display_close();

	kill(0, SIGTSTP);
	return TRUE;
}

void rtfrmshell(void)
{
	tcapopen();
	curwp->w_flag = WFHARD;
	screen_garbage = TRUE;
}

int cmd_shell_command(int f, int n)
{
	int s;
	char line[NLINE];

	if (restflag)
		return restricted_error();

	if ((s = ask_string("!", line, NLINE)) != TRUE)
		return s;
	ttflush();
	tcapclose();
	tcapkclose();
	record_status(system(line));
	fflush(stdout);
	tcapopen();

	if (executing_command_line == FALSE) {
		msg_append("(End)");
		ttflush();
		while ((s = tgetc()) != '\r' && s != ' ') ;
		msg_append("\r\n");
	}
	tcapkopen();
	screen_garbage = TRUE;
	return TRUE;
}

int cmd_execute_program(int f, int n)
{
	int s;
	char line[NLINE];

	if (restflag)
		return restricted_error();

	if ((s = ask_string("!", line, NLINE)) != TRUE)
		return s;
	ttputc('\n');
	ttflush();
	tcapclose();
	tcapkclose();
	record_status(system(line));
	fflush(stdout);
	tcapopen();
	msg_append("(End)");
	ttflush();
	while ((s = tgetc()) != '\r' && s != ' ') ;
	screen_garbage = TRUE;
	return TRUE;
}

int cmd_pipe_command(int f, int n)
{
	struct window *wp;
	struct buffer *bp;
	char line[NLINE];
	int s;
	static char bname[] = "command";
	static char filnam[] = "command";

	if (restflag)
		return restricted_error();

	if ((s = ask_string("@", line, NLINE)) != TRUE)
		return s;

	bp = find_buffer(bname, FALSE, 0);
	if (bp != NULL) {
		for (wp = window_head; wp != NULL; wp = wp->w_wndp) {
			if (wp->w_bufp != bp)
				continue;
			if (wp == curwp)
				cmd_delete_window(FALSE, 1);
			else
				cmd_delete_other_windows(FALSE, 1);
			break;
		}
		if (destroy_buffer(bp) != TRUE)
			return FALSE;
	}

	ttflush();
	tcapclose();
	tcapkclose();

	strcat(line, " >");
	strcat(line, filnam);
	record_status(system(line));
	tcapopen();
	tcapkopen();
	ttflush();
	screen_garbage = TRUE;

	if (cmd_split_current_window(FALSE, 1) == FALSE)
		return FALSE;
	if (getfile(filnam, FALSE) == FALSE)
		return FALSE;

	curwp->w_bufp->b_mode |= MDVIEW;
	update_modeline();
	unlink(filnam);
	return TRUE;
}

int cmd_filter_buffer(int f, int n)
{
	int s;
	struct buffer *bp;
	char line[NLINE];
	char tmpnam[NFILEN];
	struct filestate tmpstate;
	static char bname1[] = "fltinp";

	static char filnam1[] = "fltinp";
	static char filnam2[] = "fltout";

	if (restflag)
		return restricted_error();

	if (curbp->b_mode & MDVIEW)
		return readonly_error();

	if ((s = ask_string("#", line, NLINE)) != TRUE)
		return s;

	bp = curbp;
	strcpy(tmpnam, bp->b_fname);
	tmpstate = bp->b_fstate;
	strcpy(bp->b_fname, bname1);

	if (writeout(filnam1) != TRUE) {
		msg_printf("(Cannot write filter file)");
		strcpy(bp->b_fname, tmpnam);
		bp->b_fstate = tmpstate;
		return FALSE;
	}
	ttputc('\n');
	ttflush();
	tcapclose();
	tcapkclose();
	strcat(line, " <fltinp >fltout");
	record_status(system(line));
	tcapopen();
	tcapkopen();
	ttflush();
	screen_garbage = TRUE;
	s = TRUE;

	if (s != TRUE || (readin(filnam2, FALSE) == FALSE)) {
		msg_printf("(Execution failed)");
		strcpy(bp->b_fname, tmpnam);
		bp->b_fstate = tmpstate;
		unlink(filnam1);
		unlink(filnam2);
		return s;
	}

	strcpy(bp->b_fname, tmpnam);
	bp->b_fstate = tmpstate;
	bp->b_flag |= BFCHG;

	unlink(filnam1);
	unlink(filnam2);
	return TRUE;
}
