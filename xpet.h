#pragma once

#include <X11/Xlib.h>

#define VERSION "v0.1"

struct pet {
	Window window;
	char*  name;
	char*  message;
	int    x;
	int    y;
} pet;

struct mouse {
	int    x;
	int    y;
} mouse;
