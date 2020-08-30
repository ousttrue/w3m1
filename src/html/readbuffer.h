#pragma once

#include "frontend/line.h"
#include "anchor.h"
#include "html_tag.h"
#include <wc.h>

struct FontStat
{
    char in_bold = 0;
    char in_under = 0;
    char in_italic = 0;
    char in_strike = 0;
    char in_ins = 0;
    char in_stand = 0;
    char _ = 0;
};
static_assert(sizeof(FontStat) == 7);

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
    RB_DEL = 0x100000,
    RB_S = 0x200000,
};

enum TableModes
{
    TBLM_NONE = RB_NONE,
    TBLM_PRE = RB_PRE,
    TBLM_SCRIPT = RB_SCRIPT,
    TBLM_STYLE = RB_STYLE,
    TBLM_PLAIN = RB_PLAIN,
    TBLM_NOBR = RB_NOBR,
    TBLM_PRE_INT = RB_PRE_INT,
    TBLM_INTXTA = RB_INTXTA,
    TBLM_INSELECT = RB_INSELECT,
    TBLM_PREMODE = RB_PREMODE,
    TBLM_SPECIAL = RB_SPECIAL,
    TBLM_DEL = RB_DEL,
    TBLM_S = RB_S,
    TBLM_ANCHOR = 0x1000000,
};

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

    char init_flag = 1;
    // initialize
    Anchor anchor;
    Str img_alt;
    FontStat fontstat;
    short nobr_level;
    Lineprop prev_ctype;

public:
    int pos() const { return _pos; }
    int len() const { return _len; }
    int tlen() const { return _tlen; }
    void set(const struct readbuffer *obuf, int tag_length);
    void back_to(struct readbuffer *obuf);
};

struct cmdtable
{
    char *cmdname;
    HtmlTags cmd;
};

#define RB_STACK_SIZE 10
#define FONT_STACK_SIZE 5
#define TAG_STACK_SIZE 10

/* status flags */
enum TokenStatusTypes
{
    R_ST_NORMAL = 0,  /* normal */
    R_ST_TAG0 = 1,    /* within tag, just after < */
    R_ST_TAG = 2,     /* within tag */
    R_ST_QUOTE = 3,   /* within single quote */
    R_ST_DQUOTE = 4,  /* within double quote */
    R_ST_EQL = 5,     /* = */
    R_ST_AMP = 6,     /* within ampersand quote */
    R_ST_EOL = 7,     /* end of file */
    R_ST_CMNT1 = 8,   /* <!  */
    R_ST_CMNT2 = 9,   /* <!- */
    R_ST_CMNT = 10,   /* within comment */
    R_ST_NCMNT1 = 11, /* comment - */
    R_ST_NCMNT2 = 12, /* comment -- */
    R_ST_NCMNT3 = 13, /* comment -- space */
    R_ST_IRRTAG = 14, /* within irregular tag */
    R_ST_VALUE = 15,  /* within tag attribule value */
};

inline bool ST_IS_REAL_TAG(TokenStatusTypes s)
{
    return ((s) == R_ST_TAG || (s) == R_ST_TAG0 || (s) == R_ST_EQL || (s) == R_ST_VALUE);
}

struct readbuffer
{
    Str line;
    Lineprop cprop;
    short pos;
    Str prevchar;
    ReadBufferFlags flag;
    ReadBufferFlags RB_GET_ALIGN() const { return flag & RB_ALIGN; }
    void RB_SET_ALIGN(ReadBufferFlags align)
    {
        flag &= ~RB_ALIGN;
        flag |= align;
    }

    ReadBufferFlags flag_stack[RB_STACK_SIZE];
    int flag_sp;
    void RB_SAVE_FLAG()
    {
        if (flag_sp < RB_STACK_SIZE)
            flag_stack[flag_sp++] = RB_GET_ALIGN();
    }
    void RB_RESTORE_FLAG()
    {
        if (flag_sp > 0)
            RB_SET_ALIGN(flag_stack[--flag_sp]);
    }

    TokenStatusTypes status;
    HtmlTags end_tag;
    short table_level;
    short nobr_level;
    Anchor anchor;
    Str img_alt;
    FontStat fontstat;
    FontStat fontstat_stack[FONT_STACK_SIZE];
    int fontstat_sp;
    Lineprop prev_ctype;
    Breakpoint bp;
    struct cmdtable *tag_stack[TAG_STACK_SIZE];
    int tag_sp;
    short top_margin;
    short bottom_margin;

    readbuffer();
    void reset();

    void passthrough(char *str, int back);
    int close_effect0(int cmd);
    char *has_hidden_link(int cmd);
    void process_idattr(int cmd, HtmlTagPtr tag);
    void proc_escape(const char **str_return);

    void push_nchars(int width, const char *str, int len, Lineprop mode);
    void push_tag(const char *cmdname, HtmlTags cmd);
    void push_charp(int width, const char *str, Lineprop mode);
    void push_str(int width, Str str, Lineprop mode);
    void check_breakpoint(int pre_mode, const char *ch);
    void push_char(int pre_mode, char ch);
    void PUSH(int c);
    void set_space_to_prevchar();
    void push_spaces(int pre_mode, int width);
    void fillline(int indent);
    void proc_mchar(int pre_mode, int width, const char **str, Lineprop mode);

    void append_tags();
    void save_fonteffect();
    void restore_fonteffect();
    void clear_ignore_p_flag(int cmd);
    void set_alignment(HtmlTagPtr tag);
};

int sloppy_parse_line(char **str);
TokenStatusTypes next_status(char c, TokenStatusTypes status);
std::tuple<std::string_view, TokenStatusTypes, std::string> read_token(std::string_view instr, TokenStatusTypes status, bool pre);
inline std::string_view read_token(std::string_view instr, Str buf, TokenStatusTypes *status, bool pre, bool append)
{
    auto [remain, new_status, token] = read_token(instr, *status, pre);
    *status = new_status;
    if (!append)
    {
        buf->Clear();
    }
    buf->Push(token);
    return remain;
}
