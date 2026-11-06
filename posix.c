#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "utf8.h"

static int kbdflgs;
static int kbdpoll;

static struct termios otermios;
static struct termios ntermios;

#define TBUFSIZ 65536
static char tobuf[TBUFSIZ];

void ttopen(void)
{
	tcgetattr(0, &otermios);

	ntermios = otermios;

	ntermios.c_iflag &= ~(IGNBRK | BRKINT | IGNPAR | PARMRK | INPCK | INLCR | IGNCR | ICRNL | IXON | IXOFF);

	ntermios.c_oflag &= ~(OPOST | ONLCR | OLCUC | OCRNL | ONOCR | ONLRET);

	ntermios.c_lflag &= ~(ISIG | ICANON | XCASE | ECHO | ECHOE | ECHOK
			      | ECHONL | NOFLSH | TOSTOP | ECHOCTL |
			      ECHOPRT | ECHOKE | FLUSHO | PENDIN | IEXTEN);

	ntermios.c_cc[VMIN] = 1;
	ntermios.c_cc[VTIME] = 0;
	tcsetattr(0, TCSADRAIN, &ntermios);

	setvbuf(stdout, &tobuf[0], _IOFBF, TBUFSIZ);

	kbdflgs = fcntl(0, F_GETFL, 0);
	kbdpoll = FALSE;

	shown_row = 999;
	shown_col = 999;
}

void ttclose(void)
{
	tcsetattr(0, TCSADRAIN, &otermios);
}

int ttputc(int c)
{
	char utf8[6];
	int bytes;

	if ((unsigned)c < 0x80) {
		putc_unlocked(c, stdout);
		return 0;
	}
	bytes = unicode_to_utf8(c, utf8);
	fwrite(utf8, 1, bytes, stdout);
	return 0;
}

void ttflush(void)
{
	int status;

	status = fflush(stdout);
	while (status < 0 && errno == EAGAIN) {
		sleep(1);
		status = fflush(stdout);
	}
	if (status < 0)
		exit(15);
}

static struct {
	int nr;
	int pos;
	char buf[8192];
} TT;

static void tt_compact(void)
{
	if (TT.pos) {
		memmove(TT.buf, TT.buf + TT.pos, TT.nr);
		TT.pos = 0;
	}
}

static void pause_read(int pause)
{
	int n;

	tt_compact();
	if (TT.nr >= (int)sizeof(TT.buf))
		return;
	ntermios.c_cc[VMIN] = 0;
	ntermios.c_cc[VTIME] = pause;
	tcsetattr(0, TCSANOW, &ntermios);

	{
		size_t room = sizeof(TT.buf) - (size_t)TT.nr;
		n = read(0, TT.buf + (size_t)TT.nr, room > sizeof(TT.buf) ? 0 : room);
	}

	ntermios.c_cc[VMIN] = 1;
	ntermios.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &ntermios);

	if (n > 0)
		TT.nr += n;
}

void ttpause(void)
{
	if (term.t_pause && !TT.nr)
		pause_read(term.t_pause);
}

static int pushback = -1;

void ttungetc(int c)
{
	if (pushback < 0)
		pushback = c;
}

int ttgetc(void)
{
	unicode_t c;
	int count, bytes = 1, expected;
	char *d;

	if (pushback >= 0) {
		c = pushback;
		pushback = -1;
		return c;
	}

	count = TT.nr;
	while (!count) {
		TT.pos = 0;
		count = read(0, TT.buf, sizeof(TT.buf));
		if (count > 0) {
			TT.nr = count;
			break;
		}
		if (count < 0 && errno == EINTR) {
			checkwinsize();
			count = 0;
			continue;
		}
		if (count == 0 || (count < 0 && errno != EAGAIN))
			emergency_exit(SIGHUP);
		count = 0;
	}

	d = TT.buf + TT.pos;
	c = (unsigned char)d[0];
	if (c != 27 && c < 128)
		goto done;

	if (c == 27)
		expected = 2;
	else if ((c & 0xf8) == 0xf0)
		expected = 4;
	else if ((c & 0xf0) == 0xe0)
		expected = 3;
	else
		expected = 2;

	if (TT.nr < expected) {
		pause_read(1);
		d = TT.buf + TT.pos;
	}

	if (TT.nr > 1) {
		unsigned char second = d[1];

		if (c == 27 && second == '[') {
			bytes = 2;
			c = 128 + 27;
			goto done;
		}
	}
	bytes = utf8_to_unicode(d, 0, TT.nr, &c);

	if (c == 0xa0)
		c = ' ';
 done:
	TT.nr -= bytes;
	TT.pos += bytes;
	if (TT.nr == 0)
		TT.pos = 0;
	return c;
}

int typahead(void)
{
	int x;

	if (ioctl(0, FIONREAD, &x) < 0)
		x = 0;
	return x + TT.nr;
}
