void	die(char*, ...);
void*	emalloc(ulong);

void	sinit(Str*, char*, int);

void	dbusthread(void*);
void	notithread(void*);

void	fontinit(char*);
void	putfont(u32int*, int, int, int, int, Rune);
int	fontadvance(Rune);
