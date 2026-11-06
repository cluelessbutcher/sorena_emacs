#ifndef EVAR_H_
#define EVAR_H_

#define	NVSIZE	10

struct user_variable {
	char u_name[NVSIZE + 1];
	char *u_value;
};

static char *envars[] = {
	"fillcol",
	"pagelen",
	"curcol",
	"curline",
	"ram",
	"flicker",
	"curwidth",
	"cbufname",
	"cfname",
	"sres",
	"debug",
	"status",
	"palette",
	"asave",
	"acount",
	"lastkey",
	"curchar",
	"discmd",
	"version",
	"progname",
	"seed",
	"disinp",
	"nopewline",
	"nopecwline",
	"target",
	"search",
	"replace",
	"match",
	"kill",
	"cmode",
	"gmode",
	"tpause",
	"pending",
	"lwidth",
	"line",
	"gflags",
	"rval",
	"tab",
	"overlap",
	"jump",
};

#define	EVFILLCOL	0
#define	EVPAGELEN	1
#define	EVCURCOL	2
#define	EVCURLINE	3
#define	EVRAM		4
#define	EVFLICKER	5
#define	EVCURWIDTH	6
#define	EVCBUFNAME	7
#define	EVCFNAME	8
#define	EVSRES		9
#define	EVDEBUG		10
#define	EVSTATUS	11
#define	EVPALETTE	12
#define	EVASAVE		13
#define	EVACOUNT	14
#define	EVLASTKEY	15
#define	EVCURCHAR	16
#define	EVDISCMD	17
#define	EVVERSION	18
#define	EVPROGNAME	19
#define	EVSEED		20
#define	EVDISINP	21

#define	EVTARGET	24
#define	EVSEARCH	25
#define	EVREPLACE	26
#define	EVMATCH		27
#define	EVKILL		28
#define	EVCMODE		29
#define	EVGMODE		30
#define	EVTPAUSE	31
#define	EVPENDING	32
#define	EVLWIDTH	33
#define	EVLINE		34
#define	EVGFLAGS	35
#define	EVRVAL		36
#define EVTAB		37
#define EVOVERLAP	38
#define EVSCROLLCOUNT	39
#define EVSCROLL	40

enum function_type {
	NILNAMIC = 0,
	MONAMIC,
	DYNAMIC,
	TRINAMIC,
};

struct user_function {
	char *f_name;
	enum function_type f_type;
};

static struct user_function funcs[] = {
	{ "add", DYNAMIC },
	{ "sub", DYNAMIC },
	{ "tim", DYNAMIC },
	{ "div", DYNAMIC },
	{ "mod", DYNAMIC },
	{ "neg", MONAMIC },
	{ "cat", DYNAMIC },
	{ "lef", DYNAMIC },
	{ "rig", DYNAMIC },
	{ "mid", TRINAMIC },
	{ "not", MONAMIC },
	{ "equ", DYNAMIC },
	{ "les", DYNAMIC },
	{ "gre", DYNAMIC },
	{ "seq", DYNAMIC },
	{ "sle", DYNAMIC },
	{ "sgr", DYNAMIC },
	{ "ind", MONAMIC },
	{ "and", DYNAMIC },
	{ "or", DYNAMIC },
	{ "len", MONAMIC },
	{ "upp", MONAMIC },
	{ "low", MONAMIC },
	{ "tru", MONAMIC },
	{ "asc", MONAMIC },
	{ "chr", MONAMIC },
	{ "gtk", NILNAMIC },
	{ "rnd", MONAMIC },
	{ "abs", MONAMIC },
	{ "sin", DYNAMIC },
	{ "env", MONAMIC },
	{ "bin", MONAMIC },
	{ "exi", MONAMIC },
	{ "fin", MONAMIC },
	{ "ban", DYNAMIC },
	{ "bor", DYNAMIC },
	{ "bxo", DYNAMIC },
	{ "bno", MONAMIC },
	{ "xla", TRINAMIC },
};

#define	UFADD		0
#define	UFSUB		1
#define	UFTIMES		2
#define	UFDIV		3
#define	UFMOD		4
#define	UFNEG		5
#define	UFCAT		6
#define	UFLEFT		7
#define	UFRIGHT		8
#define	UFMID		9
#define	UFNOT		10
#define	UFEQUAL		11
#define	UFLESS		12
#define	UFGREATER	13
#define	UFSEQUAL	14
#define	UFSLESS		15
#define	UFSGREAT	16
#define	UFIND		17
#define	UFAND		18
#define	UFOR		19
#define	UFLENGTH	20
#define	UFUPPER		21
#define	UFLOWER		22
#define	UFTRUTH		23
#define	UFASCII		24
#define	UFCHR		25
#define	UFGTKEY		26
#define	UFRND		27
#define	UFABS		28
#define	UFSINDEX	29
#define	UFENV		30
#define	UFBIND		31
#define	UFEXIST		32
#define	UFFIND		33
#define UFBAND		34
#define UFBOR		35
#define UFBXOR		36
#define	UFBNOT		37
#define	UFXLATE		38

#endif
