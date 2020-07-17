#pragma once

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
