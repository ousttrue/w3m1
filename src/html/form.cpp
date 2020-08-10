/* 
 * HTML forms
 */
#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "html/form.h"
#include "file.h"
#include "public.h"
#include "html/parsetag.h"
#include "rc.h"
#include "http/cookie.h"
#include "html/html.h"
#include "myctype.h"
#include "transport/local.h"
#include "regex.h"
#include "frontend/menu.h"
#include "mime/mimetypes.h"
#include "frontend/display.h"
#include "html/anchor.h"
#include "charset.h"
extern Str *textarea_str;
extern FormSelectOption *select_option;

/* *INDENT-OFF* */
struct
{
    char *action;
    void (*rout)(struct parsed_tagarg *);
} internal_action[] = {
    {"map", follow_map},
    {"option", panel_set_option},
#ifdef USE_COOKIE
    {"cookie", set_cookie_flag},
#endif /* USE_COOKIE */
    {"download", download_action},
#ifdef USE_M17N
    {"charset", change_charset},
#endif
    {"none", NULL},
    {NULL, NULL},
};
/* *INDENT-ON* */

FormList *
newFormList(const char *action, const char *method, const char *charset, const char *enctype,
            const char *target, const char *name, FormList *_next)
{
    FormList *l;
    Str a = Strnew(action ? action : "");
    FormMethodTypes m = FORM_METHOD_GET;
    int e = FORM_ENCTYPE_URLENCODED;
    CharacterEncodingScheme c = WC_CES_NONE;

    if (method == NULL || !strcasecmp(method, "get"))
        m = FORM_METHOD_GET;
    else if (!strcasecmp(method, "post"))
        m = FORM_METHOD_POST;
    else if (!strcasecmp(method, "internal"))
        m = FORM_METHOD_INTERNAL;
    /* unknown method is regarded as 'get' */

    if (enctype != NULL && !strcasecmp(enctype, "multipart/form-data"))
    {
        e = FORM_ENCTYPE_MULTIPART;
        if (m == FORM_METHOD_GET)
            m = FORM_METHOD_POST;
    }

    if (charset != NULL)
        c = wc_guess_charset(charset, WC_CES_NONE);

    l = New(FormList);
    l->item = l->lastitem = NULL;
    l->action = a;
    l->method = m;
    l->charset = c;
    l->enctype = e;
    l->target = target;
    l->name = name;
    l->next = _next;
    l->nitems = 0;
    l->body = NULL;
    l->length = 0;
    return l;
}

/* 
 * add <input> element to form_list
 */
FormItemList *
formList_addInput(FormList *fl, struct parsed_tag *tag)
{
    FormItemList *item;
    char *p;
    int i;

    /* if not in <form>..</form> environment, just ignore <input> tag */
    if (fl == NULL)
        return NULL;

    item = New(FormItemList);
    item->type = FORM_UNKNOWN;
    item->size = -1;
    item->rows = 0;
    item->checked = item->init_checked = 0;
    item->accept = 0;
    item->name = NULL;
    item->value = item->init_value = NULL;
    item->readonly = 0;
    if (tag->TryGetAttributeValue(ATTR_TYPE, &p))
    {
        item->type = formtype(p);
        if (item->size < 0 &&
            (item->type == FORM_INPUT_TEXT ||
             item->type == FORM_INPUT_FILE ||
             item->type == FORM_INPUT_PASSWORD))
            item->size = FORM_I_TEXT_DEFAULT_SIZE;
    }
    if (tag->TryGetAttributeValue(ATTR_NAME, &p))
        item->name = Strnew(p);
    if (tag->TryGetAttributeValue(ATTR_VALUE, &p))
        item->value = item->init_value = Strnew(p);
    item->checked = item->init_checked = tag->HasAttribute(ATTR_CHECKED);
    item->accept = tag->HasAttribute(ATTR_ACCEPT);
    tag->TryGetAttributeValue(ATTR_SIZE, &item->size);
    tag->TryGetAttributeValue(ATTR_MAXLENGTH, &item->maxlength);
    item->readonly = tag->HasAttribute(ATTR_READONLY);
    if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &i))
        item->value = item->init_value = textarea_str[i];
#ifdef MENU_SELECT
    if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &i))
        item->select_option = select_option[i].first;
#endif /* MENU_SELECT */
    if (tag->TryGetAttributeValue(ATTR_ROWS, &p))
        item->rows = atoi(p);
    if (item->type == FORM_UNKNOWN)
    {
        /* type attribute is missing. Ignore the tag. */
        return NULL;
    }
#ifdef MENU_SELECT
    if (item->type == FORM_SELECT)
    {
        chooseSelectOption(item, item->select_option);
        item->init_selected = item->selected;
        item->init_value = item->value;
        item->init_label = item->label;
    }
#endif /* MENU_SELECT */
    if (item->type == FORM_INPUT_FILE && item->value && item->value->Size())
    {
        /* security hole ! */
        return NULL;
    }
    item->parent = fl;
    item->next = NULL;
    if (fl->item == NULL)
    {
        fl->item = fl->lastitem = item;
    }
    else
    {
        fl->lastitem->next = item;
        fl->lastitem = item;
    }
    if (item->type == FORM_INPUT_HIDDEN)
        return NULL;
    fl->nitems++;
    return item;
}

static const char *_formtypetbl[] = {
    "text", "password", "checkbox", "radio", "submit", "reset", "hidden",
    "image", "select", "textarea", "button", "file", NULL};

static const char *_formmethodtbl[] = {
    "GET", "POST", "INTERNAL", "HEAD"};

char *
form2str(FormItemList *fi)
{
    Str tmp = Strnew();

    if (fi->type != FORM_SELECT && fi->type != FORM_TEXTAREA)
        tmp->Push("input type=");
    tmp->Push(_formtypetbl[fi->type]);
    if (fi->name && fi->name->Size())
        Strcat_m_charp(tmp, " name=\"", fi->name->ptr, "\"", NULL);
    if ((fi->type == FORM_INPUT_RADIO || fi->type == FORM_INPUT_CHECKBOX ||
         fi->type == FORM_SELECT) &&
        fi->value)
        Strcat_m_charp(tmp, " value=\"", fi->value->ptr, "\"", NULL);
    Strcat_m_charp(tmp, " (", _formmethodtbl[fi->parent->method], " ",
                   fi->parent->action->ptr, ")", NULL);
    return tmp->ptr;
}

int formtype(const char *typestr)
{
    int i;
    for (i = 0; _formtypetbl[i]; i++)
    {
        if (!strcasecmp(typestr, _formtypetbl[i]))
            return i;
    }
    return FORM_UNKNOWN;
}

void formRecheckRadio(const Anchor *a, BufferPtr buf, FormItemList *fi)
{
    int i;
    Anchor *a2;
    FormItemList *f2;

    for (i = 0; i < buf->formitem.size(); i++)
    {
        a2 = &buf->formitem.anchors[i];
        f2 = a2->item;
        if (f2->parent == fi->parent && f2 != fi &&
            f2->type == FORM_INPUT_RADIO && f2->name->Cmp(fi->name) == 0)
        {
            f2->checked = 0;
            formUpdateBuffer(a2, buf, f2);
        }
    }
    fi->checked = 1;
    formUpdateBuffer(a, buf, fi);
}

void formResetBuffer(BufferPtr buf, AnchorList &formitem)
{
    int i;
    Anchor *a;
    FormItemList *f1, *f2;

    if (buf == NULL || !buf->formitem || !formitem)
        return;
    for (i = 0; i < buf->formitem.size() && i < formitem.size(); i++)
    {
        a = &buf->formitem.anchors[i];
        if (a->y != a->start.line)
            continue;
        f1 = a->item;
        f2 = formitem.anchors[i].item;
        if (f1->type != f2->type ||
            strcmp(((f1->name == NULL) ? "" : f1->name->ptr),
                   ((f2->name == NULL) ? "" : f2->name->ptr)))
            break; /* What's happening */
        switch (f1->type)
        {
        case FORM_INPUT_TEXT:
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_FILE:
        case FORM_TEXTAREA:
            f1->value = f2->value;
            f1->init_value = f2->init_value;
            break;
        case FORM_INPUT_CHECKBOX:
        case FORM_INPUT_RADIO:
            f1->checked = f2->checked;
            f1->init_checked = f2->init_checked;
            break;
        case FORM_SELECT:
#ifdef MENU_SELECT
            f1->select_option = f2->select_option;
            f1->value = f2->value;
            f1->label = f2->label;
            f1->selected = f2->selected;
            f1->init_value = f2->init_value;
            f1->init_label = f2->init_label;
            f1->init_selected = f2->init_selected;
#endif /* MENU_SELECT */
            break;
        default:
            continue;
        }
        formUpdateBuffer(a, buf, f1);
    }
}

static int form_update_line(LinePtr line, char **str, int spos, int epos, int width, int newline, int password)
{
    int c_len = 1, c_width = 1, w, i, len, pos;
    char *p, *buf;
    Lineprop c_type, effect, *prop;

    for (p = *str, w = 0, pos = 0; *p && w < width;)
    {
        c_type = get_mctype(*p);
        c_len = get_mclen(p);
        c_width = get_mcwidth(p);

        if (c_type == PC_CTRL)
        {
            if (newline && *p == '\n')
                break;
            if (*p != '\r')
            {
                w++;
                pos++;
            }
        }
        else if (password)
        {
#ifdef USE_M17N
            if (w + c_width > width)
                break;
#endif
            w += c_width;
            pos += c_width;
#ifdef USE_M17N
        }
        else if (c_type & PC_UNKNOWN)
        {
            w++;
            pos++;
        }
        else
        {
            if (w + c_width > width)
                break;
#endif
            w += c_width;
            pos += c_len;
        }
        p += c_len;
    }
    pos += width - w;

    len = line->len() + pos + spos - epos;
    buf = New_N(char, len);
    prop = New_N(Lineprop, len);
    bcopy((void *)line->lineBuf(), (void *)buf, spos * sizeof(char));
    bcopy((void *)line->propBuf(), (void *)prop, spos * sizeof(Lineprop));

    effect = CharEffect(line->propBuf()[spos]);
    for (p = *str, w = 0, pos = spos; *p && w < width;)
    {
        c_type = get_mctype(*p);
        c_len = get_mclen(p);
        c_width = get_mcwidth(p);

        if (c_type == PC_CTRL)
        {
            if (newline && *p == '\n')
                break;
            if (*p != '\r')
            {
                buf[pos] = password ? '*' : ' ';
                prop[pos] = effect | PC_ASCII;
                pos++;
                w++;
            }
        }
        else if (password)
        {
#ifdef USE_M17N
            if (w + c_width > width)
                break;
#endif
            for (i = 0; i < c_width; i++)
            {
                buf[pos] = '*';
                prop[pos] = effect | PC_ASCII;
                pos++;
                w++;
            }
#ifdef USE_M17N
        }
        else if (c_type & PC_UNKNOWN)
        {
            buf[pos] = ' ';
            prop[pos] = effect | PC_ASCII;
            pos++;
            w++;
        }
        else
        {
            if (w + c_width > width)
                break;
#else
        }
        else
        {
#endif
            buf[pos] = *p;
            prop[pos] = effect | c_type;
            pos++;
#ifdef USE_M17N
            c_type = (c_type & ~PC_WCHAR1) | PC_WCHAR2;
            for (i = 1; i < c_len; i++)
            {
                buf[pos] = p[i];
                prop[pos] = effect | c_type;
                pos++;
            }
#endif
            w += c_width;
        }
        p += c_len;
    }
    for (; w < width; w++)
    {
        buf[pos] = ' ';
        prop[pos] = effect | PC_ASCII;
        pos++;
    }
    if (newline)
    {
        if (!FoldTextarea)
        {
            while (*p && *p != '\r' && *p != '\n')
                p++;
        }
        if (*p == '\r')
            p++;
        if (*p == '\n')
            p++;
    }
    *str = p;

    bcopy((void *)&line->lineBuf()[epos], (void *)&buf[pos],
          (line->len() - epos) * sizeof(char));
    bcopy((void *)&line->propBuf()[epos], (void *)&prop[pos],
          (line->len() - epos) * sizeof(Lineprop));

    line->buffer = {buf, prop, len};

    return pos;
}

void formUpdateBuffer(const Anchor *a, BufferPtr buf, FormItemList *form)
{
    char *p;
    int spos, epos, rows, c_rows, pos, col = 0;
    LinePtr l;

    auto save = buf->Copy();
    buf->GotoLine(a->start.line);
    switch (form->type)
    {
    case FORM_TEXTAREA:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_CHECKBOX:
    case FORM_INPUT_RADIO:
    case FORM_SELECT:
        spos = a->start.pos;
        epos = a->end.pos;
        break;

    default:
        spos = a->start.pos + 1;
        epos = a->end.pos - 1;
    }
    switch (form->type)
    {
    case FORM_INPUT_CHECKBOX:
    case FORM_INPUT_RADIO:
        if (form->checked)
            buf->CurrentLine()->lineBuf()[spos] = '*';
        else
            buf->CurrentLine()->lineBuf()[spos] = ' ';
        break;
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_PASSWORD:
    case FORM_TEXTAREA:
    case FORM_SELECT:
        if (form->type == FORM_SELECT)
        {
            p = form->label->ptr;
            updateSelectOption(form, form->select_option);
        }
        else
            p = form->value->ptr;
        l = buf->CurrentLine();
        if (form->type == FORM_TEXTAREA)
        {
            int n = a->y - buf->CurrentLine()->linenumber;
            if (n > 0)
                for (; l && n; l = buf->PrevLine(l), n--)
                    ;
            else if (n < 0)
                for (; l && n; l = buf->PrevLine(l), n++)
                    ;
            if (!l)
                break;
        }
        rows = form->rows ? form->rows : 1;
        col = l->COLPOS(a->start.pos);
        for (c_rows = 0; c_rows < rows; c_rows++, l = buf->NextLine(l))
        {
            if (rows > 1)
            {
                pos = columnPos(l, col);
                auto a = buf->formitem.RetrieveAnchor(l->linenumber, pos);
                if (a == NULL)
                    break;
                spos = a->start.pos;
                epos = a->end.pos;
            }
            pos = form_update_line(l, &p, spos, epos, l->COLPOS(epos) - col,
                                   rows > 1,
                                   form->type == FORM_INPUT_PASSWORD);
            if (pos != epos)
            {
                auto bp = BufferPoint{
                    line : a->start.line,
                    pos : spos,
                };
                buf->shiftAnchorPosition(buf->href, bp, pos - epos);
                buf->shiftAnchorPosition(buf->name, bp, pos - epos);
                buf->shiftAnchorPosition(buf->img, bp, pos - epos);
                buf->shiftAnchorPosition(buf->formitem, bp, pos - epos);
            }
        }
        break;
    }
    buf->CopyFrom(save);
    buf->ArrangeLine();
}

Str textfieldrep(Str s, int width)
{
    Lineprop c_type;
    Str n = Strnew_size(width + 2);
    int i, j, k, c_len;

    j = 0;
    for (i = 0; i < s->Size(); i += c_len)
    {
        c_type = get_mctype(s->ptr[i]);
        c_len = get_mclen(&s->ptr[i]);
        if (s->ptr[i] == '\r')
            continue;
        k = j + get_mcwidth(&s->ptr[i]);
        if (k > width)
            break;
        if (c_type == PC_CTRL)
            n->Push(' ');
#ifdef USE_M17N
        else if (c_type & PC_UNKNOWN)
            n->Push(' ');
#endif
        else if (s->ptr[i] == '&')
            n->Push("&amp;");
        else if (s->ptr[i] == '<')
            n->Push("&lt;");
        else if (s->ptr[i] == '>')
            n->Push("&gt;");
        else
            n->Push(&s->ptr[i], c_len);
        j = k;
    }
    for (; j < width; j++)
        n->Push(' ');
    return n;
}

static void
form_fputs_decode(Str s, FILE *f)
{
    char *p;
    Str z = Strnew();

    for (p = s->ptr; *p;)
    {
        switch (*p)
        {
        default:
            z->Push(*p);
            p++;
            break;
        }
    }
#ifdef USE_M17N
    z = wc_Str_conv_strict(z, w3mApp::Instance().InnerCharset, w3mApp::Instance().DisplayCharset);
#endif
    z->Puts(f);
}

void input_textarea(FormItemList *fi)
{
    char *tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    Str tmp;
    FILE *f;

    CharacterEncodingScheme charset = w3mApp::Instance().DisplayCharset;
    uint8_t auto_detect;

    f = fopen(tmpf, "w");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't open temporary file", FALSE);
        return;
    }
    if (fi->value)
        form_fputs_decode(fi->value, f);
    fclose(f);

    fmTerm();
    system(myEditor(Editor, tmpf, 1)->ptr);
    fmInit();

    if (fi->readonly)
        goto input_end;
    f = fopen(tmpf, "r");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't open temporary file", FALSE);
        goto input_end;
    }
    fi->value = Strnew();

    auto_detect = WcOption.auto_detect;
    WcOption.auto_detect = WC_OPT_DETECT_ON;

    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        if (tmp->Size() == 1 && tmp->ptr[tmp->Size() - 1] == '\n')
        {
            /* null line with bare LF */
            tmp = Strnew("\r\n");
        }
        else if (tmp->Size() > 1 && tmp->ptr[tmp->Size() - 1] == '\n' &&
                 tmp->ptr[tmp->Size() - 2] != '\r')
        {
            tmp->Pop(1);
            tmp->Push("\r\n");
        }
        tmp = convertLine(NULL, tmp, RAW_MODE, &charset, w3mApp::Instance().DisplayCharset);
        fi->value->Push(tmp);
    }

    WcOption.auto_detect = (AutoDetectTypes)auto_detect;

    fclose(f);
input_end:
    unlink(tmpf);
}

void do_internal(char *action, char *data)
{
    int i;

    for (i = 0; internal_action[i].action; i++)
    {
        if (strcasecmp(internal_action[i].action, action) == 0)
        {
            if (internal_action[i].rout)
                internal_action[i].rout(cgistr2tagarg(data));
            return;
        }
    }
}

#ifdef MENU_SELECT
void addSelectOption(FormSelectOption *fso, Str value, Str label, int chk)
{
    FormSelectOptionItem *o;
    o = New(FormSelectOptionItem);
    if (value == NULL)
        value = label;
    o->value = value;
    Strip(label);
    o->label = label;
    o->checked = chk;
    o->next = NULL;
    if (fso->first == NULL)
        fso->first = fso->last = o;
    else
    {
        fso->last->next = o;
        fso->last = o;
    }
}

void chooseSelectOption(FormItemList *fi, FormSelectOptionItem *item)
{
    FormSelectOptionItem *opt;
    int i;

    fi->selected = 0;
    if (item == NULL)
    {
        fi->value = Strnew_size(0);
        fi->label = Strnew_size(0);
        return;
    }
    fi->value = item->value;
    fi->label = item->label;
    for (i = 0, opt = item; opt != NULL; i++, opt = opt->next)
    {
        if (opt->checked)
        {
            fi->value = opt->value;
            fi->label = opt->label;
            fi->selected = i;
            break;
        }
    }
    updateSelectOption(fi, item);
}

void updateSelectOption(FormItemList *fi, FormSelectOptionItem *item)
{
    int i;

    if (fi == NULL || item == NULL)
        return;
    for (i = 0; item != NULL; i++, item = item->next)
    {
        if (i == fi->selected)
            item->checked = TRUE;
        else
            item->checked = FALSE;
    }
}

int formChooseOptionByMenu(FormItemList *fi, int x, int y)
{
    int i, n, selected = -1, init_select = fi->selected;
    FormSelectOptionItem *opt;
    char **label;

    for (n = 0, opt = fi->select_option; opt != NULL; n++, opt = opt->next)
        ;
    label = New_N(char *, n + 1);
    for (i = 0, opt = fi->select_option; opt != NULL; i++, opt = opt->next)
        label[i] = opt->label->ptr;
    label[n] = NULL;

    optionMenu(x, y, label, &selected, init_select, NULL);

    if (selected < 0)
        return 0;
    for (i = 0, opt = fi->select_option; opt != NULL; i++, opt = opt->next)
    {
        if (i == selected)
        {
            fi->selected = selected;
            fi->value = opt->value;
            fi->label = opt->label;
            break;
        }
    }
    updateSelectOption(fi, fi->select_option);
    return 1;
}
#endif /* MENU_SELECT */

void form_write_data(FILE *f, char *boundary, char *name, char *value)
{
    fprintf(f, "--%s\r\n", boundary);
    fprintf(f, "Content-Disposition: form-data; name=\"%s\"\r\n\r\n", name);
    fprintf(f, "%s\r\n", value);
}

void form_write_from_file(FILE *f, char *boundary, char *name, char *filename,
                          char *file)
{
    FILE *fd;
    struct stat st;
    int c;

    fprintf(f, "--%s\r\n", boundary);
    fprintf(f,
            "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n",
            name, mybasename(filename));
    auto type = guessContentType(file);
    fprintf(f, "Content-Type: %s\r\n\r\n",
            type ? type : "application/octet-stream");

    if (lstat(file, &st) < 0)
        goto write_end;
    if (S_ISDIR(st.st_mode))
        goto write_end;
    fd = fopen(file, "r");
    if (fd != NULL)
    {
        while ((c = fgetc(fd)) != EOF)
            fputc(c, f);
        fclose(fd);
    }
write_end:
    fprintf(f, "\r\n");
}

struct pre_form_item
{
    int type;
    char *name;
    char *value;
    int checked;
    struct pre_form_item *next;
};

struct pre_form
{
    char *url;
    Regex *re_url;
    char *name;
    char *action;
    struct pre_form_item *item;
    struct pre_form *next;
};

static struct pre_form *PreForm = NULL;

static struct pre_form *
add_pre_form(struct pre_form *prev, char *url, char *name, char *action)
{
    URL pu;
    struct pre_form *newForm;

    if (prev)
        newForm = prev->next = New(struct pre_form);
    else
        newForm = PreForm = New(struct pre_form);
    if (url && *url == '/')
    {
        int l = strlen(url);
        if (l > 1 && url[l - 1] == '/')
            newForm->url = allocStr(url + 1, l - 2);
        else
            newForm->url = url + 1;
        newForm->re_url = newRegex(newForm->url, FALSE, NULL, NULL);
        if (!newForm->re_url)
            newForm->url = NULL;
    }
    else if (url)
    {
        pu.Parse2(url, NULL);
        newForm->url = pu.ToStr()->ptr;
        newForm->re_url = NULL;
    }
    newForm->name = (name && *name) ? name : NULL;
    newForm->action = (action && *action) ? action : NULL;
    newForm->item = NULL;
    newForm->next = NULL;
    return newForm;
}

static struct pre_form_item *
add_pre_form_item(struct pre_form *pf, struct pre_form_item *prev, int type,
                  char *name, char *value, char *checked)
{
    struct pre_form_item *newForm;

    if (!pf)
        return NULL;
    if (prev)
        newForm = prev->next = New(struct pre_form_item);
    else
        newForm = pf->item = New(struct pre_form_item);
    newForm->type = type;
    newForm->name = name;
    newForm->value = value;
    if (checked && *checked && (!strcmp(checked, "0") || strcasecmp(checked, "off") || !strcasecmp(checked, "no")))
        newForm->checked = 0;
    else
        newForm->checked = 1;
    newForm->next = NULL;
    return newForm;
}

/*
 * url <url>|/<re-url>/
 * form [<name>] <action>
 * text <name> <value>
 * file <name> <value>
 * passwd <name> <value>
 * checkbox <name> <value> [<checked>]
 * radio <name> <value>
 * select <name> <value>
 * submit [<name> [<value>]]
 * image [<name> [<value>]]
 * textarea <name>
 * <value>
 * /textarea
 */

void loadPreForm(void)
{
    FILE *fp;
    Str line = NULL, textarea = NULL;
    struct pre_form *pf = NULL;
    struct pre_form_item *pi = NULL;
    int type = -1;
    char *name = NULL;

    PreForm = NULL;
    fp = openSecretFile(pre_form_file);
    if (fp == NULL)
        return;
    while (1)
    {
        char *p, *s, *arg;

        line = Strfgets(fp);
        if (line->Size() == 0)
            break;
        if (textarea && !(!strncmp(line->ptr, "/textarea", 9) &&
                          IS_SPACE(line->ptr[9])))
        {
            textarea->Push(line);
            continue;
        }
        Strip(line);
        p = line->ptr;
        if (*p == '#' || *p == '\0')
            continue; /* comment or empty line */
        s = getWord(&p);
        arg = getWord(&p);

        if (!strcmp(s, "url"))
        {
            if (!arg || !*arg)
                continue;
            p = getQWord(&p);
            pf = add_pre_form(pf, arg, NULL, p);
            pi = pf->item;
            continue;
        }
        if (!pf)
            continue;
        if (!strcmp(s, "form"))
        {
            if (!arg || !*arg)
                continue;
            s = getQWord(&p);
            p = getQWord(&p);
            if (!p || !*p)
            {
                p = s;
                s = NULL;
            }
            if (pf->item)
            {
                struct pre_form *prev = pf;
                pf = add_pre_form(prev, "", s, p);
                /* copy previous URL */
                pf->url = prev->url;
                pf->re_url = prev->re_url;
            }
            else
            {
                pf->name = s;
                pf->action = (p && *p) ? p : NULL;
            }
            pi = pf->item;
            continue;
        }
        if (!strcmp(s, "text"))
            type = FORM_INPUT_TEXT;
        else if (!strcmp(s, "file"))
            type = FORM_INPUT_FILE;
        else if (!strcmp(s, "passwd") || !strcmp(s, "password"))
            type = FORM_INPUT_PASSWORD;
        else if (!strcmp(s, "checkbox"))
            type = FORM_INPUT_CHECKBOX;
        else if (!strcmp(s, "radio"))
            type = FORM_INPUT_RADIO;
        else if (!strcmp(s, "submit"))
            type = FORM_INPUT_SUBMIT;
        else if (!strcmp(s, "image"))
            type = FORM_INPUT_IMAGE;
        else if (!strcmp(s, "select"))
            type = FORM_SELECT;
        else if (!strcmp(s, "textarea"))
        {
            type = FORM_TEXTAREA;
            name = Strnew(arg)->ptr;
            textarea = Strnew();
            continue;
        }
        else if (textarea && name && !strcmp(s, "/textarea"))
        {
            pi = add_pre_form_item(pf, pi, type, name, textarea->ptr, NULL);
            textarea = NULL;
            name = NULL;
            continue;
        }
        else
            continue;
        s = getQWord(&p);
        pi = add_pre_form_item(pf, pi, type, arg, s, getQWord(&p));
    }
    fclose(fp);
}

void preFormUpdateBuffer(BufferPtr buf)
{
    struct pre_form *pf;
    struct pre_form_item *pi;
    int i;
    Anchor *a;
    FormList *fl;
    FormItemList *fi;
#ifdef MENU_SELECT
    FormSelectOptionItem *opt;
    int j;
#endif

    if (!buf || !buf->formitem || !PreForm)
        return;

    for (pf = PreForm; pf; pf = pf->next)
    {
        if (pf->re_url)
        {
            Str url = buf->currentURL.ToStr();
            if (!RegexMatch(pf->re_url, url->ptr, url->Size(), 1))
                continue;
        }
        else if (pf->url)
        {
            if (buf->currentURL.ToStr()->Cmp(pf->url) != 0)
                continue;
        }
        else
            continue;
        for (i = 0; i < buf->formitem.size(); i++)
        {
            a = &buf->formitem.anchors[i];
            fi = a->item;
            fl = fi->parent;
            if (pf->name && (!fl->name || strcmp(fl->name, pf->name)))
                continue;
            if (pf->action && (!fl->action || fl->action->Cmp(pf->action) != 0))
                continue;
            for (pi = pf->item; pi; pi = pi->next)
            {
                if (pi->type != fi->type)
                    continue;
                if (pi->type == FORM_INPUT_SUBMIT ||
                    pi->type == FORM_INPUT_IMAGE)
                {
                    if ((!pi->name || !*pi->name ||
                         (fi->name && fi->name->Cmp(pi->name) == 0)) &&
                        (!pi->value || !*pi->value ||
                         (fi->value && fi->value->Cmp(pi->value) == 0)))
                        buf->submit = a;
                    continue;
                }
                if (!pi->name || !fi->name || fi->name->Cmp(pi->name) != 0)
                    continue;
                switch (pi->type)
                {
                case FORM_INPUT_TEXT:
                case FORM_INPUT_FILE:
                case FORM_INPUT_PASSWORD:
                case FORM_TEXTAREA:
                    fi->value = Strnew(pi->value);
                    formUpdateBuffer(a, buf, fi);
                    break;
                case FORM_INPUT_CHECKBOX:
                    if (pi->value && fi->value &&
                        fi->value->Cmp(pi->value) == 0)
                    {
                        fi->checked = pi->checked;
                        formUpdateBuffer(a, buf, fi);
                    }
                    break;
                case FORM_INPUT_RADIO:
                    if (pi->value && fi->value &&
                        fi->value->Cmp(pi->value) == 0)
                        formRecheckRadio(a, buf, fi);
                    break;
#ifdef MENU_SELECT
                case FORM_SELECT:
                    for (j = 0, opt = fi->select_option; opt != NULL;
                         j++, opt = opt->next)
                    {
                        if (pi->value && opt->value &&
                            opt->value->Cmp(pi->value) == 0)
                        {
                            fi->selected = j;
                            fi->value = opt->value;
                            fi->label = opt->label;
                            updateSelectOption(fi, fi->select_option);
                            formUpdateBuffer(a, buf, fi);
                            break;
                        }
                    }
                    break;
#endif
                }
            }
        }
    }
}
