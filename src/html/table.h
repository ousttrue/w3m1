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

enum TableWidthFlags
{
    CHECK_NONE = 0,
    CHECK_MINIMUM = 1,
    CHECK_FIXED = 2,
};

enum TableFlags
{
    TBL_IN_ROW = 1,
    TBL_EXPAND_OK = 2,
    TBL_IN_COL = 4,
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

    static table *begin(BorderModes border, int spacing, int padding, int vspace);
    void end();

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
    int get_spec_cell_width(int row, int col);
    int check_table_width(double *newwidth, MAT *minv, int itr);
    int get_table_width(short *orgwidth, short *cellwidth, TableWidthFlags flag);
    int minimum_table_width()
    {
        return get_table_width(minimum_width, cell.minimum_width, CHECK_NONE);
    }
    int maximum_table_width()
    {
        return get_table_width(tabwidth, cell.width, CHECK_FIXED);
    }
    int fixed_table_width()
    {
        return get_table_width(fixed_width, cell.fixed_width, CHECK_MINIMUM);
    }
    int skip_space(const char *line, struct table_linfo *linfo, int checkminimum);
    void check_minimum_width(short *tabwidth);
    void set_integered_width(double *dwidth, short *iwidth, int symbolWidth);
    void check_minimum0(int min);
    void check_maximum_width();
    void addcontentssize(int width);
    int check_compressible_cell(MAT *minv,
                                double *newwidth, double *swidth, short *cwidth,
                                double totalwidth, double *Sxx,
                                int icol, int icell, double sxx, int corr, int symbolWidth);
    void set_table_width(short *newwidth, int maxwidth);
    void check_table_height();
    void clearcontentssize(struct table_mode *mode);
    void table_close_anchor0(struct table_mode *mode);
    void begin_cell(struct table_mode *mode);
    void check_rowcol(struct table_mode *mode);
    void feed_table_inline_tag(const char *line, struct table_mode *mode, int width);
    void set_table_matrix(int width);
    void check_relative_width(int maxwidth);
    void set_table_matrix0(int maxwidth);
    void correct_table_matrix4(int col, int cspan, char *flags, double s, double b);
    void correct_table_matrix3(int col, char *flags, double s, double b);
    void correct_table_matrix2(int col, int cspan, double s, double b);
    int correct_table_matrix(int col, int cspan, int a, double b);
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

enum TagActions
{
    TAG_ACTION_NONE = 0,
    TAG_ACTION_FEED = 1,
    TAG_ACTION_TABLE = 2,
    TAG_ACTION_N_TABLE = 3,
    TAG_ACTION_PLAIN = 4,
};

void align(struct TextLine *lbuf, int width, AlignTypes mode);
int ceil_at_intervals(int x, int step);
int visible_length(const char *str);
int bsearch_2short(short e1, short *ent1, short e2, short *ent2, int base, short *indexarray, int nent);
int maximum_visible_length(const char *str, int offset);
int maximum_visible_length_plain(const char *str, int offset);

struct TableState
{
    bool prev_is_hangul = 0;
    table *tbl = NULL;
    table_mode *tbl_mode = NULL;
    int tbl_width = 0;

    ReadBufferFlags pre_mode(const readbuffer &obuf) const
    {
        return (ReadBufferFlags)((obuf.table_level >= 0) ? (int)tbl_mode->pre_mode : (int)obuf.flag);
    }

    HtmlTags end_tag(const readbuffer &obuf) const
    {
        return (obuf.table_level >= 0) ? tbl_mode->end_tag : obuf.end_tag;
    }

    bool close_table(const readbuffer &obuf, table *m_tables[], table_mode *m_table_modes);
};
