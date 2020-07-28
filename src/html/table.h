/* $Id: table.h,v 1.12 2003/09/22 21:02:21 ukai Exp $ */
#if (defined(MESCHACH) && !defined(MATRIX))
#define MATRIX
#endif				/* (defined(MESCHACH) && !defined(MATRIX)) */

#ifdef MATRIX
#ifdef MESCHACH
#include <matrix2.h>
#else				/* not MESCHACH */
#include "matrix.h"
#endif				/* not MESCHACH */
#endif				/* MATRIX */

#include "Str.h"
#include "line.h"

#define MAX_TABLE 20		/* maximum nest level of table */
#define MAX_TABLE_N 20		/* maximum number of table in same level */

#define MAXROW 50
#define MAXCOL 50

#define MAX_WIDTH 80

#define BORDER_NONE 0
#define BORDER_THIN 1
#define BORDER_THICK 2
#define BORDER_NOWIN 3

typedef unsigned short table_attr;

/* flag */
#define TBL_IN_ROW     1
#define TBL_EXPAND_OK  2
#define TBL_IN_COL     4

#define MAXCELL 20
#define MAXROWCELL 1000
struct table_cell {
    short col[MAXCELL];
    short colspan[MAXCELL];
    short index[MAXCELL];
    short maxcell;
    short icell;
#ifdef MATRIX
    short eindex[MAXCELL];
    short necell;
#endif				/* MATRIX */
    short width[MAXCELL];
    short minimum_width[MAXCELL];
    short fixed_width[MAXCELL];
};

struct table_in {
    struct table *ptr;
    short col;
    short row;
    short cell;
    short indent;
    TextLineList *buf;
};

struct table_linfo {
    Lineprop prev_ctype;
    signed char prev_spaces;
    Str prevchar;
    short length;
};

struct table {
    int row;
    int col;
    int maxrow;
    int maxcol;
    int max_rowsize;
    int border_mode;
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
#endif				/* TABLE_EXPAND */
    Str caption;
#ifdef ID_EXT
    Str id;
#endif
    GeneralList ***tabdata;
    table_attr **tabattr;
    table_attr trattr;
#ifdef ID_EXT
    Str **tabidvalue;
    Str *tridvalue;
#endif
    short tabwidth[MAXCOL];
    short minimum_width[MAXCOL];
    short fixed_width[MAXCOL];
    struct table_cell cell;
    short *tabheight;
    struct table_in *tables;
    short ntable;
    short tables_size;
    TextList *suspended_data;
    /* use for counting skipped spaces */
    struct table_linfo linfo;
#ifdef MATRIX
    MAT *matrix;
    VEC *vector;
#endif				/* MATRIX */
    int sloppy_width;
};

#define RB_PRE		0x01
#define RB_SCRIPT	0x02
#define RB_STYLE	0x04
#define RB_PLAIN	0x08
#define RB_LEFT		0x10
#define RB_CENTER	0x20
#define RB_RIGHT	0x40
#define RB_ALIGN	(RB_LEFT| RB_CENTER | RB_RIGHT)
#define RB_NOBR		0x80
#define RB_P		0x100
#define RB_PRE_INT	0x200
#define RB_IN_DT	0x400
#define RB_INTXTA	0x800
#define RB_INSELECT	0x1000
#define RB_IGNORE_P	0x2000
#define RB_TITLE	0x4000
#define RB_NFLUSHED	0x8000
#define RB_NOFRAMES	0x10000
#define RB_INTABLE	0x20000
#define RB_PREMODE	(RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_INTXTA)
#define RB_SPECIAL	(RB_PRE | RB_PRE_INT | RB_SCRIPT | RB_STYLE | RB_PLAIN | RB_NOBR)
#define RB_PLAIN_PRE	0x40000

#define TBLM_PRE	RB_PRE
#define TBLM_SCRIPT	RB_SCRIPT
#define TBLM_STYLE	RB_STYLE
#define TBLM_PLAIN	RB_PLAIN
#define TBLM_NOBR	RB_NOBR
#define TBLM_PRE_INT	RB_PRE_INT
#define TBLM_INTXTA	RB_INTXTA
#define TBLM_INSELECT	RB_INSELECT
#define TBLM_PREMODE	(TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_INTXTA)
#define TBLM_SPECIAL	(TBLM_PRE | TBLM_PRE_INT | TBLM_SCRIPT | TBLM_STYLE | TBLM_PLAIN | TBLM_NOBR)
#define TBLM_DEL	RB_DEL
#define TBLM_S		RB_S
#define TBLM_ANCHOR	0x1000000

struct table_mode {
    unsigned int pre_mode;
    char indent_level;
    char caption;
    short nobr_offset;
    char nobr_level;
    short anchor_offset;
    unsigned char end_tag;
};

/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */

void align(TextLine *lbuf, int width, int mode);
