#include        <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include	"estruct.h"
#include        "globals.h"
#include	"efunc.h"

static FILE *ffp;
static int eofflag;

int file_line_len = 0;
static char *rdbuf = NULL;
static char *wrbuf = NULL;
#define IOBUF (1 << 18)

int file_open_read(char *fn)
{
	struct stat st;

	if ((ffp = fopen(fn, "r")) == NULL)
		return FIOFNF;
	if (fstat(fileno(ffp), &st) == 0 && S_ISDIR(st.st_mode)) {
		fclose(ffp);
		ffp = NULL;
		return FIOFNF;
	}
	if (rdbuf == NULL)
		rdbuf = malloc(IOBUF);
	if (rdbuf)
		setvbuf(ffp, rdbuf, _IOFBF, IOBUF);
	eofflag = FALSE;
	return FIOSUC;
}

int file_open_write(char *fn)
{
	if ((ffp = fopen(fn, "w")) == NULL) {
		msg_printf("Cannot open file for writing");
		return FIOERR;
	}
	if (wrbuf == NULL)
		wrbuf = malloc(IOBUF);
	if (wrbuf)
		setvbuf(ffp, wrbuf, _IOFBF, IOBUF);
	return FIOSUC;
}

int file_close(void)
{
	if (file_line) {
		free(file_line);
		file_line = NULL;
		file_line_size = 0;
	}
	eofflag = FALSE;
	if (ffp == NULL)
		return FIOSUC;
	if (fclose(ffp) != FALSE) {
		ffp = NULL;
		msg_printf("Error closing file");
		return FIOERR;
	}
	ffp = NULL;
	return FIOSUC;
}

int file_put_line(char *buf, int nbuf)
{
	if (nbuf > 0)
		fwrite(buf, 1, nbuf, ffp);
	putc_unlocked('\n', ffp);
	if (ferror(ffp)) {
		msg_printf("Write I/O error");
		return FIOERR;
	}
	return FIOSUC;
}

int file_get_line(void)
{
	ssize_t n;
	size_t sz = file_line_size;

	if (eofflag)
		return FIOEOF;
	n = getline(&file_line, &sz, ffp);
	file_line_size = sz;
	if (n < 0) {
		if (ferror(ffp)) {
			msg_printf("File read error");
			return FIOERR;
		}
		file_line_len = 0;
		return FIOEOF;
	}
	if (n > 0 && file_line[n - 1] == '\n')
		n--;
	else
		eofflag = TRUE;
	if (!accept_nulls && memchr(file_line, 0, n) != NULL) {
		ssize_t i, j = 0;
		for (i = 0; i < n; i++)
			if (file_line[i])
				file_line[j++] = file_line[i];
		n = j;
	}
	file_line[n] = 0;
	file_line_len = n;
	return FIOSUC;
}

int file_exists(char *fname)
{
	return access(fname, F_OK) == 0;
}
