CC = 9c
LD = 9l
CFLAGS = `pkg-config --cflags dbus-1`

LIBS = `pkg-config --libs dbus-1` -lxcb -lXau -lm

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

snoti: $(OBJS)
	$(LD) -o $@ $(OBJS) $(LIBS)

%.o: %.c dat.h fn.h
	$(CC) $(CFLAGS) $<

clean:
	rm -f $(OBJS) snoti
