#include <xcb/xcb.h>
#include "dat.h"
#include "fn.h"

static xcb_connection_t *xconn;
static xcb_screen_t *scr;
static xcb_window_t wins[Maxnotify];
static xcb_gcontext_t gc;
static Noti notifs[Maxnotify];
static vlong createdat[Maxnotify];
static int expanded[Maxnotify];
static u32int *img;
static int imgcap;
static int depth;
static int head, count;

static int
nth(int i)
{
	return (head - count + i + Maxnotify) % Maxnotify;
}

static int
wraplines(Rune *r, int n, int maxw)
{
	int i, x, adv, lines;

	if(n <= 0)
		return 0;
	x = 0;
	lines = 1;
	for(i = 0; i < n; i++){
		adv = fontadvance(r[i]);
		if(x + adv > maxw && x > 0){
			x = 0;
			lines++;
		}
		x += adv;
	}
	return lines;
}

static int
notiheight(int slot)
{
	int textw, sl, bl, h;
	Noti *n;

	if(!expanded[nth(slot)])
		return Winh;
	n = &notifs[nth(slot)];
	textw = Winw - 2*Padding;
	sl = wraplines(n->summary.r, n->summary.n, textw);
	bl = wraplines(n->body.r, n->body.n, textw);
	if(sl < 1) sl = 1;
	if(bl < 1) bl = 1;
	h = 2*Padding + (sl + bl) * Lineh;
	if(h < Winh)
		h = Winh;
	return h;
}

static void
drawline(u32int *buf, int w, int h, int x, int y, Rune *r, int n, int maxw)
{
	int i, adv, used, ellaw;

	used = 0;
	for(i = 0; i < n; i++)
		used += fontadvance(r[i]);
	if(used <= maxw){
		for(i = 0; i < n; i++){
			putfont(buf, w, h, x, y, r[i]);
			x += fontadvance(r[i]);
		}
		return;
	}
	ellaw = fontadvance(0x2026);
	used = 0;
	for(i = 0; i < n; i++){
		adv = fontadvance(r[i]);
		if(used + adv + ellaw > maxw)
			break;
		putfont(buf, w, h, x, y, r[i]);
		x += adv;
		used += adv;
	}
	putfont(buf, w, h, x, y, 0x2026);
}

static void
drawwrap(u32int *buf, int w, int h, int x0, int y0, Rune *r, int n, int maxw)
{
	int i, x, y, adv;

	x = x0;
	y = y0;
	for(i = 0; i < n; i++){
		adv = fontadvance(r[i]);
		if(x + adv > x0 + maxw && x > x0){
			x = x0;
			y += Lineh;
		}
		if(y >= h)
			break;
		putfont(buf, w, h, x, y, r[i]);
		x += adv;
	}
}

static void
draw(Noti *n, int wh, int isexpanded)
{
	int i, total, textw, y, sl;

	total = Winw * wh;
	if(total > imgcap){
		free(img);
		img = emalloc(total * sizeof(u32int));
		imgcap = total;
	}
	for(i = 0; i < total; i++)
		img[i] = Colbg;
	textw = Winw - 2*Padding;
	y = Padding;
	if(isexpanded){
		sl = wraplines(n->summary.r, n->summary.n, textw);
		if(sl < 1) sl = 1;
		drawwrap(img, Winw, wh, Padding, y, n->summary.r, n->summary.n, textw);
		y += sl * Lineh;
		drawwrap(img, Winw, wh, Padding, y, n->body.r, n->body.n, textw);
	} else {
		drawline(img, Winw, wh, Padding, y, n->summary.r, n->summary.n, textw);
		y += Lineh;
		drawline(img, Winw, wh, Padding, y, n->body.r, n->body.n, textw);
	}
}

static void
redraw(void)
{
	int i, x, y, wh;
	u32int mask, vals[3];

	for(i = 0; i < Maxnotify; i++){
		if(wins[i]){
			xcb_destroy_window(xconn, wins[i]);
			wins[i] = 0;
		}
	}
	y = Margin;
	for(i = 0; i < count && i < maxshow; i++){
		wh = notiheight(i);
		x = scr->width_in_pixels - Winw - Margin;
		wins[i] = xcb_generate_id(xconn);
		mask = XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK;
		vals[0] = Colbg;
		vals[1] = 1;
		vals[2] = XCB_EVENT_MASK_BUTTON_PRESS;
		xcb_create_window(xconn, XCB_COPY_FROM_PARENT, wins[i], scr->root,
			x, y, Winw, wh, 1,
			XCB_WINDOW_CLASS_INPUT_OUTPUT, scr->root_visual, mask, vals);
		xcb_map_window(xconn, wins[i]);
		draw(&notifs[nth(i)], wh, expanded[nth(i)]);
		xcb_put_image(xconn, XCB_IMAGE_FORMAT_Z_PIXMAP, wins[i], gc,
			Winw, wh, 0, 0, 0, depth, Winw * wh * 4, (u8int*)img);
		y += wh + Gap;
	}
	xcb_flush(xconn);
}

static void
emitclose(u32int id, u32int reason)
{
	CloseEv ev;

	ev.id = id;
	ev.reason = reason;
	nbsend(closec, &ev);
}

static void
push(Noti *n)
{
	int i;
	CloseEv ev;

	if(n->id != 0){
		for(i = 0; i < count; i++){
			if(notifs[nth(i)].id == n->id){
				notifs[nth(i)] = *n;
				createdat[nth(i)] = nsec() + (vlong)timeout * 1000000LL;
				expanded[nth(i)] = 0;
				redraw();
				return;
			}
		}
	}
	if(count == Maxnotify){
		ev.id = notifs[head].id;
		ev.reason = 4;
		nbsend(closec, &ev);
	}
	notifs[head] = *n;
	createdat[head] = nsec() + (vlong)timeout * 1000000LL;
	expanded[head] = 0;
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
hide(int slot, int reason)
{
	if(slot < 0 || slot >= count)
		return;
	if(reason > 0)
		emitclose(notifs[nth(slot)].id, reason);
	for(; slot < count - 1; slot++){
		notifs[nth(slot)] = notifs[nth(slot + 1)];
		createdat[nth(slot)] = createdat[nth(slot + 1)];
		expanded[nth(slot)] = expanded[nth(slot + 1)];
	}
	head = (head - 1 + Maxnotify) % Maxnotify;
	count--;
	redraw();
}

static void
closebyid(u32int id)
{
	int i;

	for(i = 0; i < count; i++){
		if(notifs[nth(i)].id == id){
			hide(i, 3);
			return;
		}
	}
}

static void
delexpired(void)
{
	vlong now;
	int c0;

	now = nsec();
	c0 = count;
	while(count > 0){
		if(expanded[nth(0)])
			break;
		if(createdat[nth(0)] >= now)
			break;
		emitclose(notifs[nth(0)].id, 1);
		count--;
	}
	if(count != c0)
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
	fontinit(fontpath);
}

void
notithread(void*)
{
	Noti n;
	xcb_generic_event_t *ev;
	xcb_button_press_event_t *be;
	u32int closeid;
	int slot;

	threadsetname("noti");
	wininit();
	for(;;){
		while((ev = xcb_poll_for_event(xconn)) != nil){
			if((ev->response_type & ~0x80) == XCB_BUTTON_PRESS){
				be = (xcb_button_press_event_t*)ev;
				slot = findslotbywin(be->event);
				if(slot >= 0){
					if(be->detail == 1){
						expanded[nth(slot)] = !expanded[nth(slot)];
						redraw();
					} else if(be->detail == 3){
						hide(slot, 2);
					}
				}
			}
			free(ev);
		}
		while(nbrecv(notic, &n) > 0)
			push(&n);
		while(nbrecv(closereqc, &closeid) > 0)
			closebyid(closeid);
		delexpired();
		sleep(100);
	}
}
