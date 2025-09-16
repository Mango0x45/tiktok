#define _GNU_SOURCE
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <libintl.h>

#define _(x) gettext(x)

#define NSEC_PER_SEC UINT64_C(1'000'000'000)

#if __APPLE__
#define TIMER_ABSTIME (1)

int clock_nanosleep(clockid_t, int, const struct timespec *, struct timespec *);
#endif

time_t syncs(time_t), syncm(time_t), synch(time_t);

[[noreturn]] static void
usage(const char *argv0)
{
	fprintf(stderr,
	        _("Usage: %s [-i interval] [format ...]\n"
	          "       %s -h\n"),
	        argv0, argv0);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	setlocale(LC_ALL, "");
	bindtextdomain("tiktok", PODIR);
	textdomain("tiktok");

	char interval = 's';
	const char *argv0 = argv[0] = basename(argv[0]);
	const char *optstr = "hi:";
	static struct option longopts[] = {
		{"help",     no_argument,       nullptr, 'h'},
		{"interval", required_argument, nullptr, 'i'},
		{nullptr,    0,                 nullptr,  0 },
	};

	for (;;) {
		int opt = getopt_long(argc, argv, optstr, longopts, nullptr);
		if (opt == -1)
			break;

		switch (opt) {
		case 'h':
			execlp("man", "man", "1", argv0, nullptr);
			err(EXIT_FAILURE, "execlp: man");
		case 'i':
			if (strlen(optarg) == 1) {
				interval = *optarg;
				if (interval == 's' || interval == 'm' || interval == 'h')
					break;
			}
			errx(EXIT_FAILURE,
				_("invalid interval ‘%s’\nRead the %s(1) manual page for valid intervals"),
				optarg, argv0);
		default:
			usage(argv0);
		}
	}

	argc -= optind;
	argv += optind;

	char *default_args[] = {"%c"};

	if (argc == 0) {
		argc = 1;
		argv = default_args;
	}

	time_t (*sync)(time_t) =
		  interval == 's' ? syncs
		: interval == 'm' ? syncm
		: interval == 'h' ? synch
		: /* default */     syncs;

	const char *tzorig = getenv("TZ");

	for (;;) {
		struct timespec now, then;
		if (clock_gettime(CLOCK_REALTIME, &now) == -1)
			warn(_("failed to get the time"));

		if (tzorig != nullptr) {
			if (setenv("TZ", tzorig, true) == -1)
				warn(_("failed to set the timezone"));
		} else if (unsetenv("TZ") == -1)
			warn(_("failed to set the timezone"));

		for (int i = 0; i < argc; i++) {
			if (strlen(argv[i]) >= 3 && memcmp("TZ=", argv[i], 3) == 0) {
				if (putenv(argv[i]) == -1)
					warn(_("failed to set the timezone"));
			} else {
				static char *buf;
				static size_t bufsz = 1024;
				if (buf == nullptr && (buf = malloc(bufsz)) == nullptr)
					err(EXIT_FAILURE, "malloc");

				struct tm *tm = localtime(&now.tv_sec);
				size_t n = strftime(buf, bufsz, argv[i], tm);
				while (n == 0) {
					bufsz *= 2;
					if ((buf = realloc(buf, bufsz)) == nullptr)
						err(EXIT_FAILURE, "malloc");
					n = strftime(buf, bufsz, argv[i], tm);
				}
				fputs(buf, stdout);
			}
		}
		putchar('\n');

		if (clock_gettime(CLOCK_REALTIME, &then) == -1)
			warn(_("failed to get the time"));

		int rv;
		struct timespec rqtp = {
			.tv_sec  = sync(then.tv_sec),
			.tv_nsec = 0,
		};
		do
			rv = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &rqtp, nullptr);
		while (rv == -1 && errno == EINTR);
	}

	/* We should never get here */
	return EXIT_FAILURE;
}

time_t
syncs(time_t n)
{
	return n + 1;
}

time_t
syncm(time_t n)
{
	return n + 60 - n%60;
}

time_t
synch(time_t n)
{
	return n + 3600 - n%3600;
}

#if __APPLE__

int
clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *req,
				struct timespec *rem)
{
	/* The rest not implemented */
	assert(flags == TIMER_ABSTIME);

	struct timespec now, Δ;

	if (clock_gettime(clock_id, &now) != 0)
		return errno;

	Δ.tv_sec = req->tv_sec - now.tv_sec;
	Δ.tv_nsec = req->tv_nsec - now.tv_nsec;

	if (Δ.tv_sec < 0) {
		Δ.tv_sec  = 0;
		Δ.tv_nsec = 0;
		goto out;
	}

	if (Δ.tv_nsec < 0) {
		if (Δ.tv_sec == 0) {
			Δ.tv_sec  = 0;
			Δ.tv_nsec = 0;
			goto out;
		}

		Δ.tv_sec--;
		Δ.tv_nsec += NSEC_PER_SEC;
	}

out:
	return nanosleep(&Δ, rem);
}

#endif
