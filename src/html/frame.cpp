#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "html/frame.h"
#include "html/html.h"
#include "myctype.h"
#include "file.h"
#include "frontend/terms.h"
#include "html/table.h"
#include "transport/url.h"
#include "entity.h"
#include "transport/loader.h"
#include "html/html_processor.h"
#include "charset.h"
#include "transport/istream.h"
#include <signal.h>
#include <setjmp.h>

static JMP_BUF AbortLoading;
struct frameset *renderFrameSet = NULL;

static void KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
}

static int
parseFrameSetLength(char *s, char ***ret)
{
    int i, len;
    char *p, *q, **lv;

    i = 1;

    if (s)
        for (p = s; (p = strchr(p, ',')); ++p)
            ++i;
    else
        s = "*";

    lv = New_N(char *, i);

    for (i = 0, p = s;; ++p)
    {
        SKIP_BLANKS(&p);
        len = strtol(p, &q, 10);

        switch (*q)
        {
        case '%':
            lv[i++] = Sprintf("%d%%", len)->ptr;
            break;
        case '*':
            lv[i++] = "*";
            break;
        default:
            lv[i++] = Sprintf("%d", len)->ptr;
            break;
        }

        if (!(p = strchr(q, ',')))
            break;
    }

    *ret = lv;
    return i;
}

struct frameset *
newFrameSet(struct parsed_tag *tag)
{
    struct frameset *f;
    int i;
    char *cols = NULL, *rows = NULL;

    f = New(struct frameset);
    f->attr = F_FRAMESET;
    f->name = NULL;
    f->currentURL = {};
    tag->TryGetAttributeValue(ATTR_COLS, &cols);
    tag->TryGetAttributeValue(ATTR_ROWS, &rows);
    f->col = parseFrameSetLength(cols, &f->width);
    f->row = parseFrameSetLength(rows, &f->height);
    f->i = 0;
    i = f->row * f->col;
    f->frame = New_N(union frameset_element, i);
    do
    {
        f->frame[--i].element = NULL;
    } while (i);
    return f;
}

struct frame_body *
newFrame(struct parsed_tag *tag, BufferPtr buf)
{
    struct frame_body *body;
    char *p;

    body = New(struct frame_body);
    bzero((void *)body, sizeof(*body));
    body->attr = F_UNLOADED;
    body->flags = 0;
    body->baseURL = *buf->BaseURL();
    if (tag)
    {
        if (tag->TryGetAttributeValue(ATTR_SRC, &p))
            body->url = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
        if (tag->TryGetAttributeValue(ATTR_NAME, &p) && *p != '_')
            body->name = wc_conv_strict(p, w3mApp::Instance().InnerCharset, buf->document_charset)->ptr;
    }
    return body;
}

static void
unloadFrame(struct frame_body *b)
{
    b->attr = F_UNLOADED;
}

void deleteFrame(struct frame_body *b)
{
    if (b == NULL)
        return;
    unloadFrame(b);
    bzero((void *)b, sizeof(*b));
}

void addFrameSetElement(struct frameset *f, union frameset_element element)
{
    int i;

    if (f == NULL)
        return;
    i = f->i;
    if (i >= f->col * f->row)
        return;
    f->frame[i] = element;
    f->i++;
}

void deleteFrameSet(struct frameset *f)
{
    int i;

    if (f == NULL)
        return;
    for (i = 0; i < f->col * f->row; i++)
    {
        deleteFrameSetElement(f->frame[i]);
    }
    f->name = NULL;
    f->currentURL = {};
    return;
}

void deleteFrameSetElement(union frameset_element e)
{
    if (e.element == NULL)
        return;
    switch (e.element->attr)
    {
    case F_UNLOADED:
        break;
    case F_BODY:
        deleteFrame(e.body);
        break;
    case F_FRAMESET:
        deleteFrameSet(e.set);
        break;
    default:
        break;
    }
    return;
}

static struct frame_body *
copyFrame(struct frame_body *ob)
{
    struct frame_body *rb;

    rb = New(struct frame_body);
    bcopy((const void *)ob, (void *)rb, sizeof(struct frame_body));
    return rb;
}

struct frameset *
copyFrameSet(struct frameset *of)
{
    struct frameset *rf;
    int n;

    rf = New(struct frameset);
    n = of->col * of->row;
    bcopy((const void *)of, (void *)rf, sizeof(struct frameset));
    rf->width = New_N(char *, rf->col);
    bcopy((const void *)of->width,
          (void *)rf->width, sizeof(char *) * rf->col);
    rf->height = New_N(char *, rf->row);
    bcopy((const void *)of->height,
          (void *)rf->height, sizeof(char *) * rf->row);
    rf->frame = New_N(union frameset_element, n);
    while (n)
    {
        n--;
        if (!of->frame[n].element)
            goto attr_default;
        switch (of->frame[n].element->attr)
        {
        case F_UNLOADED:
        case F_BODY:
            rf->frame[n].body = copyFrame(of->frame[n].body);
            break;
        case F_FRAMESET:
            rf->frame[n].set = copyFrameSet(of->frame[n].set);
            break;
        default:
        attr_default:
            rf->frame[n].element = NULL;
            break;
        }
    }
    return rf;
}

void flushFrameSet(struct frameset *fs)
{
    int n = fs->i;

    while (n)
    {
        n--;
        if (!fs->frame[n].element)
            goto attr_default;
        switch (fs->frame[n].element->attr)
        {
        case F_UNLOADED:
        case F_BODY:
            fs->frame[n].body->nameList.clear();
            break;
        case F_FRAMESET:
            flushFrameSet(fs->frame[n].set);
            break;
        default:
        attr_default:
            /* nothing to do */
            break;
        }
    }
}

void pushFrameTree(struct frameset_queue **fqpp, struct frameset *fs, BufferPtr buf)
{
    struct frameset_queue *rfq, *cfq = *fqpp;

    if (!fs)
        return;

    rfq = New(struct frameset_queue);
    rfq->linenumber = (buf && buf->CurrentLine()) ? buf->CurrentLine()->linenumber : 1;
    rfq->top_linenumber = (buf && buf->TopLine()) ? buf->TopLine()->linenumber : 1;
    rfq->pos = buf ? buf->pos : 0;
    rfq->currentColumn = buf ? buf->currentColumn : 0;
    rfq->formitem = buf->formitem;

    rfq->back = cfq;
    if (cfq)
    {
        rfq->next = cfq->next;
        if (cfq->next)
            cfq->next->back = rfq;
        cfq->next = rfq;
    }
    else
        rfq->next = cfq;
    rfq->frameset = fs;
    *fqpp = rfq;
    return;
}

struct frameset *
popFrameTree(struct frameset_queue **fqpp)
{
    struct frameset_queue *rfq = NULL, *cfq = *fqpp;
    struct frameset *rfs = NULL;

    if (!cfq)
        return rfs;

    rfs = cfq->frameset;
    if (cfq->next)
    {
        (rfq = cfq->next)->back = cfq->back;
    }
    if (cfq->back)
    {
        (rfq = cfq->back)->next = cfq->next;
    }
    *fqpp = rfq;
    bzero((void *)cfq, sizeof(struct frameset_queue));
    return rfs;
}

void resetFrameElement(union frameset_element *f_element,
                       BufferPtr buf, char *referer, FormList *request)
{
    char *f_name;
    struct frame_body *f_body;

    f_name = f_element->element->name;
    if (buf->frameset)
    {
        /* frame cascade */
        deleteFrameSetElement(*f_element);
        f_element->set = buf->frameset;
        f_element->set->currentURL = buf->currentURL;
        buf->frameset = popFrameTree(&(buf->frameQ));
        f_element->set->name = f_name;
    }
    else
    {
        f_body = newFrame(NULL, buf);
        f_body->attr = F_BODY;
        f_body->name = f_name;
        f_body->url = buf->currentURL.ToStr()->ptr;
        f_body->source = Strnew(buf->sourcefile)->ptr;
        buf->sourcefile.clear();
        if (buf->mailcap_source.size())
        {
            f_body->source = Strnew(buf->mailcap_source)->ptr;
            buf->mailcap_source.clear();
        }
        f_body->type = Strnew(buf->type)->ptr;
        f_body->referer = referer;
        f_body->request = request;
        deleteFrameSetElement(*f_element);
        f_element->body = f_body;
    }
}

static struct frameset *
frame_download_source(struct frame_body *b, URL *currentURL,
                      URL *baseURL, LoadFlags flag)
{
    BufferPtr buf;
    struct frameset *ret_frameset = NULL;
    URL url;

    if (b == NULL || b->url == NULL || b->url[0] == '\0')
        return NULL;
    if (b->baseURL)
        *baseURL = b->baseURL;
    url.Parse2(b->url, currentURL);
    switch (url.scheme)
    {
    case SCM_LOCAL:
#if 0
	b->source = url.real_file;
#endif
        b->flags = 0;
    default:
        is_redisplay = TRUE;
        w3mApp::Instance().w3m_dump |= DUMP_FRAME;
        buf = loadGeneralFile(b->url,
                              baseURL ? baseURL : currentURL,
                              b->referer, flag | RG_FRAME_SRC, b->request);

        /* XXX certificate? */
        if (buf)
            b->ssl_certificate = Strnew(buf->ssl_certificate)->ptr;

        w3mApp::Instance().w3m_dump &= ~DUMP_FRAME;
        is_redisplay = FALSE;
        break;
    }

    if (buf == NULL)
    {
        b->source = NULL;
        b->flags = 0;
        return NULL;
    }
    b->url = buf->currentURL.ToStr()->ptr;
    b->type = Strnew(buf->type)->ptr;
    b->source = Strnew(buf->sourcefile)->ptr;
    buf->sourcefile.clear();
    if (buf->mailcap_source.size())
    {
        b->source = Strnew(buf->mailcap_source)->ptr;
        buf->mailcap_source.clear();
    }
    b->attr = F_BODY;
    if (buf->frameset)
    {
        ret_frameset = buf->frameset;
        ret_frameset->name = b->name;
        ret_frameset->currentURL = buf->currentURL;
        buf->frameset = popFrameTree(&(buf->frameQ));
    }
    return ret_frameset;
}

#define CASE_TABLE_TAG    \
    case HTML_TR:         \
    case HTML_N_TR:       \
    case HTML_TD:         \
    case HTML_N_TD:       \
    case HTML_TH:         \
    case HTML_N_TH:       \
    case HTML_THEAD:      \
    case HTML_N_THEAD:    \
    case HTML_TBODY:      \
    case HTML_N_TBODY:    \
    case HTML_TFOOT:      \
    case HTML_N_TFOOT:    \
    case HTML_COLGROUP:   \
    case HTML_N_COLGROUP: \
    case HTML_COL

static int
createFrameFile(struct frameset *f, FILE *f1, BufferPtr current, int level,
                int force_reload)
{
    int r, c, t_stack;
    CharacterEncodingScheme charset, doc_charset;
    char *d_target, *p_target, *s_target, *t_target;
    URL *currentURL, base;
    MySignalHandler prevtrap = NULL;
    // LoadFlags flag;

    if (f == NULL)
        return -1;

    if (level > 7)
    {
        fputs("Too many frameset tasked.\n", f1);
        return -1;
    }

    if (level == 0)
    {
        f->name = "_top";
    }

    auto success = TrapJmp(level == 0, [&]() {
        if (level == 0)
        {
            fprintf(f1, "<html><head><title>%s</title></head><body>\n",
                    html_quote(current->buffername.c_str()));
            fputs("<table hborder width=\"100%\">\n", f1);
        }
        else
            fputs("<table hborder>\n", f1);

        currentURL = f->currentURL ? &f->currentURL : &current->currentURL;
        for (r = 0; r < f->row; r++)
        {
            fputs("<tr valign=top>\n", f1);
            for (c = 0; c < f->col; c++)
            {
                union frameset_element frame;
                struct frameset *f_frameset;
                int i = c + r * f->col;
                char *p = "";
                int status = R_ST_NORMAL;
                Str tok = Strnew();
                int pre_mode = 0;
                int end_tag = 0;

                frame = f->frame[i];

                if (frame.element == NULL)
                {
                    fputs("<td>\n</td>\n", f1);
                    continue;
                }

                fputs("<td", f1);
                if (frame.element->name)
                    fprintf(f1, " id=\"_%s\"", html_quote(frame.element->name));
                if (!r)
                    fprintf(f1, " width=\"%s\"", f->width[c]);
                fputs(">\n", f1);

                auto flag = RG_NONE;
                if (force_reload)
                {
                    flag |= RG_NOCACHE;
                    if (frame.element->attr == F_BODY)
                        unloadFrame(frame.body);
                }
                switch (frame.element->attr)
                {
                default:
                    /* FIXME: gettextize? */
                    fprintf(f1, "Frameset \"%s\" frame %d: type unrecognized",
                            html_quote(f->name), i + 1);
                    break;
                case F_UNLOADED:
                    if (!frame.body->name && f->name)
                    {
                        frame.body->name = Sprintf("%s_%d", f->name, i)->ptr;
                    }
                    fflush(f1);
                    f_frameset = frame_download_source(frame.body,
                                                       currentURL,
                                                       current->baseURL ? &current->baseURL : nullptr, flag);
                    if (f_frameset)
                    {
                        deleteFrame(frame.body);
                        f->frame[i].set = frame.set = f_frameset;
                        goto render_frameset;
                    }
                /* fall through */
                case F_BODY:
                {
                    URLFile f2(SCM_LOCAL, NULL);
                    if (frame.body->source)
                    {
                        fflush(f1);
                        f2.examineFile(frame.body->source);
                    }
                    if (f2.stream == NULL)
                    {
                        frame.body->attr = F_UNLOADED;
                        if (frame.body->flags & FB_NO_BUFFER)
                            /* FIXME: gettextize? */
                            fprintf(f1, "Open %s with other method",
                                    html_quote(frame.body->url));
                        else if (frame.body->url)
                            /* FIXME: gettextize? */
                            fprintf(f1, "Can't open %s",
                                    html_quote(frame.body->url));
                        else
                            /* FIXME: gettextize? */
                            fprintf(f1,
                                    "This frame (%s) contains no src attribute",
                                    frame.body->name ? html_quote(frame.body->name)
                                                     : "(no name)");
                        break;
                    }
                    base.Parse2(frame.body->url, currentURL);
                    p_target = f->name;
                    s_target = frame.body->name;
                    t_target = "_blank";
                    d_target = TargetSelf ? s_target : t_target;

                    charset = WC_CES_US_ASCII;
                    if (current->document_charset != WC_CES_US_ASCII)
                        doc_charset = current->document_charset;
                    else
                        doc_charset = w3mApp::Instance().DocumentCharset;

                    t_stack = 0;
                    if (frame.body->type &&
                        !strcasecmp(frame.body->type, "text/plain"))
                    {
                        Str tmp;
                        fprintf(f1, "<pre>\n");
                        while ((tmp = f2.StrmyISgets())->Size())
                        {
                            tmp = convertLine(NULL, tmp, HTML_MODE, &charset,
                                              doc_charset);
                            fprintf(f1, "%s", html_quote(tmp->ptr));
                        }
                        fprintf(f1, "</pre>\n");
                        f2.Close();
                        break;
                    }
                    do
                    {
                        int is_tag = FALSE;
                        const char *q;
                        struct parsed_tag *tag;

                        do
                        {
                            if (*p == '\0')
                            {
                                Str tmp = f2.StrmyISgets();
                                if (tmp->Size() == 0)
                                    break;
                                tmp = convertLine(NULL, tmp, HTML_MODE, &charset,
                                                  doc_charset);
                                p = tmp->ptr;
                            }
                            read_token(tok, &p, &status, 1, status != R_ST_NORMAL);
                        } while (status != R_ST_NORMAL);

                        if (tok->Size() == 0)
                            continue;

                        if (tok->ptr[0] == '<')
                        {
                            if (tok->ptr[1] &&
                                REALLY_THE_BEGINNING_OF_A_TAG(tok->ptr))
                                is_tag = TRUE;
                            else if (!(pre_mode & (RB_PLAIN | RB_INTXTA |
                                                   RB_SCRIPT | RB_STYLE)))
                            {
                                p = Strnew_m_charp(tok->ptr + 1, p, NULL)->ptr;
                                tok = Strnew("&lt;");
                            }
                        }
                        if (is_tag)
                        {
                            if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_SCRIPT |
                                            RB_STYLE))
                            {
                                q = tok->ptr;
                                if ((tag = parse_tag(&q, FALSE)) &&
                                    tag->tagid == end_tag)
                                {
                                    if (pre_mode & RB_PLAIN)
                                    {
                                        fputs("</PRE_PLAIN>", f1);
                                        pre_mode = 0;
                                        end_tag = 0;
                                        goto token_end;
                                    }
                                    pre_mode = 0;
                                    end_tag = 0;
                                    goto proc_normal;
                                }
                                if (strncmp(tok->ptr, "<!--", 4) &&
                                    (q = strchr(tok->ptr + 1, '<')))
                                {
                                    tok = Strnew_charp_n(tok->ptr, q - tok->ptr);
                                    p = Strnew_m_charp(q, p, NULL)->ptr;
                                    status = R_ST_NORMAL;
                                }
                                is_tag = FALSE;
                            }
                            else if (pre_mode & RB_INSELECT)
                            {
                                q = tok->ptr;
                                if ((tag = parse_tag(&q, FALSE)))
                                {
                                    if ((tag->tagid == end_tag) ||
                                        (tag->tagid == HTML_N_FORM))
                                    {
                                        if (tag->tagid == HTML_N_FORM)
                                            fputs("</SELECT>", f1);
                                        pre_mode = 0;
                                        end_tag = 0;
                                        goto proc_normal;
                                    }
                                    if (t_stack)
                                    {
                                        switch (tag->tagid)
                                        {
                                        case HTML_TABLE:
                                        case HTML_N_TABLE:
                                        CASE_TABLE_TAG:
                                            fputs("</SELECT>", f1);
                                            pre_mode = 0;
                                            end_tag = 0;
                                            goto proc_normal;
                                        }
                                    }
                                }
                            }
                        }

                    proc_normal:
                        if (is_tag)
                        {
                            const char *q = tok->ptr;
                            int j, a_target = 0;
                            URL url;

                            if (!(tag = parse_tag(&q, FALSE)))
                                goto token_end;

                            switch (tag->tagid)
                            {
                            case HTML_TITLE:
                                fputs("<!-- title:", f1);
                                goto token_end;
                            case HTML_N_TITLE:
                                fputs("-->", f1);
                                goto token_end;
                            case HTML_BASE:
                                /* "BASE" is prohibit tag */
                                if (tag->TryGetAttributeValue(ATTR_HREF, &q))
                                {
                                    q = wc_conv_strict(remove_space(q), w3mApp::Instance().InnerCharset, charset)->ptr;
                                    base.Parse(q, NULL);
                                }
                                if (tag->TryGetAttributeValue(ATTR_TARGET, &q))
                                {
                                    if (!strcasecmp(q, "_self"))
                                        d_target = s_target;
                                    else if (!strcasecmp(q, "_parent"))
                                        d_target = p_target;
                                    else
                                        d_target = wc_conv_strict(q, w3mApp::Instance().InnerCharset, charset)->ptr;
                                }
                                tok->Delete(0, 1);
                                tok->Pop(1);
                                fprintf(f1, "<!-- %s -->", html_quote(tok->ptr));
                                goto token_end;
                            case HTML_META:
                                if (tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &q) && !strcasecmp(q, "refresh"))
                                {
                                    if (tag->TryGetAttributeValue(ATTR_CONTENT, &q))
                                    {
                                        Str s_tmp = NULL;
                                        int refresh_interval =
                                            getMetaRefreshParam(q, &s_tmp);
                                        if (s_tmp)
                                        {
                                            q = html_quote(s_tmp->ptr);
                                            fprintf(f1,
                                                    "Refresh (%d sec) <a href=\"%s\">%s</a>\n",
                                                    refresh_interval, q, q);
                                        }
                                    }
                                }
                                if (w3mApp::Instance().UseContentCharset &&
                                    tag->TryGetAttributeValue(ATTR_HTTP_EQUIV, &q) && !strcasecmp(q, "Content-Type") && tag->TryGetAttributeValue(ATTR_CONTENT, &q) && (q = strcasestr(q, "charset")) != NULL)
                                {
                                    q += 7;
                                    SKIP_BLANKS(&q);
                                    if (*q == '=')
                                    {
                                        CharacterEncodingScheme c;
                                        q++;
                                        SKIP_BLANKS(&q);
                                        if ((c = wc_guess_charset(q, WC_CES_NONE)) != 0)
                                        {
                                            doc_charset = c;
                                            charset = WC_CES_US_ASCII;
                                        }
                                    }
                                }

                                /* fall thru, "META" is prohibit tag */
                            case HTML_HEAD:
                            case HTML_N_HEAD:
                            case HTML_BODY:
                            case HTML_N_BODY:
                            case HTML_DOCTYPE:
                                /* prohibit_tags */
                                tok->Delete(0, 1);
                                tok->Pop(1);
                                fprintf(f1, "<!-- %s -->", html_quote(tok->ptr));
                                goto token_end;
                            case HTML_TABLE:
                                t_stack++;
                                break;
                            case HTML_N_TABLE:
                                t_stack--;
                                if (t_stack < 0)
                                {
                                    t_stack = 0;
                                    tok->Delete(0, 1);
                                    tok->Pop(1);
                                    fprintf(f1,
                                            "<!-- table stack underflow: %s -->",
                                            html_quote(tok->ptr));
                                    goto token_end;
                                }
                                break;
                            CASE_TABLE_TAG:
                                /* table_tags MUST be in table stack */
                                if (!t_stack)
                                {
                                    tok->Delete(0, 1);
                                    tok->Pop(1);
                                    fprintf(f1, "<!-- %s -->",
                                            html_quote(tok->ptr));
                                    goto token_end;
                                }
                                break;
                            case HTML_SELECT:
                                pre_mode = RB_INSELECT;
                                end_tag = HTML_N_SELECT;
                                break;
                            case HTML_TEXTAREA:
                                pre_mode = RB_INTXTA;
                                end_tag = HTML_N_TEXTAREA;
                                break;
                            case HTML_SCRIPT:
                                pre_mode = RB_SCRIPT;
                                end_tag = HTML_N_SCRIPT;
                                break;
                            case HTML_STYLE:
                                pre_mode = RB_STYLE;
                                end_tag = HTML_N_STYLE;
                                break;
                            case HTML_LISTING:
                                pre_mode = RB_PLAIN;
                                end_tag = HTML_N_LISTING;
                                fputs("<PRE_PLAIN>", f1);
                                goto token_end;
                            case HTML_XMP:
                                pre_mode = RB_PLAIN;
                                end_tag = HTML_N_XMP;
                                fputs("<PRE_PLAIN>", f1);
                                goto token_end;
                            case HTML_PLAINTEXT:
                                pre_mode = RB_PLAIN;
                                end_tag = MAX_HTMLTAG;
                                fputs("<PRE_PLAIN>", f1);
                                goto token_end;
                            default:
                                break;
                            }
                            for (j = 0; j < TagMAP[tag->tagid].max_attribute; j++)
                            {
                                switch (tag->attrid[j])
                                {
                                case ATTR_SRC:
                                case ATTR_HREF:
                                case ATTR_ACTION:
                                    if (!tag->value[j])
                                        break;
                                    tag->value[j] =
                                        wc_conv_strict(remove_space(tag->value[j]), w3mApp::Instance().InnerCharset, charset)->ptr;
                                    tag->need_reconstruct = TRUE;
                                    url.Parse2(tag->value[j], &base);
                                    if (url.scheme == SCM_UNKNOWN ||
                                        url.scheme == SCM_MAILTO ||
                                        url.scheme == SCM_MISSING)
                                        break;
                                    a_target |= 1;
                                    tag->value[j] = url.ToStr()->ptr;
                                    tag->SetAttributeValue(
                                                        ATTR_REFERER,
                                                        base.ToStr()->ptr);

                                    if (tag->attrid[j] == ATTR_ACTION &&
                                        charset != WC_CES_US_ASCII)
                                        tag->SetAttributeValue(
                                                            ATTR_CHARSET,
                                                            wc_ces_to_charset(charset));

                                    break;
                                case ATTR_TARGET:
                                    if (!tag->value[j])
                                        break;
                                    a_target |= 2;
                                    if (!strcasecmp(tag->value[j], "_self"))
                                    {
                                        tag->SetAttributeValue(
                                                            ATTR_TARGET, s_target);
                                    }
                                    else if (!strcasecmp(tag->value[j], "_parent"))
                                    {
                                        tag->SetAttributeValue(
                                                            ATTR_TARGET, p_target);
                                    }
                                    break;
                                case ATTR_NAME:
                                case ATTR_ID:
                                    if (!tag->value[j])
                                        break;
                                    tag->SetAttributeValue(
                                                        ATTR_FRAMENAME, s_target);
                                    break;
                                }
                            }
                            if (a_target == 1)
                            {
                                /* there is HREF attribute and no TARGET
			     * attribute */
                                tag->SetAttributeValue( ATTR_TARGET, d_target);
                            }
                            if (tag->need_reconstruct)
                                tok = tag->ToStr();
                            tok->Puts(f1);
                        }
                        else
                        {
                            if (pre_mode & RB_PLAIN)
                                fprintf(f1, "%s", html_quote(tok->ptr));
                            else if (pre_mode & RB_INTXTA)
                                fprintf(f1, "%s",
                                        html_quote(html_unquote(tok->ptr, w3mApp::Instance().InnerCharset)));
                            else
                                tok->Puts(f1);
                        }
                    token_end:
                        tok->Clear();
                    } while (*p != '\0' || !iseos(f2.stream));
                    if (pre_mode & RB_PLAIN)
                        fputs("</PRE_PLAIN>\n", f1);
                    else if (pre_mode & RB_INTXTA)
                        fputs("</TEXTAREA></FORM>\n", f1);
                    else if (pre_mode & RB_INSELECT)
                        fputs("</SELECT></FORM>\n", f1);
                    else if (pre_mode & (RB_SCRIPT | RB_STYLE))
                    {
                        if (status != R_ST_NORMAL)
                            fputs(correct_irrtag(status)->ptr, f1);
                        if (pre_mode & RB_SCRIPT)
                            fputs("</SCRIPT>\n", f1);
                        else if (pre_mode & RB_STYLE)
                            fputs("</STYLE>\n", f1);
                    }
                    while (t_stack--)
                        fputs("</TABLE>\n", f1);
                    f2.Close();
                    break;
                }
                case F_FRAMESET:
                render_frameset:
                    if (!frame.set->name && f->name)
                    {
                        frame.set->name = Sprintf("%s_%d", f->name, i)->ptr;
                    }
                    createFrameFile(frame.set, f1, current, level + 1,
                                    force_reload);
                    break;
                }
                fputs("</td>\n", f1);
            }
            fputs("</tr>\n", f1);
        }

        fputs("</table>\n", f1);

        return true;
    });

    if (!success)
    {
        return -1;
    }

    if (level == 0)
    {
        fputs("</body></html>\n", f1);
    }
    return 0;
}

BufferPtr
renderFrame(BufferPtr Cbuf, int force_reload)
{
    Str tmp;
    FILE *f;
    BufferPtr buf;
    int flag;
    struct frameset *fset;
    CharacterEncodingScheme doc_charset = w3mApp::Instance().DocumentCharset;

    tmp = tmpfname(TMPF_FRAME, ".html");
    f = fopen(tmp->ptr, "w");
    if (f == NULL)
        return NULL;
    /* 
     * if (Cbuf->frameQ != NULL) fset = Cbuf->frameQ->frameset; else */
    fset = Cbuf->frameset;
    if (fset == NULL || createFrameFile(fset, f, Cbuf, 0, force_reload) < 0)
        return NULL;
    fclose(f);
    {
        auto flag = RG_FRAME;
        if ((Cbuf->currentURL).is_nocache)
            flag |= RG_NOCACHE;
        renderFrameSet = Cbuf->frameset;
        flushFrameSet(renderFrameSet);
        w3mApp::Instance().DocumentCharset = w3mApp::Instance().InnerCharset;
        buf = loadGeneralFile(tmp->ptr, NULL, NULL, flag, NULL);
    }
    w3mApp::Instance().DocumentCharset = doc_charset;
    renderFrameSet = NULL;
    if (buf == NULL)
        return NULL;
    buf->sourcefile = tmp->ptr;
    buf->document_charset = Cbuf->document_charset;
    buf->currentURL = Cbuf->currentURL;
    preFormUpdateBuffer(buf);
    return buf;
}

union frameset_element *
search_frame(struct frameset *fset, char *name)
{
    int i;
    union frameset_element *e = NULL;

    for (i = 0; i < fset->col * fset->row; i++)
    {
        e = &(fset->frame[i]);
        if (e->element != NULL)
        {
            if (e->element->name && !strcmp(e->element->name, name))
            {
                return e;
            }
            else if (e->element->attr == F_FRAMESET &&
                     (e = search_frame(e->set, name)))
            {
                return e;
            }
        }
    }
    return NULL;
}
