#include "stream/loader.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "mytime.h"
#include "urimethod.h"
#include "stream/local_cgi.h"
#include "stream/http.h"
#include "charset.h"
#include "stream/auth.h"
#include "mime/mimetypes.h"
#include "mime/mimeencoding.h"
#include "myctype.h"
#include "stream/compression.h"
#include "stream/cookie.h"
#include "public.h"
#include "html/image.h"
#include "frontend/display.h"
#include "frontend/terms.h"
#include "mime/mailcap.h"
#include "mime/mimetypes.h"
#include "frontend/lineinput.h"
#include "stream/istream.h"
#include "html/html_processor.h"
#include "html/form.h"
#include <assert.h>
#include <memory>

#include <setjmp.h>
#include <signal.h>

static int checkCopyFile(const char *path1, const char *path2)
{
    struct stat st1, st2;

    if (*path2 == '|' && PermitSaveToPipe)
        return 0;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

static int _MoveFile(const char *path1, const char *path2)
{
    auto f1 = openIS(path1);
    if (f1 == NULL)
        return -1;

    FILE *f2;
    bool is_pipe;
    if (*path2 == '|' && PermitSaveToPipe)
    {
        is_pipe = TRUE;
        f2 = popen(path2 + 1, "w");
    }
    else
    {
        is_pipe = FALSE;
        f2 = fopen(path2, "wb");
    }
    if (f2 == NULL)
    {
        return -1;
    }

    clen_t linelen = 0;
    clen_t trbyte = 0;
    auto buf = Strnew_size(SAVE_BUF_SIZE);
    while (f1->readto(buf, SAVE_BUF_SIZE))
    {
        buf->Puts(f2);
        linelen += buf->Size();
        showProgress(&linelen, &trbyte, 0);
    }
    if (is_pipe)
        pclose(f2);
    else
        fclose(f2);
    return 0;
}

static int _doFileCopy(const char *tmpf, const char *defstr, int download)
{
#ifndef __MINGW32_VERSION
    Str msg;
    Str filen;
    char *p, *q = NULL;
    pid_t pid;
    char *lock;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
    FILE *f;
#endif
    struct stat st;
    clen_t size = 0;
    int is_pipe = FALSE;

    if (fmInitialized)
    {
        p = searchKeyData();
        if (p == NULL || *p == '\0')
        {
            /* FIXME: gettextize? */
            q = inputLineHist("(Download)Save file to: ",
                              defstr, IN_COMMAND, w3mApp::Instance().SaveHist);
            if (q == NULL || *q == '\0')
                return FALSE;
            p = conv_to_system(q);
        }
        if (*p == '|' && PermitSaveToPipe)
            is_pipe = TRUE;
        else
        {
            if (q)
            {
                p = unescape_spaces(Strnew(q))->ptr;
                p = conv_to_system(q);
            }
            p = expandPath(p);
            if (checkOverWrite(p) < 0)
                return -1;
        }
        if (checkCopyFile(tmpf, p) < 0)
        {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't copy. %s and %s are identical.",
                          conv_from_system(tmpf), conv_from_system(p));
            disp_err_message(msg->ptr, FALSE);
            return -1;
        }
        if (!download)
        {
            if (_MoveFile(tmpf, p) < 0)
            {
                /* FIXME: gettextize? */
                msg = Sprintf("Can't save to %s", conv_from_system(p));
                disp_err_message(msg->ptr, FALSE);
            }
            return -1;
        }
        lock = tmpfname(TMPF_DFL, ".lock")->ptr;
#if defined(HAVE_SYMLINK) && defined(HAVE_LSTAT)
        symlink(p, lock);
#else
        f = fopen(lock, "w");
        if (f)
            fclose(f);
#endif
        flush_tty();
        pid = fork();
        if (!pid)
        {
            setup_child(FALSE, 0, -1);
            if (!_MoveFile(tmpf, p) && PreserveTimestamp && !is_pipe &&
                !stat(tmpf, &st))
                setModtime(p, st.st_mtime);
            unlink(lock);
            exit(0);
        }
        if (!stat(tmpf, &st))
            size = st.st_size;
        addDownloadList(pid, conv_from_system(tmpf), p, lock, size);
    }
    else
    {
        q = searchKeyData();
        if (q == NULL || *q == '\0')
        {
            /* FIXME: gettextize? */
            printf("(Download)Save file to: ");
            fflush(stdout);
            filen = Strfgets(stdin);
            if (filen->Size() == 0)
                return -1;
            q = filen->ptr;
        }
        for (p = q + strlen(q) - 1; IS_SPACE(*p); p--)
            ;
        *(p + 1) = '\0';
        if (*q == '\0')
            return -1;
        p = q;
        if (*p == '|' && PermitSaveToPipe)
            is_pipe = TRUE;
        else
        {
            p = expandPath(p);
            if (checkOverWrite(p) < 0)
                return -1;
        }
        if (checkCopyFile(tmpf, p) < 0)
        {
            /* FIXME: gettextize? */
            printf("Can't copy. %s and %s are identical.", tmpf, p);
            return -1;
        }
        if (_MoveFile(tmpf, p) < 0)
        {
            /* FIXME: gettextize? */
            printf("Can't save to %s\n", p);
            return -1;
        }
        if (PreserveTimestamp && !is_pipe && !stat(tmpf, &st))
            setModtime(p, st.st_mtime);
    }
#endif /* __MINGW32_VERSION */
    return 0;
}

// BufferPtr loadSomething(const URL &url, const InputStreamPtr &stream, LoaderFunc loadproc)
// {
//     auto buf = loadproc(url, stream);
//     if (buf)
//     {
//         // buf->filename = path;
//         // if (buf->buffername.empty() || buf->buffername[0] == '\0')
//         // {
//         //     auto buffername = checkHeader(buf, "Subject:");
//         //     buf->buffername = buffername ? buffername : "";
//         //     if (buf->buffername.empty())
//         //         buf->buffername = conv_from_system(lastFileName(path));
//         // }
//         // if (buf->currentURL.scheme == SCM_UNKNOWN)
//         //     buf->currentURL.scheme = f->scheme;
//         // buf->real_scheme = f->scheme;
//         // if (f->scheme == SCM_LOCAL && buf->sourcefile.empty())
//         //     buf->sourcefile = path;
//     }

//     return buf;
// }

BufferPtr loadcmdout(const char *cmd, LoaderFunc loadproc, CharacterEncodingScheme content_charset)
{
    if (cmd == NULL || *cmd == '\0')
        return NULL;

    FILE *popen(const char *, const char *);
    auto f = popen(cmd, "r");
    if (f == NULL)
        return NULL;

    auto stream = newFileStream(f, pclose);
    auto url = URL::Parse(cmd);

    return loadproc(url, stream, content_charset);
}

BufferPtr LoadStream(const URL &url, const InputStreamPtr &stream, std::string_view content_type, CharacterEncodingScheme content_charset)
{
    // if (t_buf == NULL)
    auto t_buf = newBuffer(url);

    // readHeader(f, t_buf, FALSE, &pu);
    // char *p;
    // if (((http_response_code >= 301 && http_response_code <= 303) || http_response_code == 307) &&
    //     (p = checkHeader(t_buf, "Location:")) != NULL && checkRedirection(pu))
    // {
    //     /* document moved */
    //     /* 301: Moved Permanently */
    //     /* 302: Found */
    //     /* 303: See Other */
    //     /* 307: Temporary Redirect (HTTP/1.1) */
    //     auto tpath = wc_conv_strict(p, w3mApp::Instance().InnerCharset, w3mApp::Instance().DocumentCharset)->ptr;
    //     // request = NULL;
    //     // f.Close();
    //     // *current = pu;
    //     t_buf = newBuffer(INIT_BUFFER_WIDTH());
    //     t_buf->bufferprop |= BP_REDIRECTED;
    //     // auto status = HTST_NORMAL;
    //     // goto load_doc;

    //     // TODO: REDIRECT

    //     assert(false);
    // }
    // auto t = "text/plain";
    // // const char *t = checkContentType(t_buf);
    // // if (t == NULL && pu.path.size())
    // // {
    // //     if (!((http_response_code >= 400 && http_response_code <= 407) ||
    // //           (http_response_code >= 500 && http_response_code <= 505)))
    // //         t = guessContentType(pu.path);
    // // }
    // if (t == NULL)
    //     t = "text/plain";

    /* XXX: can we use guess_type to give the type to loadHTMLStream
         *      to support default utf8 encoding for XHTML here? */
    // f->guess_type = t;

    // if (real_type == NULL)
    //     real_type = t;
    auto proc = loadBuffer;

    *GetCurBaseUrl() = url;

    // if ((p = checkHeader(t_buf, "Content-Length:")) != NULL)
    //     current_content_length = strtoclen(p);
    if (do_download)
    {
        /* download only */
        char *file;
        // TRAP_OFF;
        // if (DecodeCTE && f->stream->type() != IST_ENCODED)
        //     f->stream = newEncodedStream(f->stream, f->encoding);
        // if (pu.scheme == SCM_LOCAL)
        // {
        //     struct stat st;
        //     if (PreserveTimestamp && !stat(pu.real_file.c_str(), &st))
        //         f->modtime = st.st_mtime;
        //     file = conv_from_system(guess_save_name(NULL, pu.real_file.c_str()));
        // }
        // else
        //     file = guess_save_name(t_buf, pu.path);

        // f->DoFileSave(file, 0);

        return nullptr;
    }

    // if ((f->content_encoding != CMP_NOCOMPRESS) && AutoUncompress && !(w3mApp::Instance().w3m_dump & DUMP_EXTRA))
    // {
    //     // TODO:
    //     // pu.real_file = uncompress_stream(&f, true);
    // }
    // else

    // if (f->compression != CMP_NOCOMPRESS)
    // {
    //     // compressed
    //     if (!(w3mApp::Instance().w3m_dump & DUMP_SOURCE) &&
    //         (w3mApp::Instance().w3m_dump & ~DUMP_FRAME || is_text_type(t) || searchExtViewer(t)))
    //     {
    //         // extract compression
    //         if (t_buf == NULL)
    //             t_buf = newBuffer(INIT_BUFFER_WIDTH());
    //         t_buf->sourcefile = uncompress_stream(f, true);
    //         auto [type, ext] = uncompressed_file_type(pu.path);
    //         if (ext.size())
    //         {
    //             f->ext = ext.data();
    //         }
    //     }
    //     else
    //     {
    //         t = compress_application_type(f->compression).data();
    //         f->compression = CMP_NOCOMPRESS;
    //     }
    // }

    // if (image_source)
    // {
    //     BufferPtr b = NULL;
    //     if (f->stream->type() != IST_ENCODED)
    //         f->stream = newEncodedStream(f->stream, f->encoding);
    //     if (save2tmp(f, image_source) == 0)
    //     {
    //         b = newBuffer(INIT_BUFFER_WIDTH());
    //         b->sourcefile = image_source;
    //         b->real_type = t;
    //     }
    //     // f->Close();
    //     // TRAP_OFF;
    //     return b;
    // }

    if (!(w3mApp::Instance().w3m_dump & ~DUMP_FRAME) || is_dump_text_type(content_type))
    {
        BufferPtr b = NULL;
        if (!do_download)
        {
            // auto url = URL::Parse(pu.real_file.size() ? pu.real_file.c_str() : pu.path.c_str());
            auto b = doExternal(url, stream, content_type);
            if (b)
            {
                b->real_scheme = url.scheme;
                b->real_type = content_type;
                if (b->currentURL.host.empty() && b->currentURL.path.empty())
                    b->currentURL = url;
            }
            // f.Close();
            // TRAP_OFF;
            return b;
        }
        else
        {
            // TRAP_OFF;
            // if (pu.scheme == SCM_LOCAL)
            // {
            //     // f.Close();
            //     _doFileCopy(const_cast<char *>(pu.real_file.c_str()),
            //                 conv_from_system(guess_save_name(NULL, pu.real_file)), TRUE);
            // }
            // else
            // {
            //     if (DecodeCTE && f->stream->type() != IST_ENCODED)
            //         f->stream = newEncodedStream(f->stream, f->encoding);
            //     f->DoFileSave(guess_save_name(t_buf, pu.path), current_content_length);
            // }
            return nullptr;
        }
    }
    else if (w3mApp::Instance().w3m_dump & DUMP_FRAME)
        return NULL;

    // if (flag & RG_FRAME)
    // {
    //     if (t_buf == NULL)
    //         t_buf = newBuffer(url);
    //     t_buf->bufferprop |= BP_FRAME;
    // }

    // if (t_buf && f->ssl_certificate)
    // {
    //     t_buf->ssl_certificate = f->ssl_certificate;
    // }

    // frame_source = flag & RG_FRAME_SRC;
    // auto b = loadSomething(f, pu.real_file.size() ? const_cast<char *>(pu.real_file.c_str()) : const_cast<char *>(pu.path.c_str()), proc);
    BufferPtr b;
    if (is_html_type(content_type))
    {
        b = loadHTMLStream(url, stream, content_charset);
    }
    else if (is_plain_text_type(content_type))
    {
        b = loadBuffer(url, stream, content_charset);
    }
    else if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && !useExtImageViewer &&
             !(w3mApp::Instance().w3m_dump & ~DUMP_FRAME) && content_type.starts_with("image/"))
    {
        b = loadImageBuffer(url, stream);
    }
    else
    {
        assert(false);
    }
    if (b)
    {
        b->real_scheme = url.scheme;
        b->real_type = content_type;
        if (b->currentURL.host.empty() && b->currentURL.path.empty())
            b->currentURL = url;
        if (is_html_type(content_type))
            b->type = "text/html";
        else if (w3mApp::Instance().w3m_backend)
        {
            Str s = Strnew(content_type);
            b->type = s->ptr;
        }
        // else if (proc == loadImageBuffer)
        //     b->type = "text/html";
        // else
        //     b->type = "text/plain";
        if (url.fragment.size())
        {
            // if (proc == loadHTMLBuffer)
            // {
            //     auto a = searchURLLabel(b, const_cast<char *>(url.fragment.c_str()));
            //     if (a != NULL)
            //     {
            //         b->Goto(a->start, label_topline);
            //     }
            // }
            // else
            // { /* plain text */
            //     int l = atoi(url.fragment.c_str());
            //     b->GotoRealLine(l);
            //     b->pos = 0;
            //     b->ArrangeCursor();
            // }
        }
    }
    if (w3mApp::Instance().header_string.size())
        w3mApp::Instance().header_string.clear();
    preFormUpdateBuffer(b);
    return b;
}

/* 
 * loadBuffer: read file and make new buffer
 */
BufferPtr loadBuffer(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset)
{
    FILE *src = NULL;

    CharacterEncodingScheme charset = WC_CES_US_ASCII;
    CharacterEncodingScheme doc_charset = w3mApp::Instance().DocumentCharset;

    Str lineBuf2;
    char pre_lbuf = '\0';
    int nlines;
    Str tmpf;
    clen_t linelen = 0, trbyte = 0;

    MySignalHandler prevtrap = NULL;

    lineBuf2 = Strnew();

    // if (SETJMP(AbortLoading) != 0)
    // {
    //     goto _end;
    // }
    // TRAP_ON;

    auto newBuf = newBuffer(url);
    // if (newBuf->sourcefile.empty() &&
    //     (uf->scheme != SCM_LOCAL || newBuf->mailcap))
    // {
    //     tmpf = tmpfname(TMPF_SRC, NULL);
    //     src = fopen(tmpf->ptr, "w");
    //     if (src)
    //         newBuf->sourcefile = tmpf->ptr;
    // }

    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (content_charset && w3mApp::Instance().UseContentCharset)
        doc_charset = content_charset;

    nlines = 0;
    // if (uf->stream->type() != IST_ENCODED)
    //     uf->stream = newEncodedStream(uf->stream, uf->encoding);
    while ((lineBuf2 = stream->mygets())->Size())
    {
        if (src)
            lineBuf2->Puts(src);
        linelen += lineBuf2->Size();
        // if (w3mApp::Instance().w3m_dump & DUMP_EXTRA)
        //     printf("W3m-in-progress: %s\n", convert_size2(linelen, current_content_length, TRUE));
        if (w3mApp::Instance().w3m_dump & DUMP_SOURCE)
            continue;
        showProgress(&linelen, &trbyte, 0);
        // if (frame_source)
        //     continue;
        lineBuf2 =
            convertLine(url.scheme, lineBuf2, PAGER_MODE, &charset, doc_charset);
        if (w3mApp::Instance().squeezeBlankLine)
        {
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n')
            {
                ++nlines;
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        ++nlines;
        StripRight(lineBuf2);
        newBuf->AddNewLine(PropertiedString(lineBuf2), nlines);
    }
_end:
    TRAP_OFF;

    newBuf->CurrentAsLast();

    newBuf->trbyte = trbyte + linelen;
    newBuf->document_charset = charset;
    if (src)
        fclose(src);
}

int doFileCopy(const char *tmpf, const char *defstr)
{
    return _doFileCopy(tmpf, defstr, FALSE);
}

BufferPtr LoadPage(Str page, CharacterEncodingScheme charset, const URL &pu, const char *t)
{
    FILE *src;
    if (image_source)
        return NULL;

    auto tmp = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmp->ptr, "w");
    if (src)
    {
        Str s;
        s = wc_Str_conv_strict(page, w3mApp::Instance().InnerCharset, charset);
        s->Puts(src);
        fclose(src);
    }
    if (do_download)
    {
        char *file;
        if (!src)
            return NULL;
        file = guess_filename(pu.path);
        doFileMove(tmp->ptr, file);
        return nullptr;
    }
    auto b = loadHTMLString({}, page);
    if (b)
    {
        b->currentURL = pu;
        b->real_scheme = pu.scheme;
        b->real_type = t;
        if (src)
            b->sourcefile = tmp->ptr;
        b->document_charset = charset;
    }
    return b;
}

/* 
 * Dispatch URL, return Buffer
 */
BufferPtr loadGeneralFile(const URL &url, const URL *_current, HttpReferrerPolicy referer, FormList *form)
{
    if (url.scheme == SCM_HTTP || url.scheme == SCM_HTTPS)
    {
        //
        // HTTP
        //
        HttpClient client;
        return client.Request(url, _current, referer, form);
    }

    if (url.scheme == SCM_LOCAL_CGI)
    {
        //
        // local CGI
        //
        LocalCGI cgi;
        return cgi.Request(url, _current, referer, form);
    }

    if (url.scheme == SCM_LOCAL)
    {
        //
        // local file
        //
        auto stream = StreamFromFile(url.real_file);
        if (!stream)
        {
            // fail to open file
            assert(false);
            return nullptr;
        }
        return LoadStream(url, stream);
    }

    // not implemened
    assert(false);
    return nullptr;
}

// if (f.stream == NULL)
// {
//     switch (f.scheme)
//     {
//     case SCM_LOCAL:
//     {
//         struct stat st;
//         if (stat(pu.real_file.c_str(), &st) < 0)
//             return NULL;
//         if (S_ISDIR(st.st_mode))
//         {
//             if (UseExternalDirBuffer)
//             {
//                 Str cmd = Sprintf("%s?dir=%s#current",
//                                   DirBufferCommand, pu.file);
//                 auto b = loadGeneralFile(cmd->ptr, NULL, HttpReferrerPolicy::NoReferer, RG_NONE, NULL);
//                 if (b != NULL)
//                 {
//                     b->currentURL = pu;
//                     b->filename = b->currentURL.real_file;
//                 }
//                 return b;
//             }
//             else
//             {
//                 page = loadLocalDir(const_cast<char *>(pu.real_file.c_str()));
//                 t = "local:directory";
//                 charset = w3mApp::Instance().SystemCharset;
//             }
//         }
//     }
//     break;

//     case SCM_UNKNOWN:
//     {
//         auto tmp = searchURIMethods(&pu);
//         if (tmp != NULL)
//         {
//             auto b = loadGeneralFile(tmp->ptr, _current, referer, flag, request);
//             if (b != NULL)
//                 b->currentURL = pu;
//             return b;
//         }
//         /* FIXME: gettextize? */
//         disp_err_message(Sprintf("Unknown URI: %s",
//                                  pu.ToStr()->ptr)
//                              ->ptr,
//                          FALSE);
//         break;
//     }

// f.modtime = mymktime(checkHeader(t_buf, "Last-Modified:"));
