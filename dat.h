#include <u.h>
#include <libc.h>
#include <thread.h>

enum
{
	Fontsz		= 24,
	Fontbase	= 4,
	Nglyphs		= 0x20000,

	Maxnotify	= 16,

	Winw		= 300,
	Winh		= 80,
	Margin		= 20,
	Padding		= 10,

	Colfg		= 0x000000,
	Colbg		= 0xffffea,
};

typedef struct Str Str;
struct Str
{
	Rune	r[256];
	int	n;
};

typedef struct Noti Noti;
struct Noti
{
	Str	summary;
	Str	body;
};

extern Channel *notic;
extern char *fontpath;
extern char *soundpath;
extern int maxshow;
extern int timeout;
