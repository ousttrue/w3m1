/* 
 * HTML table
 */
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "indep.h"
#include "gc_helper.h"
#include "myctype.h"
#include "symbol.h"
#include "html/table.h"
#include "entity.h"
#include "file.h"
#include "tagstack.h"
#include "textlist.h"
#include "html.h"
#include "html/html.h"
#include "html/html_context.h"
#include "frontend/terminal.h"

#define MAXROW 50
#define MAXROWCELL 1000
#define MAX_INDENT_LEVEL 10
#define MAX_TABLE_N 20 /* maximum number of table in same level */

#define TBL_IN_ROW 1
#define TBL_EXPAND_OK 2
#define TBL_IN_COL 4

static int visible_length_offset = 0;
int visible_length(const char *str)
{
    int len = 0, n, max_len = 0;
    TokenStatusTypes status = R_ST_NORMAL;
    TokenStatusTypes prev_status = status;
    Str tagbuf = Strnew();
    int amp_len = 0;

    auto t = str;
    while (*str)
    {
        prev_status = status;
        if (next_status(*str, &status))
        {
            len += get_mcwidth(str);
            n = get_mclen(str);
        }
        else
        {
            n = 1;
        }
        if (status == R_ST_TAG0)
        {
            tagbuf->Clear();
            tagbuf->Push(str, n);
        }
        else if (status == R_ST_TAG || status == R_ST_DQUOTE || status == R_ST_QUOTE || status == R_ST_EQL || status == R_ST_VALUE)
        {
            tagbuf->Push(str, n);
        }
        else if (status == R_ST_AMP)
        {
            if (prev_status == R_ST_NORMAL)
            {
                tagbuf->Clear();
                len--;
                amp_len = 0;
            }
            else
            {
                tagbuf->Push(str, n);
                amp_len++;
            }
        }
        else if (status == R_ST_NORMAL && prev_status == R_ST_AMP)
        {
            tagbuf->Push(str, n);
            auto [r2, view] = getescapecmd(tagbuf->ptr, w3mApp::Instance().InnerCharset);
            if (r2[0] == '\0' && (view[0] == '\r' || view[0] == '\n'))
            {
                if (len > max_len)
                    max_len = len;
                len = 0;
            }
            else
                len += get_strwidth(t) + get_strwidth(r2);
        }
        else if (status == R_ST_NORMAL && ST_IS_REAL_TAG(prev_status))
        {
            ;
        }
        else if (*str == '\t')
        {
            len--;
            do
            {
                len++;
            } while ((visible_length_offset + len) % w3mApp::Instance().Tabstop != 0);
        }
        else if (*str == '\r' || *str == '\n')
        {
            len--;
            if (len > max_len)
                max_len = len;
            len = 0;
        }
        str += n;
    }
    if (status == R_ST_AMP)
    {
        auto [r2, view] = getescapecmd(tagbuf->ptr, w3mApp::Instance().InnerCharset);
        if (view[0] != '\r' && view[0] != '\n')
            len += get_strwidth(t) + get_strwidth(r2);
    }
    return len > max_len ? len : max_len;
}

int visible_length_plain(const char *str)
{
    int len = 0, max_len = 0;

    while (*str)
    {
        if (*str == '\t')
        {
            do
            {
                len++;
            } while ((visible_length_offset + len) % w3mApp::Instance().Tabstop != 0);
            str++;
        }
        else if (*str == '\r' || *str == '\n')
        {
            if (len > max_len)
                max_len = len;
            len = 0;
            str++;
        }
        else
        {
            len += get_mcwidth(str);
            str += get_mclen(str);
        }
    }
    return len > max_len ? len : max_len;
}

static int
maximum_visible_length(const char *str, int offset)
{
    visible_length_offset = offset;
    return visible_length(str);
}

static int
maximum_visible_length_plain(const char *str, int offset)
{
    visible_length_offset = offset;
    return visible_length_plain(str);
}

#define RULE(mode, n) (((mode) == BORDER_THICK) ? ((n) + 16) : (n))
#define TK_VERTICALBAR(mode) RULE(mode, 5)

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif /* not max */
#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */
#ifndef abs
#define abs(a) ((a) >= 0. ? (a) : -(a))
#endif /* not abs */

// #define set_prevchar(x,y,n) Strcopy_charp_n((x),(y),(n))
static inline void set_space_to_prevchar(Str x)
{
    x->CopyFrom(" ", 1);
}

#ifdef MATRIX
#ifndef MESCHACH
#include "matrix.c"
#endif /* not MESCHACH */
#endif /* MATRIX */

#ifdef MATRIX
int correct_table_matrix(struct table *, int, int, int, double);
void set_table_matrix(struct table *, int);
#endif /* MATRIX */

#ifdef MATRIX
static double
weight(int x)
{

    if (x < ::Terminal::columns())
        return (double)x;
    else
        return ::Terminal::columns() * (log((double)x / ::Terminal::columns()) + 1.);
}

static double
weight2(int a)
{
    return (double)a / ::Terminal::columns() * 4 + 1.;
}

#define sigma_td(a) (0.5 * weight2(a))     /* <td width=...> */
#define sigma_td_nw(a) (32 * weight2(a))   /* <td ...> */
#define sigma_table(a) (0.25 * weight2(a)) /* <table width=...> */
#define sigma_table_nw(a) (2 * weight2(a)) /* <table...> */
#else                                      /* not MATRIX */
#define LOG_MIN 1.0
static double
weight3(int x)
{
    if (x < 0.1)
        return 0.1;
    if (x < LOG_MIN)
        return (double)x;
    else
        return LOG_MIN * (log((double)x / LOG_MIN) + 1.);
}
#endif /* not MATRIX */

int bsearch_2short(short e1, short *ent1, short e2, short *ent2, int base,
                   short *indexarray, int nent)
{
    int n = nent;
    int k = 0;

    int e = e1 * base + e2;
    while (n > 0)
    {
        int nn = n / 2;
        int idx = indexarray[k + nn];
        int ne = ent1[idx] * base + ent2[idx];
        if (ne == e)
        {
            k += nn;
            break;
        }
        else if (ne < e)
        {
            n -= nn + 1;
            k += nn + 1;
        }
        else
        {
            n = nn;
        }
    }
    return k;
}

static int
bsearch_double(double e, double *ent, short *indexarray, int nent)
{
    int n = nent;
    int k = 0;

    while (n > 0)
    {
        int nn = n / 2;
        int idx = indexarray[k + nn];
        double ne = ent[idx];
        if (ne == e)
        {
            k += nn;
            break;
        }
        else if (ne > e)
        {
            n -= nn + 1;
            k += nn + 1;
        }
        else
        {
            n = nn;
        }
    }
    return k;
}

static int
ceil_at_intervals(int x, int step)
{
    int mo = x % step;
    if (mo > 0)
        x += step - mo;
    else if (mo < 0)
        x -= mo;
    return x;
}

static int
floor_at_intervals(int x, int step)
{
    int mo = x % step;
    if (mo > 0)
        x -= mo;
    else if (mo < 0)
        x += step - mo;
    return x;
}

#define round(x) ((int)floor((x) + 0.5))

#ifndef MATRIX
static void
dv2sv(double *dv, short *iv, int size)
{
    int i, k, iw;
    short *indexarray;
    double *edv;
    double w = 0., x;

    indexarray = NewAtom_N(short, size);
    edv = NewAtom_N(double, size);
    for (i = 0; i < size; i++)
    {
        iv[i] = ceil(dv[i]);
        edv[i] = (double)iv[i] - dv[i];
    }

    w = 0.;
    for (k = 0; k < size; k++)
    {
        x = edv[k];
        w += x;
        i = bsearch_double(x, edv, indexarray, k);
        if (k > i)
        {
            int ii;
            for (ii = k; ii > i; ii--)
                indexarray[ii] = indexarray[ii - 1];
        }
        indexarray[i] = k;
    }
    iw = min((int)(w + 0.5), size);
    if (iw == 0)
        return;
    x = edv[(int)indexarray[iw - 1]];
    for (i = 0; i < size; i++)
    {
        k = indexarray[i];
        if (i >= iw && abs(edv[k] - x) > 1e-6)
            break;
        iv[k]--;
    }
}
#endif

///
/// table
///
int table::table_colspan(int row, int col)
{
    int i;
    for (i = col + 1; i <= this->maxcol && (this->tabattr[row][i] & HTT_X); i++)
        ;
    return i - col;
}

int table::table_rowspan(int row, int col)
{
    int i;
    if (!this->tabattr[row])
        return 0;
    for (i = row + 1; i <= this->maxrow && this->tabattr[i] &&
                      (this->tabattr[i][col] & HTT_Y);
         i++)
        ;
    return i - row;
}

static int minimum_cellspacing(BorderModes border_mode, int symbolWidth)
{
    switch (border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    case BORDER_NOWIN:
        return symbolWidth;
    case BORDER_NONE:
        return 1;
    default:
        /* not reached */
        return 0;
    }
}

int table::table_border_width(int symbolWidth)
{
    switch (this->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
        return this->maxcol * this->cellspacing + 2 * (symbolWidth + this->cellpadding);
    case BORDER_NOWIN:
    case BORDER_NONE:
        return this->maxcol * this->cellspacing;
    default:
        /* not reached */
        return 0;
    }
}

struct table *newTable()
{
    struct table *t;
    int i, j;

    t = New(struct table);
    t->max_rowsize = MAXROW;
    t->tabdata = New_N(GeneralList **, MAXROW);
    t->tabattr = New_N(TableAttributes *, MAXROW);
    t->tabheight = NewAtom_N(short, MAXROW);
#ifdef ID_EXT
    t->tabidvalue = New_N(Str *, MAXROW);
    t->tridvalue = New_N(Str, MAXROW);
#endif /* ID_EXT */

    for (i = 0; i < MAXROW; i++)
    {
        t->tabdata[i] = NULL;
        t->tabattr[i] = 0;
        t->tabheight[i] = 0;
#ifdef ID_EXT
        t->tabidvalue[i] = NULL;
        t->tridvalue[i] = NULL;
#endif /* ID_EXT */
    }
    for (j = 0; j < MAXCOL; j++)
    {
        t->tabwidth[j] = 0;
        t->minimum_width[j] = 0;
        t->fixed_width[j] = 0;
    }
    t->cell.maxcell = -1;
    t->cell.icell = -1;
    t->ntable = 0;
    t->tables_size = 0;
    t->tables = NULL;
#ifdef MATRIX
    t->matrix = NULL;
    t->vector = NULL;
#endif /* MATRIX */
#if 0
    t->tabcontentssize = 0;
    t->indent = 0;
    t->linfo.prev_ctype = PC_ASCII;
    t->linfo.prev_spaces = -1;
#endif
    t->linfo.prevchar = Strnew_size(8);
    t->linfo.prevchar->CopyFrom("", 0);
    t->trattr = {};

    t->caption = Strnew();
    t->suspended_data = NULL;
#ifdef ID_EXT
    t->id = NULL;
#endif
    return t;
}

void table::check_row(int row)
{
    int i, r;
    GeneralList ***tabdata;
    TableAttributes **tabattr;
    short *tabheight;
#ifdef ID_EXT
    Str **tabidvalue;
    Str *tridvalue;
#endif /* ID_EXT */

    if (row >= this->max_rowsize)
    {
        r = max(this->max_rowsize * 2, row + 1);
        tabdata = New_N(GeneralList **, r);
        tabattr = New_N(TableAttributes *, r);
        tabheight = NewAtom_N(short, r);
#ifdef ID_EXT
        tabidvalue = New_N(Str *, r);
        tridvalue = New_N(Str, r);
#endif /* ID_EXT */
        for (i = 0; i < this->max_rowsize; i++)
        {
            tabdata[i] = this->tabdata[i];
            tabattr[i] = this->tabattr[i];
            tabheight[i] = this->tabheight[i];
#ifdef ID_EXT
            tabidvalue[i] = this->tabidvalue[i];
            tridvalue[i] = this->tridvalue[i];
#endif /* ID_EXT */
        }
        for (; i < r; i++)
        {
            tabdata[i] = NULL;
            tabattr[i] = NULL;
            tabheight[i] = 0;
#ifdef ID_EXT
            tabidvalue[i] = NULL;
            tridvalue[i] = NULL;
#endif /* ID_EXT */
        }
        this->tabdata = tabdata;
        this->tabattr = tabattr;
        this->tabheight = tabheight;
#ifdef ID_EXT
        this->tabidvalue = tabidvalue;
        this->tridvalue = tridvalue;
#endif /* ID_EXT */
        this->max_rowsize = r;
    }

    if (this->tabdata[row] == NULL)
    {
        this->tabdata[row] = New_N(GeneralList *, MAXCOL);
        this->tabattr[row] = NewAtom_N(TableAttributes, MAXCOL);
#ifdef ID_EXT
        this->tabidvalue[row] = New_N(Str, MAXCOL);
#endif /* ID_EXT */
        for (i = 0; i < MAXCOL; i++)
        {
            this->tabdata[row][i] = NULL;
            this->tabattr[row][i] = {};
#ifdef ID_EXT
            this->tabidvalue[row][i] = NULL;
#endif /* ID_EXT */
        }
    }
}

void table::pushdata(int row, int col, const char *data)
{
    this->check_row(row);
    if (this->tabdata[row][col] == NULL)
        this->tabdata[row][col] = newGeneralList();

    pushText((TextList *)this->tabdata[row][col], data ? data : "");
}

void table::suspend_or_pushdata(const char *line)
{
    if (this->flag & TBL_IN_COL)
        this->pushdata(this->row, this->col, line);
    else
    {
        if (!this->suspended_data)
            this->suspended_data = newTextList();
        pushText(this->suspended_data, line ? line : (char *)"");
    }
}

void align(TextLine *lbuf, int width, AlignTypes mode)
{
    Str line = lbuf->line;
    if (line->Size() == 0)
    {
        for (int i = 0; i < width; i++)
            line->Push(' ');
        lbuf->pos = width;
        return;
    }

    auto buf = Strnew();
    auto l = width - lbuf->pos;
    switch (mode)
    {
    case ALIGN_CENTER:
    {
        auto l1 = l / 2;
        auto l2 = l - l1;
        for (int i = 0; i < l1; i++)
            buf->Push(' ');
        buf->Push(line);
        for (int i = 0; i < l2; i++)
            buf->Push(' ');
        break;
    }
    case ALIGN_LEFT:
        buf->Push(line);
        for (int i = 0; i < l; i++)
            buf->Push(' ');
        break;
    case ALIGN_RIGHT:
        for (int i = 0; i < l; i++)
            buf->Push(' ');
        buf->Push(line);
        break;
    default:
        return;
    }
    lbuf->line = buf;
    if (lbuf->pos < width)
        lbuf->pos = width;
}

void table::print_item(int row, int col, int width, Str buf)
{
    if (!this->tabdata[row])
    {
        return;
    }

    auto lbuf = popTextLine((TextLineList *)this->tabdata[row][col]);
    if (lbuf)
    {
        this->check_row(row);
        auto alignment = ALIGN_CENTER;
        if ((this->tabattr[row][col] & HTT_ALIGN) == HTT_LEFT)
            alignment = ALIGN_LEFT;
        else if ((this->tabattr[row][col] & HTT_ALIGN) == HTT_RIGHT)
            alignment = ALIGN_RIGHT;
        else if ((this->tabattr[row][col] & HTT_ALIGN) == HTT_CENTER)
            alignment = ALIGN_CENTER;
        align(lbuf, width, alignment);
        buf->Push(lbuf->line);
    }
    else
    {
        lbuf = newTextLine(NULL, 0);
        align(lbuf, width, ALIGN_CENTER);
        buf->Push(lbuf->line);
    }
}

void table::print_sep(int row, VerticalAlignTypes type, int maxcol, Str buf, int symbolWidth)
{
    int forbid;
    int rule_mode;
    int i, k, l, m;

    if (row >= 0)
        this->check_row(row);
    this->check_row(row + 1);
    if ((type == VALIGN_TOP || type == VALIGN_BOTTOM) && this->border_mode == BORDER_THICK)
    {
        rule_mode = BORDER_THICK;
    }
    else
    {
        rule_mode = BORDER_THIN;
    }
    forbid = 1;
    if (type == VALIGN_TOP)
        forbid |= 2;
    else if (type == VALIGN_BOTTOM)
        forbid |= 8;
    else if (this->tabattr[row + 1][0] & HTT_Y)
    {
        forbid |= 4;
    }
    if (this->border_mode != BORDER_NOWIN)
    {
        push_symbol(buf, RULE(this->border_mode, forbid), symbolWidth, 1);
    }
    for (i = 0; i <= maxcol; i++)
    {
        forbid = 10;
        if (type != VALIGN_BOTTOM && (this->tabattr[row + 1][i] & HTT_Y))
        {
            if (this->tabattr[row + 1][i] & HTT_X)
            {
                goto do_last_sep;
            }
            else
            {
                for (k = row;
                     k >= 0 && this->tabattr[k] && (this->tabattr[k][i] & HTT_Y);
                     k--)
                    ;
                m = this->tabwidth[i] + 2 * this->cellpadding;
                for (l = i + 1; l <= this->maxcol && (this->tabattr[row][l] & HTT_X);
                     l++)
                    m += this->tabwidth[l] + this->cellspacing;
                this->print_item(k, i, m, buf);
            }
        }
        else
        {
            int w = this->tabwidth[i] + 2 * this->cellpadding;
            if (symbolWidth == 2)
                w = (w + 1) / symbolWidth;
            push_symbol(buf, RULE(rule_mode, forbid), symbolWidth, w);
        }
    do_last_sep:
        if (i < maxcol)
        {
            forbid = 0;
            if (type == VALIGN_TOP)
                forbid |= 2;
            else if (this->tabattr[row][i + 1] & HTT_X)
            {
                forbid |= 2;
            }
            if (type == VALIGN_BOTTOM)
                forbid |= 8;
            else
            {
                if (this->tabattr[row + 1][i + 1] & HTT_X)
                {
                    forbid |= 8;
                }
                if (this->tabattr[row + 1][i + 1] & HTT_Y)
                {
                    forbid |= 4;
                }
                if (this->tabattr[row + 1][i] & HTT_Y)
                {
                    forbid |= 1;
                }
            }
            if (forbid != 15) /* forbid==15 means 'no rule at all' */
                push_symbol(buf, RULE(rule_mode, forbid), symbolWidth, 1);
        }
    }
    forbid = 4;
    if (type == VALIGN_TOP)
        forbid |= 2;
    if (type == VALIGN_BOTTOM)
        forbid |= 8;
    if (this->tabattr[row + 1][maxcol] & HTT_Y)
    {
        forbid |= 1;
    }
    if (this->border_mode != BORDER_NOWIN)
        push_symbol(buf, RULE(this->border_mode, forbid), symbolWidth, 1);
}

int table::get_spec_cell_width(int row, int col)
{
    int w = this->tabwidth[col];
    for (int i = col + 1; i <= this->maxcol; i++)
    {
        this->check_row(row);
        if (this->tabattr[row][i] & HTT_X)
            w += this->tabwidth[i] + this->cellspacing;
        else
            break;
    }
    return w;
}

int table::table_rule_width(int symbolWidth) const
{
    if (this->border_mode == BORDER_NONE)
        return 1;
    return symbolWidth;
}

static void
check_cell_width(short *tabwidth, short *cellwidth,
                 short *col, short *colspan, short maxcell,
                 short *indexarray, int space, int dir)
{
    int i, j, k, bcol, ecol;
    int swidth, width;

    for (k = 0; k <= maxcell; k++)
    {
        j = indexarray[k];
        if (cellwidth[j] <= 0)
            continue;
        bcol = col[j];
        ecol = bcol + colspan[j];
        swidth = 0;
        for (i = bcol; i < ecol; i++)
            swidth += tabwidth[i];

        width = cellwidth[j] - (colspan[j] - 1) * space;
        if (width > swidth)
        {
            int w = (width - swidth) / colspan[j];
            int r = (width - swidth) % colspan[j];
            for (i = bcol; i < ecol; i++)
                tabwidth[i] += w;
            /* dir {0: horizontal, 1: vertical} */
            if (dir == 1 && r > 0)
                r = colspan[j];
            for (i = 1; i <= r; i++)
                tabwidth[ecol - i]++;
        }
    }
}

void check_minimum_width(struct table *t, short *tabwidth)
{
    int i;
    struct table_cell *cell = &t->cell;

    for (i = 0; i <= t->maxcol; i++)
    {
        if (tabwidth[i] < t->minimum_width[i])
            tabwidth[i] = t->minimum_width[i];
    }

    check_cell_width(tabwidth, cell->minimum_width, cell->col, cell->colspan,
                     cell->maxcell, cell->index, t->cellspacing, 0);
}

void check_maximum_width(struct table *t)
{
    struct table_cell *cell = &t->cell;
#ifdef MATRIX
    int i, j, bcol, ecol;
    int swidth, width;

    cell->necell = 0;
    for (j = 0; j <= cell->maxcell; j++)
    {
        bcol = cell->col[j];
        ecol = bcol + cell->colspan[j];
        swidth = 0;
        for (i = bcol; i < ecol; i++)
            swidth += t->tabwidth[i];

        width = cell->width[j] - (cell->colspan[j] - 1) * t->cellspacing;
        if (width > swidth)
        {
            cell->eindex[cell->necell] = j;
            cell->necell++;
        }
    }
#else  /* not MATRIX */
    check_cell_width(t->tabwidth, cell->width, cell->col, cell->colspan,
                     cell->maxcell, cell->index, t->cellspacing, 0);
    check_minimum_width(t, t->tabwidth);
#endif /* not MATRIX */
}

#ifdef MATRIX
static void
set_integered_width(struct table *t, double *dwidth, short *iwidth, int symbolWidth)
{
    int i, j, k, n, bcol, ecol, step;
    short *indexarray;
    char *fixed;
    double *mod;
    double sum = 0., x = 0.;
    struct table_cell *cell = &t->cell;
    int rulewidth = t->table_rule_width(symbolWidth);

    indexarray = NewAtom_N(short, t->maxcol + 1);
    mod = NewAtom_N(double, t->maxcol + 1);
    for (i = 0; i <= t->maxcol; i++)
    {
        iwidth[i] = ceil_at_intervals(ceil(dwidth[i]), rulewidth);
        mod[i] = (double)iwidth[i] - dwidth[i];
    }

    sum = 0.;
    for (k = 0; k <= t->maxcol; k++)
    {
        x = mod[k];
        sum += x;
        i = bsearch_double(x, mod, indexarray, k);
        if (k > i)
        {
            int ii;
            for (ii = k; ii > i; ii--)
                indexarray[ii] = indexarray[ii - 1];
        }
        indexarray[i] = k;
    }

    fixed = NewAtom_N(char, t->maxcol + 1);
    bzero(fixed, t->maxcol + 1);
    for (step = 0; step < 2; step++)
    {
        for (i = 0; i <= t->maxcol; i += n)
        {
            int nn;
            char *idx;
            double nsum;
            if (sum < 0.5)
                return;
            for (n = 0; i + n <= t->maxcol; n++)
            {
                int ii = indexarray[i + n];
                if (n == 0)
                    x = mod[ii];
                else if (fabs(mod[ii] - x) > 1e-6)
                    break;
            }
            for (k = 0; k < n; k++)
            {
                int ii = indexarray[i + k];
                if (fixed[ii] < 2 &&
                    iwidth[ii] - rulewidth < t->minimum_width[ii])
                    fixed[ii] = 2;
                if (fixed[ii] < 1 &&
                    iwidth[ii] - rulewidth < t->tabwidth[ii] &&
                    (double)rulewidth - mod[ii] > 0.5)
                    fixed[ii] = 1;
            }
            idx = NewAtom_N(char, n);
            for (k = 0; k < cell->maxcell; k++)
            {
                int kk, w, width, m;
                j = cell->index[k];
                bcol = cell->col[j];
                ecol = bcol + cell->colspan[j];
                m = 0;
                for (kk = 0; kk < n; kk++)
                {
                    int ii = indexarray[i + kk];
                    if (ii >= bcol && ii < ecol)
                    {
                        idx[m] = ii;
                        m++;
                    }
                }
                if (m == 0)
                    continue;
                width = (cell->colspan[j] - 1) * t->cellspacing;
                for (kk = bcol; kk < ecol; kk++)
                    width += iwidth[kk];
                w = 0;
                for (kk = 0; kk < m; kk++)
                {
                    if (fixed[(int)idx[kk]] < 2)
                        w += rulewidth;
                }
                if (width - w < cell->minimum_width[j])
                {
                    for (kk = 0; kk < m; kk++)
                    {
                        if (fixed[(int)idx[kk]] < 2)
                            fixed[(int)idx[kk]] = 2;
                    }
                }
                w = 0;
                for (kk = 0; kk < m; kk++)
                {
                    if (fixed[(int)idx[kk]] < 1 &&
                        (double)rulewidth - mod[(int)idx[kk]] > 0.5)
                        w += rulewidth;
                }
                if (width - w < cell->width[j])
                {
                    for (kk = 0; kk < m; kk++)
                    {
                        if (fixed[(int)idx[kk]] < 1 &&
                            (double)rulewidth - mod[(int)idx[kk]] > 0.5)
                            fixed[(int)idx[kk]] = 1;
                    }
                }
            }
            nn = 0;
            for (k = 0; k < n; k++)
            {
                int ii = indexarray[i + k];
                if (fixed[ii] <= step)
                    nn++;
            }
            nsum = sum - (double)(nn * rulewidth);
            if (nsum < 0. && fabs(sum) <= fabs(nsum))
                return;
            for (k = 0; k < n; k++)
            {
                int ii = indexarray[i + k];
                if (fixed[ii] <= step)
                {
                    iwidth[ii] -= rulewidth;
                    fixed[ii] = 3;
                }
            }
            sum = nsum;
        }
    }
}

static double
correlation_coefficient(double sxx, double syy, double sxy)
{
    double coe, tmp;
    tmp = sxx * syy;
    if (tmp < Tiny)
        tmp = Tiny;
    coe = sxy / sqrt(tmp);
    if (coe > 1.)
        return 1.;
    if (coe < -1.)
        return -1.;
    return coe;
}

static double
correlation_coefficient2(double sxx, double syy, double sxy)
{
    double coe, tmp;
    tmp = (syy + sxx - 2 * sxy) * sxx;
    if (tmp < Tiny)
        tmp = Tiny;
    coe = (sxx - sxy) / sqrt(tmp);
    if (coe > 1.)
        return 1.;
    if (coe < -1.)
        return -1.;
    return coe;
}

static double
recalc_width(double old, double swidth, int cwidth,
             double sxx, double syy, double sxy, int is_inclusive)
{
    double delta = swidth - (double)cwidth;
    double rat = sxy / sxx,
           coe = correlation_coefficient(sxx, syy, sxy), w, ww;
    if (old < 0.)
        old = 0.;
    if (fabs(coe) < 1e-5)
        return old;
    w = rat * old;
    ww = delta;
    if (w > 0.)
    {
        double wmin = 5e-3 * sqrt(syy * (1. - coe * coe));
        if (swidth < 0.2 && cwidth > 0 && is_inclusive)
        {
            double coe1 = correlation_coefficient2(sxx, syy, sxy);
            if (coe > 0.9 || coe1 > 0.9)
                return 0.;
        }
        if (wmin > 0.05)
            wmin = 0.05;
        if (ww < 0.)
            ww = 0.;
        ww += wmin;
    }
    else
    {
        double wmin = 5e-3 * sqrt(syy) * fabs(coe);
        if (rat > -0.001)
            return old;
        if (wmin > 0.01)
            wmin = 0.01;
        if (ww > 0.)
            ww = 0.;
        ww -= wmin;
    }
    if (w > ww)
        return ww / rat;
    return old;
}

static int
check_compressible_cell(struct table *t, MAT *minv,
                        double *newwidth, double *swidth, short *cwidth,
                        double totalwidth, double *Sxx,
                        int icol, int icell, double sxx, int corr, int symbolWidth)
{
    struct table_cell *cell = &t->cell;
    int i, j, k, m, bcol, ecol, span;
    double delta, owidth;
    double dmax, dmin, sxy;
    int rulewidth = t->table_rule_width(symbolWidth);

    if (sxx < 10.)
        return corr;

    if (icol >= 0)
    {
        owidth = newwidth[icol];
        delta = newwidth[icol] - (double)t->tabwidth[icol];
        bcol = icol;
        ecol = bcol + 1;
    }
    else if (icell >= 0)
    {
        owidth = swidth[icell];
        delta = swidth[icell] - (double)cwidth[icell];
        bcol = cell->col[icell];
        ecol = bcol + cell->colspan[icell];
    }
    else
    {
        owidth = totalwidth;
        delta = totalwidth;
        bcol = 0;
        ecol = t->maxcol + 1;
    }

    dmin = delta;
    dmax = -1.;
    for (k = 0; k <= cell->maxcell; k++)
    {
        int bcol1, ecol1;
        int is_inclusive = 0;
        if (dmin <= 0.)
            goto _end;
        j = cell->index[k];
        if (j == icell)
            continue;
        bcol1 = cell->col[j];
        ecol1 = bcol1 + cell->colspan[j];
        sxy = 0.;
        for (m = bcol1; m < ecol1; m++)
        {
            for (i = bcol; i < ecol; i++)
                sxy += m_entry(minv, i, m);
        }
        if (bcol1 >= bcol && ecol1 <= ecol)
        {
            is_inclusive = 1;
        }
        if (sxy > 0.)
            dmin = recalc_width(dmin, swidth[j], cwidth[j],
                                sxx, Sxx[j], sxy, is_inclusive);
        else
            dmax = recalc_width(dmax, swidth[j], cwidth[j],
                                sxx, Sxx[j], sxy, is_inclusive);
    }
    for (m = 0; m <= t->maxcol; m++)
    {
        int is_inclusive = 0;
        if (dmin <= 0.)
            goto _end;
        if (m == icol)
            continue;
        sxy = 0.;
        for (i = bcol; i < ecol; i++)
            sxy += m_entry(minv, i, m);
        if (m >= bcol && m < ecol)
        {
            is_inclusive = 1;
        }
        if (sxy > 0.)
            dmin = recalc_width(dmin, newwidth[m], t->tabwidth[m],
                                sxx, m_entry(minv, m, m), sxy, is_inclusive);
        else
            dmax = recalc_width(dmax, newwidth[m], t->tabwidth[m],
                                sxx, m_entry(minv, m, m), sxy, is_inclusive);
    }
_end:
    if (dmax > 0. && dmin > dmax)
        dmin = dmax;
    span = ecol - bcol;
    if ((span == t->maxcol + 1 && dmin >= 0.) ||
        (span != t->maxcol + 1 && dmin > rulewidth * 0.5))
    {
        int nwidth = ceil_at_intervals(round(owidth - dmin), rulewidth);
        correct_table_matrix(t, bcol, ecol - bcol, nwidth, 1.);
        corr++;
    }
    return corr;
}

#define MAX_ITERATION 10
int table::check_table_width(double *newwidth, MAT *minv, int itr)
{
    int i, j, k, m, bcol, ecol;
    int corr = 0;
    struct table_cell *cell = &this->cell;
#ifdef __GNUC__
    short orgwidth[this->maxcol + 1], corwidth[this->maxcol + 1];
    short cwidth[cell->maxcell + 1];
    double swidth[cell->maxcell + 1];
#else  /* __GNUC__ */
    short orgwidth[MAXCOL], corwidth[MAXCOL];
    short cwidth[MAXCELL];
    double swidth[MAXCELL];
#endif /* __GNUC__ */
    double twidth, sxy, *Sxx, stotal;

    twidth = 0.;
    stotal = 0.;
    for (i = 0; i <= this->maxcol; i++)
    {
        twidth += newwidth[i];
        stotal += m_entry(minv, i, i);
        for (m = 0; m < i; m++)
        {
            stotal += 2 * m_entry(minv, i, m);
        }
    }

    Sxx = NewAtom_N(double, cell->maxcell + 1);
    for (k = 0; k <= cell->maxcell; k++)
    {
        j = cell->index[k];
        bcol = cell->col[j];
        ecol = bcol + cell->colspan[j];
        swidth[j] = 0.;
        for (i = bcol; i < ecol; i++)
            swidth[j] += newwidth[i];
        cwidth[j] = cell->width[j] - (cell->colspan[j] - 1) * this->cellspacing;
        Sxx[j] = 0.;
        for (i = bcol; i < ecol; i++)
        {
            Sxx[j] += m_entry(minv, i, i);
            for (m = bcol; m <= ecol; m++)
            {
                if (m < i)
                    Sxx[j] += 2 * m_entry(minv, i, m);
            }
        }
    }

    /* compress table */
    corr = check_compressible_cell(this, minv, newwidth, swidth,
                                   cwidth, twidth, Sxx, -1, -1, stotal, corr, Terminal::SymbolWidth());
    if (itr < MAX_ITERATION && corr > 0)
        return corr;

    /* compress multicolumn cell */
    for (k = cell->maxcell; k >= 0; k--)
    {
        j = cell->index[k];
        corr = check_compressible_cell(this, minv, newwidth, swidth,
                                       cwidth, twidth, Sxx,
                                       -1, j, Sxx[j], corr, Terminal::SymbolWidth());
        if (itr < MAX_ITERATION && corr > 0)
            return corr;
    }

    /* compress single column cell */
    for (i = 0; i <= this->maxcol; i++)
    {
        corr = check_compressible_cell(this, minv, newwidth, swidth,
                                       cwidth, twidth, Sxx,
                                       i, -1, m_entry(minv, i, i), corr, Terminal::SymbolWidth());
        if (itr < MAX_ITERATION && corr > 0)
            return corr;
    }

    for (i = 0; i <= this->maxcol; i++)
        corwidth[i] = orgwidth[i] = round(newwidth[i]);

    check_minimum_width(this, corwidth);

    for (i = 0; i <= this->maxcol; i++)
    {
        double sx = sqrt(m_entry(minv, i, i));
        if (sx < 0.1)
            continue;
        if (orgwidth[i] < this->minimum_width[i] &&
            corwidth[i] == this->minimum_width[i])
        {
            double w = (sx > 0.5) ? 0.5 : sx * 0.2;
            sxy = 0.;
            for (m = 0; m <= this->maxcol; m++)
            {
                if (m == i)
                    continue;
                sxy += m_entry(minv, i, m);
            }
            if (sxy <= 0.)
            {
                correct_table_matrix(this, i, 1, this->minimum_width[i], w);
                corr++;
            }
        }
    }

    for (k = 0; k <= cell->maxcell; k++)
    {
        int nwidth = 0, mwidth;
        double sx;

        j = cell->index[k];
        sx = sqrt(Sxx[j]);
        if (sx < 0.1)
            continue;
        bcol = cell->col[j];
        ecol = bcol + cell->colspan[j];
        for (i = bcol; i < ecol; i++)
            nwidth += corwidth[i];
        mwidth =
            cell->minimum_width[j] - (cell->colspan[j] - 1) * this->cellspacing;
        if (mwidth > swidth[j] && mwidth == nwidth)
        {
            double w = (sx > 0.5) ? 0.5 : sx * 0.2;

            sxy = 0.;
            for (i = bcol; i < ecol; i++)
            {
                for (m = 0; m <= this->maxcol; m++)
                {
                    if (m >= bcol && m < ecol)
                        continue;
                    sxy += m_entry(minv, i, m);
                }
            }
            if (sxy <= 0.)
            {
                correct_table_matrix(this, bcol, cell->colspan[j], mwidth, w);
                corr++;
            }
        }
    }

    if (itr >= MAX_ITERATION)
        return 0;
    else
        return corr;
}

#else  /* not MATRIX */
void set_table_width(struct table *t, short *newwidth, int maxwidth)
{
    int i, j, k, bcol, ecol;
    struct table_cell *cell = &t->cell;
    char *fixed;
    int swidth, fwidth, width, nvar;
    double s;
    double *dwidth;
    int try_again;

    fixed = NewAtom_N(char, t->maxcol + 1);
    bzero(fixed, t->maxcol + 1);
    dwidth = NewAtom_N(double, t->maxcol + 1);

    for (i = 0; i <= t->maxcol; i++)
    {
        dwidth[i] = 0.0;
        if (t->fixed_width[i] < 0)
        {
            t->fixed_width[i] = -t->fixed_width[i] * maxwidth / 100;
        }
        if (t->fixed_width[i] > 0)
        {
            newwidth[i] = t->fixed_width[i];
            fixed[i] = 1;
        }
        else
            newwidth[i] = 0;
        if (newwidth[i] < t->minimum_width[i])
            newwidth[i] = t->minimum_width[i];
    }

    for (k = 0; k <= cell->maxcell; k++)
    {
        j = cell->indexarray[k];
        bcol = cell->col[j];
        ecol = bcol + cell->colspan[j];

        if (cell->fixed_width[j] < 0)
            cell->fixed_width[j] = -cell->fixed_width[j] * maxwidth / 100;

        swidth = 0;
        fwidth = 0;
        nvar = 0;
        for (i = bcol; i < ecol; i++)
        {
            if (fixed[i])
            {
                fwidth += newwidth[i];
            }
            else
            {
                swidth += newwidth[i];
                nvar++;
            }
        }
        width = max(cell->fixed_width[j], cell->minimum_width[j]) - (cell->colspan[j] - 1) * t->cellspacing;
        if (nvar > 0 && width > fwidth + swidth)
        {
            s = 0.;
            for (i = bcol; i < ecol; i++)
            {
                if (!fixed[i])
                    s += weight3(t->tabwidth[i]);
            }
            for (i = bcol; i < ecol; i++)
            {
                if (!fixed[i])
                    dwidth[i] = (width - fwidth) * weight3(t->tabwidth[i]) / s;
                else
                    dwidth[i] = (double)newwidth[i];
            }
            dv2sv(dwidth, newwidth, cell->colspan[j]);
            if (cell->fixed_width[j] > 0)
            {
                for (i = bcol; i < ecol; i++)
                    fixed[i] = 1;
            }
        }
    }

    do
    {
        nvar = 0;
        swidth = 0;
        fwidth = 0;
        for (i = 0; i <= t->maxcol; i++)
        {
            if (fixed[i])
            {
                fwidth += newwidth[i];
            }
            else
            {
                swidth += newwidth[i];
                nvar++;
            }
        }
        width = maxwidth - t->maxcol * t->cellspacing;
        if (nvar == 0 || width <= fwidth + swidth)
            break;

        s = 0.;
        for (i = 0; i <= t->maxcol; i++)
        {
            if (!fixed[i])
                s += weight3(t->tabwidth[i]);
        }
        for (i = 0; i <= t->maxcol; i++)
        {
            if (!fixed[i])
                dwidth[i] = (width - fwidth) * weight3(t->tabwidth[i]) / s;
            else
                dwidth[i] = (double)newwidth[i];
        }
        dv2sv(dwidth, newwidth, t->maxcol + 1);

        try_again = 0;
        for (i = 0; i <= t->maxcol; i++)
        {
            if (!fixed[i])
            {
                if (newwidth[i] > t->tabwidth[i])
                {
                    newwidth[i] = t->tabwidth[i];
                    fixed[i] = 1;
                    try_again = 1;
                }
                else if (newwidth[i] < t->minimum_width[i])
                {
                    newwidth[i] = t->minimum_width[i];
                    fixed[i] = 1;
                    try_again = 1;
                }
            }
        }
    } while (try_again);
}
#endif /* not MATRIX */

void check_table_height(struct table *t)
{
    int i, j, k;
    struct
    {
        short *row;
        short *rowspan;
        short *indexarray;
        short maxcell;
        short size;
        short *height;
    } cell;
    int space = 0;

    cell.size = 0;
    cell.maxcell = -1;

    for (j = 0; j <= t->maxrow; j++)
    {
        if (!t->tabattr[j])
            continue;
        for (i = 0; i <= t->maxcol; i++)
        {
            int t_dep, rowspan;
            if (t->tabattr[j][i] & (HTT_X | HTT_Y))
                continue;

            if (t->tabdata[j][i] == NULL)
                t_dep = 0;
            else
                t_dep = t->tabdata[j][i]->nitem;

            rowspan = t->table_rowspan(j, i);
            if (rowspan > 1)
            {
                int c = cell.maxcell + 1;
                k = bsearch_2short(rowspan, cell.rowspan,
                                   j, cell.row, t->maxrow + 1, cell.indexarray,
                                   c);
                if (k <= cell.maxcell)
                {
                    int idx = cell.indexarray[k];
                    if (cell.row[idx] == j && cell.rowspan[idx] == rowspan)
                        c = idx;
                }
                if (c >= MAXROWCELL)
                    continue;
                if (c >= cell.size)
                {
                    if (cell.size == 0)
                    {
                        cell.size = max(MAXCELL, c + 1);
                        cell.row = NewAtom_N(short, cell.size);
                        cell.rowspan = NewAtom_N(short, cell.size);
                        cell.indexarray = NewAtom_N(short, cell.size);
                        cell.height = NewAtom_N(short, cell.size);
                    }
                    else
                    {
                        cell.size = max(cell.size + MAXCELL, c + 1);
                        cell.row = New_Reuse(short, cell.row, cell.size);
                        cell.rowspan = New_Reuse(short, cell.rowspan,
                                                 cell.size);
                        cell.indexarray = New_Reuse(short, cell.indexarray,
                                                    cell.size);
                        cell.height = New_Reuse(short, cell.height, cell.size);
                    }
                }
                if (c > cell.maxcell)
                {
                    cell.maxcell++;
                    cell.row[cell.maxcell] = j;
                    cell.rowspan[cell.maxcell] = rowspan;
                    cell.height[cell.maxcell] = 0;
                    if (cell.maxcell > k)
                    {
                        int ii;
                        for (ii = cell.maxcell; ii > k; ii--)
                            cell.indexarray[ii] = cell.indexarray[ii - 1];
                    }
                    cell.indexarray[k] = cell.maxcell;
                }

                if (cell.height[c] < t_dep)
                    cell.height[c] = t_dep;
                continue;
            }
            if (t->tabheight[j] < t_dep)
                t->tabheight[j] = t_dep;
        }
    }

    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    case BORDER_NOWIN:
        space = 1;
        break;
    case BORDER_NONE:
        space = 0;
    }
    check_cell_width(t->tabheight, cell.height, cell.row, cell.rowspan,
                     cell.maxcell, cell.indexarray, space, 1);
}

int table::get_table_width(short *orgwidth, short *cellwidth, TableWidthFlags flag)
{
#ifdef __GNUC__
    short newwidth[this->maxcol + 1];
#else  /* not __GNUC__ */
    short newwidth[MAXCOL];
#endif /* not __GNUC__ */
    int i;
    int swidth;
    struct table_cell *cell = &this->cell;
    int rulewidth = this->table_rule_width(Terminal::SymbolWidth());

    for (i = 0; i <= this->maxcol; i++)
        newwidth[i] = max(orgwidth[i], 0);

    if (flag & CHECK_FIXED)
    {
#ifdef __GNUC__
        short ccellwidth[cell->maxcell + 1];
#else  /* not __GNUC__ */
        short ccellwidth[MAXCELL];
#endif /* not __GNUC__ */
        for (i = 0; i <= this->maxcol; i++)
        {
            if (newwidth[i] < this->fixed_width[i])
                newwidth[i] = this->fixed_width[i];
        }
        for (i = 0; i <= cell->maxcell; i++)
        {
            ccellwidth[i] = cellwidth[i];
            if (ccellwidth[i] < cell->fixed_width[i])
                ccellwidth[i] = cell->fixed_width[i];
        }
        check_cell_width(newwidth, ccellwidth, cell->col, cell->colspan,
                         cell->maxcell, cell->index, this->cellspacing, 0);
    }
    else
    {
        check_cell_width(newwidth, cellwidth, cell->col, cell->colspan,
                         cell->maxcell, cell->index, this->cellspacing, 0);
    }
    if (flag & CHECK_MINIMUM)
        check_minimum_width(this, newwidth);

    swidth = 0;
    for (i = 0; i <= this->maxcol; i++)
    {
        swidth += ceil_at_intervals(newwidth[i], rulewidth);
    }
    swidth += this->table_border_width(Terminal::SymbolWidth());
    return swidth;
}

static void renderCoTable(struct table *tbl, int maxlimit, HtmlContext *seq)
{
    struct readbuffer obuf;
    struct table *t;
    int i, col, row;
    int indent, maxwidth;

    for (i = 0; i < tbl->ntable; i++)
    {
        t = tbl->tables[i].ptr;
        col = tbl->tables[i].col;
        row = tbl->tables[i].row;
        indent = tbl->tables[i].indent;

        html_feed_environ h_env(&obuf, tbl->tables[i].buf,
                                tbl->get_spec_cell_width(row, col), indent);
        tbl->check_row(row);
        if (h_env.limit > maxlimit)
            h_env.limit = maxlimit;
        if (t->total_width == 0)
            maxwidth = h_env.limit - indent;
        else if (t->total_width > 0)
            maxwidth = t->total_width;
        else
            maxwidth = t->total_width = -t->total_width * h_env.limit / 100;
        renderTable(t, maxwidth, &h_env, seq);
    }
}

void renderTable(struct table *t, int max_width, struct html_feed_environ *h_env, HtmlContext *seq)
{
    t->total_height = 0;
    if (t->maxcol < 0)
    {
        seq->make_caption(t, h_env);
        return;
    }

    if (t->sloppy_width > max_width)
        max_width = t->sloppy_width;

    int rulewidth = t->table_rule_width(Terminal::SymbolWidth());

    max_width -= t->table_border_width(Terminal::SymbolWidth());

    if (rulewidth > 1)
        max_width = floor_at_intervals(max_width, rulewidth);

    if (max_width < rulewidth)
        max_width = rulewidth;

    check_maximum_width(t);

#ifdef MATRIX
    if (t->maxcol == 0)
    {
        if (t->tabwidth[0] > max_width)
            t->tabwidth[0] = max_width;
        if (t->total_width > 0)
            t->tabwidth[0] = max_width;
        else if (t->fixed_width[0] > 0)
            t->tabwidth[0] = t->fixed_width[0];
        if (t->tabwidth[0] < t->minimum_width[0])
            t->tabwidth[0] = t->minimum_width[0];
    }
    else
    {
        set_table_matrix(t, max_width);

        int itr = 0;
        auto mat = m_get(t->maxcol + 1, t->maxcol + 1);
        auto pivot = px_get(t->maxcol + 1);
        auto newwidth = v_get(t->maxcol + 1);
        auto minv = m_get(t->maxcol + 1, t->maxcol + 1);
        do
        {
            m_copy(t->matrix, mat);
            LUfactor(mat, pivot);
            LUsolve(mat, pivot, t->vector, newwidth);
            LUinverse(mat, pivot, minv);
#ifdef TABLE_DEBUG
            set_integered_width(t, newwidth->ve, new_tabwidth);
            fprintf(stderr, "itr=%d\n", itr);
            fprintf(stderr, "max_width=%d\n", max_width);
            fprintf(stderr, "minimum : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", t->minimum_width[i]);
            fprintf(stderr, "\nfixed : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", t->fixed_width[i]);
            fprintf(stderr, "\ndecided : ");
            for (i = 0; i <= t->maxcol; i++)
                fprintf(stderr, "%2d ", new_tabwidth[i]);
            fprintf(stderr, "\n");
#endif /* TABLE_DEBUG */
            itr++;

        } while (t->check_table_width(newwidth->ve, minv, itr));
        short new_tabwidth[MAXCOL];
        set_integered_width(t, newwidth->ve, new_tabwidth, Terminal::SymbolWidth());
        check_minimum_width(t, new_tabwidth);
        v_free(newwidth);
        px_free(pivot);
        m_free(mat);
        m_free(minv);
        m_free(t->matrix);
        v_free(t->vector);
        for (int i = 0; i <= t->maxcol; i++)
        {
            t->tabwidth[i] = new_tabwidth[i];
        }
    }
#else  /* not MATRIX */
    set_table_width(t, new_tabwidth, max_width);
    for (i = 0; i <= t->maxcol; i++)
    {
        t->tabwidth[i] = new_tabwidth[i];
    }
#endif /* not MATRIX */

    check_minimum_width(t, t->tabwidth);
    for (int i = 0; i <= t->maxcol; i++)
        t->tabwidth[i] = ceil_at_intervals(t->tabwidth[i], rulewidth);

    renderCoTable(t, h_env->limit, seq);

    for (int i = 0; i <= t->maxcol; i++)
    {
        for (int j = 0; j <= t->maxrow; j++)
        {
            t->check_row(j);
            if (t->tabattr[j][i] & HTT_Y)
                continue;
            seq->do_refill(t, j, i, h_env->limit);
        }
    }

    check_minimum_width(t, t->tabwidth);
    t->total_width = 0;
    for (int i = 0; i <= t->maxcol; i++)
    {
        t->tabwidth[i] = ceil_at_intervals(t->tabwidth[i], rulewidth);
        t->total_width += t->tabwidth[i];
    }

    t->total_width += t->table_border_width(Terminal::SymbolWidth());

    check_table_height(t);

    for (int i = 0; i <= t->maxcol; i++)
    {
        for (int j = 0; j <= t->maxrow; j++)
        {
            if ((t->tabattr[j][i] & HTT_Y) ||
                (t->tabattr[j][i] & HTT_TOP) || (t->tabdata[j][i] == NULL))
                continue;
            auto h = t->tabheight[j];
            for (auto k = j + 1; k <= t->maxrow; k++)
            {
                if (!(t->tabattr[k][i] & HTT_Y))
                    break;
                h += t->tabheight[k];
                switch (t->border_mode)
                {
                case BORDER_THIN:
                case BORDER_THICK:
                case BORDER_NOWIN:
                    h += 1;
                    break;
                }
            }
            h -= t->tabdata[j][i]->nitem;
            if (t->tabattr[j][i] & HTT_MIDDLE)
                h /= 2;
            if (h <= 0)
                continue;
            auto l = newTextLineList();
            for (auto k = 0; k < h; k++)
                pushTextLine(l, newTextLine(NULL, 0));
            t->tabdata[j][i] = appendGeneralList((GeneralList *)l,
                                                 t->tabdata[j][i]);
        }
    }

    /* table output */
    auto width = t->total_width;

    seq->make_caption(t, h_env);

    seq->HTMLlineproc0("<pre for_table>", h_env, true);

    if (t->id != NULL)
    {
        auto idtag = Sprintf("<_id id=\"%s\">", html_quote((t->id)->ptr));
        seq->HTMLlineproc0(idtag->ptr, h_env, true);
    }

    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    {
        auto renderbuf = Strnew();
        t->print_sep(-1, VALIGN_TOP, t->maxcol, renderbuf, Terminal::SymbolWidth());
        h_env->push_render_image(renderbuf, width, t->total_width);
        t->total_height += 1;
        break;
    }
    }

    Str vrulea = nullptr;
    Str vruleb = Strnew();
    Str vrulec = nullptr;
    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
        vrulea = Strnew();
        vrulec = Strnew();
        push_symbol(vrulea, TK_VERTICALBAR(t->border_mode), Terminal::SymbolWidth(), 1);
        for (int i = 0; i < t->cellpadding; i++)
        {
            vrulea->Push(' ');
            vruleb->Push(' ');
            vrulec->Push(' ');
        }
        push_symbol(vrulec, TK_VERTICALBAR(t->border_mode), Terminal::SymbolWidth(), 1);
    case BORDER_NOWIN:
        push_symbol(vruleb, TK_VERTICALBAR(BORDER_THIN), Terminal::SymbolWidth(), 1);
        for (int i = 0; i < t->cellpadding; i++)
            vruleb->Push(' ');
        break;
    case BORDER_NONE:
        for (int i = 0; i < t->cellspacing; i++)
            vruleb->Push(' ');
    }

    for (int r = 0; r <= t->maxrow; r++)
    {
        for (int h = 0; h < t->tabheight[r]; h++)
        {
            auto renderbuf = Strnew();
            if (t->border_mode == BORDER_THIN || t->border_mode == BORDER_THICK)
                renderbuf->Push(vrulea);

            if (t->tridvalue[r] != NULL && h == 0)
            {
                auto idtag = Sprintf("<_id id=\"%s\">",
                                     html_quote((t->tridvalue[r])->ptr));
                renderbuf->Push(idtag);
            }

            for (int i = 0; i <= t->maxcol; i++)
            {
                t->check_row(r);

                if (t->tabidvalue[r][i] != NULL && h == 0)
                {
                    auto idtag = Sprintf("<_id id=\"%s\">",
                                         html_quote((t->tabidvalue[r][i])->ptr));
                    renderbuf->Push(idtag);
                }

                if (!(t->tabattr[r][i] & HTT_X))
                {
                    int w = t->tabwidth[i];
                    for (int j = i + 1;
                         j <= t->maxcol && (t->tabattr[r][j] & HTT_X); j++)
                        w += t->tabwidth[j] + t->cellspacing;
                    if (t->tabattr[r][i] & HTT_Y)
                    {
                        int j = r - 1;
                        for (; j >= 0 && t->tabattr[j] && (t->tabattr[j][i] & HTT_Y); j--)
                            ;
                        t->print_item(j, i, w, renderbuf);
                    }
                    else
                        t->print_item(r, i, w, renderbuf);
                }
                if (i < t->maxcol && !(t->tabattr[r][i + 1] & HTT_X))
                    renderbuf->Push(vruleb);
            }
            switch (t->border_mode)
            {
            case BORDER_THIN:
            case BORDER_THICK:
                renderbuf->Push(vrulec);
                t->total_height += 1;
                break;
            }
            h_env->push_render_image(renderbuf, width, t->total_width);
        }
        if (r < t->maxrow && t->border_mode != BORDER_NONE)
        {
            auto renderbuf = Strnew();
            t->print_sep(r, VALIGN_MIDDLE, t->maxcol, renderbuf, Terminal::SymbolWidth());
            h_env->push_render_image(renderbuf, width, t->total_width);
        }
        t->total_height += t->tabheight[r];
    }
    switch (t->border_mode)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    {
        auto renderbuf = Strnew();
        t->print_sep(t->maxrow, VALIGN_BOTTOM, t->maxcol, renderbuf, Terminal::SymbolWidth());
        h_env->push_render_image(renderbuf, width, t->total_width);
        t->total_height += 1;
        break;
    }
    }
    if (t->total_height == 0)
    {
        auto renderbuf = Strnew(" ");
        t->total_height++;
        t->total_width = 1;
        h_env->push_render_image(renderbuf, 1, t->total_width);
    }
    seq->HTMLlineproc0("</pre>", h_env, true);
}

#ifdef TABLE_NO_COMPACT
#define THR_PADDING 2
#else
#define THR_PADDING 4
#endif
struct table *begin_table(BorderModes border, int spacing, int padding, int vspace, HtmlContext *seq)
{
    struct table *t;
    int mincell = minimum_cellspacing(border, Terminal::SymbolWidth());
    int rcellspacing;
    int mincell_pixels = round(mincell * ImageManager::Instance().pixel_per_char);
    int ppc = round(ImageManager::Instance().pixel_per_char);

    t = newTable();
    t->row = t->col = -1;
    t->maxcol = -1;
    t->maxrow = -1;
    t->border_mode = border;
    t->flag = 0;
    if (border == BORDER_NOWIN)
        t->flag |= TBL_EXPAND_OK;

    rcellspacing = spacing + 2 * padding;
    switch (border)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    case BORDER_NOWIN:
        t->cellpadding = padding - (mincell_pixels - 4) / 2;
        break;
    case BORDER_NONE:
        t->cellpadding = rcellspacing - mincell_pixels;
    }
    if (t->cellpadding >= ppc)
        t->cellpadding /= ppc;
    else if (t->cellpadding > 0)
        t->cellpadding = 1;
    else
        t->cellpadding = 0;

    switch (border)
    {
    case BORDER_THIN:
    case BORDER_THICK:
    case BORDER_NOWIN:
        t->cellspacing = 2 * t->cellpadding + mincell;
        break;
    case BORDER_NONE:
        t->cellspacing = t->cellpadding + mincell;
    }

    if (border == BORDER_NONE)
    {
        if (rcellspacing / 2 + vspace <= 1)
            t->vspace = 0;
        else
            t->vspace = 1;
    }
    else
    {
        if (vspace < ppc)
            t->vspace = 0;
        else
            t->vspace = 1;
    }

    if (border == BORDER_NONE)
    {
        if (rcellspacing <= THR_PADDING)
            t->vcellpadding = 0;
        else
            t->vcellpadding = 1;
    }
    else
    {
        if (padding < 2 * ppc - 2)
            t->vcellpadding = 0;
        else
            t->vcellpadding = 1;
    }

    return t;
}

void end_table(struct table *tbl, HtmlContext *seq)
{
    struct table_cell *cell = &tbl->cell;
    int rulewidth = tbl->table_rule_width(Terminal::SymbolWidth());
    if (rulewidth > 1)
    {
        if (tbl->total_width > 0)
            tbl->total_width = ceil_at_intervals(tbl->total_width, rulewidth);
        for (int i = 0; i <= tbl->maxcol; i++)
        {
            tbl->minimum_width[i] =
                ceil_at_intervals(tbl->minimum_width[i], rulewidth);
            tbl->tabwidth[i] = ceil_at_intervals(tbl->tabwidth[i], rulewidth);
            if (tbl->fixed_width[i] > 0)
                tbl->fixed_width[i] =
                    ceil_at_intervals(tbl->fixed_width[i], rulewidth);
        }
        for (int i = 0; i <= cell->maxcell; i++)
        {
            cell->minimum_width[i] =
                ceil_at_intervals(cell->minimum_width[i], rulewidth);
            cell->width[i] = ceil_at_intervals(cell->width[i], rulewidth);
            if (cell->fixed_width[i] > 0)
                cell->fixed_width[i] =
                    ceil_at_intervals(cell->fixed_width[i], rulewidth);
        }
    }
    tbl->sloppy_width = tbl->fixed_table_width();
    if (tbl->total_width > tbl->sloppy_width)
        tbl->sloppy_width = tbl->total_width;
}

static void
check_minimum0(struct table *t, int min)
{
    int i, w, ww;
    struct table_cell *cell;

    if (t->col < 0)
        return;
    if (t->tabwidth[t->col] < 0)
        return;
    t->check_row(t->row);
    w = t->table_colspan(t->row, t->col);
    min += t->indent;
    if (w == 1)
        ww = min;
    else
    {
        cell = &t->cell;
        ww = 0;
        if (cell->icell >= 0 && cell->minimum_width[cell->icell] < min)
            cell->minimum_width[cell->icell] = min;
    }
    for (i = t->col;
         i <= t->maxcol && (i == t->col || (t->tabattr[t->row][i] & HTT_X));
         i++)
    {
        if (t->minimum_width[i] < ww)
            t->minimum_width[i] = ww;
    }
}

int table::setwidth0(struct table_mode *mode)
{
    if (this->col < 0)
        return -1;
    if (this->tabwidth[this->col] < 0)
        return -1;

    struct table_cell *cell = &this->cell;
    int width = this->tabcontentssize;
    this->check_row(this->row);
    if (this->linfo.prev_spaces > 0)
        width -= this->linfo.prev_spaces;
    int w = this->table_colspan(this->row, this->col);
    if (w == 1)
    {
        if (this->tabwidth[this->col] < width)
            this->tabwidth[this->col] = width;
    }
    else if (cell->icell >= 0)
    {
        if (cell->width[cell->icell] < width)
            cell->width[cell->icell] = width;
    }
    return width;
}

void table::setwidth(struct table_mode *mode)
{
    int width = this->setwidth0(mode);
    if (width < 0)
        return;
#ifdef NOWRAP
    if (this->tabattr[this->row][this->col] & HTT_NOWRAP)
        check_minimum0(this, width);
#endif /* NOWRAP */
    if (mode->pre_mode & (TBLM_NOBR | TBLM_PRE | TBLM_PRE_INT) &&
        mode->nobr_offset >= 0)
        check_minimum0(this, width - mode->nobr_offset);
}

static void
addcontentssize(struct table *t, int width)
{

    if (t->col < 0)
        return;
    if (t->tabwidth[t->col] < 0)
        return;
    t->check_row(t->row);
    t->tabcontentssize += width;
}

static void table_close_anchor0(struct table *tbl, struct table_mode *mode);

static void
clearcontentssize(struct table *t, struct table_mode *mode)
{
    table_close_anchor0(t, mode);
    mode->nobr_offset = 0;
    t->linfo.prev_spaces = -1;
    set_space_to_prevchar(t->linfo.prevchar);
    t->linfo.prev_ctype = PC_ASCII;
    t->linfo.length = 0;
    t->tabcontentssize = 0;
}

static void
begin_cell(struct table *t, struct table_mode *mode)
{
    clearcontentssize(t, mode);
    mode->indent_level = 0;
    mode->nobr_level = 0;
    mode->pre_mode = TBLM_NONE;
    t->indent = 0;
    t->flag |= TBL_IN_COL;

    if (t->suspended_data)
    {
        t->check_row(t->row);
        if (t->tabdata[t->row][t->col] == NULL)
            t->tabdata[t->row][t->col] = newGeneralList();
        appendGeneralList(t->tabdata[t->row][t->col],
                          (GeneralList *)t->suspended_data);
        t->suspended_data = NULL;
    }
}

void check_rowcol(struct table *tbl, struct table_mode *mode)
{
    int row = tbl->row, col = tbl->col;

    if (!(tbl->flag & TBL_IN_ROW))
    {
        tbl->flag |= TBL_IN_ROW;
        tbl->row++;
        if (tbl->row > tbl->maxrow)
            tbl->maxrow = tbl->row;
        tbl->col = -1;
    }
    if (tbl->row == -1)
        tbl->row = 0;
    if (tbl->col == -1)
        tbl->col = 0;

    for (;; tbl->row++)
    {
        tbl->check_row(tbl->row);
        for (; tbl->col < MAXCOL &&
               tbl->tabattr[tbl->row][tbl->col] & (HTT_X | HTT_Y);
             tbl->col++)
            ;
        if (tbl->col < MAXCOL)
            break;
        tbl->col = 0;
    }
    if (tbl->row > tbl->maxrow)
        tbl->maxrow = tbl->row;
    if (tbl->col > tbl->maxcol)
        tbl->maxcol = tbl->col;

    if (tbl->row != row || tbl->col != col)
        begin_cell(tbl, mode);
    tbl->flag |= TBL_IN_COL;
}

int skip_space(struct table *t, const char *line, struct table_linfo *linfo,
               int checkminimum)
{
    int skip = 0, s = linfo->prev_spaces;
    Lineprop ctype, prev_ctype = linfo->prev_ctype;
    Str prevchar = linfo->prevchar;
    int w = linfo->length;
    int min = 1;

    if (*line == '<' && line[strlen(line) - 1] == '>')
    {
        if (checkminimum)
            check_minimum0(t, visible_length(line));
        return 0;
    }

    while (*line)
    {
        const char *save = line, *c = line;
        int len, wlen, plen;
        ctype = get_mctype(*line);
        len = get_mcwidth(line);
        wlen = plen = get_mclen(line);

        if (min < w)
            min = w;
        if (ctype == PC_ASCII && IS_SPACE(*c))
        {
            w = 0;
            s++;
        }
        else
        {
            if (*c == '&')
            {
                auto [pos, ec] = ucs4_from_entity(line);
                line = pos.data();
                if (ec >= 0)
                {
                    c = (char *)from_unicode(ec, w3mApp::Instance().InnerCharset);
                    ctype = get_mctype(*c);
                    len = get_strwidth(c);
                    wlen = line - save;
                    plen = get_mclen(c);
                }
            }
            if (prevchar->Size() && is_boundary((unsigned char *)prevchar->ptr,
                                                (unsigned char *)c))
            {
                w = len;
            }
            else
            {
                w += len;
            }
            if (s > 0)
            {
#ifdef USE_M17N
                if (ctype == PC_KANJI1 && prev_ctype == PC_KANJI1)
                    skip += s;
                else
#endif
                    skip += s - 1;
            }
            s = 0;
            prev_ctype = ctype;
        }
        prevchar->CopyFrom(c, plen);
        line = save + wlen;
    }
    if (s > 1)
    {
        skip += s - 1;
        linfo->prev_spaces = 1;
    }
    else
    {
        linfo->prev_spaces = s;
    }
    linfo->prev_ctype = prev_ctype;
    linfo->prevchar = prevchar;

    if (checkminimum)
    {
        if (min < w)
            min = w;
        linfo->length = w;
        check_minimum0(t, min);
    }
    return skip;
}

static void
feed_table_inline_tag(struct table *tbl,
                      const char *line, struct table_mode *mode, int width)
{
    check_rowcol(tbl, mode);
    tbl->pushdata(tbl->row, tbl->col, line);
    if (width >= 0)
    {
        check_minimum0(tbl, width);
        addcontentssize(tbl, width);
        tbl->setwidth(mode);
    }
}

void table::feed_table_block_tag(const char *line, struct table_mode *mode, int indent, int cmd)
{
    if (mode->indent_level <= 0 && indent == -1)
        return;
    this->setwidth(mode);
    feed_table_inline_tag(this, line, mode, -1);
    clearcontentssize(this, mode);
    if (indent == 1)
    {
        mode->indent_level++;
        if (mode->indent_level <= MAX_INDENT_LEVEL)
            this->indent += w3mApp::Instance().IndentIncr;
    }
    else if (indent == -1)
    {
        mode->indent_level--;
        if (mode->indent_level < MAX_INDENT_LEVEL)
            this->indent -= w3mApp::Instance().IndentIncr;
    }
    int offset = this->indent;
    if (cmd == HTML_DT)
    {
        if (mode->indent_level > 0 && mode->indent_level <= MAX_INDENT_LEVEL)
            offset -= w3mApp::Instance().IndentIncr;
    }
    if (this->indent > 0)
    {
        check_minimum0(this, 0);
        addcontentssize(this, offset);
    }
}

static void
table_close_select(struct table *tbl, struct table_mode *mode, int width, HtmlContext *seq)
{
    Str tmp = seq->process_n_select();
    mode->pre_mode &= ~TBLM_INSELECT;
    mode->end_tag = HTML_UNKNOWN;
    feed_table1(tbl, tmp, mode, width, seq);
}

static void
table_close_textarea(struct table *tbl, struct table_mode *mode, int width, HtmlContext *seq)
{
    Str tmp = seq->process_n_textarea();
    mode->pre_mode &= ~TBLM_INTXTA;
    mode->end_tag = HTML_UNKNOWN;
    feed_table1(tbl, tmp, mode, width, seq);
}

static void
table_close_anchor0(struct table *tbl, struct table_mode *mode)
{
    if (!(mode->pre_mode & TBLM_ANCHOR))
        return;
    mode->pre_mode &= ~TBLM_ANCHOR;
    if (tbl->tabcontentssize == mode->anchor_offset)
    {
        check_minimum0(tbl, 1);
        addcontentssize(tbl, 1);
        tbl->setwidth(mode);
    }
    else if (tbl->linfo.prev_spaces > 0 &&
             tbl->tabcontentssize - 1 == mode->anchor_offset)
    {
        if (tbl->linfo.prev_spaces > 0)
            tbl->linfo.prev_spaces = -1;
    }
}

#define TAG_ACTION_NONE 0
#define TAG_ACTION_FEED 1
#define TAG_ACTION_TABLE 2
#define TAG_ACTION_N_TABLE 3
#define TAG_ACTION_PLAIN 4

#define CASE_TABLE_TAG    \
    case HTML_TABLE:      \
    case HTML_N_TABLE:    \
    case HTML_TR:         \
    case HTML_N_TR:       \
    case HTML_TD:         \
    case HTML_N_TD:       \
    case HTML_TH:         \
    case HTML_N_TH:       \
    case HTML_THEAD:      \
    case HTML_N_THEAD:    \
    case HTML_TBODY:      \
    case HTML_N_TBODY:    \
    case HTML_TFOOT:      \
    case HTML_N_TFOOT:    \
    case HTML_COLGROUP:   \
    case HTML_N_COLGROUP: \
    case HTML_COL

#define ATTR_ROWSPAN_MAX 32766

static int
feed_table_tag(struct table *tbl, const char *line, struct table_mode *mode,
               int width, struct parsed_tag *tag, HtmlContext *seq)
{
    int cmd;
#ifdef ID_EXT
    char *p;
#endif
    struct table_cell *cell = &tbl->cell;
    int colspan, rowspan;
    int col, prev_col;
    int i, j, k, v, v0, w, id;
    Str tok, tmp, anchor;
    TableAttributes align, valign;

    cmd = tag->tagid;

    if (mode->pre_mode & TBLM_PLAIN)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_PLAIN;
            mode->end_tag = HTML_UNKNOWN;
            tbl->feed_table_block_tag(line, mode, 0, cmd);
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    if (mode->pre_mode & TBLM_INTXTA)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_TEXTAREA:
            table_close_textarea(tbl, mode, width, seq);
            if (cmd == HTML_N_TEXTAREA)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }
    if (mode->pre_mode & TBLM_SCRIPT)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_SCRIPT;
            mode->end_tag = HTML_UNKNOWN;
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    if (mode->pre_mode & TBLM_STYLE)
    {
        if (mode->end_tag == cmd)
        {
            mode->pre_mode &= ~TBLM_STYLE;
            mode->end_tag = HTML_UNKNOWN;
            return TAG_ACTION_NONE;
        }
        return TAG_ACTION_PLAIN;
    }
    /* failsafe: a tag other than <option></option>and </select> in *
     * <select> environment is regarded as the end of <select>. */
    if (mode->pre_mode & TBLM_INSELECT)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_FORM:
        case HTML_N_SELECT: /* mode->end_tag */
            table_close_select(tbl, mode, width, seq);
            if (cmd == HTML_N_SELECT)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }
    if (mode->caption)
    {
        switch (cmd)
        {
        CASE_TABLE_TAG:
        case HTML_N_CAPTION:
            mode->caption = 0;
            if (cmd == HTML_N_CAPTION)
                return TAG_ACTION_NONE;
            break;
        default:
            return TAG_ACTION_FEED;
        }
    }

    if (mode->pre_mode & TBLM_PRE)
    {
        switch (cmd)
        {
        case HTML_NOBR:
        case HTML_N_NOBR:
        case HTML_PRE_INT:
        case HTML_N_PRE_INT:
            return TAG_ACTION_NONE;
        }
    }

    switch (cmd)
    {
    case HTML_TABLE:
        check_rowcol(tbl, mode);
        return TAG_ACTION_TABLE;
    case HTML_N_TABLE:
        if (tbl->suspended_data)
            check_rowcol(tbl, mode);
        return TAG_ACTION_N_TABLE;
    case HTML_TR:
        if (tbl->col >= 0 && tbl->tabcontentssize > 0)
            tbl->setwidth(mode);
        tbl->col = -1;
        tbl->row++;
        tbl->flag |= TBL_IN_ROW;
        tbl->flag &= ~TBL_IN_COL;
        align = {};
        valign = {};
        if (tag->TryGetAttributeValue(ATTR_ALIGN, &i))
        {
            switch (i)
            {
            case ALIGN_LEFT:
                align = (HTT_LEFT | HTT_TRSET);
                break;
            case ALIGN_RIGHT:
                align = (HTT_RIGHT | HTT_TRSET);
                break;
            case ALIGN_CENTER:
                align = (HTT_CENTER | HTT_TRSET);
                break;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_VALIGN, &i))
        {
            switch (i)
            {
            case VALIGN_TOP:
                valign = (HTT_TOP | HTT_VTRSET);
                break;
            case VALIGN_MIDDLE:
                valign = (HTT_MIDDLE | HTT_VTRSET);
                break;
            case VALIGN_BOTTOM:
                valign = (HTT_BOTTOM | HTT_VTRSET);
                break;
            }
        }
#ifdef ID_EXT
        if (tag->TryGetAttributeValue(ATTR_ID, &p))
            tbl->tridvalue[tbl->row] = Strnew(p);
#endif /* ID_EXT */
        tbl->trattr = align | valign;
        break;
    case HTML_TH:
    case HTML_TD:
        prev_col = tbl->col;
        if (tbl->col >= 0 && tbl->tabcontentssize > 0)
            tbl->setwidth(mode);
        if (tbl->row == -1)
        {
            /* for broken HTML... */
            tbl->row = -1;
            tbl->col = -1;
            tbl->maxrow = tbl->row;
        }
        if (tbl->col == -1)
        {
            if (!(tbl->flag & TBL_IN_ROW))
            {
                tbl->row++;
                tbl->flag |= TBL_IN_ROW;
            }
            if (tbl->row > tbl->maxrow)
                tbl->maxrow = tbl->row;
        }
        tbl->col++;
        tbl->check_row(tbl->row);
        while (tbl->tabattr[tbl->row][tbl->col])
        {
            tbl->col++;
        }
        if (tbl->col > MAXCOL - 1)
        {
            tbl->col = prev_col;
            return TAG_ACTION_NONE;
        }
        if (tbl->col > tbl->maxcol)
        {
            tbl->maxcol = tbl->col;
        }
        colspan = rowspan = 1;
        if (tbl->trattr & HTT_TRSET)
            align = (tbl->trattr & HTT_ALIGN);
        else if (cmd == HTML_TH)
            align = HTT_CENTER;
        else
            align = HTT_LEFT;
        if (tbl->trattr & HTT_VTRSET)
            valign = (tbl->trattr & HTT_VALIGN);
        else
            valign = HTT_MIDDLE;
        if (tag->TryGetAttributeValue(ATTR_ROWSPAN, &rowspan))
        {
            if (rowspan > ATTR_ROWSPAN_MAX)
            {
                rowspan = ATTR_ROWSPAN_MAX;
            }
            if ((tbl->row + rowspan) >= tbl->max_rowsize)
                tbl->check_row(tbl->row + rowspan);
        }
        if (tag->TryGetAttributeValue(ATTR_COLSPAN, &colspan))
        {
            if ((tbl->col + colspan) >= MAXCOL)
            {
                /* Can't expand column */
                colspan = MAXCOL - tbl->col;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_ALIGN, &i))
        {
            switch (i)
            {
            case ALIGN_LEFT:
                align = HTT_LEFT;
                break;
            case ALIGN_RIGHT:
                align = HTT_RIGHT;
                break;
            case ALIGN_CENTER:
                align = HTT_CENTER;
                break;
            }
        }
        if (tag->TryGetAttributeValue(ATTR_VALIGN, &i))
        {
            switch (i)
            {
            case VALIGN_TOP:
                valign = HTT_TOP;
                break;
            case VALIGN_MIDDLE:
                valign = HTT_MIDDLE;
                break;
            case VALIGN_BOTTOM:
                valign = HTT_BOTTOM;
                break;
            }
        }
#ifdef NOWRAP
        if (tag->HasAttribute(ATTR_NOWRAP))
            tbl->tabattr[tbl->row][tbl->col] |= HTT_NOWRAP;
#endif /* NOWRAP */
        v = 0;
        if (tag->TryGetAttributeValue(ATTR_WIDTH, &v))
        {
#ifdef TABLE_EXPAND
            if (v > 0)
            {
                if (tbl->real_width > 0)
                    v = -(v * 100) / (tbl->real_width * ImageManager::Instance().pixel_per_char);
                else
                    v = (int)(v / ImageManager::Instance().pixel_per_char);
            }
#else
            v = RELATIVE_WIDTH(v);
#endif /* not TABLE_EXPAND */
        }
#ifdef ID_EXT
        if (tag->TryGetAttributeValue(ATTR_ID, &p))
            tbl->tabidvalue[tbl->row][tbl->col] = Strnew(p);
#endif /* ID_EXT */
#ifdef NOWRAP
        if (v != 0)
        {
            /* NOWRAP and WIDTH= conflicts each other */
            tbl->tabattr[tbl->row][tbl->col] &= ~HTT_NOWRAP;
        }
#endif /* NOWRAP */
        tbl->tabattr[tbl->row][tbl->col] &= ~(HTT_ALIGN | HTT_VALIGN);
        tbl->tabattr[tbl->row][tbl->col] |= (align | valign);
        if (colspan > 1)
        {
            col = tbl->col;

            cell->icell = cell->maxcell + 1;
            k = bsearch_2short(colspan, cell->colspan, col, cell->col, MAXCOL,
                               cell->index, cell->icell);
            if (k <= cell->maxcell)
            {
                i = cell->index[k];
                if (cell->col[i] == col && cell->colspan[i] == colspan)
                    cell->icell = i;
            }
            if (cell->icell > cell->maxcell && cell->icell < MAXCELL)
            {
                cell->maxcell++;
                cell->col[cell->maxcell] = col;
                cell->colspan[cell->maxcell] = colspan;
                cell->width[cell->maxcell] = 0;
                cell->minimum_width[cell->maxcell] = 0;
                cell->fixed_width[cell->maxcell] = 0;
                if (cell->maxcell > k)
                {
                    int ii;
                    for (ii = cell->maxcell; ii > k; ii--)
                        cell->index[ii] = cell->index[ii - 1];
                }
                cell->index[k] = cell->maxcell;
            }
            if (cell->icell > cell->maxcell)
                cell->icell = -1;
        }
        if (v != 0)
        {
            if (colspan == 1)
            {
                v0 = tbl->fixed_width[tbl->col];
                if (v0 == 0 || (v0 > 0 && v > v0) || (v0 < 0 && v < v0))
                {
#ifdef FEED_TABLE_DEBUG
                    fprintf(stderr, "width(%d) = %d\n", tbl->col, v);
#endif /* TABLE_DEBUG */
                    tbl->fixed_width[tbl->col] = v;
                }
            }
            else if (cell->icell >= 0)
            {
                v0 = cell->fixed_width[cell->icell];
                if (v0 == 0 || (v0 > 0 && v > v0) || (v0 < 0 && v < v0))
                    cell->fixed_width[cell->icell] = v;
            }
        }
        for (i = 0; i < rowspan; i++)
        {
            tbl->check_row(tbl->row + i);
            for (j = 0; j < colspan; j++)
            {
#if 0
		tbl->tabattr[tbl->row + i][tbl->col + j] &= ~(HTT_X | HTT_Y);
#endif
                if (!(tbl->tabattr[tbl->row + i][tbl->col + j] &
                      (HTT_X | HTT_Y)))
                {
                    tbl->tabattr[tbl->row + i][tbl->col + j] |=
                        ((i > 0) ? HTT_Y : HTT_NONE) | ((j > 0) ? HTT_X : HTT_NONE);
                }
                if (tbl->col + j > tbl->maxcol)
                {
                    tbl->maxcol = tbl->col + j;
                }
            }
            if (tbl->row + i > tbl->maxrow)
            {
                tbl->maxrow = tbl->row + i;
            }
        }
        begin_cell(tbl, mode);
        break;
    case HTML_N_TR:
        tbl->setwidth(mode);
        tbl->col = -1;
        tbl->flag &= ~(TBL_IN_ROW | TBL_IN_COL);
        return TAG_ACTION_NONE;
    case HTML_N_TH:
    case HTML_N_TD:
        tbl->setwidth(mode);
        tbl->flag &= ~TBL_IN_COL;
#ifdef FEED_TABLE_DEBUG
        {
            TextListItem *it;
            int i = tbl->col, j = tbl->row;
            fprintf(stderr, "(a) row,col: %d, %d\n", j, i);
            if (tbl->tabdata[j] && tbl->tabdata[j][i])
            {
                for (it = ((TextList *)tbl->tabdata[j][i])->first;
                     it; it = it->next)
                    fprintf(stderr, "  [%s] \n", it->ptr);
            }
        }
#endif
        return TAG_ACTION_NONE;
    case HTML_P:
    case HTML_BR:
    case HTML_CENTER:
    case HTML_N_CENTER:
    case HTML_DIV:
    case HTML_N_DIV:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
    case HTML_DT:
    case HTML_DD:
    case HTML_H:
    case HTML_N_H:
    case HTML_LI:
    case HTML_PRE:
    case HTML_N_PRE:
    case HTML_HR:
    case HTML_LISTING:
    case HTML_XMP:
    case HTML_PLAINTEXT:
    case HTML_PRE_PLAIN:
    case HTML_N_PRE_PLAIN:
        tbl->feed_table_block_tag(line, mode, 0, cmd);
        switch (cmd)
        {
        case HTML_PRE:
        case HTML_PRE_PLAIN:
            mode->pre_mode |= TBLM_PRE;
            break;
        case HTML_N_PRE:
        case HTML_N_PRE_PLAIN:
            mode->pre_mode &= ~TBLM_PRE;
            break;
        case HTML_LISTING:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = HTML_N_LISTING;
            break;
        case HTML_XMP:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = HTML_N_XMP;
            break;
        case HTML_PLAINTEXT:
            mode->pre_mode |= TBLM_PLAIN;
            mode->end_tag = MAX_HTMLTAG;
            break;
        }
        break;
    case HTML_DL:
    case HTML_BLQ:
    case HTML_OL:
    case HTML_UL:
        tbl->feed_table_block_tag(line, mode, 1, cmd);
        break;
    case HTML_N_DL:
    case HTML_N_BLQ:
    case HTML_N_OL:
    case HTML_N_UL:
        tbl->feed_table_block_tag(line, mode, -1, cmd);
        break;
    case HTML_NOBR:
    case HTML_WBR:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
    case HTML_PRE_INT:
        feed_table_inline_tag(tbl, line, mode, -1);
        switch (cmd)
        {
        case HTML_NOBR:
            mode->nobr_level++;
            if (mode->pre_mode & TBLM_NOBR)
                return TAG_ACTION_NONE;
            mode->pre_mode |= TBLM_NOBR;
            break;
        case HTML_PRE_INT:
            if (mode->pre_mode & TBLM_PRE_INT)
                return TAG_ACTION_NONE;
            mode->pre_mode |= TBLM_PRE_INT;
            tbl->linfo.prev_spaces = 0;
            break;
        }
        mode->nobr_offset = -1;
        if (tbl->linfo.length > 0)
        {
            check_minimum0(tbl, tbl->linfo.length);
            tbl->linfo.length = 0;
        }
        break;
    case HTML_N_NOBR:
        if (!(tbl->flag & TBL_IN_ROW))
            break;
        feed_table_inline_tag(tbl, line, mode, -1);
        if (mode->nobr_level > 0)
            mode->nobr_level--;
        if (mode->nobr_level == 0)
            mode->pre_mode &= ~TBLM_NOBR;
        break;
    case HTML_N_PRE_INT:
        feed_table_inline_tag(tbl, line, mode, -1);
        mode->pre_mode &= ~TBLM_PRE_INT;
        break;
    case HTML_IMG:
        check_rowcol(tbl, mode);
        w = tbl->fixed_width[tbl->col];
        if (w < 0)
        {
            if (tbl->total_width > 0)
                w = -tbl->total_width * w / 100;
            else if (width > 0)
                w = -width * w / 100;
            else
                w = 0;
        }
        else if (w == 0)
        {
            if (tbl->total_width > 0)
                w = tbl->total_width;
            else if (width > 0)
                w = width;
        }
        tok = seq->process_img(tag, w);
        feed_table1(tbl, tok, mode, width, seq);
        break;
    case HTML_FORM:
        tbl->feed_table_block_tag("", mode, 0, cmd);
        tmp = seq->FormOpen(tag);
        if (tmp)
            feed_table1(tbl, tmp, mode, width, seq);
        break;
    case HTML_N_FORM:
        tbl->feed_table_block_tag("", mode, 0, cmd);
        seq->FormClose();
        break;
    case HTML_INPUT:
        tmp = seq->process_input(tag);
        feed_table1(tbl, tmp, mode, width, seq);
        break;
    case HTML_SELECT:
        tmp = seq->process_select(tag);
        if (tmp)
            feed_table1(tbl, tmp, mode, width, seq);
        mode->pre_mode |= TBLM_INSELECT;
        mode->end_tag = HTML_N_SELECT;
        break;
    case HTML_N_SELECT:
    case HTML_OPTION:
        /* nothing */
        break;
    case HTML_TEXTAREA:
        w = 0;
        check_rowcol(tbl, mode);
        if (tbl->col + 1 <= tbl->maxcol &&
            tbl->tabattr[tbl->row][tbl->col + 1] & HTT_X)
        {
            if (cell->icell >= 0 && cell->fixed_width[cell->icell] > 0)
                w = cell->fixed_width[cell->icell];
        }
        else
        {
            if (tbl->fixed_width[tbl->col] > 0)
                w = tbl->fixed_width[tbl->col];
        }
        tmp = seq->process_textarea(tag, w);
        if (tmp)
            feed_table1(tbl, tmp, mode, width, seq);
        mode->pre_mode |= TBLM_INTXTA;
        mode->end_tag = HTML_N_TEXTAREA;
        break;
    case HTML_A:
        table_close_anchor0(tbl, mode);
        anchor = NULL;
        i = 0;
        tag->TryGetAttributeValue(ATTR_HREF, &anchor);
        tag->TryGetAttributeValue(ATTR_HSEQ, &i);
        if (anchor)
        {
            check_rowcol(tbl, mode);
            if (i == 0)
            {
                Str tmp = seq->process_anchor(tag, line);
                if (w3mApp::Instance().displayLinkNumber)
                {
                    Str t = seq->GetLinkNumberStr(-1);
                    feed_table_inline_tag(tbl, NULL, mode, t->Size());
                    tmp->Push(t);
                }
                tbl->pushdata(tbl->row, tbl->col, tmp->ptr);
            }
            else
                tbl->pushdata(tbl->row, tbl->col, line);
            if (i >= 0)
            {
                mode->pre_mode |= TBLM_ANCHOR;
                mode->anchor_offset = tbl->tabcontentssize;
            }
        }
        else
            tbl->suspend_or_pushdata(line);
        break;
    case HTML_DEL:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode |= TBLM_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            feed_table_inline_tag(tbl, line, mode, 5); /* [DEL: */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            feed_table_inline_tag(tbl, line, mode, -1);
            break;
        }
        break;
    case HTML_N_DEL:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode &= ~TBLM_DEL;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            feed_table_inline_tag(tbl, line, mode, 5); /* :DEL] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            feed_table_inline_tag(tbl, line, mode, -1);
            break;
        }
        break;
    case HTML_S:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode |= TBLM_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            feed_table_inline_tag(tbl, line, mode, 3); /* [S: */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            feed_table_inline_tag(tbl, line, mode, -1);
            break;
        }
        break;
    case HTML_N_S:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            mode->pre_mode &= ~TBLM_S;
            break;
        case DISPLAY_INS_DEL_NORMAL:
            feed_table_inline_tag(tbl, line, mode, 3); /* :S] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            feed_table_inline_tag(tbl, line, mode, -1);
            break;
        }
        break;
    case HTML_INS:
    case HTML_N_INS:
        switch (w3mApp::Instance().displayInsDel)
        {
        case DISPLAY_INS_DEL_SIMPLE:
            break;
        case DISPLAY_INS_DEL_NORMAL:
            feed_table_inline_tag(tbl, line, mode, 5); /* [INS:, :INS] */
            break;
        case DISPLAY_INS_DEL_FONTIFY:
            feed_table_inline_tag(tbl, line, mode, -1);
            break;
        }
        break;
    case HTML_SUP:
    case HTML_SUB:
    case HTML_N_SUB:
        if (!(mode->pre_mode & (TBLM_DEL | TBLM_S)))
            feed_table_inline_tag(tbl, line, mode, 1); /* ^, [, ] */
        break;
    case HTML_N_SUP:
        break;
    case HTML_TABLE_ALT:
        id = -1;
        w = 0;
        tag->TryGetAttributeValue(ATTR_TID, &id);
        if (id >= 0 && id < tbl->ntable)
        {
            struct table *tbl1 = tbl->tables[id].ptr;
            tbl->feed_table_block_tag(line, mode, 0, cmd);
            addcontentssize(tbl, tbl1->maximum_table_width());
            check_minimum0(tbl, tbl1->sloppy_width);
#ifdef TABLE_EXPAND
            w = tbl1->total_width;
            v = 0;
            colspan = table_colspan(tbl, tbl->row, tbl->col);
            if (colspan > 1)
            {
                if (cell->icell >= 0)
                    v = cell->fixed_width[cell->icell];
            }
            else
                v = tbl->fixed_width[tbl->col];
            if (v < 0 && tbl->real_width > 0 && tbl1->real_width > 0)
                w = -(tbl1->real_width * 100) / tbl->real_width;
            else
                w = tbl1->real_width;
            if (w > 0)
                check_minimum0(tbl, w);
            else if (w < 0 && v < w)
            {
                if (colspan > 1)
                {
                    if (cell->icell >= 0)
                        cell->fixed_width[cell->icell] = w;
                }
                else
                    tbl->fixed_width[tbl->col] = w;
            }
#endif
            tbl->setwidth0(mode);
            clearcontentssize(tbl, mode);
        }
        break;
    case HTML_CAPTION:
        mode->caption = 1;
        break;
    case HTML_N_CAPTION:
    case HTML_THEAD:
    case HTML_N_THEAD:
    case HTML_TBODY:
    case HTML_N_TBODY:
    case HTML_TFOOT:
    case HTML_N_TFOOT:
    case HTML_COLGROUP:
    case HTML_N_COLGROUP:
    case HTML_COL:
        break;
    case HTML_SCRIPT:
        mode->pre_mode |= TBLM_SCRIPT;
        mode->end_tag = HTML_N_SCRIPT;
        break;
    case HTML_STYLE:
        mode->pre_mode |= TBLM_STYLE;
        mode->end_tag = HTML_N_STYLE;
        break;
    case HTML_N_A:
        table_close_anchor0(tbl, mode);
    case HTML_FONT:
    case HTML_N_FONT:
    case HTML_NOP:
        tbl->suspend_or_pushdata(line);
        break;
    case HTML_INTERNAL:
    case HTML_N_INTERNAL:
    case HTML_FORM_INT:
    case HTML_N_FORM_INT:
    case HTML_INPUT_ALT:
    case HTML_N_INPUT_ALT:
    case HTML_SELECT_INT:
    case HTML_N_SELECT_INT:
    case HTML_OPTION_INT:
    case HTML_TEXTAREA_INT:
    case HTML_N_TEXTAREA_INT:
    case HTML_IMG_ALT:
    case HTML_SYMBOL:
    case HTML_N_SYMBOL:
    default:
        /* unknown tag: put into table */
        return TAG_ACTION_FEED;
    }
    return TAG_ACTION_NONE;
}

int feed_table(struct table *tbl, const char *line, struct table_mode *mode,
               int width, int internal, HtmlContext *seq)
{
    int i;
    Str tmp;
    struct table_linfo *linfo = &tbl->linfo;

    if (*line == '<' && line[1] && REALLY_THE_BEGINNING_OF_A_TAG(line))
    {
        struct parsed_tag *tag;
        const char *p = line;
        tag = parse_tag(&p, internal);
        if (tag)
        {
            switch (feed_table_tag(tbl, line, mode, width, tag, seq))
            {
            case TAG_ACTION_NONE:
                return -1;
            case TAG_ACTION_N_TABLE:
                return 0;
            case TAG_ACTION_TABLE:
                return 1;
            case TAG_ACTION_PLAIN:
                break;
            case TAG_ACTION_FEED:
            default:
                if (tag->need_reconstruct)
                    line = tag->ToStr()->ptr;
            }
        }
        else
        {
            if (!(mode->pre_mode & (TBLM_PLAIN | TBLM_INTXTA | TBLM_INSELECT |
                                    TBLM_SCRIPT | TBLM_STYLE)))
                return -1;
        }
    }
    else
    {
        if (mode->pre_mode & (TBLM_DEL | TBLM_S))
            return -1;
    }
    if (mode->caption)
    {
        tbl->caption->Push(line);
        return -1;
    }
    if (mode->pre_mode & TBLM_SCRIPT)
        return -1;
    if (mode->pre_mode & TBLM_STYLE)
        return -1;
    if (mode->pre_mode & TBLM_INTXTA)
    {
        seq->feed_textarea(line);
        return -1;
    }
    if (mode->pre_mode & TBLM_INSELECT)
    {
        seq->feed_select(line);
        return -1;
    }
    if (!(mode->pre_mode & TBLM_PLAIN) &&
        !(*line == '<' && line[strlen(line) - 1] == '>') &&
        strchr(line, '&') != NULL)
    {
        tmp = Strnew();
        for (auto p = line; *p;)
        {
            const char *q, *r;
            if (*p == '&')
            {
                if (!strncasecmp(p, "&amp;", 5) ||
                    !strncasecmp(p, "&gt;", 4) || !strncasecmp(p, "&lt;", 4))
                {
                    /* do not convert */
                    tmp->Push(*p);
                    p++;
                }
                else
                {
                    q = p;
                    auto [pos, ec] = ucs4_from_entity(p);
                    p = pos.data();
                    switch (ec)
                    {
                    case '<':
                        tmp->Push("&lt;");
                        break;
                    case '>':
                        tmp->Push("&gt;");
                        break;
                    case '&':
                        tmp->Push("&amp;");
                        break;
                    case '\r':
                        tmp->Push('\n');
                        break;
                    default:
                        r = (char *)from_unicode(ec, w3mApp::Instance().InnerCharset);
                        if (r != NULL && strlen(r) == 1 &&
                            ec == (unsigned char)*r)
                        {
                            tmp->Push(*r);
                            break;
                        }
                    case -1:
                        tmp->Push(*q);
                        p = q + 1;
                        break;
                    }
                }
            }
            else
            {
                tmp->Push(*p);
                p++;
            }
        }
        line = tmp->ptr;
    }
    if (!(mode->pre_mode & (TBLM_SPECIAL & ~TBLM_NOBR)))
    {
        if (!(tbl->flag & TBL_IN_COL) || linfo->prev_spaces != 0)
            while (IS_SPACE(*line))
                line++;
        if (*line == '\0')
            return -1;
        check_rowcol(tbl, mode);
        if (mode->pre_mode & TBLM_NOBR && mode->nobr_offset < 0)
            mode->nobr_offset = tbl->tabcontentssize;

        /* count of number of spaces skipped in normal mode */
        i = skip_space(tbl, line, linfo, !(mode->pre_mode & TBLM_NOBR));
        addcontentssize(tbl, visible_length(line) - i);
        tbl->setwidth(mode);
        tbl->pushdata(tbl->row, tbl->col, line);
    }
    else if (mode->pre_mode & TBLM_PRE_INT)
    {
        check_rowcol(tbl, mode);
        if (mode->nobr_offset < 0)
            mode->nobr_offset = tbl->tabcontentssize;
        addcontentssize(tbl, maximum_visible_length(line, tbl->tabcontentssize));
        tbl->setwidth(mode);
        tbl->pushdata(tbl->row, tbl->col, line);
    }
    else
    {
        /* <pre> mode or something like it */
        check_rowcol(tbl, mode);
        while (*line)
        {
            int nl = false;
            const char *p;
            if ((p = strchr(const_cast<char *>(line), '\r')) || (p = strchr(const_cast<char *>(line), '\n')))
            {
                if (*p == '\r' && p[1] == '\n')
                    p++;
                if (p[1])
                {
                    p++;
                    tmp = Strnew_charp_n(line, p - line);
                    line = p;
                    p = tmp->ptr;
                }
                else
                {
                    p = line;
                    line = "";
                }
                nl = true;
            }
            else
            {
                p = line;
                line = "";
            }
            if (mode->pre_mode & TBLM_PLAIN)
                i = maximum_visible_length_plain(p, tbl->tabcontentssize);
            else
                i = maximum_visible_length(p, tbl->tabcontentssize);
            addcontentssize(tbl, i);
            tbl->setwidth(mode);
            if (nl)
                clearcontentssize(tbl, mode);
            tbl->pushdata(tbl->row, tbl->col, p);
        }
    }
    return -1;
}

void feed_table1(struct table *tbl, Str tok, struct table_mode *mode, int width, HtmlContext *seq)
{
    Str tokbuf;
    TokenStatusTypes status;
    char *line;
    if (!tok)
        return;
    tokbuf = Strnew();
    status = R_ST_NORMAL;
    line = tok->ptr;
    while (read_token(tokbuf, &line, &status, mode->pre_mode & TBLM_PREMODE, 0))
        feed_table(tbl, tokbuf->ptr, mode, width, true, seq);
}

void table::pushTable(table *tbl1)
{
    if (this->ntable >= this->tables_size)
    {
        struct table_in *tmp;
        this->tables_size += MAX_TABLE_N;
        tmp = New_N(struct table_in, this->tables_size);
        if (this->tables)
            bcopy(this->tables, tmp, this->ntable * sizeof(struct table_in));
        this->tables = tmp;
    }

    this->tables[this->ntable].ptr = tbl1;
    this->tables[this->ntable].col = col;
    this->tables[this->ntable].row = row;
    this->tables[this->ntable].indent = this->indent;
    this->tables[this->ntable].buf = newTextLineList();
    this->check_row(row);
    if (col + 1 <= this->maxcol && this->tabattr[row][col + 1] & HTT_X)
        this->tables[this->ntable].cell = this->cell.icell;
    else
        this->tables[this->ntable].cell = -1;
    this->ntable++;
}

#ifdef MATRIX
int correct_table_matrix(struct table *t, int col, int cspan, int a, double b)
{
    int i, j;
    int ecol = col + cspan;
    double w = 1. / (b * b);

    for (i = col; i < ecol; i++)
    {
        v_add_val(t->vector, i, w * a);
        for (j = i; j < ecol; j++)
        {
            m_add_val(t->matrix, i, j, w);
            m_set_val(t->matrix, j, i, m_entry(t->matrix, i, j));
        }
    }
    return i;
}

static void
correct_table_matrix2(struct table *t, int col, int cspan, double s, double b)
{
    int i, j;
    int ecol = col + cspan;
    int size = t->maxcol + 1;
    double w = 1. / (b * b);
    double ss;

    for (i = 0; i < size; i++)
    {
        for (j = i; j < size; j++)
        {
            if (i >= col && i < ecol && j >= col && j < ecol)
                ss = (1. - s) * (1. - s);
            else if ((i >= col && i < ecol) || (j >= col && j < ecol))
                ss = -(1. - s) * s;
            else
                ss = s * s;
            m_add_val(t->matrix, i, j, w * ss);
        }
    }
}

static void
correct_table_matrix3(struct table *t, int col, char *flags, double s,
                      double b)
{
    int i, j;
    double ss;
    int size = t->maxcol + 1;
    double w = 1. / (b * b);
    int flg = (flags[col] == 0);

    for (i = 0; i < size; i++)
    {
        if (!((flg && flags[i] == 0) || (!flg && flags[i] != 0)))
            continue;
        for (j = i; j < size; j++)
        {
            if (!((flg && flags[j] == 0) || (!flg && flags[j] != 0)))
                continue;
            if (i == col && j == col)
                ss = (1. - s) * (1. - s);
            else if (i == col || j == col)
                ss = -(1. - s) * s;
            else
                ss = s * s;
            m_add_val(t->matrix, i, j, w * ss);
        }
    }
}

static void
correct_table_matrix4(struct table *t, int col, int cspan, char *flags,
                      double s, double b)
{
    int i, j;
    double ss;
    int ecol = col + cspan;
    int size = t->maxcol + 1;
    double w = 1. / (b * b);

    for (i = 0; i < size; i++)
    {
        if (flags[i] && !(i >= col && i < ecol))
            continue;
        for (j = i; j < size; j++)
        {
            if (flags[j] && !(j >= col && j < ecol))
                continue;
            if (i >= col && i < ecol && j >= col && j < ecol)
                ss = (1. - s) * (1. - s);
            else if ((i >= col && i < ecol) || (j >= col && j < ecol))
                ss = -(1. - s) * s;
            else
                ss = s * s;
            m_add_val(t->matrix, i, j, w * ss);
        }
    }
}

static void
set_table_matrix0(struct table *t, int maxwidth)
{
    int size = t->maxcol + 1;
    int i, j, k, bcol, ecol;
    int width;
    double w0, w1, w, s, b;
#ifdef __GNUC__
    double we[size];
    char expand[size];
#else  /* not __GNUC__ */
    double we[MAXCOL];
    char expand[MAXCOL];
#endif /* not __GNUC__ */
    struct table_cell *cell = &t->cell;

    w0 = 0.;
    for (i = 0; i < size; i++)
    {
        we[i] = weight(t->tabwidth[i]);
        w0 += we[i];
    }
    if (w0 <= 0.)
        w0 = 1.;

    if (cell->necell == 0)
    {
        for (i = 0; i < size; i++)
        {
            s = we[i] / w0;
            b = sigma_td_nw((int)(s * maxwidth));
            correct_table_matrix2(t, i, 1, s, b);
        }
        return;
    }

    bzero(expand, size);

    for (k = 0; k < cell->necell; k++)
    {
        j = cell->eindex[k];
        bcol = cell->col[j];
        ecol = bcol + cell->colspan[j];
        width = cell->width[j] - (cell->colspan[j] - 1) * t->cellspacing;
        w1 = 0.;
        for (i = bcol; i < ecol; i++)
        {
            w1 += t->tabwidth[i] + 0.1;
            expand[i]++;
        }
        for (i = bcol; i < ecol; i++)
        {
            w = weight(width * (t->tabwidth[i] + 0.1) / w1);
            if (w > we[i])
                we[i] = w;
        }
    }

    w0 = 0.;
    w1 = 0.;
    for (i = 0; i < size; i++)
    {
        w0 += we[i];
        if (expand[i] == 0)
            w1 += we[i];
    }
    if (w0 <= 0.)
        w0 = 1.;

    for (k = 0; k < cell->necell; k++)
    {
        j = cell->eindex[k];
        bcol = cell->col[j];
        width = cell->width[j] - (cell->colspan[j] - 1) * t->cellspacing;
        w = weight(width);
        s = w / (w1 + w);
        b = sigma_td_nw((int)(s * maxwidth));
        correct_table_matrix4(t, bcol, cell->colspan[j], expand, s, b);
    }

    for (i = 0; i < size; i++)
    {
        if (expand[i] == 0)
        {
            s = we[i] / max(w1, 1.);
            b = sigma_td_nw((int)(s * maxwidth));
        }
        else
        {
            s = we[i] / max(w0 - w1, 1.);
            b = sigma_td_nw(maxwidth);
        }
        correct_table_matrix3(t, i, expand, s, b);
    }
}

void check_relative_width(struct table *t, int maxwidth)
{
    int i;
    double rel_total = 0;
    int size = t->maxcol + 1;
    double *rcolwidth = New_N(double, size);
    struct table_cell *cell = &t->cell;
    int n_leftcol = 0;

    for (i = 0; i < size; i++)
        rcolwidth[i] = 0;

    for (i = 0; i < size; i++)
    {
        if (t->fixed_width[i] < 0)
            rcolwidth[i] = -(double)t->fixed_width[i] / 100.0;
        else if (t->fixed_width[i] > 0)
            rcolwidth[i] = (double)t->fixed_width[i] / maxwidth;
        else
            n_leftcol++;
    }
    for (i = 0; i <= cell->maxcell; i++)
    {
        if (cell->fixed_width[i] < 0)
        {
            double w = -(double)cell->fixed_width[i] / 100.0;
            double r;
            int j, k;
            int n_leftcell = 0;
            k = cell->col[i];
            r = 0.0;
            for (j = 0; j < cell->colspan[i]; j++)
            {
                if (rcolwidth[j + k] > 0)
                    r += rcolwidth[j + k];
                else
                    n_leftcell++;
            }
            if (n_leftcell == 0)
            {
                /* w must be identical to r */
                if (w != r)
                    cell->fixed_width[i] = -100 * r;
            }
            else
            {
                if (w <= r)
                {
                    /* make room for the left(width-unspecified) cell */
                    /* the next formula is an estimation of required width */
                    w = r * cell->colspan[i] / (cell->colspan[i] - n_leftcell);
                    cell->fixed_width[i] = -100 * w;
                }
                for (j = 0; j < cell->colspan[i]; j++)
                {
                    if (rcolwidth[j + k] == 0)
                        rcolwidth[j + k] = (w - r) / n_leftcell;
                }
            }
        }
        else if (cell->fixed_width[i] > 0)
        {
            /* todo */
        }
    }
    /* sanity check */
    for (i = 0; i < size; i++)
        rel_total += rcolwidth[i];

    if ((n_leftcol == 0 && rel_total < 0.9) || 1.1 < rel_total)
    {
        for (i = 0; i < size; i++)
        {
            rcolwidth[i] /= rel_total;
        }
        for (i = 0; i < size; i++)
        {
            if (t->fixed_width[i] < 0)
                t->fixed_width[i] = -rcolwidth[i] * 100;
        }
        for (i = 0; i <= cell->maxcell; i++)
        {
            if (cell->fixed_width[i] < 0)
            {
                double r;
                int j, k;
                k = cell->col[i];
                r = 0.0;
                for (j = 0; j < cell->colspan[i]; j++)
                    r += rcolwidth[j + k];
                cell->fixed_width[i] = -r * 100;
            }
        }
    }
}

void set_table_matrix(struct table *t, int width)
{
    int size = t->maxcol + 1;
    int i, j;
    double b, s;
    int a;
    struct table_cell *cell = &t->cell;

    if (size < 1)
        return;

    t->matrix = m_get(size, size);
    t->vector = v_get(size);
    for (i = 0; i < size; i++)
    {
        for (j = i; j < size; j++)
            m_set_val(t->matrix, i, j, 0.);
        v_set_val(t->vector, i, 0.);
    }

    check_relative_width(t, width);

    for (i = 0; i < size; i++)
    {
        if (t->fixed_width[i] > 0)
        {
            a = max(t->fixed_width[i], t->minimum_width[i]);
            b = sigma_td(a);
            correct_table_matrix(t, i, 1, a, b);
        }
        else if (t->fixed_width[i] < 0)
        {
            s = -(double)t->fixed_width[i] / 100.;
            b = sigma_td((int)(s * width));
            correct_table_matrix2(t, i, 1, s, b);
        }
    }

    for (j = 0; j <= cell->maxcell; j++)
    {
        if (cell->fixed_width[j] > 0)
        {
            a = max(cell->fixed_width[j], cell->minimum_width[j]);
            b = sigma_td(a);
            correct_table_matrix(t, cell->col[j], cell->colspan[j], a, b);
        }
        else if (cell->fixed_width[j] < 0)
        {
            s = -(double)cell->fixed_width[j] / 100.;
            b = sigma_td((int)(s * width));
            correct_table_matrix2(t, cell->col[j], cell->colspan[j], s, b);
        }
    }

    set_table_matrix0(t, width);

    if (t->total_width > 0)
    {
        b = sigma_table(width);
    }
    else
    {
        b = sigma_table_nw(width);
    }
    correct_table_matrix(t, 0, size, width, b);
}
#endif /* MATRIX */

/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */
