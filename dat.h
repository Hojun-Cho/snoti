#include <u.h>
#include <libc.h>
#include <thread.h>

enum
{
	Fontsz		= 24,
	Fontbase	= 4,
	Nglyphs		= 0x20000,

	Maxnotify	= 16,

	Lineh		= Fontsz,

	Winw		= 300,
	Winh		= 80,
	Margin		= 20,
	Padding		= 10,
	Gap		= 10,

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
	u32int	id;
	Str	summary;
	Str	body;
};

typedef struct CloseEv CloseEv;
struct CloseEv
{
	u32int	id;
	u32int	reason;
};

extern Channel *notic;
extern Channel *closec;
extern Channel *closereqc;
extern char *fontpath;
extern char *soundpath;
extern int maxshow;
extern int timeout;
