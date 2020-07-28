#pragma once
#include "types.h"

#define MAX_UL_LEVEL 9
#define UL_SYMBOL(x) (N_GRAPH_SYMBOL + (x))
#define UL_SYMBOL_DISC UL_SYMBOL(9)
#define UL_SYMBOL_CIRCLE UL_SYMBOL(10)
#define UL_SYMBOL_SQUARE UL_SYMBOL(11)

#define HR_SYMBOL 26

#define RB_STACK_SIZE 10
#define TAG_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define FONTSTAT_SIZE 7

///
/// 行の折り返し処理？
///
class Breakpoint
{
    // line length
    int _len;
    // tag length
    int _tlen;

    // line position
    int _pos;
    long flag;
    short top_margin;
    short bottom_margin;

    char init_flag;
    // initialize
    Anchor anchor;
    Str img_alt;
    char fontstat[FONTSTAT_SIZE];
    short nobr_level;
    Lineprop prev_ctype;

public:
    void initialize()
    {
        init_flag = 1;
    }
    int pos() const
    {
        return _pos;
    }
    int len() const
    {
        return _len;
    }
    int tlen() const
    {
        return _tlen;
    }
    void set(const struct readbuffer *obuf, int tag_length);
    void back_to(struct readbuffer *obuf);
};

struct cmdtable
{
    char *cmdname;
    int cmd;
};

struct readbuffer
{
    Str line;
    Lineprop cprop;
    short pos;
    Str prevchar;
    long flag;
    long flag_stack[RB_STACK_SIZE];
    int flag_sp;
    int status;
    unsigned char end_tag;
    short table_level;
    short nobr_level;
    Anchor anchor;
    Str img_alt;
    char fontstat[FONTSTAT_SIZE];
    char fontstat_stack[FONT_STACK_SIZE][FONTSTAT_SIZE];
    int fontstat_sp;
    Lineprop prev_ctype;
    Breakpoint bp;
    struct cmdtable *tag_stack[TAG_STACK_SIZE];
    int tag_sp;
    short top_margin;
    short bottom_margin;
};

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
    struct environment *envs;
    int nenv;
    int envc;
    int envc_real;
    char *title;
    int blank_lines;
};

struct HtmlContext
{
    ParsedURL *BaseUrl = nullptr;
    Str Title = nullptr;
    wc_ces MetaCharset;
    int HSeq = 0;
};

void renderTable(struct table *t, int max_width,
                 struct html_feed_environ *h_env);

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

int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env);
void HTMLlineproc0(const char *istr, html_feed_environ *h_env, bool internal);
inline void HTMLlineproc1(const char *x, html_feed_environ *y)
{
    HTMLlineproc0(x, y, true);
}
void init_henv(struct html_feed_environ *, struct readbuffer *,
               struct environment *, int, TextLineList *, int, int);
void completeHTMLstream(struct html_feed_environ *, struct readbuffer *);
void SetCurTitle(Str title);
