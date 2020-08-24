#pragma once
#include "frontend/buffer.h"

void displayBuffer(BufferPtr  buf, DisplayMode mode);
void displayCurrentbuf(DisplayMode mode);
void do_color(Linecolor c);
void clear_effect();

// message
void show_message(std::string_view s);
void message(std::string_view s, int return_x, int return_y);
inline void message(const char *s, const TermRect &rect)
{
    auto [x, y] = rect.globalXY();
    message(s, x, y);
}

void disp_err_message(const char *s, int redraw_current);
void disp_message_nsec(const char *s, int redraw_current, int sec, int purge, int mouse);
void disp_message(const char *s, int redraw_current);
void disp_message_nomouse(const char *s, int redraw_current);
void disp_srchresult(int result, std::string_view prompt, std::string_view str);
void set_delayed_message(const char *s);
void record_err_message(const char *s);

void fmTerm(void);
void fmInit(void);
