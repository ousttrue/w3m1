#pragma once
#include "html.h"
#include <wc.h>

/* Parsed Tag structure */

#define HTML_UNKNOWN 0
#define HTML_A 1
#define HTML_N_A 2
#define HTML_H 3
#define HTML_N_H 4
#define HTML_P 5
#define HTML_BR 6
#define HTML_B 7
#define HTML_N_B 8
#define HTML_UL 9
#define HTML_N_UL 10
#define HTML_LI 11
#define HTML_OL 12
#define HTML_N_OL 13
#define HTML_TITLE 14
#define HTML_N_TITLE 15
#define HTML_HR 16
#define HTML_DL 17
#define HTML_N_DL 18
#define HTML_DT 19
#define HTML_DD 20
#define HTML_PRE 21
#define HTML_N_PRE 22
#define HTML_BLQ 23
#define HTML_N_BLQ 24
#define HTML_IMG 25
#define HTML_LISTING 26
#define HTML_N_LISTING 27
#define HTML_XMP 28
#define HTML_N_XMP 29
#define HTML_PLAINTEXT 30
#define HTML_TABLE 31
#define HTML_N_TABLE 32
#define HTML_META 33
#define HTML_N_P 34
#define HTML_FRAME 35
#define HTML_FRAMESET 36
#define HTML_N_FRAMESET 37
#define HTML_CENTER 38
#define HTML_N_CENTER 39
#define HTML_FONT 40
#define HTML_N_FONT 41
#define HTML_FORM 42
#define HTML_N_FORM 43
#define HTML_INPUT 44
#define HTML_TEXTAREA 45
#define HTML_N_TEXTAREA 46
#define HTML_SELECT 47
#define HTML_N_SELECT 48
#define HTML_OPTION 49
#define HTML_NOBR 50
#define HTML_N_NOBR 51
#define HTML_DIV 52
#define HTML_N_DIV 53
#define HTML_ISINDEX 54
#define HTML_MAP 55
#define HTML_N_MAP 56
#define HTML_AREA 57
#define HTML_SCRIPT 58
#define HTML_N_SCRIPT 59
#define HTML_BASE 60
#define HTML_DEL 61
#define HTML_N_DEL 62
#define HTML_INS 63
#define HTML_N_INS 64
#define HTML_U 65
#define HTML_N_U 66
#define HTML_STYLE 67
#define HTML_N_STYLE 68
#define HTML_WBR 69
#define HTML_EM 70
#define HTML_N_EM 71
#define HTML_BODY 72
#define HTML_N_BODY 73
#define HTML_TR 74
#define HTML_N_TR 75
#define HTML_TD 76
#define HTML_N_TD 77
#define HTML_CAPTION 78
#define HTML_N_CAPTION 79
#define HTML_TH 80
#define HTML_N_TH 81
#define HTML_THEAD 82
#define HTML_N_THEAD 83
#define HTML_TBODY 84
#define HTML_N_TBODY 85
#define HTML_TFOOT 86
#define HTML_N_TFOOT 87
#define HTML_COLGROUP 88
#define HTML_N_COLGROUP 89
#define HTML_COL 90
#define HTML_BGSOUND 91
#define HTML_APPLET 92
#define HTML_EMBED 93
#define HTML_N_OPTION 94
#define HTML_HEAD 95
#define HTML_N_HEAD 96
#define HTML_DOCTYPE 97
#define HTML_NOFRAMES 98
#define HTML_N_NOFRAMES 99
#define HTML_SUP 100
#define HTML_N_SUP 101
#define HTML_SUB 102
#define HTML_N_SUB 103
#define HTML_LINK 104
#define HTML_S 105
#define HTML_N_S 106
#define HTML_Q 107
#define HTML_N_Q 108
#define HTML_I 109
#define HTML_N_I 110
#define HTML_STRONG 111
#define HTML_N_STRONG 112
#define HTML_SPAN 113
#define HTML_N_SPAN 114
#define HTML_ABBR 115
#define HTML_N_ABBR 116
#define HTML_ACRONYM 117
#define HTML_N_ACRONYM 118
#define HTML_BASEFONT 119
#define HTML_BDO 120
#define HTML_N_BDO 121
#define HTML_BIG 122
#define HTML_N_BIG 123
#define HTML_BUTTON 124
#define HTML_FIELDSET 125
#define HTML_N_FIELDSET 126
#define HTML_IFRAME 127
#define HTML_LABEL 128
#define HTML_N_LABEL 129
#define HTML_LEGEND 130
#define HTML_N_LEGEND 131
#define HTML_NOSCRIPT 132
#define HTML_N_NOSCRIPT 133
#define HTML_OBJECT 134
#define HTML_OPTGROUP 135
#define HTML_N_OPTGROUP 136
#define HTML_PARAM 137
#define HTML_SMALL 138
#define HTML_N_SMALL 139

/* pseudo tag */
#define HTML_SELECT_INT 160
#define HTML_N_SELECT_INT 161
#define HTML_OPTION_INT 162
#define HTML_TEXTAREA_INT 163
#define HTML_N_TEXTAREA_INT 164
#define HTML_TABLE_ALT 165
#define HTML_SYMBOL 166
#define HTML_N_SYMBOL 167
#define HTML_PRE_INT 168
#define HTML_N_PRE_INT 169
#define HTML_TITLE_ALT 170
#define HTML_FORM_INT 171
#define HTML_N_FORM_INT 172
#define HTML_DL_COMPACT 173
#define HTML_INPUT_ALT 174
#define HTML_N_INPUT_ALT 175
#define HTML_IMG_ALT 176
#define HTML_N_IMG_ALT 177
#define HTML_NOP 178
#define HTML_PRE_PLAIN 179
#define HTML_N_PRE_PLAIN 180
#define HTML_INTERNAL 181
#define HTML_N_INTERNAL 182
#define HTML_DIV_INT 183
#define HTML_N_DIV_INT 184

#define MAX_HTMLTAG 185

/* Tag attribute */

#define ATTR_UNKNOWN 0
#define ATTR_ACCEPT 1
#define ATTR_ACCEPT_CHARSET 2
#define ATTR_ACTION 3
#define ATTR_ALIGN 4
#define ATTR_ALT 5
#define ATTR_ARCHIVE 6
#define ATTR_BACKGROUND 7
#define ATTR_BORDER 8
#define ATTR_CELLPADDING 9
#define ATTR_CELLSPACING 10
#define ATTR_CHARSET 11
#define ATTR_CHECKED 12
#define ATTR_COLS 13
#define ATTR_COLSPAN 14
#define ATTR_CONTENT 15
#define ATTR_ENCTYPE 16
#define ATTR_HEIGHT 17
#define ATTR_HREF 18
#define ATTR_HTTP_EQUIV 19
#define ATTR_ID 20
#define ATTR_LINK 21
#define ATTR_MAXLENGTH 22
#define ATTR_METHOD 23
#define ATTR_MULTIPLE 24
#define ATTR_NAME 25
#define ATTR_NOWRAP 26
#define ATTR_PROMPT 27
#define ATTR_ROWS 28
#define ATTR_ROWSPAN 29
#define ATTR_SIZE 30
#define ATTR_SRC 31
#define ATTR_TARGET 32
#define ATTR_TYPE 33
#define ATTR_USEMAP 34
#define ATTR_VALIGN 35
#define ATTR_VALUE 36
#define ATTR_VSPACE 37
#define ATTR_WIDTH 38
#define ATTR_COMPACT 39
#define ATTR_START 40
#define ATTR_SELECTED 41
#define ATTR_LABEL 42
#define ATTR_READONLY 43
#define ATTR_SHAPE 44
#define ATTR_COORDS 45
#define ATTR_ISMAP 46
#define ATTR_REL 47
#define ATTR_REV 48
#define ATTR_TITLE 49
#define ATTR_ACCESSKEY 50

/* Internal attribute */
#define ATTR_XOFFSET 60
#define ATTR_YOFFSET 61
#define ATTR_TOP_MARGIN 62
#define ATTR_BOTTOM_MARGIN 63
#define ATTR_TID 64
#define ATTR_FID 65
#define ATTR_FOR_TABLE 66
#define ATTR_FRAMENAME 67
#define ATTR_HBORDER 68
#define ATTR_HSEQ 69
#define ATTR_NO_EFFECT 70
#define ATTR_REFERER 71
#define ATTR_SELECTNUMBER 72
#define ATTR_TEXTAREANUMBER 73
#define ATTR_PRE_INT 74

#define MAX_TAGATTR 75

/* HTML Tag Information Table */
struct TagInfo
{
    char *name;
    unsigned char *accept_attribute;
    unsigned char max_attribute;
    unsigned char flag;
};
extern TagInfo TagMAP[];

/* HTML Tag Attribute Information Table */
struct TagAttrInfo
{
    char *name;
    unsigned char vtype;
    unsigned char flag;
};
extern TagAttrInfo AttrMAP[];

#define AFLG_INT 1

#define VTYPE_NONE 0
#define VTYPE_STR 1
#define VTYPE_NUMBER 2
#define VTYPE_LENGTH 3
#define VTYPE_ALIGN 4
#define VTYPE_VALIGN 5
#define VTYPE_ACTION 6
#define VTYPE_ENCTYPE 7
#define VTYPE_METHOD 8
#define VTYPE_MLENGTH 9
#define VTYPE_TYPE 10

struct parsed_tag
{
    unsigned char tagid;
    unsigned char *attrid;
    char **value;
    unsigned char *map;
    char need_reconstruct;
};

#define parsedtag_accepts(tag, id) ((tag)->map && (tag)->map[id] != MAX_TAGATTR)
#define parsedtag_exists(tag, id) (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] != ATTR_UNKNOWN))
#define parsedtag_delete(tag, id) (parsedtag_accepts(tag, id) && ((tag)->attrid[(tag)->map[id]] = ATTR_UNKNOWN))
#define parsedtag_need_reconstruct(tag) ((tag)->need_reconstruct)
#define parsedtag_attname(tag, i) (AttrMAP[(tag)->attrid[i]].name)

extern struct parsed_tag *parse_tag(char **s, int internal);
extern int parsedtag_get_value(struct parsed_tag *tag, int id, void *value);
inline std::string_view parsedtag_get_value(const struct parsed_tag &tag, int id)
{
    char *value;
    parsedtag_get_value(const_cast<struct parsed_tag *>(&tag), id, &value);
    if (!value)
    {
        return "";
    }
    return value;
}

extern int parsedtag_set_value(struct parsed_tag *tag, int id, const char *value);
extern Str parsedtag2str(struct parsed_tag *tag);
