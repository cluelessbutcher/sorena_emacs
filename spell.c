#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "estruct.h"
#include "globals.h"
#include "efunc.h"
#include "utf8.h"
#include "util.h"

#ifdef HUNSPELL

#include <hunspell.h>

static const char *languages[] = { "en_US", "en_GB" };

static const char *dictionary_path[] = {
	"/usr/share/hunspell", "/usr/share/myspell",
	"/usr/local/share/hunspell", "/usr/local/share/myspell",
};

static Hunhandle *spellers[ARRAY_SIZE(languages)];
static int nspellers;

int spellcheck(const char *word)
{
	for (int i = 0; i < nspellers; i++)
		if (Hunspell_spell(spellers[i], word))
			return 1;

	return nspellers == 0;
}

static void local_dictionary(Hunhandle *handle, const char *filename)
{
	struct stat st;
	if (!stat(filename, &st) && S_ISREG(st.st_mode))
		Hunspell_add_dic(handle, filename);
}

static Hunhandle *open_dictionary(const char *dir, int dirlen, const char *lang)
{
	char aff[NFILEN], dic[NFILEN];

	if (dirlen <= 0)
		return NULL;
	snprintf(aff, sizeof(aff), "%.*s/%s.aff", dirlen, dir, lang);
	snprintf(dic, sizeof(dic), "%.*s/%s.dic", dirlen, dir, lang);
	if (access(aff, R_OK) || access(dic, R_OK))
		return NULL;
	return Hunspell_create(aff, dic);
}

static Hunhandle *find_dictionary(const char *lang)
{
	const char *dicpath = getenv("DICPATH");

	while (dicpath && *dicpath) {
		const char *end = strchr(dicpath, ':');
		int len = end ? end - dicpath : (int)strlen(dicpath);
		Hunhandle *h = open_dictionary(dicpath, len, lang);

		if (h)
			return h;
		dicpath = end ? end + 1 : NULL;
	}
	for (int i = 0; i < ARRAY_SIZE(dictionary_path); i++) {
		Hunhandle *h = open_dictionary(dictionary_path[i],
					       strlen(dictionary_path[i]), lang);
		if (h)
			return h;
	}
	return NULL;
}

void spell_init(void)
{
	const char *home;

	for (int i = 0; i < ARRAY_SIZE(languages); i++) {
		Hunhandle *h = find_dictionary(languages[i]);

		if (h)
			spellers[nspellers++] = h;
	}
	if (nspellers == 0)
		return;

	local_dictionary(spellers[0], ".dictionary");
	home = getenv("HOME");
	if (home) {
		char buf[NFILEN];

		snprintf(buf, sizeof(buf), "%s/.dictionary", home);
		local_dictionary(spellers[0], buf);
	}
}

#else

void spell_init(void)
{
}

int spellcheck(const char *word)
{
	return 1;
}

#endif
