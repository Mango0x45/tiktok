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

#include <libintl.h>

#define _(x) gettext(x)

#define NSEC_PER_SEC UINT64_C(1'000'000'000)

#if __APPLE__
#define TIMER_ABSTIME (1)

int clock_nanosleep(clockid_t, int, const struct timespec *, struct timespec *);
#endif

time_t syncs(time_t), syncm(time_t), synch(time_t);

static void
usage(const char *argv0, int code)
{
	fprintf(stderr, _("Usage: %s [-h] [-i interval] [format]\n"), argv0);
	exit(code);
}

int
main(int argc, char **argv)
{
	setlocale(LC_ALL, "");
	bindtextdomain("messages", "./po");
	textdomain("messages");

	char interval = 's';
	const char *argv0 = basename(argv[0]);
	const char *dfmt = "%c", *optstr = "hi:";
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
			usage(argv0, EXIT_SUCCESS);
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
			usage(argv0, EXIT_FAILURE);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 1)
		usage(argv0, EXIT_FAILURE);
	if (argc != 0)
		dfmt = argv[0];

	time_t (*sync)(time_t) =
		  interval == 's' ? syncs
		: interval == 'm' ? syncm
		: interval == 'h' ? synch
		: /* default */     syncs;

	for (;;) {
		struct timespec before, after;
		if (clock_gettime(CLOCK_REALTIME, &before) == -1)
			warn(_("failed to get the time"));

		time_t now = time(NULL);
		struct tm *tm = localtime(&now);

		char buf[256];
		size_t n = strftime(buf, sizeof(buf), dfmt, tm);
		if (n == 0)
		   	warnx(_("buffer too small"));
		puts(buf);

		if (clock_gettime(CLOCK_REALTIME, &after) == -1)
			warn(_("failed to get the time"));

		/* Duration of the clock formatting and printing */
		struct timespec Δ = {
			after.tv_sec  - before.tv_sec,
			after.tv_nsec - before.tv_nsec,
		};
		if (Δ.tv_nsec < 0) {
			Δ.tv_nsec += NSEC_PER_SEC;
			Δ.tv_sec--;
		}

		struct timespec rqtp = {
			.tv_sec  = sync(after.tv_sec),
			.tv_nsec = 0,
		};

		int rv;
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
