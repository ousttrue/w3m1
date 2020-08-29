#pragma once
#include <wc.h>
#include <gc_cpp.h>

//
// N is close tag
//
enum HtmlTags
{
    HTML_UNKNOWN = 0,
    HTML_A = 1,
    HTML_N_A = 2,
    HTML_H = 3,
    HTML_N_H = 4,
    HTML_P = 5,
    HTML_BR = 6,
    HTML_B = 7,
    HTML_N_B = 8,
    HTML_UL = 9,
    HTML_N_UL = 10,
    HTML_LI = 11,
    HTML_OL = 12,
    HTML_N_OL = 13,
    HTML_TITLE = 14,
    HTML_N_TITLE = 15,
    HTML_HR = 16,
    HTML_DL = 17,
    HTML_N_DL = 18,
    HTML_DT = 19,
    HTML_DD = 20,
    HTML_PRE = 21,
    HTML_N_PRE = 22,
    HTML_BLQ = 23,
    HTML_N_BLQ = 24,
    HTML_IMG = 25,
    HTML_LISTING = 26,
    HTML_N_LISTING = 27,
    HTML_XMP = 28,
    HTML_N_XMP = 29,
    HTML_PLAINTEXT = 30,
    HTML_TABLE = 31,
    HTML_N_TABLE = 32,
    HTML_META = 33,
    HTML_N_P = 34,
    HTML_FRAME = 35,
    HTML_FRAMESET = 36,
    HTML_N_FRAMESET = 37,
    HTML_CENTER = 38,
    HTML_N_CENTER = 39,
    HTML_FONT = 40,
    HTML_N_FONT = 41,
    HTML_FORM = 42,
    HTML_N_FORM = 43,
    HTML_INPUT = 44,
    HTML_TEXTAREA = 45,
    HTML_N_TEXTAREA = 46,
    HTML_SELECT = 47,
    HTML_N_SELECT = 48,
    HTML_OPTION = 49,
    HTML_NOBR = 50,
    HTML_N_NOBR = 51,
    HTML_DIV = 52,
    HTML_N_DIV = 53,
    HTML_ISINDEX = 54,
    HTML_MAP = 55,
    HTML_N_MAP = 56,
    HTML_AREA = 57,
    HTML_SCRIPT = 58,
    HTML_N_SCRIPT = 59,
    HTML_BASE = 60,
    HTML_DEL = 61,
    HTML_N_DEL = 62,
    HTML_INS = 63,
    HTML_N_INS = 64,
    HTML_U = 65,
    HTML_N_U = 66,
    HTML_STYLE = 67,
    HTML_N_STYLE = 68,
    HTML_WBR = 69,
    HTML_EM = 70,
    HTML_N_EM = 71,
    HTML_BODY = 72,
    HTML_N_BODY = 73,
    HTML_TR = 74,
    HTML_N_TR = 75,
    HTML_TD = 76,
    HTML_N_TD = 77,
    HTML_CAPTION = 78,
    HTML_N_CAPTION = 79,
    HTML_TH = 80,
    HTML_N_TH = 81,
    HTML_THEAD = 82,
    HTML_N_THEAD = 83,
    HTML_TBODY = 84,
    HTML_N_TBODY = 85,
    HTML_TFOOT = 86,
    HTML_N_TFOOT = 87,
    HTML_COLGROUP = 88,
    HTML_N_COLGROUP = 89,
    HTML_COL = 90,
    HTML_BGSOUND = 91,
    HTML_APPLET = 92,
    HTML_EMBED = 93,
    HTML_N_OPTION = 94,
    HTML_HEAD = 95,
    HTML_N_HEAD = 96,
    HTML_DOCTYPE = 97,
    HTML_NOFRAMES = 98,
    HTML_N_NOFRAMES = 99,
    HTML_SUP = 100,
    HTML_N_SUP = 101,
    HTML_SUB = 102,
    HTML_N_SUB = 103,
    HTML_LINK = 104,
    HTML_S = 105,
    HTML_N_S = 106,
    HTML_Q = 107,
    HTML_N_Q = 108,
    HTML_I = 109,
    HTML_N_I = 110,
    HTML_STRONG = 111,
    HTML_N_STRONG = 112,
    HTML_SPAN = 113,
    HTML_N_SPAN = 114,
    HTML_ABBR = 115,
    HTML_N_ABBR = 116,
    HTML_ACRONYM = 117,
    HTML_N_ACRONYM = 118,
    HTML_BASEFONT = 119,
    HTML_BDO = 120,
    HTML_N_BDO = 121,
    HTML_BIG = 122,
    HTML_N_BIG = 123,
    HTML_BUTTON = 124,
    HTML_FIELDSET = 125,
    HTML_N_FIELDSET = 126,
    HTML_IFRAME = 127,
    HTML_LABEL = 128,
    HTML_N_LABEL = 129,
    HTML_LEGEND = 130,
    HTML_N_LEGEND = 131,
    HTML_NOSCRIPT = 132,
    HTML_N_NOSCRIPT = 133,
    HTML_OBJECT = 134,
    HTML_OPTGROUP = 135,
    HTML_N_OPTGROUP = 136,
    HTML_PARAM = 137,
    HTML_SMALL = 138,
    HTML_N_SMALL = 139,

    /* pseudo tag */
    HTML_SELECT_INT = 160,
    HTML_N_SELECT_INT = 161,
    HTML_OPTION_INT = 162,
    HTML_TEXTAREA_INT = 163,
    HTML_N_TEXTAREA_INT = 164,
    HTML_TABLE_ALT = 165,
    HTML_SYMBOL = 166,
    HTML_N_SYMBOL = 167,
    HTML_PRE_INT = 168,
    HTML_N_PRE_INT = 169,
    HTML_TITLE_ALT = 170,
    HTML_FORM_INT = 171,
    HTML_N_FORM_INT = 172,
    HTML_DL_COMPACT = 173,
    HTML_INPUT_ALT = 174,
    HTML_N_INPUT_ALT = 175,
    HTML_IMG_ALT = 176,
    HTML_N_IMG_ALT = 177,
    HTML_NOP = 178,
    HTML_PRE_PLAIN = 179,
    HTML_N_PRE_PLAIN = 180,
    HTML_INTERNAL = 181,
    HTML_N_INTERNAL = 182,
    HTML_DIV_INT = 183,
    HTML_N_DIV_INT = 184,

    MAX_HTMLTAG = 185,
};

enum HtmlTagAttributes
{
    ATTR_UNKNOWN = 0,
    ATTR_ACCEPT = 1,
    ATTR_ACCEPT_CHARSET = 2,
    ATTR_ACTION = 3,
    ATTR_ALIGN = 4,
    ATTR_ALT = 5,
    ATTR_ARCHIVE = 6,
    ATTR_BACKGROUND = 7,
    ATTR_BORDER = 8,
    ATTR_CELLPADDING = 9,
    ATTR_CELLSPACING = 10,
    ATTR_CHARSET = 11,
    ATTR_CHECKED = 12,
    ATTR_COLS = 13,
    ATTR_COLSPAN = 14,
    ATTR_CONTENT = 15,
    ATTR_ENCTYPE = 16,
    ATTR_HEIGHT = 17,
    ATTR_HREF = 18,
    ATTR_HTTP_EQUIV = 19,
    ATTR_ID = 20,
    ATTR_LINK = 21,
    ATTR_MAXLENGTH = 22,
    ATTR_METHOD = 23,
    ATTR_MULTIPLE = 24,
    ATTR_NAME = 25,
    ATTR_NOWRAP = 26,
    ATTR_PROMPT = 27,
    ATTR_ROWS = 28,
    ATTR_ROWSPAN = 29,
    ATTR_SIZE = 30,
    ATTR_SRC = 31,
    ATTR_TARGET = 32,
    ATTR_TYPE = 33,
    ATTR_USEMAP = 34,
    ATTR_VALIGN = 35,
    ATTR_VALUE = 36,
    ATTR_VSPACE = 37,
    ATTR_WIDTH = 38,
    ATTR_COMPACT = 39,
    ATTR_START = 40,
    ATTR_SELECTED = 41,
    ATTR_LABEL = 42,
    ATTR_READONLY = 43,
    ATTR_SHAPE = 44,
    ATTR_COORDS = 45,
    ATTR_ISMAP = 46,
    ATTR_REL = 47,
    ATTR_REV = 48,
    ATTR_TITLE = 49,
    ATTR_ACCESSKEY = 50,

    /* Internal attribute */
    ATTR_XOFFSET = 60,
    ATTR_YOFFSET = 61,
    ATTR_TOP_MARGIN = 62,
    ATTR_BOTTOM_MARGIN = 63,
    ATTR_TID = 64,
    ATTR_FID = 65,
    ATTR_FOR_TABLE = 66,
    ATTR_FRAMENAME = 67,
    ATTR_HBORDER = 68,
    ATTR_HSEQ = 69,
    ATTR_NO_EFFECT = 70,
    ATTR_REFERER = 71,
    ATTR_SELECTNUMBER = 72,
    ATTR_TEXTAREANUMBER = 73,
    ATTR_PRE_INT = 74,

    MAX_TAGATTR = 75,
};

enum AlignTypes
{
    ALIGN_CENTER = 0,
    ALIGN_LEFT = 1,
    ALIGN_RIGHT = 2,
    ALIGN_MIDDLE = 4,
    ALIGN_TOP = 5,
    ALIGN_BOTTOM = 6,
};

enum VerticalAlignTypes
{
    VALIGN_MIDDLE = 0,
    VALIGN_TOP = 1,
    VALIGN_BOTTOM = 2,
};

HtmlTags GetTag(const char *src, HtmlTags value);

enum TFlags
{
    TFLG_NONE = 0,
    TFLG_END = 1,
    TFLG_INT = 2,
};

/* HTML Tag Information Table */
struct TagInfo
{
    const char *name;
    HtmlTagAttributes *accept_attribute;
    unsigned char max_attribute;
    TFlags flag;
};
extern TagInfo TagMAP[];

struct parsed_tag : gc_cleanup
{
    HtmlTags tagid = HTML_UNKNOWN;
    unsigned char *attrid = nullptr;
    char **value;
    unsigned char *map;
    bool need_reconstruct = false;

    bool CanAcceptAttribute(HtmlTagAttributes id) const;
    bool HasAttribute(HtmlTagAttributes id) const;
    bool TryGetAttributeValue(HtmlTagAttributes id, void *value) const;
    bool SetAttributeValue(HtmlTagAttributes id, const char *value);
    std::string_view GetAttributeValue(HtmlTagAttributes id) const
    {
        char *value = nullptr;
        TryGetAttributeValue(id, &value);
        if (!value)
        {
            return "";
        }
        return value;
    }
    Str ToStr() const;

    parsed_tag(HtmlTags tag)
        : tagid(tag)
    {
    }

    std::string_view parse(std::string_view s, bool internal);

    int ul_type(int default_type = 0)
    {
        char *p;
        if (TryGetAttributeValue(ATTR_TYPE, &p))
        {
            if (!strcasecmp(p, "disc"))
                return (int)'d';
            else if (!strcasecmp(p, "circle"))
                return (int)'c';
            else if (!strcasecmp(p, "square"))
                return (int)'s';
        }
        return default_type;
    }

private:
    std::tuple<std::string_view, bool> parse_attr(std::string_view s, int nattr, bool internal);
};

std::tuple<std::string_view, struct parsed_tag *> parse_tag(std::string_view s, bool internal);
