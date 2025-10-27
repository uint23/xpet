/*
 * xpet:
 *
 * your small pet (un)helper who wanders your
 * screen!
 *
 * > uint 2025
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/select.h>

#include <X11/Xlib.h>

#include "xpet.h"
#include "config.h"

void create_window(void);
void get_mouse_pos(void);
void goto_mouse(void);
void setup(void);
void quit(void);
void run(void);
void xsleep(long ms);

Display* dpy = NULL;
Window root;

int scr;
int scr_width;
int scr_height;
int revert_to;

struct mouse mouse;
struct pet pet;

Bool running = False;

void create_window(void)
{
	pet.x = scr_width / 2;
	pet.y = scr_height / 2;

	/* disable other programs managing window */
	XSetWindowAttributes attrs;
	attrs.override_redirect = True;

	pet.window = XCreateWindow(
		dpy, root, pet.x, pet.y, 64, 64, 0,
		CopyFromParent, InputOutput, CopyFromParent, CWOverrideRedirect,
		&attrs
	);

	XMapWindow(dpy, pet.window);
}

void get_mouse_pos(void)
{
	Window root_return;
	Window child_return;
	int ret_x;
	int ret_y;
	int win_x_ret;
	int win_y_ret;
	unsigned int mask_ret;

	XQueryPointer(dpy, root,
			&root_return, &child_return,
			&ret_x, &ret_y, &win_x_ret, &win_y_ret, &mask_ret);
	mouse.x = ret_x;
	mouse.y = ret_y;
}

void goto_mouse(void)
{
	get_mouse_pos();

	/*
	if (abs(mouse.x - pet.x) < 2 && abs(mouse.y - pet.y) < 2) {
	 	return; 
	}
	*/

	int diff_x = (mouse.x - pet.x) / PET_SMOOTH;
	int diff_y = (mouse.y - pet.y) / PET_SMOOTH;

	pet.x += diff_x;
	pet.y += diff_y;

	XMoveWindow(dpy, pet.window, pet.x, pet.y);
}

void setup(void)
{
	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "xpet: failed to open display\n");
		exit(EXIT_FAILURE);
	}

	scr = XDefaultScreen(dpy);
	scr_width = XDisplayWidth(dpy, scr);
	scr_height = XDisplayHeight(dpy, scr);
	root = RootWindow(dpy, scr);

	create_window();
}

void quit(void)
{
	XCloseDisplay(dpy);
	exit(EXIT_SUCCESS);
}

void run(void)
{
	running = True;
	while (running) {
		while (XPending(dpy)) {
			XEvent ev;
			XNextEvent(dpy, &ev);

			if (ev.type == KeyPress) {
				running = False;
			}
		}
		goto_mouse();
		xsleep(16); /* 60 updates/s */
	}
}

void xsleep(long ms)
{
	/* needed to implement own sleep */
	struct timeval time_val;
	time_val.tv_sec = ms / 1000;
	time_val.tv_usec = (ms % 1000) * 1000;
	select(0, NULL, NULL, NULL, &time_val);
}

int main(int argc, char** argv)
{
	(void) argv;
	if (argc > 1) {
		printf(
			"xpets "VERSION"\r\n"
			"uint 2025"
		);
	}

	setup();
	run();
	return EXIT_SUCCESS;
}
