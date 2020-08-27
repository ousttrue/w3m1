#pragma once
#include "html/html.h"
#include "html/readbuffer.h"
#include "matrix.h"

enum BorderModes
{
    BORDER_NONE = 0,
    BORDER_THIN = 1,
    BORDER_THICK = 2,
    BORDER_NOWIN = 3,
};

enum TableAttributes
{
    HTT_NONE = 0,
    HTT_X = 1,
    HTT_Y = 2,
    HTT_NOWRAP = 4,
    HTT_ALIGN = 0x30,
    HTT_LEFT = 0x00,
    HTT_CENTER = 0x10,
    HTT_RIGHT = 0x20,
    HTT_TRSET = 0x40,
    HTT_VALIGN = 0x700,
    HTT_TOP = 0x100,
    HTT_MIDDLE = 0x200,
    HTT_BOTTOM = 0x400,
    HTT_VTRSET = 0x800,
};

#define MAXCELL 20
struct table_cell
{
    short col[MAXCELL];
    short colspan[MAXCELL];
    short index[MAXCELL];
    short maxcell;
    short icell;
#ifdef MATRIX
    short eindex[MAXCELL];
    short necell;
#endif /* MATRIX */
    short width[MAXCELL];
    short minimum_width[MAXCELL];
    short fixed_width[MAXCELL];
};

struct table_in
{
    struct table *ptr;
    short col;
    short row;
    short cell;
    short indent;
    struct TextLineList *buf;
};

struct table_linfo
{
    Lineprop prev_ctype;
    signed char prev_spaces;
    Str prevchar;
    short length;
};

#define MAXCOL 50
struct table
{
    int row;
    int col;
    int maxrow;
    int maxcol;
    int max_rowsize;
    BorderModes border_mode;
    int total_width;
    int total_height;
    int tabcontentssize;
    int indent;
    int cellspacing;
    int cellpadding;
    int vcellpadding;
    int vspace;
    int flag;
#ifdef TABLE_EXPAND
    int real_width;
#endif /* TABLE_EXPAND */
    Str caption;

    Str id;

    struct GeneralList ***tabdata;
    TableAttributes **tabattr;
    TableAttributes trattr;
    Str **tabidvalue;
    Str *tridvalue;
    short tabwidth[MAXCOL];
    short minimum_width[MAXCOL];
    short fixed_width[MAXCOL];
    struct table_cell cell;
    short *tabheight;
    struct table_in *tables;
    short ntable;
    short tables_size;
    struct TextList *suspended_data;
    /* use for counting skipped spaces */
    struct table_linfo linfo;
#ifdef MATRIX
    MAT *matrix;
    VEC *vector;
#endif /* MATRIX */
    int sloppy_width;

    int table_border_width(int symbolWidth);
    int table_colspan(int row, int col);
    int table_rowspan(int row, int col);
    int setwidth0(struct table_mode *mode);
    void setwidth(struct table_mode *mode);
    void pushTable(struct table *);
    void check_row(int row);
    void feed_table_block_tag(const char *line, struct table_mode *mode, int indent, int cmd);
    void pushdata(int row, int col, const char *data);
    void suspend_or_pushdata(const char *line);
    void print_item(int row, int col, int width, Str buf);
    void print_sep(int row, VerticalAlignTypes type, int maxcol, Str buf, int symbolWidth);
    int table_rule_width(int symbolWidth) const;
};

struct table_mode
{
    TableModes pre_mode;
    char indent_level;
    char caption;
    short nobr_offset;
    char nobr_level;
    short anchor_offset;
    HtmlTags end_tag;
};

void align(struct TextLine *lbuf, int width, AlignTypes mode);

int feed_table(struct table *tbl, const char *line, struct table_mode *mode, int width, int internal, class HtmlContext *seq);
void do_refill(struct table *tbl, int row, int col, int maxlimit, class HtmlContext *seq);
void feed_table1(struct table *tbl, Str tok, struct table_mode *mode, int width, class HtmlContext *seq);
struct table *begin_table(BorderModes border, int spacing, int padding, int vspace, class HtmlContext *seq);
void end_table(struct table *tbl, class HtmlContext *seq);
int visible_length(const char *str);
struct table *newTable(void);
