#pragma once

#include "readbuffer.h"

#define MAX_UL_LEVEL 9
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
#define UL_SYMBOL_DISC UL_SYMBOL(9)
#define UL_SYMBOL_CIRCLE UL_SYMBOL(10)
#define UL_SYMBOL_SQUARE UL_SYMBOL(11)

#define HR_SYMBOL 26

struct TextLineList;

struct environment
{
    HtmlTags env = HTML_UNKNOWN;
    int type = 0;
    int count = 0;
    int indent = 0;
};

struct html_feed_environ
{
private:
    TextLineList *buf;

public:
    TextLineList *list() const { return buf; }
    struct readbuffer *obuf;
    Str tagbuf;
    int limit;
    int maxlimit;

    std::vector<environment> envs;

public:
    html_feed_environ(readbuffer *obuf, TextLineList *buf, int width, int indent = 0);

    void PUSH_ENV(HtmlTags cmd);
    void POP_ENV();

    char *title;
    int blank_lines;

    bool need_flushline(Lineprop mode);
    void flushline(int indent, int force, int width);
    void push_render_image(Str str, int width, int limit);
    void purgeline();
    void do_blankline(struct readbuffer *obuf, int indent, int indent_incr, int width);
};
