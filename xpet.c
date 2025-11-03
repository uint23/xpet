/*
 * xpet:
 *
 * a suckless, X (and hopefully wayland) implementation of
 * xneko and other on screen pets
 *
 * > uint 2025
*/

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/xpm.h>

#include "xpet.h"
#include "config.h"

void create_window(void);
void draw_bubble(void);
void get_mouse_pos(void);
void grab_keys(void);
void hide_speech_bubble(void);
void load_animations(void);
void move_to(int tx, int ty);
enum state find_octant(int dx, int dy);
void on_button_press(XButtonEvent* b);
void on_button_release(XButtonEvent* b);
void on_key(KeySym sym);
void on_motion(XMotionEvent* m);
void pick_random_destination(void);
void quit(void);
void run(void);
void set_pet_state(enum state ns);
void setup(void);
void show_speech_bubble(const char* s);
void step(enum state d, double* step_x, double* step_y);
void update_animation(void);
void wander(void);
Bool walking(enum state s);
void xsleep(long ms);

Display* dpy;
Window root;
int scr;
int scr_width;
int scr_height;
XpmAttributes xpm_attrs;
XFontSet font_set;

struct mouse mouse;
struct pet pet;

void create_window(void)
{
	pet.x = scr_width / 2;
	pet.y = scr_height / 2;
	pet.subpixel_x = pet.x;
	pet.subpixel_y = pet.y;

	XSetWindowAttributes attrs;
	attrs.override_redirect = True;
	pet.window = XCreateWindow(
		dpy, root, pet.x, pet.y, 64, 64, 0, CopyFromParent,
		InputOutput, CopyFromParent, CWOverrideRedirect, &attrs
	);
	struct frame* f = &pet.current_animation->frames[0];
	XShapeCombineMask(
		dpy, pet.window, ShapeBounding,
		0, 0, f->mask, ShapeSet
	);
	XSetWindowBackgroundPixmap(dpy, pet.window, f->pix);
	XSelectInput(
		dpy, pet.window,
		ButtonPressMask | ButtonReleaseMask | PointerMotionMask
	);
	XMapWindow(dpy, pet.window);
	XClearWindow(dpy, pet.window);
	XRaiseWindow(dpy, pet.window);
}

void draw_bubble(void)
{
	if (!pet.speech || !pet.bubble_window) {
		return;
	}

	XClearWindow(dpy, pet.bubble_window);
	GC graphics_context = DefaultGC(dpy, scr);
	XSetForeground(dpy, graphics_context, BlackPixel(dpy, scr));

	if (font_set) {
		XRectangle ink;
		XRectangle log;

		XmbTextExtents(font_set, pet.speech, (int)strlen(pet.speech), &ink, &log);
		XmbDrawString(
			dpy, pet.bubble_window, font_set, graphics_context,
			SPEECH_PAD_X, SPEECH_PAD_Y - log.y, pet.speech, strlen(pet.speech)
		);
	}
	else {
		XFontStruct* f = XQueryFont(dpy, XGContextFromGC(graphics_context));
		int ascent = f ? f->ascent : 12;
		XDrawString(
			dpy, pet.bubble_window, graphics_context,
			SPEECH_PAD_X, SPEECH_PAD_Y + ascent, pet.speech, strlen(pet.speech)
		);

		if (f) {
			XFreeFontInfo(NULL, f, 1);
		}
	}
}

void get_mouse_pos(void)
{
	Window root_ret;
	Window child_ret;
	int x_ret;
	int y_ret;
	int winx_ret ;
	int winy_ret ;
	unsigned int mask_ret;

	XQueryPointer(
		dpy, root, &root_ret, &child_ret,
		&x_ret, &y_ret, &winx_ret, &winy_ret, &mask_ret
	);

	mouse.x = x_ret;
	mouse.y = y_ret;
}

void grab_keys(void)
{
	unsigned int mods[] = {
		0, LockMask, Mod2Mask, LockMask | Mod2Mask
	};

	for (unsigned i = 0; i < N_KEYBINDS; i++) {
		KeyCode code = XKeysymToKeycode(dpy, bindings[i].sym);
		for (unsigned j = 0; j < sizeof(mods) / sizeof(mods[0]); j++) {
			XGrabKey(
				dpy, code, bindings[i].mask | mods[j], root, True,
				GrabModeAsync, GrabModeAsync
			);
		}
	}
}

void hide_speech_bubble(void)
{
	if (pet.bubble_window) {
		XUnmapWindow(dpy, pet.bubble_window);
	}

	pet.speech = NULL;
	pet.speech_time = 0;
}

void load_animations(void)
{
	memset(&xpm_attrs, 0, sizeof(xpm_attrs));
	for (int i = 0; i < STATE_LAST; i++) {
		int frame_count = animations[i].n_frames;
		if (frame_count == 0) {
			char path[512];
			frame_count = 0;

			for (;;) {
				snprintf(path, sizeof(path), "%s/%s/%d.xpm",
					PET_ASSET_DIR, animations[i].name, frame_count
				);

				FILE* f = fopen(path, "r");
				if (!f) {
					break;
				}

				fclose(f);
				frame_count++;
			}

			if (frame_count == 0) {
				fprintf(stderr, "xpet: warning: no frames for '%s'\n",
					animations[i].name
				);
				animations[i].frames = NULL;
				continue;
			}
		}

		animations[i].frames = malloc(sizeof(struct frame) * frame_count);
		if (!animations[i].frames) {
			perror("malloc");
			exit(1);
		}

		for (int frame_index = 0; frame_index < frame_count; frame_index++) {
			char path[512];
			snprintf(path, sizeof(path), "%s/%s/%d.xpm",
				PET_ASSET_DIR, animations[i].name, frame_index
			);

			Pixmap pixmap = 0;
			Pixmap mask = 0;
			int xpm_result = XpmReadFileToPixmap(
				dpy, root, path, &pixmap, &mask, &xpm_attrs
			);

			if (xpm_result != XpmSuccess) {
				fprintf(stderr, "xpet: load failed '%s': %d\n", path, xpm_result);
				exit(EXIT_FAILURE);
			}
			animations[i].frames[frame_index].pix = pixmap;
			animations[i].frames[frame_index].mask = mask;
			animations[i].frames[frame_index].duration =
				animations[i].frame_durations ?
				animations[i].frame_durations[frame_index] : FRAME_DURATION;
		}
		animations[i].n_frames = frame_count;
	}
}

void move_to(int target_x, int target_y)
{
	double delta_x = target_x - pet.subpixel_x;
	double delta_y = target_y - pet.subpixel_y;
	double distance_squared = delta_x * delta_x + delta_y * delta_y;

	if (distance_squared < 4.0) {
		set_pet_state(IDLE);
		return;
	}

	enum state direction = find_octant((int)delta_x, (int)delta_y);
	double step_x = 0;
	double step_y = 0;
	step(direction, &step_x, &step_y);

	if (distance_squared <= (PET_SPEED * PET_SPEED)) {
		pet.subpixel_x = target_x;
		pet.subpixel_y = target_y;
	}
	else {
		pet.subpixel_x += step_x;
		pet.subpixel_y += step_y;
	}

	pet.x = (int)(pet.subpixel_x + (pet.subpixel_x >= 0 ? 0.5 : -0.5));
	pet.y = (int)(pet.subpixel_y + (pet.subpixel_y >= 0 ? 0.5 : -0.5));
	set_pet_state(direction);

	XMoveWindow(dpy, pet.window, pet.x, pet.y);
}

enum state find_octant(int delta_x, int delta_y)
{
	int abs_delta_x = ABS(delta_x);
	int abs_delta_y = ABS(delta_y);

	/* if horizontal movement stronger than vertical */
	if (abs_delta_x > abs_delta_y * 2) {
		return delta_x > 0 ? E : W;
	}

	/* if vertical movement stronger than horizontal */
	if (abs_delta_y > abs_delta_x * 2) {
		return delta_y > 0 ? S : N;
	}

	return delta_x > 0 ?
		(delta_y < 0 ? NE : SE) : (delta_y < 0 ? NW : SW); 
}

void on_button_press(XButtonEvent* b)
{
	if (b->button == Button1) {
		pet.was_chasing = pet.chasing;
		pet.was_frozen = pet.frozen;
		pet.dragging = True;
		pet.drag_offset_x = b->x;
		pet.drag_offset_y = b->y;
		pet.frozen = True;
		pet.chasing = False;
		set_pet_state(DRAGGED);
	}
	else if (b->button == Button3) {
		if (pet.state != HAPPY) {
			pet.previous_state = pet.state;
			set_pet_state(HAPPY);
			pet.happy_time = 0;

			int n = 0;
			while (pet_phrases[n]) {
				n++;
			}

			if (n > 0) {
				show_speech_bubble(pet_phrases[rand() % n]);
			}
		}
	}
}

void on_button_release(XButtonEvent* b)
{
	if (b->button != Button1) {
		return;
	}

	pet.dragging = False;
	pet.frozen = pet.was_frozen;
	pet.chasing = pet.was_chasing;
	pet.subpixel_x = pet.x;
	pet.subpixel_y = pet.y;

	if (pet.was_frozen) {
		set_pet_state(IDLE);
		pet.frozen_time = 0;
	}
	else if (!pet.was_chasing) {
		pick_random_destination();
		set_pet_state(IDLE);
	}
	else {
		set_pet_state(E);
	}
}

void on_key(KeySym sym)
{
	if (sym == bindings[0].sym) { /* chasing */
		pet.chasing = !pet.chasing;
		if (pet.chasing) {
			pet.frozen = False;
			set_pet_state(E);
		}
		else {
			pick_random_destination();
		}
	}
	else if (sym == bindings[1].sym) { /* freeze */
		pet.frozen = !pet.frozen;
		if (pet.frozen) {
			set_pet_state(IDLE);
			pet.frozen_time = 0;
		}
	}
	else if (sym == bindings[2].sym) { /* quit */
		quit();
	}
}

void on_motion(XMotionEvent* m)
{
	(void)m;
	if (!pet.dragging) {
		return;
	}

	get_mouse_pos();
	pet.x = mouse.x - pet.drag_offset_x;
	pet.y = mouse.y - pet.drag_offset_y;

	XMoveWindow(dpy, pet.window, pet.x, pet.y);
	if (pet.speech) {
		show_speech_bubble(pet.speech);
	}
}

void pick_random_destination(void)
{
	int min_x = WANDER_MARGIN;
	int min_y = WANDER_MARGIN;
	int max_x = scr_width - WANDER_MARGIN - 64;
	int max_y = scr_height - WANDER_MARGIN - 64;

	/* clamping */
	if (max_x < min_x) {
		max_x = min_x;
	}

	if (max_y < min_y) {
		max_y = min_y;
	}

	pet.target_x = min_x + (rand() % (max_x - min_x + 1));
	pet.target_y = min_y + (rand() % (max_y - min_y + 1));
	pet.wander_wait = 0;
}

void quit(void)
{
	if (pet.bubble_window) {
		XDestroyWindow(dpy, pet.bubble_window);
	}
	if (font_set) {
		XFreeFontSet(dpy, font_set);
	}
	for (int i = 0; i < STATE_LAST; i++) {
		if (animations[i].frames) {
			for (int j = 0; j < animations[i].n_frames; j++) {
				XFreePixmap(dpy, animations[i].frames[j].pix);
				XFreePixmap(dpy, animations[i].frames[j].mask);
			}
			free(animations[i].frames);
		}
	}
	XCloseDisplay(dpy);
	exit(EXIT_SUCCESS);
}

void run(void)
{
	for (;;) {
		while (XPending(dpy)) {
			XEvent ev;
			XNextEvent(dpy, &ev);
			if (ev.type == KeyPress) {
				on_key(XLookupKeysym(&ev.xkey, 0));
			}
			else if (ev.type == ButtonPress && ev.xbutton.window == pet.window) {
				on_button_press(&ev.xbutton);
			}
			else if (ev.type == ButtonRelease && ev.xbutton.window == pet.window) {
				on_button_release(&ev.xbutton);
			}
			else if (ev.type == MotionNotify && ev.xmotion.window == pet.window) {
				on_motion(&ev.xmotion);
			}
			else if (ev.type == Expose && ev.xexpose.window == pet.bubble_window) {
				draw_bubble();
			}
		}
		if (pet.speech) {
			pet.speech_time += PET_REFRESH;
			if (pet.speech_time >= SPEECH_DURATION) {
				hide_speech_bubble();
			}
		}
		if (pet.state == HAPPY) {
			pet.happy_time += PET_REFRESH;
			if (pet.happy_time >= HAPPY_DURATION) {
				set_pet_state(pet.previous_state);
			}
		}
		else if (pet.dragging) { /* animate only */
		}
		else if (!pet.frozen) {
			if (pet.chasing) {
				get_mouse_pos();
				move_to(mouse.x, mouse.y);
			}
			else {
				wander();
			}
		}
		else {
			pet.frozen_time += PET_REFRESH;
			if (pet.frozen_time >= SLEEP_DELAY && pet.state != SLEEPING) {
				set_pet_state(SLEEPING);
			}
		}

		update_animation();

		/* keep windows on top of others */
		XRaiseWindow(dpy, pet.window);
		if (pet.bubble_window) {
			XRaiseWindow(dpy, pet.bubble_window);
		}

		xsleep(PET_REFRESH);
	}
}

void set_pet_state(enum state new_state)
{
	if (pet.state != new_state) {
		if (walking(pet.state) && walking(new_state)) {
			int current_frame = pet.current_frame;
			long frame_time = pet.frame_time;
			pet.state = new_state;
			pet.current_animation = &animations[new_state];

			if (pet.current_animation->n_frames > 0) {
				pet.current_frame = current_frame % pet.current_animation->n_frames;
				int dur = pet.current_animation->frames[pet.current_frame].duration;
				pet.frame_time = (frame_time < dur) ? frame_time : 0;
			}
			else {
				pet.current_frame = pet.frame_time = 0;
			}
		}
		else {
			pet.state = new_state;
			pet.current_animation = &animations[new_state];
			pet.current_frame = 0;
			pet.frame_time = 0;
		}
	}
	else if (pet.current_animation != &animations[new_state]) {
		pet.current_animation = &animations[new_state];
		if (!walking(new_state)) {
			pet.current_frame = pet.frame_time = 0;
		}
		else if (pet.current_animation->n_frames > 0) {
			pet.current_frame %= pet.current_animation->n_frames;
			int duration = pet.current_animation->frames[pet.current_frame].duration;

			if (pet.frame_time >= duration) {
				pet.frame_time = 0;
			}
		}
	}
}

void setup(void)
{
	/* for utf8 support */
	setlocale(LC_ALL, "");

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fputs("xpet: no display\n", stderr);
		exit(1);
	}

	scr = DefaultScreen(dpy);
	scr_width = DisplayWidth(dpy, scr);
	scr_height = DisplayHeight(dpy, scr);
	root = RootWindow(dpy, scr);

	char** missing; /* missing character sets */
	int n_missing;
	char* def; /* default string */
	const char* patterns[] = {
		"-*-*-medium-r-*-*-14-*-*-*-*-*-*-*",
		"fixed",
		"*",
		NULL
	};

	for (int i = 0; !font_set && patterns[i]; i++) {
		font_set = XCreateFontSet(
			dpy, patterns[i], &missing, &n_missing, &def
		);

		if (font_set && n_missing > 0) {
			XFreeStringList(missing);
		}
	}

	if (!font_set) {
		fputs("xpet: warn: no fontset; non-ascii will fallback\n", stderr);
	}

	srand((unsigned)time(NULL));
	load_animations();

	pet.current_frame = 0;
	pet.frame_time = 0;
	pet.frozen_time = 0;
	pet.happy_time = 0;
	pet.previous_state = IDLE;
	pet.dragging = False;
	pet.was_chasing = False;
	pet.was_frozen = False;
	pet.speech = NULL;
	pet.speech_time = 0;
	pet.bubble_window = 0;
	pet.chasing = False;
	pet.frozen = False;

	set_pet_state(IDLE);
	pick_random_destination();
	create_window();
	grab_keys();
	XSelectInput(dpy, root, KeyPressMask);
}

void show_speech_bubble(const char* s)
{
	if (!s) {
		return;
	}

	if (!font_set) {
		for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
			if (*p & 0x80) {
				/* if no font set, replace it with ":3" */
				s = ":3";
				break;
			}
		}
	}

	pet.speech = s;
	pet.speech_time = 0;
	if (!pet.bubble_window) {
		XSetWindowAttributes attr;
		attr.override_redirect = True;
		attr.background_pixel = WhitePixel(dpy, scr);
		attr.border_pixel = BlackPixel(dpy, scr);

		pet.bubble_window = XCreateWindow(
			dpy, root, 0, 0, 10, 10, 2,
			CopyFromParent, InputOutput, CopyFromParent,
			CWOverrideRedirect | CWBackPixel | CWBorderPixel, &attr
		);
		XSelectInput(dpy, pet.bubble_window, ExposureMask);
	}

	int text_width = 0;
	int text_height = 0;
	if (font_set) {
		XRectangle ink, log;
		XmbTextExtents(font_set, s, (int)strlen(s), &ink, &log);
		text_width = log.width;
		text_height = log.height;
	}
	else {
		GContext gcontext = XGContextFromGC(DefaultGC(dpy, scr));
		XFontStruct* font = XQueryFont(dpy, gcontext);
		if (font) {
			text_width = XTextWidth(font, s, (int)strlen(s));
			text_height = font->ascent + font->descent;
			XFreeFontInfo(NULL, font, 1);
		}
	}

	int bw = CLAMP(text_width + 2 * SPEECH_PAD_X, SPEECH_MIN_W, 1 << 15);
	int bh = CLAMP(text_height + 2 * SPEECH_PAD_Y, SPEECH_MIN_H, 1 << 15);
	int bx = CLAMP(pet.x + 32 - bw / 2, 10, scr_width - 10 - bw);
	int by = CLAMP(pet.y - bh - 10, 10, scr_height - 10 - bh);

	XMoveResizeWindow(dpy, pet.bubble_window, bx, by, bw, bh);
	XMapWindow(dpy, pet.bubble_window);
	draw_bubble();
	XRaiseWindow(dpy, pet.bubble_window);
}

void step(enum state d, double* step_x, double* step_y)
{
	const double s = PET_SPEED;
	const double ds = PET_SPEED * 0.70710678118654752440; /* sqrt(2)^-1 */

	switch (d) {
		case E:  *step_x = s;   *step_y = 0;   break;
		case W:  *step_x = -s;  *step_y = 0;   break;
		case N:  *step_x = 0;   *step_y = -s;  break;
		case S:  *step_x = 0;   *step_y = s;   break;
		case NE: *step_x = ds;  *step_y = -ds; break;
		case SE: *step_x = ds;  *step_y = ds;  break;
		case NW: *step_x = -ds; *step_y = -ds; break;
		case SW: *step_x = -ds; *step_y = ds;  break;
		default: *step_x = 0;   *step_y = 0;   break;
	}
}

void update_animation(void)
{
	if (!pet.current_animation || pet.current_animation->n_frames <= 0) {
		if (pet.state != IDLE) {
			set_pet_state(IDLE);
		}
		return;
	}

	pet.frame_time += PET_REFRESH;
	struct frame* frame_ptr = &pet.current_animation->frames[pet.current_frame];

	if (pet.frame_time >= frame_ptr->duration) {
		pet.frame_time = 0;
		pet.current_frame++;
		if (pet.current_frame >= pet.current_animation->n_frames) {
			pet.current_frame = pet.current_animation->loop ?
				                0 : pet.current_animation->n_frames - 1;
		}
		frame_ptr = &pet.current_animation->frames[pet.current_frame];
		XShapeCombineMask(dpy, pet.window, ShapeBounding, 0, 0, frame_ptr->mask, ShapeSet);
		XSetWindowBackgroundPixmap(dpy, pet.window, frame_ptr->pix);
		XClearWindow(dpy, pet.window);
	}
}

void wander(void)
{
	double delta_x = (double)pet.target_x - pet.subpixel_x;
	double delta_y = (double)pet.target_y - pet.subpixel_y;
	double distance_squared = delta_x * delta_x + delta_y * delta_y;

	if (distance_squared < 4.0) {
		if (pet.wander_wait <= 0) {
			pet.wander_wait =
				WANDER_MIN_WAIT + (rand() % (WANDER_MAX_WAIT - WANDER_MIN_WAIT));
			set_pet_state(IDLE);
		}
		else {
			pet.wander_wait -= PET_REFRESH;
			if (pet.wander_wait <= 0) {
				pick_random_destination();
			}
		}
		return;
	}
	move_to(pet.target_x, pet.target_y);
}

Bool walking(enum state s)
{
	return s == N || s == S || s == E || s == W ||
		   s == NW || s == NE || s == SW || s == SE;
}

void xsleep(long ms)
{
	struct timeval tv = {ms / 1000, (int)((ms % 1000) * 1000)};
	select(0, 0, 0, 0, &tv);
}

int main(int argc, char** argv)
{
	(void)argv;
	if (argc > 1) {
		printf("xpets " VERSION "\n> uint 2025");
		return 0;
	}
	setup();
	run();
	return 0;
}
