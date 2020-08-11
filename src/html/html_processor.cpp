#include "html_processor.h"
#include "html/html_context.h"
#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "html/form.h"
#include "transport/loader.h"
#include "file.h"
#include "myctype.h"
#include "entity.h"
#include "http/compression.h"
#include "html/image.h"
#include "symbol.h"
#include "tagstack.h"
#include "frame.h"
#include "public.h"
#include "commands.h"
#include "html/maparea.h"
#include "html/tokenizer.h"
#include "w3m.h"
#include "frontend/terms.h"
#include "frontend/line.h"
#include "transport/istream.h"
#include <signal.h>
#include <setjmp.h>
#include <vector>

static void
print_internal_information(struct html_feed_environ *henv)
{
    // TDOO:
    //     TextLineList *tl = newTextLineList();

    //     {
    //         auto s = Strnew("<internal>");
    //         pushTextLine(tl, newTextLine(s, 0));
    //         if (henv->title)
    //         {
    //             s = Strnew_m_charp("<title_alt title=\"",
    //                                html_quote(henv->title), "\">");
    //             pushTextLine(tl, newTextLine(s, 0));
    //         }
    //     }

    //     get_formselect()->print_internal(tl);
    //     get_textarea()->print_internal(tl);

    //     {
    //         auto s = Strnew("</internal>");
    //         pushTextLine(tl, newTextLine(s, 0));
    //     }

    //     if (henv->buf)
    //     {
    //         appendTextLineList(henv->buf, tl);
    //     }
    //     else if (henv->f)
    //     {
    //         TextLineListItem *p;
    //         for (p = tl->first; p; p = p->next)
    //             fprintf(henv->f, "%s\n", Str_conv_to_halfdump(p->ptr->line)->ptr);
    //     }
}

///
/// 1行ごとに Line の構築と html タグを解釈する
///
using FeedFunc = std::function<Str()>;
static void HTMLlineproc2body(BufferPtr buf, const FeedFunc &feed, int llimit, HtmlContext *seq)
{
    //
    // each line
    //
    Str line = nullptr;
    for (int nlines = 1; nlines != llimit; ++nlines)
    {
        if (!line)
        {
            // new line
            line = feed();
            if (!line)
            {
                break;
            }

            auto [n, t] = seq->TextareaCurrent();
            if (n >= 0 && *(line->ptr) != '<')
            { /* halfload */
                t->Push(line);
                continue;
            }

            StripRight(line);
        }

        //
        // each char
        //
        const char *str = line->ptr;
        auto endp = str + line->Size();
        PropertiedString out;
        while (str < endp)
        {
            auto mode = get_mctype(*str);
            if ((seq->effect | ex_efct(seq->ex_effect)) & PC_SYMBOL && *str != '<')
            {
                // symbol
                auto p = get_width_symbol(seq->SymbolWidth0(), seq->symbol);
                assert(p.size() > 0);
                int len = get_mclen(p.data());
                mode = get_mctype(p[0]);

                out.push(mode | seq->effect | ex_efct(seq->ex_effect), p[0]);
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    for (int i = 1; len--; ++i)
                    {
                        out.push(mode | seq->effect | ex_efct(seq->ex_effect), p[i]);
                    }
                }
                str += seq->SymbolWidth();
            }
            else if (mode == PC_CTRL || mode == PC_UNDEF)
            {
                // control
                out.push(PC_ASCII | seq->effect | ex_efct(seq->ex_effect), ' ');
                str++;
            }
            else if (mode & PC_UNKNOWN)
            {
                // unknown
                out.push(PC_ASCII | seq->effect | ex_efct(seq->ex_effect), ' ');
                str += get_mclen(str);
            }
            else if (*str != '<' && *str != '&')
            {
                // multibyte char ?
                int len = get_mclen(str);
                out.push(mode | seq->effect | ex_efct(seq->ex_effect), *(str++));
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--)
                    {
                        out.push(mode | seq->effect | ex_efct(seq->ex_effect), *(str++));
                    }
                }
            }
            else if (*str == '&')
            {
                /* 
                 * & escape processing
                 */
                char *p;
                {
                    auto [pos, view] = getescapecmd(str, w3mApp::Instance().InnerCharset);
                    str = const_cast<char *>(pos);
                    p = const_cast<char *>(view.data());
                }

                while (*p)
                {
                    mode = get_mctype(*p);
                    if (mode == PC_CTRL || mode == PC_UNDEF)
                    {
                        out.push(PC_ASCII | seq->effect | ex_efct(seq->ex_effect), ' ');
                        p++;
                    }
                    else if (mode & PC_UNKNOWN)
                    {
                        out.push(PC_ASCII | seq->effect | ex_efct(seq->ex_effect), ' ');
                        p += get_mclen(p);
                    }
                    else
                    {
                        int len = get_mclen(p);
                        out.push(mode | seq->effect | ex_efct(seq->ex_effect), *(p++));
                        if (--len)
                        {
                            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                            while (len--)
                            {
                                out.push(mode | seq->effect | ex_efct(seq->ex_effect), *(p++));
                            }
                        }
                    }
                }
            }
            else
            {
                /* tag processing */
                auto tag = parse_tag(&str, TRUE);
                if (!tag)
                    continue;

                seq->Process(tag, buf, out.len(), str);
            }
        }

        if (seq->EndLineAddBuffer())
        {
            buf->AddNewLine(out, nlines);
        }

        if (str != endp)
        {
            // advance line
            line = line->Substr(str - line->ptr, endp - str);
        }
        else
        {
            // clear for next line
            line = nullptr;
        }
    }

    buf->formlist = seq->FormEnd();

    addMultirowsForm(buf, buf->formitem);
    addMultirowsImg(buf, buf->img);
}

///
/// entry
///
void loadHTMLstream(URLFile *f, BufferPtr newBuf, FILE *src, int internal)
{
    struct environment envs[MAX_ENV_LEVEL];
    clen_t linelen = 0;
    clen_t trbyte = 0;
    Str lineBuf2 = Strnew();

    CharacterEncodingScheme charset = WC_CES_US_ASCII;
    CharacterEncodingScheme doc_charset = w3mApp::Instance().DocumentCharset;

    struct html_feed_environ htmlenv1;
    struct readbuffer obuf;

    MySignalHandler prevtrap = nullptr;

    HtmlContext context;

    int image_flag;
    if (newBuf->image_flag)
        image_flag = newBuf->image_flag;
    else if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && autoImage)
        image_flag = IMG_FLAG_AUTO;
    else
        image_flag = IMG_FLAG_SKIP;
    if (newBuf->currentURL.file.size())
    {
        *GetCurBaseUrl() = *newBuf->BaseURL();
    }

    if (w3mApp::Instance().w3m_halfload)
    {
        newBuf->buffername = "---";
        newBuf->document_charset = w3mApp::Instance().InnerCharset;
        FeedFunc feed = [f]() -> Str {
            auto s = StrISgets(f->stream);
            if (s->Size() == 0)
            {
                ISclose(f->stream);
                return nullptr;
            }
            return s;
        };
        HTMLlineproc2body(newBuf, feed, -1, &context);
        w3mApp::Instance().w3m_halfload = FALSE;
        return;
    }

    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, nullptr, newBuf->width, 0);

    if (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)
        htmlenv1.f = stdout;
    else
        htmlenv1.buf = newTextLineList();

    auto success = TrapJmp([&]() {

#ifdef USE_M17N
        if (newBuf != nullptr)
        {
            if (newBuf->bufferprop & BP_FRAME)
                charset = w3mApp::Instance().InnerCharset;
            else if (newBuf->document_charset)
                charset = doc_charset = newBuf->document_charset;
        }
        if (content_charset && w3mApp::Instance().UseContentCharset)
            doc_charset = content_charset;
        else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
            doc_charset = WC_CES_UTF_8;
        meta_charset = WC_CES_NONE;
#endif
#if 0
    do_blankline(&htmlenv1, &obuf, 0, 0, htmlenv1.limit);
    obuf.flag = RB_IGNORE_P;
#endif
        if (IStype(f->stream) != IST_ENCODED)
            f->stream = newEncodedStream(f->stream, f->encoding);
        while ((lineBuf2 = f->StrmyISgets())->Size())
        {
#ifdef USE_NNTP
            if (f->scheme == SCM_NEWS && lineBuf2->ptr[0] == '.')
            {
                lineBuf2->Delete(0, 1);
                if (lineBuf2->ptr[0] == '\n' || lineBuf2->ptr[0] == '\r' ||
                    lineBuf2->ptr[0] == '\0')
                {
                    /*
                 * iseos(f->stream) = TRUE;
                 */
                    break;
                }
            }
#endif /* USE_NNTP */
            if (src)
                lineBuf2->Puts(src);
            linelen += lineBuf2->Size();
            if (w3mApp::Instance().w3m_dump & DUMP_EXTRA)
                printf("W3m-in-progress: %s\n", convert_size2(linelen, GetCurrentContentLength(), TRUE));
            if (w3mApp::Instance().w3m_dump & DUMP_SOURCE)
                continue;
            showProgress(&linelen, &trbyte);
            /*
         * if (frame_source)
         * continue;
         */
            if (meta_charset)
            { /* <META> */
                if (content_charset == 0 && w3mApp::Instance().UseContentCharset)
                {
                    doc_charset = meta_charset;
                    charset = WC_CES_US_ASCII;
                }
                meta_charset = WC_CES_NONE;
            }

            lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);

            context.SetCES(charset);

            HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal, &context);
        }
        if (obuf.status != R_ST_NORMAL)
        {
            obuf.status = R_ST_EOL;
            HTMLlineproc0("\n", &htmlenv1, internal, &context);
        }
        obuf.status = R_ST_NORMAL;
        completeHTMLstream(&htmlenv1, &obuf, &context);
        flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
        if (htmlenv1.title)
            newBuf->buffername = htmlenv1.title;

        return true;
    });

    if (!success)
    {
        HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1, &context);
    }
    else
    {

        if (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)
        {
            print_internal_information(&htmlenv1);
            return;
        }
        if (w3mApp::Instance().w3m_backend)
        {
            print_internal_information(&htmlenv1);
            backend_halfdump_buf = htmlenv1.buf;
            return;
        }
    }

    newBuf->trbyte = trbyte + linelen;

    if (!(newBuf->bufferprop & BP_FRAME))
        newBuf->document_charset = charset;

    newBuf->image_flag = image_flag;

    {
        FeedFunc feed = [feeder = TextFeeder{htmlenv1.buf->first}]() -> Str {
            return feeder();
        };

        HTMLlineproc2body(newBuf, feed, -1, &context);
    }
}

/* 
 * loadHTMLBuffer: read file and make new buffer
 */
BufferPtr
loadHTMLBuffer(URLFile *f, BufferPtr newBuf)
{
    FILE *src = nullptr;
    Str tmp;

    if (newBuf == nullptr)
        newBuf = newBuffer(INIT_BUFFER_WIDTH());
    if (newBuf->sourcefile.empty() &&
        (f->scheme != SCM_LOCAL || newBuf->mailcap))
    {
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
            newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(f, newBuf, src, newBuf->bufferprop & BP_FRAME);

    newBuf->CurrentAsLast();

    formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

/* 
 * loadHTMLString: read string and make new buffer
 */
BufferPtr loadHTMLString(Str page)
{
    auto newBuf = newBuffer(INIT_BUFFER_WIDTH());

    auto success = TrapJmp([&]() {
        URLFile f(SCM_LOCAL, newStrStream(page));

        newBuf->document_charset = w3mApp::Instance().InnerCharset;
        loadHTMLstream(&f, newBuf, nullptr, TRUE);
        newBuf->document_charset = WC_CES_US_ASCII;

        return true;
    });

    if (!success)
    {
        return nullptr;
    }

    newBuf->CurrentAsLast();
    newBuf->type = "text/html";
    newBuf->real_type = newBuf->type;

    formResetBuffer(newBuf, newBuf->formitem);
    return newBuf;
}
