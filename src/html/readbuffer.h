#pragma once
#include "line.h"

#define FONTSTAT_SIZE 7

enum ReadBufferFlags
{
    RB_NONE = 0,
    RB_PRE = 0x01,
    RB_SCRIPT = 0x02,
    RB_STYLE = 0x04,
    RB_PLAIN = 0x08,
    RB_LEFT = 0x10,
    RB_CENTER = 0x20,
    RB_RIGHT = 0x40,
    RB_ALIGN = (RB_LEFT | RB_CENTER | RB_RIGHT),
    RB_NOBR = 0x80,
    RB_P = 0x100,
    RB_PRE_INT = 0x200,
    RB_IN_DT = 0x400,
    RB_INTXTA = 0x800,
    RB_INSELECT = 0x1000,
    RB_IGNORE_P = 0x2000,
    RB_TITLE = 0x4000,
    RB_NFLUSHED = 0x8000,
    RB_NOFRAMES = 0x10000,
    RB_INTABLE = 0x20000,
    RB_PREMODE = (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_INTXTA),
    RB_SPECIAL = (RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_NOBR),
    RB_PLAIN_PRE = 0x40000,
#ifdef FORMAT_NICE
    RB_FILL = 0x80000,
#endif /* FORMAT_NICE */
    RB_DEL = 0x100000,
    RB_S = 0x200000,
};
#include "enum_bit_operator.h"

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
    ReadBufferFlags flag = ReadBufferFlags::RB_NONE;
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
    void initialize() { init_flag = 1; }
    int pos() const { return _pos; }
    int len() const { return _len; }
    int tlen() const { return _tlen; }
    void set(const struct readbuffer *obuf, int tag_length);
    void back_to(struct readbuffer *obuf);
};

struct cmdtable
{
    char *cmdname;
    int cmd;
};

#define RB_GET_ALIGN(obuf) ((obuf)->flag & RB_ALIGN)
#define RB_SET_ALIGN(obuf, align)  \
    {                              \
        (obuf)->flag &= ~RB_ALIGN; \
        (obuf)->flag |= (align);   \
    }
#define RB_SAVE_FLAG(obuf)                                              \
    {                                                                   \
        if ((obuf)->flag_sp < RB_STACK_SIZE)                            \
            (obuf)->flag_stack[(obuf)->flag_sp++] = RB_GET_ALIGN(obuf); \
    }
#define RB_RESTORE_FLAG(obuf)                                          \
    {                                                                  \
        if ((obuf)->flag_sp > 0)                                       \
            RB_SET_ALIGN(obuf, (obuf)->flag_stack[--(obuf)->flag_sp]); \
    }
#define RB_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define TAG_STACK_SIZE 10
struct readbuffer
{
    Str line;
    Lineprop cprop;
    short pos;
    Str prevchar;
    ReadBufferFlags flag;
    ReadBufferFlags flag_stack[RB_STACK_SIZE];
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
