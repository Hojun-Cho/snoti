#include "dat.h"
#include "fn.h"

enum
{
	Maxrunes = nelem(((Str*)0)->r),
};

void
sinit(Str *s, char *src, int n)
{
	int len;

	s->n = 0;
	while(n > 0 && s->n < Maxrunes){
		len = chartorune(&s->r[s->n], src);
		s->n++;
		src += len;
		n -= len;
	}
}
