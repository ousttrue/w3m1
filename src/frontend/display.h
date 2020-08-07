#pragma once
#include "frontend/buffer.h"

enum DisplayMode
{
    B_NORMAL = 0,
    B_FORCE_REDRAW = 1,
    B_REDRAW = 2,
    B_SCROLL = 3,
    B_REDRAW_IMAGE = 4,
};
void displayBuffer(BufferPtr  buf, DisplayMode mode);
void displayCurrentbuf(DisplayMode mode);
void do_color(Linecolor c);
void clear_effect();

// message
void message(const char *s, int return_x, int return_y);
void disp_err_message(const char *s, int redraw_current);
void disp_message_nsec(const char *s, int redraw_current, int sec, int purge, int mouse);
void disp_message(const char *s, int redraw_current);
void disp_message_nomouse(const char *s, int redraw_current);
void set_delayed_message(const char *s);
void record_err_message(const char *s);
