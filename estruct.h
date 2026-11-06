#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>

#define MAXCOL	500
#define MAXROW	500

#define	GFREAD	1

#define CMPL_NONE    0
#define CMPL_FILE    1
#define CMPL_BUFFER  2
#define CMPL_COMMAND 3
#define CMPL_MODE    4
#define CMPL_SEARCH  5

#define	NBINDS	1024
#define NFILEN  1024
#define NBUFN   64
#define NLINE   256
#define	NSTRING	1024
#define NKBDM   256
#define NPAT    256
#define HUGE    1000
#define	NLOCKS	1000
#define	KBLOCK	250

#define CONTROL 0x10000000
#define META    0x20000000
#define CTLX    0x40000000
#define	SPEC	0x80000000
#define SHIFT   0x08000000
#define HOOK_CMD    (SPEC | META | CTLX | CTLC | 'C')
#define HOOK_READ   (SPEC | META | CTLX | CTLC | 'R')
#define HOOK_WRAP   (SPEC | META | CTLX | CTLC | 'W')
#define HOOK_CHANGE (SPEC | META | CTLX | CTLC | 'X')
#define CTLC    0x04000000

#ifdef	FALSE
#undef	FALSE
#endif
#ifdef	TRUE
#undef	TRUE
#endif

#define FALSE   0
#define TRUE    1
#define ABORT   2
#define	FAILED	3

#define	STOP	0
#define	PLAY	1
#define	RECORD	2

#define	DIF		0
#define DELSE		1
#define DENDIF		2
#define DGOTO		3
#define DRETURN		4
#define DENDM		5
#define DWHILE		6
#define	DENDWHILE	7
#define	DBREAK		8
#define DFORCE		9

#define NUMDIRS		10

#define	PTBEG	0
#define	PTEND	1
#define	FORWARD	0
#define REVERSE	1

#define FIOSUC  0
#define FIOFNF  1
#define FIOEOF  2
#define FIOERR  3
#define	FIOMEM	4
#define	FIOFUN	5

#define CFCPCN  0x0001
#define CFKILL  0x0002
#define CFYANK  0x0004
#define CFDABB  0x0008
#define CFMARK  0x0010
#define CFRECN  0x0020
#define CFKEEPR 0x0040

#define	BELL	0x07
#define	TAB	0x09

#define	PATHCHR	':'

#define	INTWIDTH	sizeof(int) * 3

#define	TKNUL	0
#define	TKARG	1
#define	TKBUF	2
#define	TKVAR	3
#define	TKENV	4
#define	TKFUN	5
#define	TKDIR	6
#define	TKLBL	7
#define	TKLIT	8
#define	TKSTR	9
#define	TKCMD	10

#define	nextab(a)	(a & ~tabmask) + (tabmask+1)
#ifdef	abs
#undef	abs
#endif

#ifdef	islower
#undef	islower
#endif

#ifdef	isupper
#undef	isupper
#endif

#define	DIFCASE		0x20

#define isletter(c)	isxletter((0xFF & (c)))
#define islower(c)	isxlower((0xFF & (c)))
#define isupper(c)	isxupper((0xFF & (c)))

#define isxletter(c)	(('a' <= c && 'z' >= c) || ('A' <= c && 'Z' >= c) || (192<=c && c<=255))
#define isxlower(c)	(('a' <= c && 'z' >= c) || (224 <= c && 252 >= c))
#define isxupper(c)	(('A' <= c && 'Z' >= c) || (192 <= c && 220 >= c))

struct window {
	struct window *w_wndp;
	struct buffer *w_bufp;
	struct line *w_linep;
	struct line *w_dotp;
	struct line *w_markp;
	int w_doto;
	int w_marko;
	int w_toprow;
	int w_ntrows;
	char w_force;
	char w_flag;
};

#define WFFORCE 0x01
#define WFMOVE  0x02
#define WFEDIT  0x04
#define WFHARD  0x08
#define WFMODE  0x10

#define	FSTATE_UNKNOWN	0
#define	FSTATE_ABSENT	1
#define	FSTATE_PRESENT	2

struct filestate {
	int fs_what;
	dev_t fs_dev;
	ino_t fs_ino;
	off_t fs_size;
	struct timespec fs_mtim;
};

struct buffer {
	struct buffer *b_bufp;
	struct line *b_dotp;
	struct line *b_markp;
	struct line *b_linep;
	int b_doto;
	int b_marko;
	int b_mode;
	char b_active;
	char b_nwnd;
	char b_flag;
	struct filestate b_fstate;
	struct undo *b_undo;
	int b_indent;
	int b_lang;
	char b_fname[NFILEN];
	char b_bname[NBUFN];
};

#define BFINVS  0x01
#define BFCHG   0x02
#define	BFTRUNC	0x04

#define	NUMMODES	11

#define	MDWRAP	0x0001
#define	MDCMOD	0x0002
#define	MDSPELL	0x0004
#define	MDEXACT	0x0008
#define	MDVIEW	0x0010
#define MDOVER	0x0020
#define MDMAGIC	0x0040
#define	MDASAVE	0x0080
#define MDINDENT 0x0200
#define MDPAIR  0x0400

struct region {
	struct line *r_linep;
	int r_offset;
	long r_size;
};

struct terminal {
	short t_mrow;
	short t_nrow;
	short t_mcol;
	short t_ncol;
	short t_margin;
	short t_scrsiz;
	int t_pause;
};

struct key_tab {
	int k_code;
	int (*k_fp)(int, int);
};

struct name_bind {
	char *n_name;
	int (*n_func)(int, int);
};

struct kill {
	struct kill *d_next;
	char d_chunk[KBLOCK];
};

struct variable_description {
	int v_type;
	int v_num;
};

struct while_block {
	struct line *w_begin;
	struct line *w_end;
	int w_type;
	struct while_block *w_next;
};

#define	BTWHILE		1
#define	BTBREAK		2

#define	CMDBUFLEN	256

#define	IS_ABORT	0x07
#define IS_BACKSP	0x08
#define	IS_TAB		0x09
#define IS_NEWLINE	0x0D
#define	IS_QUOTE	0x11
#define IS_REVERSE	0x12
#define	IS_FORWARD	0x13
#define	IS_QUIT		0x1B
#define	IS_RUBOUT	0x7F

#define	MCNIL		0
#define	LITCHAR		1
#define	ANY		2
#define	CCL		3
#define	NCCL		4
#define	BOL		5
#define	EOL		6
#define	DITTO		7
#define	CLOSURE		256
#define	MASKCL		(CLOSURE - 1)

#define	MC_ANY		'.'
#define	MC_CCL		'['
#define	MC_NCCL		'^'
#define	MC_RCCL		'-'
#define	MC_ECCL		']'
#define	MC_BOL		'^'
#define	MC_EOL		'$'
#define	MC_CLOSURE	'*'
#define	MC_DITTO	'&'
#define	MC_ESC		'\\'

#define	BIT(n)		(1 << (n))
#define	CHCASE(c)	((c) ^ DIFCASE)

#define	HICHAR		256
#define	HIBYTE		HICHAR >> 3

struct magic {
	short int mc_type;
	union {
		int lchar;
		char *cclmap;
	} u;
};

struct magic_replacement {
	short int mc_type;
	char *rstr;
};
