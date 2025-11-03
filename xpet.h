#pragma once

#include <X11/Xlib.h>

#define VERSION "v0.1"

#define N_KEYBINDS 3
#define ABS(a) ((a) < 0 ? -(a) : (a))
#define CLAMP(v, l, h) ((v) < (l) ? (l) : ((v) > (h) ? (h) : (v)))

enum state {
	SLEEPING, IDLE,
	N, S, E, W,
	NW, NE, SW, SE,
	DRAGGED,
	HAPPY,
	STATE_LAST
};

struct animation {
	struct frame* frames;
	const char* name;
	int*   frame_durations; /* NULL = use FRAME_DURATION */
	int    n_frames;
	Bool   loop;
};

struct bind {
	KeySym sym;
	unsigned long mask;
};

struct frame {
	Pixmap pix;
	Pixmap mask;
	int    duration;
};

struct mouse {
	int    x;
	int    y;
};

struct pet {
	Window window;
	Bool   chasing;
	Bool   frozen;
	enum state state;
	int    x;
	int    y;
	double subpixel_x;   /* subpixel x */
	double subpixel_y;   /* subpixel y */

	struct animation* current_animation;
	int    current_frame;
	long   frame_time;

	/* random wander state */
	int    target_x;
	int    target_y;
	long   wander_wait; /* time to wait at destination */

	/* sleep state */
	long   frozen_time; /* time spent frozen */

	/* happy state */
	enum state previous_state; /* state before becoming happy */
	long   happy_time; /* time spent happy */

	/* dragging state */
	Bool   dragging;
	int    drag_offset_x;
	int    drag_offset_y;
	Bool   was_chasing;  /* was chasing before drag */
	Bool   was_frozen;   /* was frozen before drag */

	/* speech bubble */
	const char* speech;
	long   speech_time;
	Window bubble_window;
};
