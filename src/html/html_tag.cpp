#include <string_view_util.h>
#include "html_tag.h"
#include "indep.h"
#include "gc_helper.h"
#include "myctype.h"
#include "w3m.h"

#define MAX_TAG_LEN 64

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
TagAttrInfo AttrMAP[MAX_TAGATTR] = {
    {NULL, VTYPE_NONE, 0},            /*  0 ATTR_UNKNOWN        */
    {"accept", VTYPE_NONE, 0},        /*  1 ATTR_ACCEPT         */
    {"accept-charset", VTYPE_STR, 0}, /*  2 ATTR_ACCEPT_CHARSET */
    {"action", VTYPE_ACTION, 0},      /*  3 ATTR_ACTION         */
    {"align", VTYPE_ALIGN, 0},        /*  4 ATTR_ALIGN          */
    {"alt", VTYPE_STR, 0},            /*  5 ATTR_ALT            */
    {"archive", VTYPE_STR, 0},        /*  6 ATTR_ARCHIVE        */
    {"background", VTYPE_STR, 0},     /*  7 ATTR_BACKGROUND     */
    {"border", VTYPE_NUMBER, 0},      /*  8 ATTR_BORDER         */
    {"cellpadding", VTYPE_NUMBER, 0}, /*  9 ATTR_CELLPADDING    */
    {"cellspacing", VTYPE_NUMBER, 0}, /* 10 ATTR_CELLSPACING    */
    {"charset", VTYPE_STR, 0},        /* 11 ATTR_CHARSET        */
    {"checked", VTYPE_NONE, 0},       /* 12 ATTR_CHECKED        */
    {"cols", VTYPE_MLENGTH, 0},       /* 13 ATTR_COLS           */
    {"colspan", VTYPE_NUMBER, 0},     /* 14 ATTR_COLSPAN        */
    {"content", VTYPE_STR, 0},        /* 15 ATTR_CONTENT        */
    {"enctype", VTYPE_ENCTYPE, 0},    /* 16 ATTR_ENCTYPE        */
    {"height", VTYPE_LENGTH, 0},      /* 17 ATTR_HEIGHT         */
    {"href", VTYPE_STR, 0},           /* 18 ATTR_HREF           */
    {"http-equiv", VTYPE_STR, 0},     /* 19 ATTR_HTTP_EQUIV     */
    {"id", VTYPE_STR, 0},             /* 20 ATTR_ID             */
    {"link", VTYPE_STR, 0},           /* 21 ATTR_LINK           */
    {"maxlength", VTYPE_NUMBER, 0},   /* 22 ATTR_MAXLENGTH      */
    {"method", VTYPE_METHOD, 0},      /* 23 ATTR_METHOD         */
    {"multiple", VTYPE_NONE, 0},      /* 24 ATTR_MULTIPLE       */
    {"name", VTYPE_STR, 0},           /* 25 ATTR_NAME           */
    {"nowrap", VTYPE_NONE, 0},        /* 26 ATTR_NOWRAP         */
    {"prompt", VTYPE_STR, 0},         /* 27 ATTR_PROMPT         */
    {"rows", VTYPE_MLENGTH, 0},       /* 28 ATTR_ROWS           */
    {"rowspan", VTYPE_NUMBER, 0},     /* 29 ATTR_ROWSPAN        */
    {"size", VTYPE_NUMBER, 0},        /* 30 ATTR_SIZE           */
    {"src", VTYPE_STR, 0},            /* 31 ATTR_SRC            */
    {"target", VTYPE_STR, 0},         /* 32 ATTR_TARGET         */
    {"type", VTYPE_TYPE, 0},          /* 33 ATTR_TYPE           */
    {"usemap", VTYPE_STR, 0},         /* 34 ATTR_USEMAP         */
    {"valign", VTYPE_VALIGN, 0},      /* 35 ATTR_VALIGN         */
    {"value", VTYPE_STR, 0},          /* 36 ATTR_VALUE          */
    {"vspace", VTYPE_NUMBER, 0},      /* 37 ATTR_VSPACE         */
    {"width", VTYPE_LENGTH, 0},       /* 38 ATTR_WIDTH          */
    {"compact", VTYPE_NONE, 0},       /* 39 ATTR_COMPACT        */
    {"start", VTYPE_NUMBER, 0},       /* 40 ATTR_START          */
    {"selected", VTYPE_NONE, 0},      /* 41 ATTR_SELECTED       */
    {"label", VTYPE_STR, 0},          /* 42 ATTR_LABEL          */
    {"readonly", VTYPE_NONE, 0},      /* 43 ATTR_READONLY       */
    {"shape", VTYPE_STR, 0},          /* 44 ATTR_SHAPE          */
    {"coords", VTYPE_STR, 0},         /* 45 ATTR_COORDS         */
    {"ismap", VTYPE_NONE, 0},         /* 46 ATTR_ISMAP          */
    {"rel", VTYPE_STR, 0},            /* 47 ATTR_REL            */
    {"rev", VTYPE_STR, 0},            /* 48 ATTR_REV            */
    {"title", VTYPE_STR, 0},          /* 49 ATTR_TITLE          */
    {"accesskey", VTYPE_STR, 0},      /* 50 ATTR_ACCESSKEY          */
    {NULL, VTYPE_NONE, 0},            /* 51 Undefined           */
    {NULL, VTYPE_NONE, 0},            /* 52 Undefined           */
    {NULL, VTYPE_NONE, 0},            /* 53 Undefined           */
    {NULL, VTYPE_NONE, 0},            /* 54 Undefined           */
    {NULL, VTYPE_NONE, 0},            /* 55 Undefined           */
    {NULL, VTYPE_NONE, 0},            /* 56 Undefined           */
    {NULL, VTYPE_NONE, 0},            /* 57 Undefined           */
    {NULL, VTYPE_NONE, 0},            /* 58 Undefined           */
    {NULL, VTYPE_NONE, 0},            /* 59 Undefined           */

    /* Internal attribute */
    {"xoffset", VTYPE_NUMBER, AFLG_INT},        /* 60 ATTR_XOFFSET        */
    {"yoffset", VTYPE_NUMBER, AFLG_INT},        /* 61 ATTR_YOFFSET        */
    {"top_margin", VTYPE_NUMBER, AFLG_INT},     /* 62 ATTR_TOP_MARGIN,    */
    {"bottom_margin", VTYPE_NUMBER, AFLG_INT},  /* 63 ATTR_BOTTOM_MARGIN, */
    {"tid", VTYPE_NUMBER, AFLG_INT},            /* 64 ATTR_TID            */
    {"fid", VTYPE_NUMBER, AFLG_INT},            /* 65 ATTR_FID            */
    {"for_table", VTYPE_NONE, AFLG_INT},        /* 66 ATTR_FOR_TABLE      */
    {"framename", VTYPE_STR, AFLG_INT},         /* 67 ATTR_FRAMENAME      */
    {"hborder", VTYPE_NONE, 0},                 /* 68 ATTR_HBORDER        */
    {"hseq", VTYPE_NUMBER, AFLG_INT},           /* 69 ATTR_HSEQ           */
    {"no_effect", VTYPE_NONE, AFLG_INT},        /* 70 ATTR_NO_EFFECT      */
    {"referer", VTYPE_STR, AFLG_INT},           /* 71 ATTR_REFERER        */
    {"selectnumber", VTYPE_NUMBER, AFLG_INT},   /* 72 ATTR_SELECTNUMBER   */
    {"textareanumber", VTYPE_NUMBER, AFLG_INT}, /* 73 ATTR_TEXTAREANUMBER */
    {"pre_int", VTYPE_NONE, AFLG_INT},          /* 74 ATTR_PRE_INT      */
};

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

/* *INDENT-OFF* */
static int (*toValFunc[])(char *, int *) = {
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

std::tuple<std::string_view, bool> HtmlTag::parse_attr(std::string_view s, int nattr, bool internal)
{
    auto q = s;
    if (q.empty() || q[0] == '>')
    {
        return {q, false};
    }

    // key
    char attrname[MAX_TAG_LEN];
    auto p = attrname;
    while (q[0] && q[0] != '=' && !IS_SPACE(q[0]) &&
           q[0] != '>' && p - attrname < MAX_TAG_LEN - 1)
    {
        *(p++) = TOLOWER(q[0]);
        q.remove_prefix(1);
    }
    *p = '\0';
    while (q[0] && q[0] != '=' && !IS_SPACE(q[0]) && q[0] != '>')
        q.remove_prefix(1);
    q = svu::strip_left(q);

    // value
    Str value_tmp = nullptr;
    if (q[0] == '=')
    {
        /* get value */
        value_tmp = Strnew();
        q.remove_prefix(1);
        q = svu::strip_left(q);
        if (q[0] == '"')
        {
            q.remove_prefix(1);
            while (q[0] && q[0] != '"')
            {
                value_tmp->Push(q[0]);
                if (!this->need_reconstruct && is_html_quote(q[0]))
                    this->need_reconstruct = true;
                q.remove_prefix(1);
            }
            if (q[0] == '"')
                q.remove_prefix(1);
        }
        else if (q[0] == '\'')
        {
            q.remove_prefix(1);
            while (q[0] && q[0] != '\'')
            {
                value_tmp->Push(q[0]);
                if (!this->need_reconstruct && is_html_quote(q[0]))
                    this->need_reconstruct = true;
                q.remove_prefix(1);
            }
            if (q[0] == '\'')
                q.remove_prefix(1);
        }
        else if (q[0])
        {
            while (q[0] && !IS_SPACE(q[0]) && q[0] != '>')
            {
                value_tmp->Push(q[0]);
                if (!this->need_reconstruct && is_html_quote(q[0]))
                    this->need_reconstruct = true;
                q.remove_prefix(1);
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

    Str value = NULL;
    if (value_tmp)
    {
        int j, hidden = false;
        for (j = 0; j < i; j++)
        {
            if (this->attrid[j] == ATTR_TYPE &&
                strcmp("hidden", this->value[j]) == 0)
            {
                hidden = true;
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
            value = Strnew();
            for (auto x = value_tmp->ptr; *x; x++)
            {
                if (*x != '\n')
                    value->Push(*x);
            }
        }
    }

    if (i != nattr)
    {
        if (!internal &&
            ((AttrMAP[attr_id].flag & AFLG_INT) ||
             (value && AttrMAP[attr_id].vtype == VTYPE_METHOD &&
              !strcasecmp(value->ptr, "internal"))))
        {
            this->need_reconstruct = true;
            return {q, true};
        }
        this->attrid[i] = attr_id;
        if (value)
            this->value[i] = html_unquote(value->ptr, w3mApp::Instance().InnerCharset);
        else
            this->value[i] = NULL;
    }
    else
    {
        this->need_reconstruct = true;
    }
    return {q, true};
}

std::string_view HtmlTag::parse(std::string_view s, bool internal)
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
    auto q = svu::strip_left(s);
    while (true)
    {
        bool process;
        std::tie(q, process) = this->parse_attr(q, nattr, internal);
        if (!process)
        {
            break;
        }
    }

    while (q.size() && q[0] != '>')
        q.remove_prefix(1);

    if (q.size() && q[0] == '>')
        q.remove_prefix(1);

    return q;
}

bool HtmlTag::CanAcceptAttribute(HtmlTagAttributes id) const
{
    return (this->map && this->map[id] != MAX_TAGATTR);
}

bool HtmlTag::HasAttribute(HtmlTagAttributes id) const
{
    return CanAcceptAttribute(id) && (this->attrid[this->map[id]] != ATTR_UNKNOWN);
}

bool HtmlTag::SetAttributeValue(HtmlTagAttributes id, const char *value)
{
    int i;

    if (!this->CanAcceptAttribute(id))
        return 0;

    i = this->map[id];
    this->attrid[i] = id;
    if (value)
        this->value[i] = allocStr(value, -1);
    else
        this->value[i] = NULL;
    this->need_reconstruct = true;
    return 1;
}

bool HtmlTag::TryGetAttributeValue(HtmlTagAttributes id, void *value) const
{
    if (!HasAttribute(id))
    {
        return false;
    }
    if (!map)
    {
        return false;
    }
    int i = map[id];
    auto v = this->value[i];
    if (!v)
    {
        return 0;
    }
    return toValFunc[AttrMAP[id].vtype](v, (int *)value);
}

Str HtmlTag::ToStr() const
{
    int i;
    int tag_id = this->tagid;
    int nattr = TagMAP[tag_id].max_attribute;
    Str tagstr = Strnew();
    tagstr->Push('<');
    tagstr->Push(TagMAP[tag_id].name);
    for (i = 0; i < nattr; i++)
    {
        if (this->attrid[i] != ATTR_UNKNOWN)
        {
            tagstr->Push(' ');
            tagstr->Push(AttrMAP[this->attrid[i]].name);
            if (this->value[i])
                tagstr->Push(Sprintf("=\"%s\"", html_quote(this->value[i])));
        }
    }
    tagstr->Push('>');
    return tagstr;
}

std::tuple<std::string_view, HtmlTagPtr> parse_tag(std::string_view s, bool internal)
{
    /* Parse tag name */
    auto q = s.substr(1); // (*s) + 1;
    char tagname[MAX_TAG_LEN];
    auto p = tagname;
    if (q.size() && q[0] == '/')
    {
        *(p++) = q[0];
        q.remove_prefix(1);
        q = svu::strip_left(q);
    }
    while (q.size() && !IS_SPACE(q[0]) && !(tagname[0] != '/' && q[0] == '/') &&
           q[0] != '>' && p - tagname < MAX_TAG_LEN - 1)
    {
        *(p++) = TOLOWER(q[0]);
        q.remove_prefix(1);
    }
    *p = '\0';
    while (q[0] && !IS_SPACE(q[0]) && !(tagname[0] != '/' && q[0] == '/') &&
           q[0] != '>')
        q.remove_prefix(1);

    auto tag_id = GetTag(tagname, HTML_UNKNOWN);
    if (tag_id == HTML_UNKNOWN)
    {
        // tag not found
        return {s, nullptr};
    }
    if (TagMAP[tag_id].flag & TFLG_INT && !internal)
    {
        return {s, nullptr};
    }

    auto tag = new HtmlTag(tag_id);
    q = tag->parse(q, internal);
    return {q, tag};
}
