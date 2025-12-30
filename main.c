#include <signal.h>
#include "dat.h"
#include "fn.h"

Channel *notic;
char *fontpath = "font";
char *soundpath = nil;
int maxshow = 4;
int timeout = 5000;

int
threadmaybackground(void)
{
	return 1;
}

void
die(char *fmt, ...)
{
	va_list arg;
	char buf[256];

	va_start(arg, fmt);
	vsnprint(buf, sizeof buf, fmt, arg);
	va_end(arg);
	fprint(2, "snoti: %s\n", buf);
	threadexitsall("error");
}

void*
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		die("malloc failed");
	memset(p, 0, n);
	return p;
}

void
usage(void)
{
	fprint(2, "usage: snoti [-n maxshow] [-t timeout] [-s soundpath] [fontpath]\n");
	threadexitsall("usage");
}

void
threadmain(int argc, char **argv)
{
	ARGBEGIN{
	case 'n':
		maxshow = atoi(EARGF(usage()));
		if(maxshow > Maxnotify)
			die("maxshow is too large.");
		break;
	case 's':
		soundpath = EARGF(usage());
		break;
	case 't':
		timeout = atoi(EARGF(usage()));
		break;
	default:
		usage();
	}ARGEND

	if(argc > 0)
		fontpath = argv[0];
	signal(SIGCHLD, SIG_IGN);
	notic = chancreate(sizeof(Noti), 4);
	proccreate(notithread, nil, 16384);
	proccreate(dbusthread, nil, 16384);

	threadexits(nil);
}
