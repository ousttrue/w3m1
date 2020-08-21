/* 
 * HTML forms
 */
#include <string_view_util.h>
#include "indep.h"
#include "gc_helper.h"
#include "html/form.h"
#include "file.h"
#include "public.h"
#include "html/parsetag.h"
#include "rc.h"
#include "stream/cookie.h"
#include "html/html.h"
#include "myctype.h"
#include "stream/local_cgi.h"
#include "regex.h"
#include "frontend/menu.h"
#include "mime/mimetypes.h"
#include "frontend/display.h"
#include "frontend/tabbar.h"
#include "html/anchor.h"
#include "charset.h"
#include "history.h"
#include "html/html_processor.h"
#include "html/html_context.h"
#include "html/maparea.h"
#include <unistd.h>

#define FORM_I_TEXT_DEFAULT_SIZE 40
#define FORM_I_SELECT_DEFAULT_SIZE 40
#define FORM_I_TEXTAREA_DEFAULT_WIDTH 40

extern FormSelectOptionList *select_option;

static void follow_map(struct parsed_tagarg *arg)
{
    auto name = tag_get_value(arg, "link");

    auto tab = GetCurrentTab();
    auto buf = tab->GetCurrentBuffer();

    auto an = buf->img.RetrieveAnchor(buf->CurrentPoint());
    auto [x, y] = buf->rect.globalXY();
    auto a = follow_map_menu(GetCurrentTab()->GetCurrentBuffer(), name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0')
    {

#ifndef MENU_MAP
        BufferPtr buf = follow_map_panel(GetCurrentTab()->GetCurrentBuffer(), name);

        if (buf != NULL)
            cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
#endif
#if defined(MENU_MAP) || defined(USE_IMAGE)
        return;
    }
    if (*(a->url) == '#')
    {
        gotoLabel(a->url + 1);
        return;
    }

    auto p_url = URL::Parse(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
    pushHashHist(w3mApp::Instance().URLHist, p_url.ToStr()->ptr);
    if (check_target() && w3mApp::Instance().open_tab_blank && a->target &&
        (!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank")))
    {
        auto tab = CreateTabSetCurrent();
        BufferPtr buf = tab->GetCurrentBuffer();
        cmd_loadURL(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL(),
                    HttpReferrerPolicy::StrictOriginWhenCrossOrigin, NULL);
        // if (buf != GetCurrentTab()->GetCurrentBuffer())
        //     GetCurrentTab()->DeleteBuffer(buf);
        // else
        //     deleteTab(GetCurrentTab());
        displayCurrentbuf(B_FORCE_REDRAW);
        return;
    }
    cmd_loadURL(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL(),
                HttpReferrerPolicy::StrictOriginWhenCrossOrigin, NULL);
#endif
}

static void change_charset(struct parsed_tagarg *arg)
{
    BufferPtr buf = GetCurrentTab()->GetCurrentBuffer()->linkBuffer[LB_N_INFO];
    CharacterEncodingScheme charset;

    if (buf == NULL)
        return;
    auto tab = GetCurrentTab();
    tab->Back(true);
    // tab->Push(buf);
    if (GetCurrentTab()->GetCurrentBuffer()->bufferprop & BP_INTERNAL)
        return;
    charset = GetCurrentTab()->GetCurrentBuffer()->document_charset;
    for (; arg; arg = arg->next)
    {
        if (!strcmp(arg->arg, "charset"))
            charset = (CharacterEncodingScheme)atoi(arg->value);
    }
    _docCSet(charset);
}

/* *INDENT-OFF* */
struct
{
    const char *action;
    void (*rout)(struct parsed_tagarg *);
} internal_action[] = {
    {"map", follow_map},
    {"option", panel_set_option},
    {"cookie", set_cookie_flag},
    {"download", download_action},
    {"charset", change_charset},
    {"none", NULL},
    {NULL, NULL},
};
/* *INDENT-ON* */

FormList *FormList::Create(
    std::string_view action,
    std::string_view method,
    std::string_view charset,
    std::string_view enctype,
    std::string_view target,
    std::string_view name)
{

    /* unknown method is regarded as 'get' */
    FormMethodTypes m = FORM_METHOD_GET;
    if (method.empty() || svu::ic_eq(method, "get"))
        m = FORM_METHOD_GET;
    else if (svu::ic_eq(method, "post"))
        m = FORM_METHOD_POST;
    else if (svu::ic_eq(method, "internal"))
        m = FORM_METHOD_INTERNAL;

    int e = FORM_ENCTYPE_URLENCODED;
    if (enctype.size() && svu::ic_eq(enctype, "multipart/form-data"))
    {
        e = FORM_ENCTYPE_MULTIPART;
        if (m == FORM_METHOD_GET)
            m = FORM_METHOD_POST;
    }

    CharacterEncodingScheme c = WC_CES_NONE;
    if (charset.size())
        c = wc_guess_charset(charset.data(), WC_CES_NONE);

    auto l = new FormList(action, m);
    l->charset = c;
    l->enctype = e;
    l->target = target;
    l->name = name;
    return l;
}

/* 
 * add <input> element to form_list
 */
FormItemListPtr formList_addInput(FormList *fl, struct parsed_tag *tag, HtmlContext *context)
{
    /* if not in <form>..</form> environment, just ignore <input> tag */
    if (fl == NULL)
        return NULL;

    auto item = std::make_shared<FormItemList>();
    fl->items.push_back(item);
    item->parent = fl;

    item->type = FORM_UNKNOWN;
    item->size = -1;
    item->rows = 0;
    item->checked = item->init_checked = 0;
    item->accept = 0;
    item->name;
    item->value = item->init_value;
    item->readonly = 0;
    const char *p;
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
        item->name = p;
    if (tag->TryGetAttributeValue(ATTR_VALUE, &p))
        item->value = item->init_value = p;
    item->checked = item->init_checked = tag->HasAttribute(ATTR_CHECKED);
    item->accept = tag->HasAttribute(ATTR_ACCEPT);
    tag->TryGetAttributeValue(ATTR_SIZE, &item->size);
    tag->TryGetAttributeValue(ATTR_MAXLENGTH, &item->maxlength);
    item->readonly = tag->HasAttribute(ATTR_READONLY);
    int i;
    if (tag->TryGetAttributeValue(ATTR_TEXTAREANUMBER, &i))
        item->value = item->init_value = context->Textarea(i)->ptr;

    if (tag->TryGetAttributeValue(ATTR_SELECTNUMBER, &i))
        item->select_option = *context->FormSelect(i);

    if (tag->TryGetAttributeValue(ATTR_ROWS, &p))
        item->rows = atoi(p);
    if (item->type == FORM_UNKNOWN)
    {
        /* type attribute is missing. Ignore the tag. */
        return NULL;
    }

    if (item->type == FORM_SELECT)
    {
        chooseSelectOption(item, item->select_option);
        item->init_selected = item->selected;
        item->init_value = item->value;
        item->init_label = item->label;
    }

    if (item->type == FORM_INPUT_FILE && item->value.size())
    {
        /* security hole ! */
        return NULL;
    }
    if (item->type == FORM_INPUT_HIDDEN)
        return NULL;
    return item;
}

static const char *_formtypetbl[] = {
    "text", "password", "checkbox", "radio", "submit", "reset", "hidden",
    "image", "select", "textarea", "button", "file"};
FormItemTypes formtype(std::string_view typestr)
{
    int i = 0;
    for (auto &name : _formtypetbl)
    {
        if (svu::ic_eq(typestr, name))
        {
            return (FormItemTypes)i;
        }
        ++i;
    }
    return FORM_UNKNOWN;
}

static const char *_formmethodtbl[] = {
    "GET", "POST", "INTERNAL", "HEAD"};

char *
form2str(FormItemList *fi)
{
    Str tmp = Strnew();

    if (fi->type != FORM_SELECT && fi->type != FORM_TEXTAREA)
        tmp->Push("input type=");
    tmp->Push(_formtypetbl[fi->type]);
    if (fi->name.size())
        Strcat_m_charp(tmp, " name=\"", fi->name, "\"", NULL);
    if ((fi->type == FORM_INPUT_RADIO || fi->type == FORM_INPUT_CHECKBOX ||
         fi->type == FORM_SELECT) &&
        fi->value.size())
        Strcat_m_charp(tmp, " value=\"", fi->value, "\"", NULL);
    Strcat_m_charp(tmp, " (", _formmethodtbl[fi->parent->method], " ",
                   fi->parent->action, ")", NULL);
    return tmp->ptr;
}

void formRecheckRadio(const Anchor *a, BufferPtr buf, FormItemListPtr fi)
{
    for (int i = 0; i < buf->formitem.size(); i++)
    {
        auto a2 = &buf->formitem.anchors[i];
        auto f2 = a2->item;
        if (f2->parent == fi->parent && f2 != fi &&
            f2->type == FORM_INPUT_RADIO && f2->name == fi->name)
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
    FormItemListPtr f1, f2;

    if (buf == NULL || !buf->formitem || !formitem)
        return;
    for (i = 0; i < buf->formitem.size() && i < formitem.size(); i++)
    {
        a = &buf->formitem.anchors[i];
        if (a->y != a->start.line)
            continue;
        f1 = a->item;
        f2 = formitem.anchors[i].item;
        if (f1->type != f2->type || f1->name != f2->name)
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

static std::tuple<std::string_view, int> form_update_line(LinePtr line, std::string_view str, int spos, int epos, int width, int newline, int password)
{
    auto p = str.data();
    auto pos = 0;
    auto w = 0;
    for (; *p && w < width;)
    {
        auto c_type = get_mctype(*p);
        auto c_len = get_mclen(p);
        auto c_width = get_mcwidth(p);
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
            if (w + c_width > width)
                break;
            w += c_width;
            pos += c_width;
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

            w += c_width;
            pos += c_len;
        }
        p += c_len;
    }
    pos += width - w;

    auto len = line->len() + pos + spos - epos;

    auto copy = line->buffer;
    auto buf = const_cast<char *>(copy.lineBuf());
    auto prop = const_cast<Lineprop *>(copy.propBuf());

    auto effect = CharEffect(line->propBuf()[spos]);
    for (p = str.data(), w = 0, pos = spos; *p && w < width;)
    {
        auto c_type = get_mctype(*p);
        auto c_len = get_mclen(p);
        auto c_width = get_mcwidth(p);

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
            if (w + c_width > width)
                break;

            for (int i = 0; i < c_width; i++)
            {
                buf[pos] = '*';
                prop[pos] = effect | PC_ASCII;
                pos++;
                w++;
            }
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
            buf[pos] = *p;
            prop[pos] = effect | c_type;
            pos++;
            c_type = (c_type & ~PC_WCHAR1) | PC_WCHAR2;
            for (int i = 1; i < c_len; i++)
            {
                buf[pos] = p[i];
                prop[pos] = effect | c_type;
                pos++;
            }
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
        if (!w3mApp::Instance().FoldTextarea)
        {
            while (*p && *p != '\r' && *p != '\n')
                p++;
        }
        if (*p == '\r')
            p++;
        if (*p == '\n')
            p++;
    }
    // *str = p;
    std::string_view remain = str.substr(p - str.data());

    line->buffer = {buf, prop, len};

    return {remain, pos};
}

void formUpdateBuffer(const Anchor *a, BufferPtr buf, FormItemListPtr form)
{
    auto save = buf->Copy();
    buf->GotoLine(a->start.line);
    int spos;
    int epos;
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
    {
        std::string_view p;
        if (form->type == FORM_SELECT)
        {
            p = form->label;
            updateSelectOption(form, form->select_option);
        }
        else
        {
            p = form->value;
        }

        auto l = buf->CurrentLine();
        if (form->type == FORM_TEXTAREA)
        {
            int n = a->y - buf->CurrentLine()->linenumber;
            if (n > 0)
                for (; l && n; l = buf->PrevLine(l), n--)
                    ;
            else if (n < 0)
                for (; l && n; l = buf->PrevLine(l), n++)
                    if (!l)
                        break;
        }

        auto rows = form->rows ? form->rows : 1;
        auto col = l->COLPOS(a->start.pos);
        for (auto c_rows = 0; c_rows < rows; c_rows++, l = buf->NextLine(l))
        {
            if (rows > 1)
            {
                auto pos = columnPos(l, col);
                auto a = buf->formitem.RetrieveAnchor({l->linenumber, pos});
                if (a == NULL)
                    break;
                spos = a->start.pos;
                epos = a->end.pos;
            }

            {
                int pos;
                std::tie(p, pos) = form_update_line(l, p, spos, epos, l->COLPOS(epos) - col,
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
        }
        break;
    }
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
form_fputs_decode(std::string_view s, FILE *f)
{
    Str z = Strnew();
    for (auto p = s.begin(); p != s.end(); ++p)
    {
        z->Push(*p);
    }
    z = wc_Str_conv_strict(z, w3mApp::Instance().InnerCharset, w3mApp::Instance().DisplayCharset);
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
        disp_err_message("Can't open temporary file", false);
        return;
    }
    if (fi->value.size())
        form_fputs_decode(fi->value.c_str(), f);
    fclose(f);

    fmTerm();
    system(myEditor(w3mApp::Instance().Editor.c_str(), tmpf, 1)->ptr);
    fmInit();

    if (fi->readonly)
        goto input_end;
    f = fopen(tmpf, "r");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't open temporary file", false);
        goto input_end;
    }
    fi->value.clear();

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
        tmp = convertLine(SCM_UNKNOWN, tmp, RAW_MODE, &charset, w3mApp::Instance().DisplayCharset);
        for (auto p = tmp->ptr; *p; ++p)
        {
            fi->value.push_back(*p);
        }
    }

    WcOption.auto_detect = (AutoDetectTypes)auto_detect;

    fclose(f);
input_end:
    unlink(tmpf);
}

void do_internal(std::string_view action, std::string_view data)
{
    for (auto i = 0; internal_action[i].action; i++)
    {
        if (svu::ic_eq(internal_action[i].action, action))
        {
            if (internal_action[i].rout)
                internal_action[i].rout(cgistr2tagarg(data.data()));
            return;
        }
    }
}

void chooseSelectOption(FormItemListPtr fi, tcb::span<FormSelectOptionItem> item)
{
    fi->selected = 0;
    if (item.empty())
    {
        fi->value.clear();
        fi->label.clear();
        return;
    }
    fi->value = item.front().value;
    fi->label = item.front().label;

    int i = 0;
    for (auto &opt : item)
    {
        if (opt.checked)
        {
            fi->value = opt.value;
            fi->label = opt.label;
            fi->selected = i;
            break;
        }
    }
    updateSelectOption(fi, item);
}

void updateSelectOption(FormItemListPtr fi, tcb::span<FormSelectOptionItem> item)
{
    if (fi == NULL || item.empty())
        return;

    int i = 0;
    for (auto &opt : item)
    {
        opt.checked = i == fi->selected;
        ++i;
    }
}

bool formChooseOptionByMenu(FormItemListPtr fi, int x, int y)
{
    int n = 0;
    for (auto opt = fi->select_option.begin(); opt != fi->select_option.end(); ++n, ++opt)
        ;

    std::vector<std::string> label;
    for (auto &opt : fi->select_option)
        label.push_back(opt.label);

    int selected = -1;
    int init_select = fi->selected;
    optionMenu(x, y, label, &selected, init_select, NULL);
    if (selected < 0)
        return 0;

    int i = 0;
    for (auto opt = fi->select_option.begin(); opt != fi->select_option.end(); ++i, ++opt)
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
    const char *url;
    Regex *re_url;
    const char *name;
    const char *action;
    struct pre_form_item *item;
    struct pre_form *next;
};

static struct pre_form *PreForm = NULL;

static struct pre_form *
add_pre_form(struct pre_form *prev, const char *url, const char *name, const char *action)
{
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
        newForm->re_url = newRegex(newForm->url, false, NULL, NULL);
        if (!newForm->re_url)
            newForm->url = NULL;
    }
    else if (url)
    {
        auto pu = URL::Parse(url, nullptr);
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
    fp = openSecretFile(w3mApp::Instance().pre_form_file.c_str());
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

void preFormUpdateBuffer(const BufferPtr &buf)
{
    struct pre_form *pf;
    struct pre_form_item *pi;
    int i;
    Anchor *a;
    FormList *fl;
    FormItemListPtr fi;
    FormSelectOptionItem *opt;
    int j;

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
            if (pf->name && (fl->name.empty() || fl->name != pf->name))
                continue;
            if (pf->action && (fl->action.empty() || fl->action != pf->action))
                continue;
            for (pi = pf->item; pi; pi = pi->next)
            {
                if (pi->type != fi->type)
                    continue;
                if (pi->type == FORM_INPUT_SUBMIT ||
                    pi->type == FORM_INPUT_IMAGE)
                {
                    if ((!pi->name || !*pi->name ||
                         (fi->name == pi->name)) &&
                        (!pi->value || !*pi->value ||
                         (fi->value == pi->value)))
                        buf->submit = a;
                    continue;
                }
                if (fi->name != pi->name)
                    continue;
                switch (pi->type)
                {
                case FORM_INPUT_TEXT:
                case FORM_INPUT_FILE:
                case FORM_INPUT_PASSWORD:
                case FORM_TEXTAREA:
                    fi->value = pi->value;
                    formUpdateBuffer(a, buf, fi);
                    break;
                case FORM_INPUT_CHECKBOX:
                    if (fi->value.size() && fi->value == pi->value)
                    {
                        fi->checked = pi->checked;
                        formUpdateBuffer(a, buf, fi);
                    }
                    break;
                case FORM_INPUT_RADIO:
                    if (fi->value.size() && fi->value == pi->value)
                    {
                        formRecheckRadio(a, buf, fi);
                    }
                    break;

                case FORM_SELECT:
                {
                    j = 0;
                    for (auto opt = fi->select_option.begin(); opt != fi->select_option.end();
                         ++j, ++opt)
                    {
                        if (pi->value && opt->value.size() && opt->value == pi->value)
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
                }
                }
            }
        }
    }
}
