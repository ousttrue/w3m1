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
    unsigned char env;
    int type;
    int count;
    char indent;
};

struct html_feed_environ
{
    struct readbuffer *obuf;
    TextLineList *buf;
    FILE *f;
    Str tagbuf;
    int limit;
    int maxlimit;

// private:
    struct environment *envs;
    int nenv;
    int envc;
    int envc_real;

public:
    environment &currentEnv() const
    {
        return envs[envc];
    }

    char *title;
    int blank_lines;
};

void renderTable(struct table *t, int max_width,
                 struct html_feed_environ *h_env, class HSequence *seq);

void push_render_image(Str str, int width, int limit,
                       struct html_feed_environ *h_env);

void flushline(struct html_feed_environ *h_env, struct readbuffer *obuf,
               int indent, int force, int width);
void do_blankline(struct html_feed_environ *h_env,
                  struct readbuffer *obuf, int indent, int indent_incr,
                  int width);
void purgeline(struct html_feed_environ *h_env);

void save_fonteffect(struct html_feed_environ *h_env,
                     struct readbuffer *obuf);
void restore_fonteffect(struct html_feed_environ *h_env,
                        struct readbuffer *obuf);

int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env, class HSequence *seq);
void HTMLlineproc0(const char *istr, html_feed_environ *h_env, bool internal, class HSequence *seq);
inline void HTMLlineproc1(const char *x, html_feed_environ *y, class HSequence *seq)
{
    HTMLlineproc0(x, y, true, seq);
}
void init_henv(struct html_feed_environ *, struct readbuffer *,
               struct environment *, int, TextLineList *, int, int);
void completeHTMLstream(struct html_feed_environ *, struct readbuffer *, class HSequence *seq);
void SetCurTitle(Str title);
