#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CBS_NO_THREADS
#include "cbs.h"

#define streq(x, y) (strcmp(x, y) == 0)

static void
	clean(void),
	extract(void),
	install(void),
	tiktok(void),
	translations(void);
static void *xmalloc(size_t n);
static char *xstrdup(const char *s);

const char *destdir = "", *prefix = "/usr/local";
static int rv = EXIT_SUCCESS;

static inline int
rv_(int n)
{
	if (n != 0 && rv != EXIT_SUCCESS)
		rv = n;
	return n;
}

[[noreturn]] static void
usage(const char *argv0)
{
	fprintf(stderr,
		"Usage: %s [clean | extract | install | tiktok | translations]\n",
		argv0);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	cbsinit(argc, argv);
	rebuild();

	argv[0] = basename(argv[0]);

	const char *ev = getenv("DESTDIR");
	if (ev != nullptr && ev[0] != 0)
		destdir = ev;
	ev = getenv("PREFIX");
	if (ev != nullptr && ev[0] != 0)
		prefix = ev;

	const char *target = "tiktok";
	if (argc == 2)
		target = argv[1];
	else if (argc > 2)
		usage(argv[0]);

	if (streq(target, "clean"))
		clean();
	else if (streq(target, "extract"))
		extract();
	else if (streq(target, "install"))
		install();
	else if (streq(target, "tiktok"))
		tiktok();
	else if (streq(target, "translations"))
		translations();
	else
		usage(argv[0]);

	return rv;
}

void
clean(void)
{
	struct strs cmd = {};
	strspushl(&cmd, "rm", "-f", "tiktok");
	cmdput(cmd);
	rv_(cmdexec(cmd));
	strszero(&cmd);
	strspushl(&cmd, "find", "po", "-name", "*.mo", "-delete");
	cmdput(cmd);
	rv_(cmdexec(cmd));
	strsfree(&cmd);
}

void
extract(void)
{
	struct strs cmd = {};
	strspushl(&cmd, "xgettext", "--from-code=UTF-8", "-k_",
		"-o", "po/tiktok.pot", "main.c");
	cmdput(cmd);
	if (rv_(cmdexec(cmd)) != 0)
		exit(rv);
	strszero(&cmd);
	strspushl(&cmd, "find", "po", "-name", "*.po",
		"-exec", "msgmerge", "{}", "po/tiktok.pot", "-o", "{}", ";");
	cmdput(cmd);
	rv_(cmdexec(cmd));
	strsfree(&cmd);
}

void
install(void)
{
	struct strs cmd = {};
	char bindir[PATH_MAX], mandir[PATH_MAX];
	snprintf(bindir, sizeof(bindir), "%s%s/bin", destdir, prefix);
	snprintf(mandir, sizeof(mandir), "%s%s/share/man/man1", destdir, prefix);
	strspushl(&cmd, "mkdir", "-p", bindir, mandir);
	cmdput(cmd);
	if (rv_(cmdexec(cmd)) != 0)
		exit(rv);
	strszero(&cmd);

	char podirbuf[PATH_MAX];
	const char *ev = getenv("PODIR");

	if (ev == nullptr || *ev == 0) {
		snprintf(podirbuf, sizeof(podirbuf), "%s%s/share/locale",
		         destdir, prefix);
		ev = podirbuf;
	}

	DIR *dirp = opendir("po");
	if (dirp == nullptr)
		err(EXIT_FAILURE, "opendir: po");

	struct dirent *dp;
	while ((dp = readdir(dirp)) != nullptr) {
		/* ‘.’, ‘..’ och filnamn */
		if (strchr(dp->d_name, '.') != nullptr)
			continue;

		char buf[PATH_MAX];
		snprintf(buf, sizeof(buf), "%s/%s/LC_MESSAGES", ev, dp->d_name);
		char *podstdir = xstrdup(buf);
		strspushl(&cmd, "mkdir", "-p", podstdir);
		cmdput(cmd);
		if (rv_(cmdexec(cmd)) != 0)
			exit(rv);
		strszero(&cmd);

		char *filepo, *filemo;
		snprintf(buf, sizeof(buf), "po/%s/LC_MESSAGES/tiktok.po", dp->d_name);
		filepo = xstrdup(buf);
		snprintf(buf, sizeof(buf), "%s/tiktok.mo", podstdir);
		filemo = buf;

		strspushl(&cmd, "msgfmt", filepo, "-o", filemo);
		cmdput(cmd);
		if (rv_(cmdexec(cmd)) != 0)
			exit(rv);

		free(filepo);
		free(podstdir);
		strszero(&cmd);
	}
	if (errno != 0)
		err(EXIT_FAILURE, "readdir: po");

	closedir(dirp);

	strspushl(&cmd, "cp", "tiktok", bindir);
	cmdput(cmd);
	if (rv_(cmdexec(cmd)) != 0)
		exit(rv);
	strszero(&cmd);

	strspushl(&cmd, "cp", "tiktok.1", mandir);
	cmdput(cmd);
	if (rv_(cmdexec(cmd)) != 0)
		exit(rv);
	strsfree(&cmd);
}

void
tiktok(void)
{
	struct strs cmd = {};
	strspushenvl(&cmd, "CC", "cc");
	strspushenvl(&cmd, "CFLAGS", "-Wall", "-Wextra", "-Wpedantic", "-std=c23",
	             "-O3", "-pipe");

	char buf[PATH_MAX];
	const char *ev = getenv("PODIR");

	if (ev == nullptr || *ev == 0) {
		snprintf(buf, sizeof(buf), "%s%s/share/locale", destdir, prefix);
		ev = buf;
	}

	size_t bufsz = sizeof(buf) + sizeof("-DPODIR=\"\"");
	char *dpodir = xmalloc(bufsz);
	snprintf(dpodir, bufsz, "-DPODIR=\"%s\"", ev);
	strspushl(&cmd, dpodir);

#if __APPLE__
	struct strs brewcmd = {};
	char *brewbuf;
	size_t brewbufsz;

	strspushl(&brewcmd, "brew", "--prefix", "gettext");
	if (rv_(cmdexec_read(brewcmd, &brewbuf, &brewbufsz)) != 0)
		exit(rv);
	strsfree(&brewcmd);

	char *dash_I = xmalloc(brewbufsz + sizeof("-I/include"));
	char *dash_L = xmalloc(brewbufsz + sizeof("-L/lib"));

	sprintf(dash_I, "-I%s/include", brewbuf);
	sprintf(dash_L, "-L%s/lib", brewbuf);

	strspushl(&cmd, dash_I, dash_L);
#endif

#if !__GLIBC__
	strspushl(&cmd, "-lintl");
#endif

	strspushl(&cmd, "-o", "tiktok", "main.c");
	cmdput(cmd);
	if (rv_(cmdexec(cmd)) != 0)
		exit(rv);

#if __APPLE__
	free(dash_L);
	free(dash_I);
#endif
	free(dpodir);
	strsfree(&cmd);
}

void
translations(void)
{
	DIR *dirp = opendir("po");
	if (dirp == nullptr)
		err(EXIT_FAILURE, "opendir: po");

	struct dirent *dp;
	struct strs cmd = {};

	while ((dp = readdir(dirp)) != nullptr) {
		/* ‘.’, ‘..’ och filnamn */
		if (strchr(dp->d_name, '.') != nullptr)
			continue;

		char *filepo, *filemo;
		char buf[PATH_MAX];

		snprintf(buf, sizeof(buf), "po/%s/LC_MESSAGES/tiktok.po", dp->d_name);
		filepo = xstrdup(buf);
		snprintf(buf, sizeof(buf), "po/%s/LC_MESSAGES/tiktok.mo", dp->d_name);
		filemo = buf;
		strspushl(&cmd, "msgfmt", filepo, "-o", filemo);
		cmdput(cmd);
		if (rv_(cmdexec(cmd)) != 0)
			exit(rv);
		free(filepo);
		strszero(&cmd);
	}
	if (errno != 0)
		err(EXIT_FAILURE, "readdir: po");

	strsfree(&cmd);
	closedir(dirp);
}

static void *
xmalloc(size_t n)
{
	void *p = malloc(n);
	if (p == nullptr)
		err(EXIT_FAILURE, "xmalloc");
	return p;
}

static char *
xstrdup(const char *s)
{
	char *p = strdup(s);
	if (p == nullptr)
		err(EXIT_FAILURE, "xstrdup");
	return p;
}
