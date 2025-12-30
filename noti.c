#include <xcb/xcb.h>
#include "dat.h"
#include "fn.h"

static xcb_connection_t *xconn;
static xcb_screen_t *scr;
static xcb_window_t wins[Maxnotify];
static xcb_gcontext_t gc;
static Noti notifs[Maxnotify];
static vlong createdat[Maxnotify];
static u32int *img;
static int depth;
static int head, count;

static int
nth(int i)
{
	return (head - count + i + Maxnotify) % Maxnotify;
}

static void
draw(Noti *n)
{
	int i, x;

	for(i = 0; i < Winw * Winh; i++)
		img[i] = Colbg;
	x = Padding;
	for(i = 0; i < n->summary.n && x < Winw - Padding; i++){
		putfont(img, Winw, Winh, x, Padding, n->summary.r[i]);
		x += Fontsz / 2;
	}
	x = Padding;
	for(i = 0; i < n->body.n && x < Winw - Padding; i++){
		putfont(img, Winw, Winh, x, Padding + Fontsz, n->body.r[i]);
		x += Fontsz / 2;
	}
}

static void
redraw(void)
{
	int i, x, y;
	u32int mask, vals[3];

	for(i = 0; i < maxshow; i++){
		if(wins[i]){
			xcb_destroy_window(xconn, wins[i]);
			wins[i] = 0;
		}
	}
	for(i = 0; i < count && i < maxshow; i++){
		x = scr->width_in_pixels - Winw - Margin;
		y = Margin + i * (Winh + 10);
		wins[i] = xcb_generate_id(xconn);
		mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
		vals[0] = Colbg;
		vals[1] = 1;
		vals[2] = XCB_EVENT_MASK_BUTTON_PRESS;
		xcb_create_window(xconn, XCB_COPY_FROM_PARENT, wins[i], scr->root,
			x, y, Winw, Winh, 1,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, scr->root_visual, mask, vals);
		xcb_map_window(xconn, wins[i]);
		draw(&notifs[nth(i)]);
		xcb_put_image(xconn, XCB_IMAGE_FORMAT_Z_PIXMAP, wins[i], gc,
			Winw, Winh, 0, 0, 0, depth, Winw * Winh * 4, (u8int*)img);
	}
	xcb_flush(xconn);
}

static void
push(Noti *n)
{
	notifs[head] = *n;
	createdat[head] = nsec() + (vlong)timeout * 1000000LL;
	head = (head + 1) % Maxnotify;
	if(count < Maxnotify)
		count++;
	if(soundpath != nil && fork() == 0){
		execl("/usr/bin/aplay", "aplay", "-q", soundpath, nil);
		_exit(1);
	}
	redraw();
}

static void
hide(int slot)
{
	if(slot < 0 || slot >= count)
		return;
	for(; slot < count - 1; slot++){
		notifs[nth(slot)] = notifs[nth(slot + 1)];
		createdat[nth(slot)] = createdat[nth(slot + 1)];
	}
	count--;
	redraw();
}

static void
delexpired(void)
{
	vlong now;

	now = nsec();
	while(count > 0){
		if(createdat[nth(0)] >= now)
			break;
		count--;
	}
	redraw();
}

static int
findslotbywin(xcb_window_t win)
{
	int i;

	for(i = 0; i < maxshow; i++)
		if(wins[i] == win)
			return i;
	return -1;
}

static void
wininit(void)
{
	int n;
	xcb_screen_iterator_t iter;

	xconn = xcb_connect(nil, &n);
	if(xconn == nil || xcb_connection_has_error(xconn))
		die("xcb_connect");
	iter = xcb_setup_roots_iterator(xcb_get_setup(xconn));
	while(n-- > 0)
		xcb_screen_next(&iter);
	scr = iter.data;
	if(scr == nil)
		die("no screen");
	depth = scr->root_depth;
	gc = xcb_generate_id(xconn);
	xcb_create_gc(xconn, gc, scr->root, 0, nil);
	img = emalloc(Winw * Winh * sizeof(u32int));
	fontinit(fontpath);
}

void
notithread(void*)
{
	Noti n;
	xcb_generic_event_t *ev;
	int slot;

	threadsetname("noti");
	wininit();
	for(;;){
		while((ev = xcb_poll_for_event(xconn)) != nil){
			if((ev->response_type & ~0x80) == XCB_BUTTON_PRESS){
				slot = findslotbywin(((xcb_button_press_event_t*)ev)->event);
				if(slot >= 0)
					hide(slot);
			}
			free(ev);
		}
		while(nbrecv(notic, &n) > 0)
			push(&n);
		delexpired();
		sleep(100);
	}
}
