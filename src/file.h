#pragma once
#include "parsetagx.h"
#include "form.h"

typedef struct
{
    int pos;
    int len;
    int tlen;
    long flag;
    Anchor anchor;
    Str img_alt;
    char fontstat[FONTSTAT_SIZE];
    short nobr_level;
    Lineprop prev_ctype;
    char init_flag;
    short top_margin;
    short bottom_margin;
} Breakpoint;

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

Str process_img(struct parsed_tag *tag, int width);
Str process_anchor(struct parsed_tag *tag, char *tagbuf);
Str process_input(struct parsed_tag *tag);
Str process_select(struct parsed_tag *tag);
Str process_textarea(struct parsed_tag *tag, int width);
Str process_form(struct parsed_tag *tag);
int HTMLtagproc1(struct parsed_tag *tag, struct html_feed_environ *h_env);
Buffer *loadGeneralFile(char *path, ParsedURL *current, char *referer, int flag, FormList *request);
Str getLinkNumberStr(int correction);
char *guess_save_name(Buffer *buf, char *file);
