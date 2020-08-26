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
    struct readbuffer *obuf;
    TextLineList *buf;

    Str tagbuf;
    int limit;
    int maxlimit;

    std::vector<environment> envs;

private:

public:
    void Initialize(TextLineList *buf, readbuffer *obuf, int limit, int indent);

    void PUSH_ENV(HtmlTags cmd);
    void POP_ENV();

    char *title;
    int blank_lines;

    bool need_flushline(Lineprop mode);
    void flushline(int indent, int force, int width);
    void push_render_image(Str str, int width, int limit);
};

void renderTable(struct table *t, int max_width,
                 struct html_feed_environ *h_env, class HtmlContext *seq);


void do_blankline(struct html_feed_environ *h_env,
                  struct readbuffer *obuf, int indent, int indent_incr,
                  int width);
void purgeline(struct html_feed_environ *h_env);

void init_henv(struct html_feed_environ *, struct readbuffer *,
               TextLineList *, int, int);
