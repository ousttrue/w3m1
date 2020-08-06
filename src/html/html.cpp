
#include "fm.h"
#include "myctype.h"
#include "indep.h"
#include "gc_helper.h"

#include "html/html.h"
#include "entity.h"
#include "w3m.h"

/* HTML Tag Attribute Information Table */
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

#define AFLG_INT 1

struct TagAttrInfo
{
    const char *name;
    unsigned char vtype;
    unsigned char flag;
};
TagAttrInfo AttrMAP[MAX_TAGATTR] ={
    { NULL, VTYPE_NONE, 0 },	/*  0 ATTR_UNKNOWN        */
    { "accept", VTYPE_NONE, 0 },	/*  1 ATTR_ACCEPT         */
    { "accept-charset", VTYPE_STR, 0 },	/*  2 ATTR_ACCEPT_CHARSET */
    { "action", VTYPE_ACTION, 0 },	/*  3 ATTR_ACTION         */
    { "align", VTYPE_ALIGN, 0 },	/*  4 ATTR_ALIGN          */
    { "alt", VTYPE_STR, 0 },	/*  5 ATTR_ALT            */
    { "archive", VTYPE_STR, 0 },	/*  6 ATTR_ARCHIVE        */
    { "background", VTYPE_STR, 0 },	/*  7 ATTR_BACKGROUND     */
    { "border", VTYPE_NUMBER, 0 },	/*  8 ATTR_BORDER         */
    { "cellpadding", VTYPE_NUMBER, 0 },	/*  9 ATTR_CELLPADDING    */
    { "cellspacing", VTYPE_NUMBER, 0 },	/* 10 ATTR_CELLSPACING    */
    { "charset", VTYPE_STR, 0 },	/* 11 ATTR_CHARSET        */
    { "checked", VTYPE_NONE, 0 },	/* 12 ATTR_CHECKED        */
    { "cols", VTYPE_MLENGTH, 0 },	/* 13 ATTR_COLS           */
    { "colspan", VTYPE_NUMBER, 0 },	/* 14 ATTR_COLSPAN        */
    { "content", VTYPE_STR, 0 },	/* 15 ATTR_CONTENT        */
    { "enctype", VTYPE_ENCTYPE, 0 },	/* 16 ATTR_ENCTYPE        */
    { "height", VTYPE_LENGTH, 0 },	/* 17 ATTR_HEIGHT         */
    { "href", VTYPE_STR, 0 },	/* 18 ATTR_HREF           */
    { "http-equiv", VTYPE_STR, 0 },	/* 19 ATTR_HTTP_EQUIV     */
    { "id", VTYPE_STR, 0 },	/* 20 ATTR_ID             */
    { "link", VTYPE_STR, 0 },	/* 21 ATTR_LINK           */
    { "maxlength", VTYPE_NUMBER, 0 },	/* 22 ATTR_MAXLENGTH      */
    { "method", VTYPE_METHOD, 0 },	/* 23 ATTR_METHOD         */
    { "multiple", VTYPE_NONE, 0 },	/* 24 ATTR_MULTIPLE       */
    { "name", VTYPE_STR, 0 },	/* 25 ATTR_NAME           */
    { "nowrap", VTYPE_NONE, 0 },	/* 26 ATTR_NOWRAP         */
    { "prompt", VTYPE_STR, 0 },	/* 27 ATTR_PROMPT         */
    { "rows", VTYPE_MLENGTH, 0 },	/* 28 ATTR_ROWS           */
    { "rowspan", VTYPE_NUMBER, 0 },	/* 29 ATTR_ROWSPAN        */
    { "size", VTYPE_NUMBER, 0 },	/* 30 ATTR_SIZE           */
    { "src", VTYPE_STR, 0 },	/* 31 ATTR_SRC            */
    { "target", VTYPE_STR, 0 },	/* 32 ATTR_TARGET         */
    { "type", VTYPE_TYPE, 0 },	/* 33 ATTR_TYPE           */
    { "usemap", VTYPE_STR, 0 },	/* 34 ATTR_USEMAP         */
    { "valign", VTYPE_VALIGN, 0 },	/* 35 ATTR_VALIGN         */
    { "value", VTYPE_STR, 0 },	/* 36 ATTR_VALUE          */
    { "vspace", VTYPE_NUMBER, 0 },	/* 37 ATTR_VSPACE         */
    { "width", VTYPE_LENGTH, 0 },	/* 38 ATTR_WIDTH          */
    { "compact", VTYPE_NONE, 0 },	/* 39 ATTR_COMPACT        */
    { "start", VTYPE_NUMBER, 0 },	/* 40 ATTR_START          */
    { "selected", VTYPE_NONE, 0 },	/* 41 ATTR_SELECTED       */
    { "label", VTYPE_STR, 0 },	/* 42 ATTR_LABEL          */
    { "readonly", VTYPE_NONE, 0 },	/* 43 ATTR_READONLY       */
    { "shape", VTYPE_STR, 0 },	/* 44 ATTR_SHAPE          */
    { "coords", VTYPE_STR, 0 },	/* 45 ATTR_COORDS         */
    { "ismap", VTYPE_NONE, 0 },	/* 46 ATTR_ISMAP          */
    { "rel", VTYPE_STR, 0 },	/* 47 ATTR_REL            */
    { "rev", VTYPE_STR, 0 },	/* 48 ATTR_REV            */
    { "title", VTYPE_STR, 0 },	/* 49 ATTR_TITLE          */
    { "accesskey", VTYPE_STR, 0 },	/* 50 ATTR_ACCESSKEY          */
    { NULL, VTYPE_NONE, 0 },	/* 51 Undefined           */
    { NULL, VTYPE_NONE, 0 },	/* 52 Undefined           */
    { NULL, VTYPE_NONE, 0 },	/* 53 Undefined           */
    { NULL, VTYPE_NONE, 0 },	/* 54 Undefined           */
    { NULL, VTYPE_NONE, 0 },	/* 55 Undefined           */
    { NULL, VTYPE_NONE, 0 },	/* 56 Undefined           */
    { NULL, VTYPE_NONE, 0 },	/* 57 Undefined           */
    { NULL, VTYPE_NONE, 0 },	/* 58 Undefined           */
    { NULL, VTYPE_NONE, 0 },	/* 59 Undefined           */

    /* Internal attribute */
    { "xoffset", VTYPE_NUMBER, AFLG_INT },	/* 60 ATTR_XOFFSET        */
    { "yoffset", VTYPE_NUMBER, AFLG_INT },	/* 61 ATTR_YOFFSET        */
    { "top_margin", VTYPE_NUMBER, AFLG_INT },	/* 62 ATTR_TOP_MARGIN,    */
    { "bottom_margin", VTYPE_NUMBER, AFLG_INT },	/* 63 ATTR_BOTTOM_MARGIN, */
    { "tid", VTYPE_NUMBER, AFLG_INT },	/* 64 ATTR_TID            */
    { "fid", VTYPE_NUMBER, AFLG_INT },	/* 65 ATTR_FID            */
    { "for_table", VTYPE_NONE, AFLG_INT },	/* 66 ATTR_FOR_TABLE      */
    { "framename", VTYPE_STR, AFLG_INT },	/* 67 ATTR_FRAMENAME      */
    { "hborder", VTYPE_NONE, 0 },	/* 68 ATTR_HBORDER        */
    { "hseq", VTYPE_NUMBER, AFLG_INT },	/* 69 ATTR_HSEQ           */
    { "no_effect", VTYPE_NONE, AFLG_INT },	/* 70 ATTR_NO_EFFECT      */
    { "referer", VTYPE_STR, AFLG_INT },	/* 71 ATTR_REFERER        */
    { "selectnumber", VTYPE_NUMBER, AFLG_INT },	/* 72 ATTR_SELECTNUMBER   */
    { "textareanumber", VTYPE_NUMBER, AFLG_INT },	/* 73 ATTR_TEXTAREANUMBER */
    { "pre_int", VTYPE_NONE, AFLG_INT },	/* 74 ATTR_PRE_INT      */
};

/* parse HTML tag */
static std::unordered_map<std::string, HtmlTags> g_tagMap ={
    /* 0 */{ "option_int", HTML_OPTION_INT },
    /* 1 */{ "/form_int", HTML_N_FORM_INT },
    /* 2 */{ "/kbd", HTML_NOP },
    /* 3 */{ "dd", HTML_DD },
    /* 4 */{ "/dir", HTML_N_UL },
    /* 5 */{ "/body", HTML_N_BODY },
    /* 6 */{ "noframes", HTML_NOFRAMES },
    /* 7 */{ "bdo", HTML_BDO },
    /* 8 */{ "base", HTML_BASE },
    /* 9 */{ "/div", HTML_N_DIV },
    /* 10 */{ "big", HTML_BIG },
    /* 11 */{ "tbody", HTML_TBODY },
    /* 12 */{ "meta", HTML_META },
    /* 13 */{ "i", HTML_I },
    /* 14 */{ "label", HTML_LABEL },
    /* 15 */{ "/_symbol", HTML_N_SYMBOL },
    /* 16 */{ "sup", HTML_SUP },
    /* 17 */{ "/p", HTML_N_P },
    /* 18 */{ "/q", HTML_N_Q },
    /* 19 */{ "input_alt", HTML_INPUT_ALT },
    /* 20 */{ "dl", HTML_DL },
    /* 21 */{ "/tbody", HTML_N_TBODY },
    /* 22 */{ "/s", HTML_N_S },
    /* 23 */{ "/label", HTML_N_LABEL },
    /* 24 */{ "del", HTML_DEL },
    /* 25 */{ "xmp", HTML_XMP },
    /* 26 */{ "br", HTML_BR },
    /* 27 */{ "iframe", HTML_IFRAME },
    /* 28 */{ "link", HTML_LINK },
    /* 29 */{ "/u", HTML_N_U },
    /* 30 */{ "em", HTML_EM },
    /* 31 */{ "title_alt", HTML_TITLE_ALT },
    /* 32 */{ "caption", HTML_CAPTION },
    /* 33 */{ "plaintext", HTML_PLAINTEXT },
    /* 34 */{ "p", HTML_P },
    /* 35 */{ "q", HTML_Q },
    /* 36 */{ "blockquote", HTML_BLQ },
    /* 37 */{ "menu", HTML_UL },
    /* 38 */{ "fieldset", HTML_FIELDSET },
    /* 39 */{ "/colgroup", HTML_N_COLGROUP },
    /* 40 */{ "dfn", HTML_NOP },
    /* 41 */{ "s", HTML_S },
    /* 42 */{ "strong", HTML_STRONG },
    /* 43 */{ "dt", HTML_DT },
    /* 44 */{ "u", HTML_U },
    /* 45 */{ "/map", HTML_N_MAP },
    /* 46 */{ "/frameset", HTML_N_FRAMESET },
    /* 47 */{ "/ol", HTML_N_OL },
    /* 48 */{ "noscript", HTML_NOSCRIPT },
    /* 49 */{ "legend", HTML_LEGEND },
    /* 50 */{ "/td", HTML_N_TD },
    /* 51 */{ "li", HTML_LI },
    /* 52 */{ "html", HTML_BODY },
    /* 53 */{ "hr", HTML_HR },
    /* 54 */{ "/strong", HTML_N_STRONG },
    /* 55 */{ "small", HTML_SMALL },
    /* 56 */{ "/th", HTML_N_TH },
    /* 57 */{ "option", HTML_OPTION },
    /* 58 */{ "/span", HTML_N_SPAN },
    /* 59 */{ "kbd", HTML_NOP },
    /* 60 */{ "dir", HTML_UL },
    /* 61 */{ "col", HTML_COL },
    /* 62 */{ "param", HTML_PARAM },
    /* 63 */{ "/small", HTML_N_SMALL },
    /* 64 */{ "/legend", HTML_N_LEGEND },
    /* 65 */{ "/caption", HTML_N_CAPTION },
    /* 66 */{ "div", HTML_DIV },
    /* 67 */{ "/abbr", HTML_N_ABBR },
    /* 68 */{ "head", HTML_HEAD },
    /* 69 */{ "ol", HTML_OL },
    /* 70 */{ "/ul", HTML_N_UL },
    /* 71 */{ "/ins", HTML_N_INS },
    /* 72 */{ "area", HTML_AREA },
    /* 73 */{ "pre_plain", HTML_PRE_PLAIN },
    /* 74 */{ "button", HTML_BUTTON },
    /* 75 */{ "td", HTML_TD },
    /* 76 */{ "/option", HTML_N_OPTION },
    /* 77 */{ "/noframes", HTML_N_NOFRAMES },
    /* 78 */{ "/tr", HTML_N_TR },
    /* 79 */{ "nobr", HTML_NOBR },
    /* 80 */{ "img_alt", HTML_IMG_ALT },
    /* 81 */{ "table_alt", HTML_TABLE_ALT },
    /* 82 */{ "th", HTML_TH },
    /* 83 */{ "script", HTML_SCRIPT },
    /* 84 */{ "/tt", HTML_NOP },
    /* 85 */{ "code", HTML_NOP },
    /* 86 */{ "optgroup", HTML_OPTGROUP },
    /* 87 */{ "samp", HTML_NOP },
    /* 88 */{ "textarea", HTML_TEXTAREA },
    /* 89 */{ "textarea_int", HTML_TEXTAREA_INT },
    /* 90 */{ "table", HTML_TABLE },
    /* 91 */{ "img", HTML_IMG },
    /* 92 */{ "/blockquote", HTML_N_BLQ },
    /* 93 */{ "applet", HTML_APPLET },
    /* 94 */{ "map", HTML_MAP },
    /* 95 */{ "ul", HTML_UL },
    /* 96 */{ "basefont", HTML_BASEFONT },
    /* 97 */{ "/script", HTML_N_SCRIPT },
    /* 98 */{ "center", HTML_CENTER },
    /* 99 */{ "/table", HTML_N_TABLE },
    /* 100 */{ "cite", HTML_NOP },
    /* 101 */{ "/h1", HTML_N_H },
    /* 102 */{ "/fieldset", HTML_N_FIELDSET },
    /* 103 */{ "tr", HTML_TR },
    /* 104 */{ "/h2", HTML_N_H },
    /* 105 */{ "image", HTML_IMG },
    /* 106 */{ "/h3", HTML_N_H },
    /* 107 */{ "pre_int", HTML_PRE_INT },
    /* 108 */{ "/font", HTML_N_FONT },
    /* 109 */{ "tt", HTML_NOP },
    /* 110 */{ "/h4", HTML_N_H },
    /* 111 */{ "body", HTML_BODY },
    /* 112 */{ "/form", HTML_N_FORM },
    /* 113 */{ "/h5", HTML_N_H },
    /* 114 */{ "/h6", HTML_N_H },
    /* 115 */{ "frame", HTML_FRAME },
    /* 116 */{ "/textarea_int", HTML_N_TEXTAREA_INT },
    /* 117 */{ "/noscript", HTML_N_NOSCRIPT },
    /* 118 */{ "/img_alt", HTML_N_IMG_ALT },
    /* 119 */{ "/center", HTML_N_CENTER },
    /* 120 */{ "/pre", HTML_N_PRE },
    /* 121 */{ "tfoot", HTML_TFOOT },
    /* 122 */{ "ins", HTML_INS },
    /* 123 */{ "/var", HTML_NOP },
    /* 124 */{ "h1", HTML_H },
    /* 125 */{ "/tfoot", HTML_N_TFOOT },
    /* 126 */{ "input", HTML_INPUT },
    /* 127 */{ "h2", HTML_H },
    /* 128 */{ "h3", HTML_H },
    /* 129 */{ "h4", HTML_H },
    /* 130 */{ "h5", HTML_H },
    /* 131 */{ "internal", HTML_INTERNAL },
    /* 132 */{ "h6", HTML_H },
    /* 133 */{ "div_int", HTML_DIV_INT },
    /* 134 */{ "select_int", HTML_SELECT_INT },
    /* 135 */{ "/pre_int", HTML_N_PRE_INT },
    /* 136 */{ "/menu", HTML_N_UL },
    /* 137 */{ "form_int", HTML_FORM_INT },
    /* 138 */{ "/sub", HTML_N_SUB },
    /* 139 */{ "style", HTML_STYLE },
    /* 140 */{ "address", HTML_BR },
    /* 141 */{ "/optgroup", HTML_N_OPTGROUP },
    /* 142 */{ "/textarea", HTML_N_TEXTAREA },
    /* 143 */{ "/input_alt", HTML_N_INPUT_ALT },
    /* 144 */{ "span", HTML_SPAN },
    /* 145 */{ "doctype", HTML_DOCTYPE },
    /* 146 */{ "/style", HTML_N_STYLE },
    /* 147 */{ "/html", HTML_N_BODY },
    /* 148 */{ "pre", HTML_PRE },
    /* 149 */{ "title", HTML_TITLE },
    /* 150 */{ "abbr", HTML_ABBR },
    /* 151 */{ "select", HTML_SELECT },
    /* 152 */{ "/bdo", HTML_N_BDO },
    /* 153 */{ "acronym", HTML_ACRONYM },
    /* 154 */{ "/div_int", HTML_N_DIV_INT },
    /* 155 */{ "var", HTML_NOP },
    /* 156 */{ "/big", HTML_N_BIG },
    /* 157 */{ "/title", HTML_N_TITLE },
    /* 158 */{ "embed", HTML_EMBED },
    /* 159 */{ "/sup", HTML_N_SUP },
    /* 160 */{ "colgroup", HTML_COLGROUP },
    /* 161 */{ "/head", HTML_N_HEAD },
    /* 162 */{ "isindex", HTML_ISINDEX },
    /* 163 */{ "strike", HTML_S },
    /* 164 */{ "listing", HTML_LISTING },
    /* 165 */{ "bgsound", HTML_BGSOUND },
    /* 166 */{ "/address", HTML_BR },
    /* 167 */{ "object", HTML_OBJECT },
    /* 168 */{ "thead", HTML_THEAD },
    /* 169 */{ "wbr", HTML_WBR },
    /* 170 */{ "/del", HTML_N_DEL },
    /* 171 */{ "/nobr", HTML_N_NOBR },
    /* 172 */{ "/select", HTML_N_SELECT },
    /* 173 */{ "frameset", HTML_FRAMESET },
    /* 174 */{ "/xmp", HTML_N_XMP },
    /* 175 */{ "/code", HTML_NOP },
    /* 176 */{ "_symbol", HTML_SYMBOL },
    /* 177 */{ "/thead", HTML_N_THEAD },
    /* 178 */{ "/samp", HTML_NOP },
    /* 179 */{ "/dfn", HTML_NOP },
    /* 180 */{ "_id", HTML_NOP },
    /* 181 */{ "/strike", HTML_N_S },
    /* 182 */{ "/a", HTML_N_A },
    /* 183 */{ "/select_int", HTML_N_SELECT_INT },
    /* 184 */{ "sub", HTML_SUB },
    /* 185 */{ "/b", HTML_N_B },
    /* 186 */{ "/internal", HTML_N_INTERNAL },
    /* 187 */{ "/acronym", HTML_N_ACRONYM },
    /* 188 */{ "/pre_plain", HTML_N_PRE_PLAIN },
    /* 189 */{ "font", HTML_FONT },
    /* 190 */{ "/dl", HTML_N_DL },
    /* 191 */{ "form", HTML_FORM },
    /* 192 */{ "/cite", HTML_NOP },
    /* 193 */{ "a", HTML_A },
    /* 194 */{ "b", HTML_B },
    /* 195 */{ "/listing", HTML_N_LISTING },
    /* 196 */{ "/em", HTML_N_EM },
    /* 197 */{ "/i", HTML_N_I },
};

HtmlTags GetTag(const char *src, HtmlTags defaultValue)
{
    auto found = g_tagMap.find(src);
    if (found == g_tagMap.end())
    {
        return defaultValue;
    }
    return found->second;
}

static int noConv(char *, int *);
static int toNumber(char *, int *);
static int toLength(char *, int *);
static int toAlign(char *, int *);
static int toVAlign(char *, int *);

/* *INDENT-OFF* */
static int (*toValFunc[])(char *, int *) ={
    noConv,   /* VTYPE_NONE    */
    noConv,   /* VTYPE_STR     */
    toNumber, /* VTYPE_NUMBER  */
    toLength, /* VTYPE_LENGTH  */
    toAlign,  /* VTYPE_ALIGN   */
    toVAlign, /* VTYPE_VALIGN  */
    noConv,   /* VTYPE_ACTION  */
    noConv,   /* VTYPE_ENCTYPE */
    noConv,   /* VTYPE_METHOD  */
    noConv,   /* VTYPE_MLENGTH */
    noConv,   /* VTYPE_TYPE    */
};
/* *INDENT-ON* */

static int
noConv(char *oval, int *_str)
{
    char **str = (char **)_str;
    *str = oval;
    return 1;
}

static int
toNumber(char *oval, int *num)
{
    char *ep;
    int x;

    x = strtol(oval, &ep, 10);

    if (ep > oval)
    {
        *num = x;
        return 1;
    }
    else
        return 0;
}

static int
toLength(char *oval, int *len)
{
    int w;
    if (!IS_DIGIT(oval[0]))
        return 0;
    w = atoi(oval);
    if (w < 0)
        return 0;
    if (w == 0)
        w = 1;
    if (oval[strlen(oval) - 1] == '%')
        *len = -w;
    else
        *len = w;
    return 1;
}

static int
toAlign(char *oval, int *align)
{
    if (strcasecmp(oval, "left") == 0)
        *align = ALIGN_LEFT;
    else if (strcasecmp(oval, "right") == 0)
        *align = ALIGN_RIGHT;
    else if (strcasecmp(oval, "center") == 0)
        *align = ALIGN_CENTER;
    else if (strcasecmp(oval, "top") == 0)
        *align = ALIGN_TOP;
    else if (strcasecmp(oval, "bottom") == 0)
        *align = ALIGN_BOTTOM;
    else if (strcasecmp(oval, "middle") == 0)
        *align = ALIGN_MIDDLE;
    else
        return 0;
    return 1;
}

static int
toVAlign(char *oval, int *valign)
{
    if (strcasecmp(oval, "top") == 0 || strcasecmp(oval, "baseline") == 0)
        *valign = VALIGN_TOP;
    else if (strcasecmp(oval, "bottom") == 0)
        *valign = VALIGN_BOTTOM;
    else if (strcasecmp(oval, "middle") == 0)
        *valign = VALIGN_MIDDLE;
    else
        return 0;
    return 1;
}

#define MAX_TAG_LEN 64

bool parsed_tag::parse_attr(char **s, int nattr, bool internal)
{
    auto q = *s;
    Str value = NULL, value_tmp = NULL;
    if (*q == '>' || *q == '\0') {
        return false;
    }

    char attrname[MAX_TAG_LEN];
    auto p = attrname;
    while (*q && *q != '=' && !IS_SPACE(*q) &&
        *q != '>' && p - attrname < MAX_TAG_LEN - 1)
    {
        *(p++) = TOLOWER(*q);
        q++;
    }
    *p = '\0';
    while (*q && *q != '=' && !IS_SPACE(*q) && *q != '>')
        q++;
    SKIP_BLANKS(&q);
    if (*q == '=')
    {
        /* get value */
        value_tmp = Strnew();
        q++;
        SKIP_BLANKS(&q);
        if (*q == '"')
        {
            q++;
            while (*q && *q != '"')
            {
                value_tmp->Push(*q);
                if (!this->need_reconstruct && is_html_quote(*q))
                    this->need_reconstruct = TRUE;
                q++;
            }
            if (*q == '"')
                q++;
        }
        else if (*q == '\'')
        {
            q++;
            while (*q && *q != '\'')
            {
                value_tmp->Push(*q);
                if (!this->need_reconstruct && is_html_quote(*q))
                    this->need_reconstruct = TRUE;
                q++;
            }
            if (*q == '\'')
                q++;
        }
        else if (*q)
        {
            while (*q && !IS_SPACE(*q) && *q != '>')
            {
                value_tmp->Push(*q);
                if (!this->need_reconstruct && is_html_quote(*q))
                    this->need_reconstruct = TRUE;
                q++;
            }
        }
    }

    int i = 0;
    HtmlTagAttributes attr_id = ATTR_UNKNOWN;
    for (; i < nattr; i++)
    {
        if (this->attrid[i] == ATTR_UNKNOWN &&
            strcmp(AttrMAP[TagMAP[this->tagid].accept_attribute[i]].name,
                attrname) == 0)
        {
            attr_id = TagMAP[this->tagid].accept_attribute[i];
            break;
        }
    }

    if (value_tmp)
    {
        int j, hidden = FALSE;
        for (j = 0; j < i; j++)
        {
            if (this->attrid[j] == ATTR_TYPE &&
                strcmp("hidden", this->value[j]) == 0)
            {
                hidden = TRUE;
                break;
            }
        }
        if ((this->tagid == HTML_INPUT || this->tagid == HTML_INPUT_ALT) &&
            attr_id == ATTR_VALUE && hidden)
        {
            value = value_tmp;
        }
        else
        {
            char *x;
            value = Strnew();
            for (x = value_tmp->ptr; *x; x++)
            {
                if (*x != '\n')
                    value->Push(*x);
            }
        }
    }
    *s = q;

    if (i != nattr)
    {
        if (!internal &&
            ((AttrMAP[attr_id].flag & AFLG_INT) ||
                (value && AttrMAP[attr_id].vtype == VTYPE_METHOD &&
                    !strcasecmp(value->ptr, "internal"))))
        {
            this->need_reconstruct = TRUE;
            return true;
        }
        this->attrid[i] = attr_id;
        if (value)
            this->value[i] = html_unquote(value->ptr, w3mApp::Instance().InnerCharset);
        else
            this->value[i] = NULL;
    }
    else
    {
        this->need_reconstruct = TRUE;
    }
    return true;
}

struct parsed_tag *parse_tag(char **s, int internal)
{
    /* Parse tag name */
    auto q = (*s) + 1;
    char tagname[MAX_TAG_LEN];
    auto p = tagname;
    if (*q == '/')
    {
        *(p++) = *(q++);
        SKIP_BLANKS(&q);
    }
    while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') &&
        *q != '>' && p - tagname < MAX_TAG_LEN - 1)
    {
        *(p++) = TOLOWER(*q);
        q++;
    }
    *p = '\0';
    while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') &&
        *q != '>')
        q++;

    auto tag_id = GetTag(tagname, HTML_UNKNOWN);
    if (tag_id == HTML_UNKNOWN ||
        (!internal && TagMAP[tag_id].flag & TFLG_INT))
    {
        return nullptr;
    }

    auto tag = new parsed_tag(tag_id);
    tag->parse(&q, internal);
    *s = q;
    return tag;
}

void parsed_tag::parse(char **s, bool internal)
{
    int nattr = TagMAP[this->tagid].max_attribute;
    if (nattr)
    {
        this->attrid = NewAtom_N(unsigned char, nattr);
        this->value = New_N(char *, nattr);
        this->map = NewAtom_N(unsigned char, MAX_TAGATTR);
        memset(this->map, MAX_TAGATTR, MAX_TAGATTR);
        memset(this->attrid, ATTR_UNKNOWN, nattr);
        for (auto i = 0; i < nattr; i++)
            this->map[TagMAP[this->tagid].accept_attribute[i]] = i;
    }

    /* Parse tag arguments */
    auto q = *s;
    SKIP_BLANKS(&q);
    while (this->parse_attr(&q, nattr, internal))
    {
    }
    while (*q != '>' && *q)
        q++;
    done_parse_tag:
    if (*q == '>')
        q++;
    *s = q;
}

bool parsed_tag::CanAcceptAttribute(HtmlTagAttributes id)const
{
    return (this->map && this->map[id] != MAX_TAGATTR);
}

bool parsed_tag::HasAttribute(HtmlTagAttributes id)const
{
    return CanAcceptAttribute(id) && (this->attrid[this->map[id]] != ATTR_UNKNOWN);
}

int parsedtag_set_value(struct parsed_tag *tag, HtmlTagAttributes id, const char *value)
{
    int i;

    if (!tag->CanAcceptAttribute(id))
        return 0;

    i = tag->map[id];
    tag->attrid[i] = id;
    if (value)
        tag->value[i] = allocStr(value, -1);
    else
        tag->value[i] = NULL;
    tag->need_reconstruct = TRUE;
    return 1;
}

bool parsed_tag::TryGetAttributeValue(HtmlTagAttributes id, void *value)const
{
    if (!HasAttribute(id))
    {
        return false;
    }
    if (!map)
    {
        return false;
    }
    int i=map[id];
    auto v = this->value[i];
    if (!v) {
        return 0;
    }
    return toValFunc[AttrMAP[id].vtype](v, (int *)value);
}

Str parsedtag2str(struct parsed_tag *tag)
{
    int i;
    int tag_id = tag->tagid;
    int nattr = TagMAP[tag_id].max_attribute;
    Str tagstr = Strnew();
    tagstr->Push('<');
    tagstr->Push(TagMAP[tag_id].name);
    for (i = 0; i < nattr; i++)
    {
        if (tag->attrid[i] != ATTR_UNKNOWN)
        {
            tagstr->Push(' ');
            tagstr->Push(AttrMAP[tag->attrid[i]].name);
            if (tag->value[i])
                tagstr->Push(Sprintf("=\"%s\"", html_quote(tag->value[i])));
        }
    }
    tagstr->Push('>');
    return tagstr;
}

#define ATTR_CORE	ATTR_ID
#define MAXA_CORE	1
HtmlTagAttributes ALST_A[] ={
    ATTR_NAME, ATTR_HREF, ATTR_REL, ATTR_CHARSET, ATTR_TARGET, ATTR_HSEQ,
    ATTR_REFERER,
    ATTR_FRAMENAME, ATTR_TITLE, ATTR_ACCESSKEY, ATTR_CORE
};
#define MAXA_A		MAXA_CORE + 10
HtmlTagAttributes ALST_P[] ={ ATTR_ALIGN, ATTR_CORE };
#define MAXA_P		MAXA_CORE + 1
HtmlTagAttributes ALST_UL[] ={ ATTR_START, ATTR_TYPE, ATTR_CORE };
#define MAXA_UL		MAXA_CORE + 2
HtmlTagAttributes ALST_LI[] ={ ATTR_TYPE, ATTR_VALUE, ATTR_CORE };
#define MAXA_LI		MAXA_CORE + 2
HtmlTagAttributes ALST_HR[] ={ ATTR_WIDTH, ATTR_ALIGN, ATTR_CORE };
#define MAXA_HR		MAXA_CORE + 2
HtmlTagAttributes ALST_LINK[] ={ ATTR_HREF, ATTR_HSEQ, ATTR_REL, ATTR_REV,
ATTR_TITLE, ATTR_TYPE, ATTR_CORE
};
#define MAXA_LINK	MAXA_CORE + sizeof ALST_LINK/sizeof ALST_LINK[0] - 1
HtmlTagAttributes ALST_DL[] ={ ATTR_COMPACT, ATTR_CORE };
#define MAXA_DL		MAXA_CORE + 1
HtmlTagAttributes ALST_PRE[] ={ ATTR_FOR_TABLE, ATTR_CORE };
#define MAXA_PRE	MAXA_CORE + 1
HtmlTagAttributes ALST_IMG[] =
{ ATTR_SRC, ATTR_ALT, ATTR_WIDTH, ATTR_HEIGHT, ATTR_ALIGN, ATTR_USEMAP,
ATTR_ISMAP, ATTR_TITLE, ATTR_PRE_INT, ATTR_CORE
};
#define MAXA_IMG	MAXA_CORE + 9
HtmlTagAttributes ALST_TABLE[] =
{ ATTR_BORDER, ATTR_WIDTH, ATTR_HBORDER, ATTR_CELLSPACING,
ATTR_CELLPADDING, ATTR_VSPACE, ATTR_CORE
};
#define MAXA_TABLE	MAXA_CORE + 6
HtmlTagAttributes ALST_META[] ={ ATTR_HTTP_EQUIV, ATTR_CONTENT, ATTR_CORE };
#define MAXA_META	MAXA_CORE + 2
HtmlTagAttributes ALST_FRAME[] ={ ATTR_SRC, ATTR_NAME, ATTR_CORE };
#define MAXA_FRAME	MAXA_CORE + 2
HtmlTagAttributes ALST_FRAMESET[] ={ ATTR_COLS, ATTR_ROWS, ATTR_CORE };
#define MAXA_FRAMESET	MAXA_CORE + 2
HtmlTagAttributes ALST_NOFRAMES[] ={ ATTR_CORE };
#define MAXA_NOFRAMES	MAXA_CORE
HtmlTagAttributes ALST_FORM[] =
{ ATTR_METHOD, ATTR_ACTION, ATTR_CHARSET, ATTR_ACCEPT_CHARSET,
ATTR_ENCTYPE, ATTR_TARGET, ATTR_NAME, ATTR_CORE
};
#define MAXA_FORM       MAXA_CORE + 7
HtmlTagAttributes ALST_INPUT[] =
{ ATTR_TYPE, ATTR_VALUE, ATTR_NAME, ATTR_CHECKED, ATTR_ACCEPT, ATTR_SIZE,
ATTR_MAXLENGTH, ATTR_ALT, ATTR_READONLY, ATTR_SRC, ATTR_WIDTH, ATTR_HEIGHT,
ATTR_CORE
};
#define MAXA_INPUT      MAXA_CORE + 12
HtmlTagAttributes ALST_TEXTAREA[] =
{ ATTR_COLS, ATTR_ROWS, ATTR_NAME, ATTR_READONLY, ATTR_CORE };
#define MAXA_TEXTAREA   MAXA_CORE + 4
HtmlTagAttributes ALST_SELECT[] ={ ATTR_NAME, ATTR_MULTIPLE, ATTR_CORE };
#define MAXA_SELECT	MAXA_CORE + 2
HtmlTagAttributes ALST_OPTION[] =
{ ATTR_VALUE, ATTR_LABEL, ATTR_SELECTED, ATTR_CORE };
#define MAXA_OPTION	MAXA_CORE + 3
HtmlTagAttributes ALST_ISINDEX[] ={ ATTR_ACTION, ATTR_PROMPT, ATTR_CORE };
#define MAXA_ISINDEX	MAXA_CORE + 2
HtmlTagAttributes ALST_MAP[] ={ ATTR_NAME, ATTR_CORE };
#define MAXA_MAP	MAXA_CORE + 1
HtmlTagAttributes ALST_AREA[] =
{ ATTR_HREF, ATTR_TARGET, ATTR_ALT, ATTR_SHAPE, ATTR_COORDS, ATTR_CORE };
#define MAXA_AREA	MAXA_CORE + 5
HtmlTagAttributes ALST_BASE[] ={ ATTR_HREF, ATTR_TARGET, ATTR_CORE };
#define MAXA_BASE	MAXA_CORE + 2
HtmlTagAttributes ALST_BODY[] ={ ATTR_BACKGROUND, ATTR_CORE };
#define MAXA_BODY	MAXA_CORE + 1
HtmlTagAttributes ALST_TR[] ={ ATTR_ALIGN, ATTR_VALIGN, ATTR_CORE };
#define MAXA_TR		MAXA_CORE + 2
HtmlTagAttributes ALST_TD[] =
{ ATTR_COLSPAN, ATTR_ROWSPAN, ATTR_ALIGN, ATTR_VALIGN, ATTR_WIDTH,
ATTR_NOWRAP, ATTR_CORE
};
#define MAXA_TD		MAXA_CORE + 6
HtmlTagAttributes ALST_BGSOUND[] ={ ATTR_SRC, ATTR_CORE };
#define MAX_BGSOUND	MAXA_CORE + 1
HtmlTagAttributes ALST_APPLET[] ={ ATTR_ARCHIVE, ATTR_CORE };
#define MAX_APPLET	MAXA_CORE + 1
HtmlTagAttributes ALST_EMBED[] ={ ATTR_SRC, ATTR_CORE };
#define MAX_EMBED	MAXA_CORE + 1

HtmlTagAttributes ALST_TEXTAREA_INT[] ={ ATTR_TEXTAREANUMBER };
#define MAXA_TEXTAREA_INT 1
HtmlTagAttributes ALST_SELECT_INT[] ={ ATTR_SELECTNUMBER };
#define MAXA_SELECT_INT	1
HtmlTagAttributes ALST_TABLE_ALT[] ={ ATTR_TID };
#define MAXA_TABLE_ALT	1
HtmlTagAttributes ALST_SYMBOL[] ={ ATTR_TYPE };
#define MAXA_SYMBOL	1
HtmlTagAttributes ALST_TITLE_ALT[] ={ ATTR_TITLE };
#define MAXA_TITLE_ALT	1
HtmlTagAttributes ALST_FORM_INT[] =
{ ATTR_METHOD, ATTR_ACTION, ATTR_CHARSET, ATTR_ACCEPT_CHARSET,
ATTR_ENCTYPE, ATTR_TARGET, ATTR_NAME, ATTR_FID
};
#define MAXA_FORM_INT  8
HtmlTagAttributes ALST_INPUT_ALT[] =
{ ATTR_HSEQ, ATTR_FID, ATTR_NO_EFFECT, ATTR_TYPE, ATTR_NAME, ATTR_VALUE,
ATTR_CHECKED, ATTR_ACCEPT, ATTR_SIZE, ATTR_MAXLENGTH, ATTR_READONLY,
ATTR_TEXTAREANUMBER,
ATTR_SELECTNUMBER, ATTR_ROWS, ATTR_TOP_MARGIN, ATTR_BOTTOM_MARGIN
};
#define MAXA_INPUT_ALT  16
HtmlTagAttributes ALST_IMG_ALT[] =
{ ATTR_SRC, ATTR_WIDTH, ATTR_HEIGHT, ATTR_USEMAP, ATTR_ISMAP, ATTR_HSEQ,
ATTR_XOFFSET, ATTR_YOFFSET, ATTR_TOP_MARGIN, ATTR_BOTTOM_MARGIN,
ATTR_TITLE
};
#define MAXA_IMG_ALT  11
HtmlTagAttributes ALST_NOP[] ={ ATTR_CORE };
#define MAXA_NOP	MAXA_CORE

TagInfo TagMAP[MAX_HTMLTAG] ={
    { NULL, NULL, 0, TFLG_NONE },		/*   0 HTML_UNKNOWN    */
    { "a", ALST_A, MAXA_A, TFLG_NONE },	/*   1 HTML_A          */
    { "/a", NULL, 0, TFLG_END },	/*   2 HTML_N_A        */
    { "h", ALST_P, MAXA_P, TFLG_NONE },	/*   3 HTML_H          */
    { "/h", NULL, 0, TFLG_END },	/*   4 HTML_N_H        */
    { "p", ALST_P, MAXA_P, TFLG_NONE },	/*   5 HTML_P          */
    { "br", ALST_NOP, MAXA_NOP, TFLG_NONE },		/*   6 HTML_BR         */
    { "b", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*   7 HTML_B          */
    { "/b", NULL, 0, TFLG_END },	/*   8 HTML_N_B        */
    { "ul", ALST_UL, MAXA_UL, TFLG_NONE },	/*   9 HTML_UL         */
    { "/ul", NULL, 0, TFLG_END },	/*  10 HTML_N_UL       */
    { "li", ALST_LI, MAXA_LI, TFLG_NONE },	/*  11 HTML_LI         */
    { "ol", ALST_UL, MAXA_UL, TFLG_NONE },	/*  12 HTML_OL         */
    { "/ol", NULL, 0, TFLG_END },	/*  13 HTML_N_OL       */
    { "title", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  14 HTML_TITLE      */
    { "/title", NULL, 0, TFLG_END },	/*  15 HTML_N_TITLE    */
    { "hr", ALST_HR, MAXA_HR, TFLG_NONE },	/*  16 HTML_HR         */
    { "dl", ALST_DL, MAXA_DL, TFLG_NONE },	/*  17 HTML_DL         */
    { "/dl", NULL, 0, TFLG_END },	/*  18 HTML_N_DL       */
    { "dt", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  19 HTML_DT         */
    { "dd", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  20 HTML_DD         */
    { "pre", ALST_PRE, MAXA_PRE, TFLG_NONE },	/*  21 HTML_PRE        */
    { "/pre", NULL, 0, TFLG_END },	/*  22 HTML_N_PRE      */
    { "blockquote", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  23 HTML_BLQ        */
    { "/blockquote", NULL, 0, TFLG_END },	/*  24 HTML_N_BLQ      */
    { "img", ALST_IMG, MAXA_IMG, TFLG_NONE },	/*  25 HTML_IMG        */
    { "listing", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  26 HTML_LISTING    */
    { "/listing", NULL, 0, TFLG_END },	/*  27 HTML_N_LISTING  */
    { "xmp", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  28 HTML_XMP        */
    { "/xmp", NULL, 0, TFLG_END },	/*  29 HTML_N_XMP      */
    { "plaintext", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  30 HTML_PLAINTEXT  */
    { "table", ALST_TABLE, MAXA_TABLE, TFLG_NONE },	/*  31 HTML_TABLE      */
    { "/table", NULL, 0, TFLG_END },	/*  32 HTML_N_TABLE    */
    { "meta", ALST_META, MAXA_META, TFLG_NONE },	/*  33 HTML_META       */
    { "/p", NULL, 0, TFLG_END },	/*  34 HTML_N_P        */
    { "frame", ALST_FRAME, MAXA_FRAME, TFLG_NONE },	/*  35 HTML_FRAME      */
    { "frameset", ALST_FRAMESET, MAXA_FRAMESET, TFLG_NONE },	/*  36 HTML_FRAMESET   */
    { "/frameset", NULL, 0, TFLG_END },	/*  37 HTML_N_FRAMESET */
    { "center", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  38 HTML_CENTER     */
    { "/center", NULL, 0, TFLG_END },	/*  39 HTML_N_CENTER   */
    { "font", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  40 HTML_FONT       */
    { "/font", NULL, 0, TFLG_END },	/*  41 HTML_N_FONT     */
    { "form", ALST_FORM, MAXA_FORM, TFLG_NONE },	/*  42 HTML_FORM       */
    { "/form", NULL, 0, TFLG_END },	/*  43 HTML_N_FORM     */
    { "input", ALST_INPUT, MAXA_INPUT, TFLG_NONE },	/*  44 HTML_INPUT      */
    { "textarea", ALST_TEXTAREA, MAXA_TEXTAREA, TFLG_NONE },	/*  45 HTML_TEXTAREA   */
    { "/textarea", NULL, 0, TFLG_END },	/*  46 HTML_N_TEXTAREA */
    { "select", ALST_SELECT, MAXA_SELECT, TFLG_NONE },	/*  47 HTML_SELECT     */
    { "/select", NULL, 0, TFLG_END },	/*  48 HTML_N_SELECT   */
    { "option", ALST_OPTION, MAXA_OPTION, TFLG_NONE },	/*  49 HTML_OPTION     */
    { "nobr", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  50 HTML_NOBR       */
    { "/nobr", NULL, 0, TFLG_END },	/*  51 HTML_N_NOBR     */
    { "div", ALST_P, MAXA_P, TFLG_NONE },	/*  52 HTML_DIV        */
    { "/div", NULL, 0, TFLG_END },	/*  53 HTML_N_DIV      */
    { "isindex", ALST_ISINDEX, MAXA_ISINDEX, TFLG_NONE },	/*  54 HTML_ISINDEX    */
    { "map", ALST_MAP, MAXA_MAP, TFLG_NONE },	/*  55 HTML_MAP        */
    { "/map", NULL, 0, TFLG_END },	/*  56 HTML_N_MAP      */
    { "area", ALST_AREA, MAXA_AREA, TFLG_NONE },	/*  57 HTML_AREA       */
    { "script", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  58 HTML_SCRIPT     */
    { "/script", NULL, 0, TFLG_END },	/*  59 HTML_N_SCRIPT   */
    { "base", ALST_BASE, MAXA_BASE, TFLG_NONE },	/*  60 HTML_BASE       */
    { "del", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  61 HTML_DEL        */
    { "/del", NULL, 0, TFLG_END },	/*  62 HTML_N_DEL      */
    { "ins", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  63 HTML_INS        */
    { "/ins", NULL, 0, TFLG_END },	/*  64 HTML_N_INS      */
    { "u", ALST_NOP, MAXA_NOP, TFLG_NONE },		/*  65 HTML_U          */
    { "/u", NULL, 0, TFLG_END },	/*  66 HTML_N_U        */
    { "style", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  67 HTML_STYLE      */
    { "/style", NULL, 0, TFLG_END },	/*  68 HTML_N_STYLE    */
    { "wbr", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  69 HTML_WBR        */
    { "em", ALST_NOP, MAXA_NOP, TFLG_NONE },		/*  70 HTML_EM         */
    { "/em", NULL, 0, TFLG_END },	/*  71 HTML_N_EM       */
    { "body", ALST_BODY, MAXA_BODY, TFLG_NONE },	/*  72 HTML_BODY       */
    { "/body", NULL, 0, TFLG_END },	/*  73 HTML_N_BODY     */
    { "tr", ALST_TR, MAXA_TR, TFLG_NONE },	/*  74 HTML_TR         */
    { "/tr", NULL, 0, TFLG_END },	/*  75 HTML_N_TR       */
    { "td", ALST_TD, MAXA_TD, TFLG_NONE },	/*  76 HTML_TD         */
    { "/td", NULL, 0, TFLG_END },	/*  77 HTML_N_TD       */
    { "caption", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  78 HTML_CAPTION    */
    { "/caption", NULL, 0, TFLG_END },	/*  79 HTML_N_CAPTION  */
    { "th", ALST_TD, MAXA_TD, TFLG_NONE },	/*  80 HTML_TH         */
    { "/th", NULL, 0, TFLG_END },	/*  81 HTML_N_TH       */
    { "thead", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  82 HTML_THEAD      */
    { "/thead", NULL, 0, TFLG_END },	/*  83 HTML_N_THEAD    */
    { "tbody", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  84 HTML_TBODY      */
    { "/tbody", NULL, 0, TFLG_END },	/*  85 HTML_N_TBODY    */
    { "tfoot", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  86 HTML_TFOOT      */
    { "/tfoot", NULL, 0, TFLG_END },	/*  87 HTML_N_TFOOT    */
    { "colgroup", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  88 HTML_COLGROUP   */
    { "/colgroup", NULL, 0, TFLG_END },	/*  89 HTML_N_COLGROUP */
    { "col", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  90 HTML_COL        */
    { "bgsound", ALST_BGSOUND, MAX_BGSOUND, TFLG_NONE },	/*  91 HTML_BGSOUND    */
    { "applet", ALST_APPLET, MAX_APPLET, TFLG_NONE },	/*  92 HTML_APPLET     */
    { "embed", ALST_EMBED, MAX_EMBED, TFLG_NONE },	/*  93 HTML_EMBED      */
    { "/option", NULL, 0, TFLG_END },	/*  94 HTML_N_OPTION   */
    { "head", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  95 HTML_HEAD       */
    { "/head", NULL, 0, TFLG_END },	/*  96 HTML_N_HEAD     */
    { "doctype", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  97 HTML_DOCTYPE    */
    { "noframes", ALST_NOFRAMES, MAXA_NOFRAMES, TFLG_NONE },	/*  98 HTML_NOFRAMES   */
    { "/noframes", NULL, 0, TFLG_END },	/*  99 HTML_N_NOFRAMES */

    { "sup", ALST_NOP, MAXA_NOP, TFLG_NONE },	/* 100 HTML_SUP       */
    { "/sup", NULL, 0, TFLG_NONE },	/* 101 HTML_N_SUP       */
    /* FIXME: Should /sup and /sub have TFLG_END ? */
    { "sub", ALST_NOP, MAXA_NOP, TFLG_NONE },	/* 102 HTML_SUB       */
    { "/sub", NULL, 0, TFLG_NONE },	/* 103 HTML_N_SUB       */
    { "link", ALST_LINK, MAXA_LINK, TFLG_NONE },	/*  104 HTML_LINK      */
    { "s", ALST_NOP, MAXA_NOP, TFLG_NONE },		/*  105 HTML_S        */
    { "/s", NULL, 0, TFLG_END },	/*  106 HTML_N_S      */
    { "q", ALST_NOP, MAXA_NOP, TFLG_NONE },		/*  107 HTML_Q */
    { "/q", NULL, 0, TFLG_END },	/*  108 HTML_N_Q */
    { "i", ALST_NOP, MAXA_NOP, TFLG_NONE },	/*  109 HTML_I */
    { "/i", NULL, 0, TFLG_END },	/*  110 HTML_N_I */
    { "strong", ALST_NOP, MAXA_NOP, TFLG_NONE },	/* 111 HTML_STRONG */
    { "/strong", NULL, 0, TFLG_END },	/* 112 HTML_N_STRONG */
    { "span", ALST_NOP, MAXA_NOP, TFLG_NONE },	/* 113 HTML_SPAN       */
    { "/span", NULL, 0, TFLG_END },	/* 114 HTML_N_SPAN     */
    { "abbr", ALST_NOP, MAXA_NOP, TFLG_NONE },		/* 115 HTML_ABBR */
    { "/abbr", NULL, 0, TFLG_END },	/* 116 HTML_N_ABBR */
    { "acronym", ALST_NOP, MAXA_NOP, TFLG_NONE },		/* 117 HTML_ACRONYM */
    { "/acronym", NULL, 0, TFLG_END },	/* 118 HTML_N_ACRONYM */
    { "basefont", ALST_NOP, MAXA_NOP, TFLG_NONE },	        /* 119 HTML_BASEFONT */
    { "bdo", ALST_NOP, MAXA_NOP, TFLG_NONE }, 		/* 120 HTML_BDO */
    { "/bdo", NULL, 0, TFLG_END },	/* 121 HTML_N_BDO */
    { "big", ALST_NOP, MAXA_NOP, TFLG_NONE },		/* 122 HTML_BIG */
    { "/big", NULL, 0, TFLG_END },	/* 123 HTML_N_BIG */
    { "button", ALST_NOP, MAXA_NOP, TFLG_NONE },		/* 124 HTML_BUTTON */
    { "fieldset", ALST_NOP, MAXA_NOP, TFLG_NONE },	        /* 125 HTML_FIELDSET */
    { "/fieldset", NULL, 0, TFLG_END },	/* 126 HTML_N_FIELDSET */
    { "iframe", ALST_NOP, MAXA_NOP, TFLG_NONE },		/* 127 HTML_IFRAME */
    { "label", ALST_NOP, MAXA_NOP, TFLG_NONE }, 		/* 128 HTML_LABEL */
    { "/label", NULL, 0, TFLG_END },	/* 129 HTML_N_LABEL */
    { "legend", ALST_NOP, MAXA_NOP, TFLG_NONE },		/* 130 HTML_LEGEND */
    { "/legend", NULL, 0, TFLG_END },	/* 131 HTML_N_LEGEND */
    { "noscript", ALST_NOP, MAXA_NOP, TFLG_NONE },	        /* 132 HTML_NOSCRIPT */
    { "/noscript", NULL, 0, TFLG_END },	/* 133 HTML_N_NOSCRIPT */
    { "object", ALST_NOP, MAXA_NOP, TFLG_NONE },		/* 134 HTML_OBJECT */
    { "optgroup", ALST_NOP, MAXA_NOP, TFLG_NONE },	        /* 135 HTML_OPTGROUP */
    { "/optgroup", NULL, 0, TFLG_END },	/* 136 HTML_N_OPTGROUP */
    { "param", ALST_NOP, MAXA_NOP, TFLG_NONE },		/* 137 HTML_PARAM */
    { "small", ALST_NOP, MAXA_NOP, TFLG_NONE }, 		/* 138 HTML_SMALL */
    { "/small", NULL, 0, TFLG_END },	/* 139 HTML_N_SMALL */

    { NULL, NULL, 0, TFLG_NONE },		/* 140 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 141 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 142 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 143 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 144 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 145 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 146 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 147 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 148 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 149 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 150 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 151 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 152 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 153 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 154 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 155 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 156 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 157 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 158 Undefined */
    { NULL, NULL, 0, TFLG_NONE },		/* 159 Undefined */

    /* pseudo tag */
    { "select_int", ALST_SELECT_INT, MAXA_SELECT_INT, TFLG_INT },	/* 160 HTML_SELECT_INT   */
    { "/select_int", NULL, 0, TFLG_INT | TFLG_END },	/* 161 HTML_N_SELECT_INT */
    { "option_int", ALST_OPTION, MAXA_OPTION, TFLG_INT },	/* 162 HTML_OPTION_INT   */
    { "textarea_int", ALST_TEXTAREA_INT, MAXA_TEXTAREA_INT, TFLG_INT },	/* 163 HTML_TEXTAREA_INT   */
    { "/textarea_int", NULL, 0, TFLG_INT | TFLG_END },	/* 164 HTML_N_TEXTAREA_INT */
    { "table_alt", ALST_TABLE_ALT, MAXA_TABLE_ALT, TFLG_INT },	/* 165 HTML_TABLE_ALT   */
    { "symbol", ALST_SYMBOL, MAXA_SYMBOL, TFLG_INT },	/* 166 HTML_SYMBOL */
    { "/symbol", NULL, 0, TFLG_INT | TFLG_END },	/* 167 HTML_N_SYMBOL      */
    { "pre_int", NULL, 0, TFLG_INT },	/* 168 HTML_PRE_INT     */
    { "/pre_int", NULL, 0, TFLG_INT | TFLG_END },	/* 169 HTML_N_PRE_INT   */
    { "title_alt", ALST_TITLE_ALT, MAXA_TITLE_ALT, TFLG_INT },	/* 170 HTML_TITLE_ALT   */
    { "form_int", ALST_FORM_INT, MAXA_FORM_INT, TFLG_INT },	/* 171 HTML_FORM_INT    */
    { "/form_int", NULL, 0, TFLG_INT | TFLG_END },	/* 172 HTML_N_FORM_INT  */
    { "dl_compact", NULL, 0, TFLG_INT },	/* 173 HTML_DL_COMPACT  */
    { "input_alt", ALST_INPUT_ALT, MAXA_INPUT_ALT, TFLG_INT },	/* 174 HTML_INPUT_ALT   */
    { "/input_alt", NULL, 0, TFLG_INT | TFLG_END },	/* 175 HTML_N_INPUT_ALT */
    { "img_alt", ALST_IMG_ALT, MAXA_IMG_ALT, TFLG_INT },	/* 176 HTML_IMG_ALT     */
    { "/img_alt", NULL, 0, TFLG_INT | TFLG_END },	/* 177 HTML_N_IMG_ALT   */
    { " ", ALST_NOP, MAXA_NOP, TFLG_INT },	/* 178 HTML_NOP         */
    { "pre_plain", NULL, 0, TFLG_INT },	/* 179 HTML_PRE_PLAIN         */
    { "/pre_plain", NULL, 0, TFLG_INT | TFLG_END },	/* 180 HTML_N_PRE_PLAIN         */
    { "internal", NULL, 0, TFLG_INT },	/* 181 HTML_INTERNAL   */
    { "/internal", NULL, 0, TFLG_INT | TFLG_END },	/* 182 HTML_N_INTERNAL   */
    { "div_int", ALST_P, MAXA_P, TFLG_INT },	/*  183 HTML_DIV_INT    */
    { "/div_int", NULL, 0, TFLG_INT | TFLG_END },	/*  184 HTML_N_DIV_INT  */
};

