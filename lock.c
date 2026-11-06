#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"

static struct filelock {
	dev_t dev;
	ino_t ino;
	int fd;
} locks[NLOCKS];
static int numlocks;

int lockchk(char *fname)
{
	struct stat st;
	int fd, i;

	fd = open(fname, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return TRUE;

	if (fstat(fd, &st) < 0) {
		close(fd);
		return TRUE;
	}

	for (i = 0; i < numlocks; i++) {
		if (locks[i].dev == st.st_dev && locks[i].ino == st.st_ino) {
			close(fd);
			return TRUE;
		}
	}

	if (numlocks == NLOCKS) {
		msg_printf("LOCK ERROR: Lock table full");
		close(fd);
		return ABORT;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		close(fd);

		if (errno != EWOULDBLOCK)
			return TRUE;

		if (ask_yesno("File in use, override") == TRUE)
			return TRUE;
		return ABORT;
	}

	locks[numlocks].dev = st.st_dev;
	locks[numlocks].ino = st.st_ino;
	locks[numlocks].fd = fd;
	numlocks++;
	return TRUE;
}
