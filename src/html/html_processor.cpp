#include "html_processor.h"
#include "html/html_context.h"

#include "indep.h"
#include "gc_helper.h"
#include "html/form.h"
#include "loader.h"
#include "file.h"
#include "myctype.h"
#include "entity.h"
#include "stream/compression.h"
#include "html/image.h"
#include "symbol.h"
#include "tagstack.h"
#include "frame.h"
#include "public.h"
#include "commands.h"
#include "html/maparea.h"
#include "html/tokenizer.h"
#include "w3m.h"
#include "textlist.h"

#include "frontend/line.h"
#include "stream/input_stream.h"
#include <signal.h>
#include <setjmp.h>
#include <vector>

///
/// entry
///
BufferPtr loadHTMLStream(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset, bool internal)
{
    auto newBuf = newBuffer(url);
    newBuf->type = "text/html";

    // if (newBuf->currentURL.path.size())
    // {
    //     *GetCurBaseUrl() = *newBuf->BaseURL();
    // }

    struct environment envs[MAX_ENV_LEVEL];
    struct html_feed_environ htmlenv1;
    struct readbuffer obuf;
    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, nullptr, newBuf->width, 0);
    htmlenv1.buf = newTextLineList();

    //
    //
    //
    clen_t linelen = 0;
    clen_t trbyte = 0;
    HtmlContext context;

    context.Initialize(newBuf, content_charset);

    auto success = TrapJmp([&]() {
        // if (stream->type() != IST_ENCODED)
        //     stream = newEncodedStream(stream, f->encoding);

        Str lineBuf2 = nullptr;
        while ((lineBuf2 = stream->mygets())->Size())
        {
            // if (f->scheme == SCM_NEWS && lineBuf2->ptr[0] == '.')
            // {
            //     lineBuf2->Delete(0, 1);
            //     if (lineBuf2->ptr[0] == '\n' || lineBuf2->ptr[0] == '\r' ||
            //         lineBuf2->ptr[0] == '\0')
            //     {
            //         /*
            //      * iseos(stream) = true;
            //      */
            //         break;
            //     }
            // }

            linelen += lineBuf2->Size();
            // if (w3mApp::Instance().w3m_dump & DUMP_EXTRA)
            //     printf("W3m-in-progress: %s\n", convert_size2(linelen, GetCurrentContentLength(), true));
            showProgress(&linelen, &trbyte, 0);
            /*
            * if (frame_source)
            * continue;
            */

            CharacterEncodingScheme detected;
            lineBuf2 = convertLine(url.scheme, lineBuf2, HTML_MODE, &detected, context.DocCharset());
            context.SetCES(detected);

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

    // if (w3mApp::Instance().w3m_dump & DUMP_HALFDUMP)
    // {
    //     context.print_internal_information(&htmlenv1);
    //     return;
    // }
    // if (w3mApp::Instance().w3m_backend)
    // {
    //     context.print_internal_information(&htmlenv1);
    //     backend_halfdump_buf = htmlenv1.buf;
    //     return;
    // }

    newBuf->trbyte = trbyte + linelen;

    if (!(newBuf->bufferprop & BP_FRAME))
        newBuf->document_charset = context.DocCharset();

    int image_flag;
    if (newBuf->image_flag)
        image_flag = newBuf->image_flag;
    else if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && w3mApp::Instance().autoImage)
        image_flag = IMG_FLAG_AUTO;
    else
        image_flag = IMG_FLAG_SKIP;
    newBuf->image_flag = image_flag;

    {
        auto feed = [feeder = TextFeeder{htmlenv1.buf->first}]() -> Str {
            return feeder();
        };

        context.BufferFromLines(newBuf, feed);
    }

    return newBuf;
}

// /*
//  * loadHTMLBuffer: read file and make new buffer
//  */
// BufferPtr loadHTMLBuffer(const URLFilePtr &f)
// {
//     // FILE *src = nullptr;
//     //
//     // auto newBuf = newBuffer(INIT_BUFFER_WIDTH());
//     // if (newBuf->sourcefile.empty() &&
//     //     (f->scheme != SCM_LOCAL || newBuf->mailcap))
//     // {
//     //     auto tmp = tmpfname(TMPF_SRC, ".html");
//     //     src = fopen(tmp->ptr, "w");
//     //     if (src)
//     //         newBuf->sourcefile = tmp->ptr;
//     // }

//     auto newBuf = loadHTMLStream(f, false /*newBuf->bufferprop & BP_FRAME*/);

//     newBuf->CurrentAsLast();

//     formResetBuffer(newBuf, newBuf->formitem);

//     return newBuf;
// }

/* 
 * loadHTMLString: read string and make new buffer
 */
BufferPtr loadHTMLString(const URL &url, std::string_view page, CharacterEncodingScheme content_charset)
{
    BufferPtr newBuf = nullptr;

    auto success = TrapJmp([&]() {
        auto f = newStrStream(page);

        newBuf = loadHTMLStream(url, f, content_charset, true);
        newBuf->document_charset = w3mApp::Instance().InnerCharset;
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
