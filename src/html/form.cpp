/* 
 * HTML forms
 */
#include <string_view_util.h>
#include <stdio.h>
#include "indep.h"
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
#include "download_list.h"
#include "stream/network.h"

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
    if (a == NULL || a->url.empty())
    {

#ifndef MENU_MAP
        BufferPtr buf = follow_map_panel(GetCurrentTab()->GetCurrentBuffer(), name);

        if (buf != NULL)
            cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
#endif
#if defined(MENU_MAP) || defined(USE_IMAGE)
        return;
    }
    if (a->url[0] == '#')
    {
        gotoLabel(a->url.c_str() + 1);
        return;
    }

    auto p_url = URL::Parse(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL());
    pushHashHist(w3mApp::Instance().URLHist, p_url.ToStr()->ptr);
    if (check_target() && w3mApp::Instance().open_tab_blank &&
        (a->target == "_new" || a->target == "_blank"))
    {
        auto tab = CreateTabSetCurrent();
        BufferPtr buf = tab->GetCurrentBuffer();
        cmd_loadURL(a->url, GetCurrentTab()->GetCurrentBuffer()->BaseURL(),
                    HttpReferrerPolicy::StrictOriginWhenCrossOrigin, NULL);
        // if (buf != GetCurrentTab()->GetCurrentBuffer())
        //     GetCurrentTab()->DeleteBuffer(buf);
        // else
        //     deleteTab(GetCurrentTab());
        // displayCurrentbuf(B_FORCE_REDRAW);
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

FormPtr Form::Create(
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

    auto l = std::make_shared<Form>(action, m);
    l->charset = c;
    l->enctype = e;
    l->target = target;
    l->name = name;
    return l;
}

/* 
 * add <input> element to form_list
 */
FormItemPtr formList_addInput(FormPtr fl, struct parsed_tag *tag, HtmlContext *context)
{
    /* if not in <form>..</form> environment, just ignore <input> tag */
    if (fl == NULL)
        return NULL;

    auto item = std::make_shared<FormItem>();
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
        item->chooseSelectOption();
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

Str FormItem::ToStr() const
{
    Str tmp = Strnew();

    if (this->type != FORM_SELECT && this->type != FORM_TEXTAREA)
        tmp->Push("input type=");
    tmp->Push(_formtypetbl[this->type]);
    if (this->name.size())
        Strcat_m_charp(tmp, " name=\"", this->name, "\"", NULL);
    if ((this->type == FORM_INPUT_RADIO || this->type == FORM_INPUT_CHECKBOX ||
         this->type == FORM_SELECT) &&
        this->value.size())
        Strcat_m_charp(tmp, " value=\"", this->value, "\"", NULL);
    Strcat_m_charp(tmp, " (", _formmethodtbl[this->parent.lock()->method], " ",
                   this->parent.lock()->action, ")", NULL);

    return tmp;
}

void formRecheckRadio(const AnchorPtr &a, BufferPtr buf, FormItemPtr fi)
{
    for (int i = 0; i < buf->formitem.size(); i++)
    {
        auto a2 = buf->formitem.anchors[i];
        auto f2 = a2->item;
        if (f2->parent.lock() == fi->parent.lock() && f2 != fi &&
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
    AnchorPtr a;
    FormItemPtr f1, f2;

    if (buf == NULL || !buf->formitem || !formitem)
        return;
    for (i = 0; i < buf->formitem.size() && i < formitem.size(); i++)
    {
        a = buf->formitem.anchors[i];
        if (a->y != a->start.line)
            continue;
        f1 = a->item;
        f2 = formitem.anchors[i]->item;
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

void formUpdateBuffer(const AnchorPtr &a, BufferPtr buf, FormItemPtr form)
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
            form->updateSelectOption();
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

void FormItem::input_textarea()
{
    char *tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
    auto f = fopen(tmpf, "w");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't open temporary file", false);
        return;
    }

    if (this->value.size())
        form_fputs_decode(this->value.c_str(), f);
    fclose(f);

    fmTerm();
    system(myEditor(w3mApp::Instance().Editor.c_str(), tmpf, 1)->ptr);
    fmInit();

    auto auto_detect = WcOption.auto_detect;
    CharacterEncodingScheme charset = w3mApp::Instance().DisplayCharset;
    if (this->readonly)
        goto input_end;
    f = fopen(tmpf, "r");
    if (f == NULL)
    {
        /* FIXME: gettextize? */
        disp_err_message("Can't open temporary file", false);
        goto input_end;
    }
    this->value.clear();

    WcOption.auto_detect = WC_OPT_DETECT_ON;

    Str tmp;
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
            this->value.push_back(*p);
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

void FormItem::chooseSelectOption()
{
    this->selected = 0;
    if (select_option.empty())
    {
        this->value.clear();
        this->label.clear();
        return;
    }
    this->value = select_option.front().value;
    this->label = select_option.front().label;

    int i = 0;
    for (auto &opt : select_option)
    {
        if (opt.checked)
        {
            this->value = opt.value;
            this->label = opt.label;
            this->selected = i;
            break;
        }
    }
    updateSelectOption();
}

void FormItem::updateSelectOption()
{
    int i = 0;
    for (auto &opt : select_option)
    {
        opt.checked = i == selected;
        ++i;
    }
}

bool FormItem::formChooseOptionByMenu(int x, int y)
{
    int n = 0;
    for (auto opt = this->select_option.begin(); opt != this->select_option.end(); ++n, ++opt)
        ;

    std::vector<std::string> label;
    for (auto &opt : this->select_option)
        label.push_back(opt.label);

    int selected = -1;
    int init_select = this->selected;
    optionMenu(x, y, label, &selected, init_select, NULL);
    if (selected < 0)
        return 0;

    int i = 0;
    for (auto opt = this->select_option.begin(); opt != this->select_option.end(); ++i, ++opt)
    {
        if (i == selected)
        {
            this->selected = selected;
            this->value = opt->value;
            this->label = opt->label;
            break;
        }
    }
    updateSelectOption();
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
    std::string name;
    std::string value;
    int checked;
};
using pre_form_item_ptr = std::shared_ptr<pre_form_item>;

struct pre_form
{
    std::string url;
    Regex *re_url;
    std::string name;
    std::string action;
    std::vector<pre_form_item_ptr> item;
};
using pre_form_ptr = std::shared_ptr<pre_form>;

static std::vector<pre_form_ptr> PreForm;

static pre_form_ptr add_pre_form(const char *url, const char *name, const char *action)
{
    auto newForm = std::make_shared<pre_form>();
    PreForm.push_back(newForm);

    if (url && *url == '/')
    {
        int l = strlen(url);
        if (l > 1 && url[l - 1] == '/')
            newForm->url = allocStr(url + 1, l - 2);
        else
            newForm->url = url + 1;
        newForm->re_url = newRegex(newForm->url.c_str(), false, NULL, NULL);
        if (!newForm->re_url)
            newForm->url.clear();
    }
    else if (url)
    {
        auto pu = URL::Parse(url, nullptr);
        newForm->url = pu.ToStr()->ptr;
        newForm->re_url = NULL;
    }
    newForm->name = (name && *name) ? name : NULL;
    newForm->action = (action && *action) ? action : NULL;

    return newForm;
}

static pre_form_item_ptr add_pre_form_item(pre_form_ptr pf, int type,
                                           std::string_view name, std::string_view value, std::string_view checked)
{
    if (!pf)
        return NULL;

    auto newForm = std::make_shared<pre_form_item>();
    newForm->type = type;
    newForm->name = name;
    newForm->value = value;
    if (checked == "0" || checked == "off" || checked == "no")
        newForm->checked = 0;
    else
        newForm->checked = 1;
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
    // FILE *fp;
    Str line = NULL;
    Str textarea = NULL;
    pre_form_ptr pf;
    pre_form_item_ptr pi;
    int type = -1;
    char *name = NULL;

    auto fp = openSecretFile(Network::Instance().pre_form_file.c_str());
    if (fp == NULL)
        return;

    PreForm.clear();
    while (1)
    {
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
        std::string_view p = line->ptr;
        if(p.empty())
            continue;
        if (p.size() && p[0] == '#')
            continue; /* comment or empty line */
        std::string s;
        std::tie(p, s) = getWord(p);
        std::string arg;
        std::tie(p, arg) = getWord(p);
        if (s == "url")
        {
            if (arg.empty())
                continue;

            std::tie(p, s)= getQWord(p);
            pf = add_pre_form(arg.c_str(), NULL, p.data());
            pi = pf->item.size() ? pf->item[0] : nullptr;
            continue;
        }
        if (!pf)
            continue;
        if (s == "form")
        {
            if (arg.empty())
                continue;
            std::tie(p, s) = getQWord(p);
            std::string x;
            std::tie(p, x) = getQWord(p);
            if (p.empty())
            {
                p = s;
                s.clear();
            }
            if (pf->item.size())
            {
                auto prev = pf;
                auto pf = add_pre_form("", s.data(), p.data());
                /* copy previous URL */
                pf->url = prev->url;
                pf->re_url = prev->re_url;
            }
            else
            {
                pf->name = s;
                pf->action = p;
            }
            pi = pf->item.size() ? pf->item[0] : nullptr;
            continue;
        }
        if (s == "text")
            type = FORM_INPUT_TEXT;
        else if (s == "file")
            type = FORM_INPUT_FILE;
        else if (s == "passwd" || s == "password")
            type = FORM_INPUT_PASSWORD;
        else if (s == "checkbox")
            type = FORM_INPUT_CHECKBOX;
        else if (s == "radio")
            type = FORM_INPUT_RADIO;
        else if (s == "submit")
            type = FORM_INPUT_SUBMIT;
        else if (s == "image")
            type = FORM_INPUT_IMAGE;
        else if (s == "select")
            type = FORM_SELECT;
        else if (s == "textarea")
        {
            type = FORM_TEXTAREA;
            name = Strnew(arg)->ptr;
            textarea = Strnew();
            continue;
        }
        else if (textarea && name && s == "/textarea")
        {
            pi = add_pre_form_item(pf, type, name, textarea->ptr, "");
            textarea = NULL;
            name = NULL;
            continue;
        }
        else
            continue;
        std::tie(p, s) = getQWord(p);
        std::string x;
        std::tie(p, x) = getQWord(p);
        pi = add_pre_form_item(pf, type, arg, s.c_str(), x.c_str());
    }
    fclose(fp);
}

void preFormUpdateBuffer(const BufferPtr &buf)
{
    int i;
    AnchorPtr a;
    FormPtr fl;
    FormItemPtr fi;
    FormSelectOptionItem *opt;
    int j;

    if (!buf || !buf->formitem || PreForm.empty())
        return;

    for (auto &pf : PreForm)
    {
        if (pf->re_url)
        {
            Str url = buf->currentURL.ToStr();
            if (!RegexMatch(pf->re_url, url->ptr, url->Size(), 1))
                continue;
        }
        else if (pf->url.size())
        {
            if (buf->currentURL.ToStr()->Cmp(pf->url.c_str()) != 0)
                continue;
        }
        else
            continue;
        for (i = 0; i < buf->formitem.size(); i++)
        {
            a = buf->formitem.anchors[i];
            fi = a->item;
            fl = fi->parent.lock();
            if (pf->name.size() && (fl->name.empty() || fl->name != pf->name))
                continue;
            if (pf->action.size() && (fl->action.empty() || fl->action != pf->action))
                continue;
            for (auto &pi : pf->item)
            {
                if (pi->type != fi->type)
                    continue;
                if (pi->type == FORM_INPUT_SUBMIT ||
                    pi->type == FORM_INPUT_IMAGE)
                {
                    if ((pi->name.empty() ||
                         (fi->name == pi->name)) &&
                        (pi->value.empty() ||
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
                        if (opt->value.size() && opt->value == pi->value)
                        {
                            fi->selected = j;
                            fi->value = opt->value;
                            fi->label = opt->label;
                            fi->updateSelectOption();
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
