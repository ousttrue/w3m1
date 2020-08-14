#include "transport/loader.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "mytime.h"
#include "urimethod.h"
#include "transport/local_cgi.h"
#include "http/http_request.h"
#include "charset.h"
#include "http/auth.h"
#include "mime/mimetypes.h"
#include "mime/mimeencoding.h"
#include "myctype.h"
#include "http/compression.h"
#include "http/cookie.h"
#include "public.h"
#include "html/image.h"
#include "frontend/display.h"
#include "frontend/terms.h"
#include "mime/mailcap.h"
#include "frontend/lineinput.h"
#include "transport/istream.h"
#include "html/html_processor.h"
#include "html/form.h"
#include <assert.h>
#include <memory>

#include <setjmp.h>
#include <signal.h>
// static JMP_BUF AbortLoading;
// static void KeyAbort(SIGNAL_ARG)
// {
//     LONGJMP(AbortLoading, 1);
//     SIGNAL_RETURN;
// }

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

static int
_MoveFile(const char *path1, const char *path2)
{
    InputStreamPtr f1;
    FILE *f2;
    int is_pipe;
    clen_t linelen = 0, trbyte = 0;
    Str buf;

    f1 = openIS(path1);
    if (f1 == NULL)
        return -1;
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
        // ISclose(f1);
        return -1;
    }
    // current_content_length = 0;
    buf = Strnew_size(SAVE_BUF_SIZE);
    while (f1->readto(buf, SAVE_BUF_SIZE))
    {
        buf->Puts(f2);
        linelen += buf->Size();
        showProgress(&linelen, &trbyte);
    }
    // ISclose(f1);
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

static bool
loadSomething(const URLFilePtr &f,
              char *path,
              LoaderFunc loadproc, BufferPtr buf)
{
    if (!loadproc(f, buf))
        return false;

    buf->filename = path;
    if (buf->buffername.empty() || buf->buffername[0] == '\0')
    {
        auto buffername = checkHeader(buf, "Subject:");
        buf->buffername = buffername ? buffername : "";
        if (buf->buffername.empty())
            buf->buffername = conv_from_system(lastFileName(path));
    }
    if (buf->currentURL.scheme == SCM_UNKNOWN)
        buf->currentURL.scheme = f->scheme;
    buf->real_scheme = f->scheme;
    if (f->scheme == SCM_LOCAL && buf->sourcefile.empty())
        buf->sourcefile = path;

    return true;
}

///
/// HTTP
///
int http_response_code;
long long current_content_length;
long long GetCurrentContentLength()
{
    return current_content_length;
}
int frame_source = 0;

//
// HTTP redirection
//
class HttpContext
{
    std::vector<URL> g_puv;

public:
    bool checkRedirection(const URL &pu)
    {
        assert(pu);
        Str tmp;

        if (g_puv.size() >= FollowRedirection)
        {
            /* FIXME: gettextize? */
            auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                               FollowRedirection, pu.ToStr()->ptr);
            disp_err_message(tmp->ptr, FALSE);
            return false;
        }

        g_puv.push_back(pu);
        return true;
    }

    BufferPtr Get(const URLFilePtr &f, const URL &pu, LoadFlags flag)
    {
        if (fmInitialized)
        {
            term_cbreak();
            /* FIXME: gettextize? */
            message(Sprintf("%s contacted. Waiting for reply...", pu.host.c_str())->ptr, 0, 0);
            refresh();
        }
        // if (t_buf == NULL)
        auto t_buf = newBuffer(INIT_BUFFER_WIDTH());

        readHeader(f, t_buf, FALSE, &pu);
        char *p;
        if (((http_response_code >= 301 && http_response_code <= 303) || http_response_code == 307) &&
            (p = checkHeader(t_buf, "Location:")) != NULL && checkRedirection(pu))
        {
            /* document moved */
            /* 301: Moved Permanently */
            /* 302: Found */
            /* 303: See Other */
            /* 307: Temporary Redirect (HTTP/1.1) */
            auto tpath = wc_conv_strict(p, w3mApp::Instance().InnerCharset, w3mApp::Instance().DocumentCharset)->ptr;
            // request = NULL;
            // f.Close();
            // *current = pu;
            t_buf = newBuffer(INIT_BUFFER_WIDTH());
            t_buf->bufferprop |= BP_REDIRECTED;
            // auto status = HTST_NORMAL;
            // goto load_doc;

            // TODO: REDIRECT

            assert(false);
        }
        const char *t = checkContentType(t_buf);
        if (t == NULL && pu.path.size())
        {
            if (!((http_response_code >= 400 && http_response_code <= 407) ||
                  (http_response_code >= 500 && http_response_code <= 505)))
                t = guessContentType(pu.path);
        }
        if (t == NULL)
            t = "text/plain";

        /* XXX: can we use guess_type to give the type to loadHTMLstream
         *      to support default utf8 encoding for XHTML here? */
        f->guess_type = t;

        // if (real_type == NULL)
        //     real_type = t;
        auto proc = loadBuffer;

        *GetCurBaseUrl() = pu;

        current_content_length = 0;
        if ((p = checkHeader(t_buf, "Content-Length:")) != NULL)
            current_content_length = strtoclen(p);
        if (do_download)
        {
            /* download only */
            char *file;
            // TRAP_OFF;
            if (DecodeCTE && f->stream->type() != IST_ENCODED)
                f->stream = newEncodedStream(f->stream, f->encoding);
            if (pu.scheme == SCM_LOCAL)
            {
                struct stat st;
                if (PreserveTimestamp && !stat(pu.real_file.c_str(), &st))
                    f->modtime = st.st_mtime;
                file = conv_from_system(guess_save_name(NULL, pu.real_file.c_str()));
            }
            else
                file = guess_save_name(t_buf, pu.path);

            f->DoFileSave(file, current_content_length);

            return nullptr;
        }

        if ((f->content_encoding != CMP_NOCOMPRESS) && AutoUncompress && !(w3mApp::Instance().w3m_dump & DUMP_EXTRA))
        {
            // TODO:
            // pu.real_file = uncompress_stream(&f, true);
        }
        else if (f->compression != CMP_NOCOMPRESS)
        {
            if (!(w3mApp::Instance().w3m_dump & DUMP_SOURCE) &&
                (w3mApp::Instance().w3m_dump & ~DUMP_FRAME || is_text_type(t) || searchExtViewer(t)))
            {
                if (t_buf == NULL)
                    t_buf = newBuffer(INIT_BUFFER_WIDTH());
                t_buf->sourcefile = uncompress_stream(f, true);
                uncompressed_file_type(pu.path.c_str(), &f->ext);
            }
            else
            {
                t = compress_application_type(f->compression);
                f->compression = CMP_NOCOMPRESS;
            }
        }

        if (image_source)
        {
            BufferPtr b = NULL;
            if (f->stream->type() != IST_ENCODED)
                f->stream = newEncodedStream(f->stream, f->encoding);
            if (save2tmp(f, image_source) == 0)
            {
                b = newBuffer(INIT_BUFFER_WIDTH());
                b->sourcefile = image_source;
                b->real_type = t;
            }
            // f->Close();
            // TRAP_OFF;
            return b;
        }

        if (is_html_type(t))
            proc = loadHTMLBuffer;
        else if (is_plain_text_type(t))
            proc = loadBuffer;
        else if (w3mApp::Instance().activeImage && w3mApp::Instance().displayImage && !useExtImageViewer &&
                 !(w3mApp::Instance().w3m_dump & ~DUMP_FRAME) && !strncasecmp(t, "image/", 6))
            proc = loadImageBuffer;
        else if (w3mApp::Instance().w3m_backend)
            ;
        else if (!(w3mApp::Instance().w3m_dump & ~DUMP_FRAME) || is_dump_text_type(t))
        {
            BufferPtr b = NULL;
            if (!do_download && doExternal(f,
                                           pu.real_file.size() ? const_cast<char *>(pu.real_file.c_str()) : const_cast<char *>(pu.path.c_str()),
                                           t, &b, t_buf))
            {
                if (b)
                {
                    b->real_scheme = f->scheme;
                    b->real_type = t;
                    if (b->currentURL.host.empty() && b->currentURL.path.empty())
                        b->currentURL = pu;
                }
                // f.Close();
                // TRAP_OFF;
                return b;
            }
            else
            {
                // TRAP_OFF;
                if (pu.scheme == SCM_LOCAL)
                {
                    // f.Close();
                    _doFileCopy(const_cast<char *>(pu.real_file.c_str()),
                                conv_from_system(guess_save_name(NULL, pu.real_file)), TRUE);
                }
                else
                {
                    if (DecodeCTE && f->stream->type() != IST_ENCODED)
                        f->stream = newEncodedStream(f->stream, f->encoding);
                    f->DoFileSave(guess_save_name(t_buf, pu.path), current_content_length);
                }
                return nullptr;
            }
        }
        else if (w3mApp::Instance().w3m_dump & DUMP_FRAME)
            return NULL;

        if (flag & RG_FRAME)
        {
            if (t_buf == NULL)
                t_buf = newBuffer(INIT_BUFFER_WIDTH());
            t_buf->bufferprop |= BP_FRAME;
        }

        if (t_buf && f->ssl_certificate)
        {
            t_buf->ssl_certificate = f->ssl_certificate;
        }

        frame_source = flag & RG_FRAME_SRC;
        auto success = loadSomething(f,
                                     pu.real_file.size() ? const_cast<char *>(pu.real_file.c_str()) : const_cast<char *>(pu.path.c_str()),
                                     proc,
                                     t_buf);
        assert(success);
        auto b = t_buf;
        f->stream = nullptr;
        // f.Close();
        frame_source = 0;
        if (success)
        {
            b->real_scheme = f->scheme;
            b->real_type = t;
            if (b->currentURL.host.empty() && b->currentURL.path.empty())
                b->currentURL = pu;
            if (is_html_type(t))
                b->type = "text/html";
            else if (w3mApp::Instance().w3m_backend)
            {
                Str s = Strnew(t);
                b->type = s->ptr;
            }
            else if (proc == loadImageBuffer)
                b->type = "text/html";
            else
                b->type = "text/plain";
            if (pu.fragment.size())
            {
                if (proc == loadHTMLBuffer)
                {
                    auto a = searchURLLabel(b, const_cast<char *>(pu.fragment.c_str()));
                    if (a != NULL)
                    {
                        b->Goto(a->start, label_topline);
                    }
                }
                else
                { /* plain text */
                    int l = atoi(pu.fragment.c_str());
                    b->GotoRealLine(l);
                    b->pos = 0;
                    b->ArrangeCursor();
                }
            }
        }
        if (w3mApp::Instance().header_string.size())
            w3mApp::Instance().header_string.clear();
        preFormUpdateBuffer(b);
        return b;
    }
};

///
/// charset
///
CharacterEncodingScheme content_charset = WC_CES_NONE;
CharacterEncodingScheme meta_charset = WC_CES_NONE;
void SetMetaCharset(CharacterEncodingScheme ces)
{
    meta_charset = ces;
}

/* 
 * loadFile: load file to buffer
 */
BufferPtr loadFile(char *path)
{
    auto uf = URLFile::OpenFile(path);
    if (uf->stream == NULL)
        return NULL;

    auto buf = newBuffer(INIT_BUFFER_WIDTH());
    current_content_length = 0;
    content_charset = WC_CES_NONE;
    auto success = loadSomething(uf, path, loadBuffer, buf);
    if (!success)
    {
        return nullptr;
    }
    return buf;
}

char *
checkContentType(BufferPtr buf)
{
    char *p;
    Str r;
    p = checkHeader(buf, "Content-Type:");
    if (p == NULL)
        return NULL;
    r = Strnew();
    while (*p && *p != ';' && !IS_SPACE(*p))
        r->Push(*p++);

    if ((p = strcasestr(p, "charset")) != NULL)
    {
        p += 7;
        SKIP_BLANKS(&p);
        if (*p == '=')
        {
            p++;
            SKIP_BLANKS(&p);
            if (*p == '"')
                p++;
            content_charset = wc_guess_charset(p, WC_CES_NONE);
        }
    }

    return r->ptr;
}

void readHeader(const URLFilePtr &uf, BufferPtr newBuf, int thru, const URL *pu)
{
    http_response_code = 0;
    if (uf->scheme == SCM_HTTP || uf->scheme == SCM_HTTPS)
    {
        http_response_code = -1;
    }

    // thru
    FILE *src = nullptr;
    if (thru && !newBuf->header_source.size() && !image_source)
    {
        auto tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
        src = fopen(tmpf, "w");
        if (src)
            newBuf->header_source = tmpf;
    }

    // char *p, *q;
    // char *emsg;
    // char c;
    // CharacterEncodingScheme charset = WC_CES_US_ASCII, mime_charset;
    // char *tmpf;
    // FILE *src = NULL;

    newBuf->document_header = newTextList();
    while (true)
    {
        auto lineBuf2 = uf->stream->mygets();
        if (lineBuf2->Size() == 0)
        {
            break;
        }
        if (w3mApp::Instance().w3m_reqlog.size())
        {
            // loging
            auto ff = fopen(w3mApp::Instance().w3m_reqlog.c_str(), "a");
            lineBuf2->Puts(ff);
            fclose(ff);
        }
        if (src)
        {
            lineBuf2->Puts(src);
        }

        cleanup_line(lineBuf2, HEADER_MODE);
        if (lineBuf2->ptr[0] == '\n' || lineBuf2->ptr[0] == '\r' || lineBuf2->ptr[0] == '\0')
        {
            /* there is no header */
            break;
            /* last header */
        }
        //         else if (!(w3mApp::Instance().w3m_dump & DUMP_HEAD))
        //         {
        //             if (lineBuf2)
        //             {
        //                 lineBuf2->Push(tmp);
        //             }
        //             else
        //             {
        //                 lineBuf2 = tmp;
        //             }
        //             c = uf->Getc();
        //             uf->UndoGetc();
        //             if (c == ' ' || c == '\t')
        //                 /* header line is continued */
        //                 continue;
        //             lineBuf2 = decodeMIME(lineBuf2, &mime_charset);
        //             lineBuf2 = convertLine(NULL, lineBuf2, RAW_MODE,
        //                                    mime_charset ? &mime_charset : &charset,
        //                                    mime_charset ? mime_charset
        //                                                 : w3mApp::Instance().DocumentCharset);
        //             /* separated with line and stored */
        //             tmp = Strnew_size(lineBuf2->Size());
        //             for (p = lineBuf2->ptr; *p; p = q)
        //             {
        //                 for (q = p; *q && *q != '\r' && *q != '\n'; q++)
        //                     ;
        //                 tmp->Push(lineBuf2);
        //                 if (thru)
        //                     newBuf->AddNewLine(PropertiedString(lineBuf2));
        //                 for (; *q && (*q == '\r' || *q == '\n'); q++)
        //                     ;
        //             }
        // #ifdef USE_IMAGE
        //             if (thru && w3mApp::Instance().activeImage && w3mApp::Instance().displayImage)
        //             {
        //                 Str src = NULL;
        //                 if (!strncasecmp(tmp->ptr, "X-Image-URL:", 12))
        //                 {
        //                     tmpf = &tmp->ptr[12];
        //                     SKIP_BLANKS(&tmpf);
        //                     src = Strnew_m_charp("<img src=\"", html_quote(tmpf),
        //                                          "\" alt=\"X-Image-URL\">", NULL);
        //                 }
        // #ifdef USE_XFACE
        //                 else if (!strncasecmp(tmp->ptr, "X-Face:", 7))
        //                 {
        //                     tmpf = xface2xpm(&tmp->ptr[7]);
        //                     if (tmpf)
        //                         src = Strnew_m_charp("<img src=\"file:",
        //                                              html_quote(tmpf),
        //                                              "\" alt=\"X-Face\"",
        //                                              " width=48 height=48>", NULL);
        //                 }
        // #endif
        //                 if (src)
        //                 {
        //                     LinePtr l;
        //                     CharacterEncodingScheme old_charset = newBuf->document_charset;
        //                     URLFile f(SCM_LOCAL, newStrStream(src));
        //                     loadHTMLstream(&f, newBuf, NULL, TRUE);
        //                     for (l = newBuf->LastLine(); l && l->real_linenumber;
        //                          l = newBuf->PrevLine(l))
        //                         l->real_linenumber = 0;
        // #ifdef USE_M17N
        //                     newBuf->document_charset = old_charset;
        // #endif
        //                 }
        //             }
        // #endif
        //             lineBuf2 = tmp;
        //         }
        // else

        if ((uf->scheme == SCM_HTTP || uf->scheme == SCM_HTTPS) &&
            http_response_code == -1)
        {
            auto p = lineBuf2->ptr;
            while (*p && !IS_SPACE(*p))
                p++;
            while (*p && IS_SPACE(*p))
                p++;
            http_response_code = atoi(p);
            if (fmInitialized)
            {
                message(lineBuf2->ptr, 0, 0);
                refresh();
            }
        }

        if (strncasecmp(lineBuf2->ptr, "content-transfer-encoding:", 26) == 0)
        {
            auto p = lineBuf2->ptr + 26;
            while (IS_SPACE(*p))
                p++;
            if (!strncasecmp(p, "base64", 6))
                uf->encoding = ENC_BASE64;
            else if (!strncasecmp(p, "quoted-printable", 16))
                uf->encoding = ENC_QUOTE;
            else if (!strncasecmp(p, "uuencode", 8) ||
                     !strncasecmp(p, "x-uuencode", 10))
                uf->encoding = ENC_UUENCODE;
            else
                uf->encoding = ENC_7BIT;
        }
        else if (strncasecmp(lineBuf2->ptr, "content-encoding:", 17) == 0)
        {
            struct compression_decoder *d;
            auto p = lineBuf2->ptr + 17;
            while (IS_SPACE(*p))
                p++;
            uf->compression = CMP_NOCOMPRESS;
            for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
            {
                const char **e;
                for (e = d->encodings; *e != NULL; e++)
                {
                    if (strncasecmp(p, *e, strlen(*e)) == 0)
                    {
                        uf->compression = d->type;
                        break;
                    }
                }
                if (uf->compression != CMP_NOCOMPRESS)
                    break;
            }
            uf->content_encoding = uf->compression;
        }
        else if (w3mApp::Instance().use_cookie && w3mApp::Instance().accept_cookie &&
                 pu && check_cookie_accept_domain(pu->host) &&
                 (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
                  !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12)))
        {
            readHeaderCookie(*pu, lineBuf2);
        }
        else if (strncasecmp(lineBuf2->ptr, "w3m-control:", 12) == 0 &&
                 uf->scheme == SCM_LOCAL_CGI)
        {
            auto p = lineBuf2->ptr + 12;
            SKIP_BLANKS(&p);
            Str funcname = Strnew();
            while (*p && !IS_SPACE(*p))
                funcname->Push(*(p++));
            SKIP_BLANKS(&p);
            Command f = getFuncList(funcname->ptr);
            if (f)
            {
                auto tmp = Strnew(p);
                StripRight(tmp);
                pushEvent(f, tmp->ptr);
            }
        }

        pushText(newBuf->document_header, lineBuf2->ptr);
    }

    if (thru)
    {
        newBuf->AddNewLine(PropertiedString("", 0));
    }

    if (src)
    {
        fclose(src);
    }
}

/* 
 * loadBuffer: read file and make new buffer
 */
bool loadBuffer(const URLFilePtr &uf, BufferPtr newBuf)
{
    assert(newBuf);
    FILE *src = NULL;

    CharacterEncodingScheme charset = WC_CES_US_ASCII;
    CharacterEncodingScheme doc_charset = w3mApp::Instance().DocumentCharset;

    Str lineBuf2;
    char pre_lbuf = '\0';
    int nlines;
    Str tmpf;
    clen_t linelen = 0, trbyte = 0;

    MySignalHandler prevtrap = NULL;

    // if (newBuf == NULL)
    //     newBuf = newBuffer(INIT_BUFFER_WIDTH());
    lineBuf2 = Strnew();

    // if (SETJMP(AbortLoading) != 0)
    // {
    //     goto _end;
    // }
    // TRAP_ON;

    if (newBuf->sourcefile.empty() &&
        (uf->scheme != SCM_LOCAL || newBuf->mailcap))
    {
        tmpf = tmpfname(TMPF_SRC, NULL);
        src = fopen(tmpf->ptr, "w");
        if (src)
            newBuf->sourcefile = tmpf->ptr;
    }

    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (content_charset && w3mApp::Instance().UseContentCharset)
        doc_charset = content_charset;

    nlines = 0;
    if (uf->stream->type() != IST_ENCODED)
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
    while ((lineBuf2 = uf->stream->mygets())->Size())
    {
#ifdef USE_NNTP
        if (uf->scheme == SCM_NEWS && lineBuf2->ptr[0] == '.')
        {
            lineBuf2->Delete(0, 1);
            if (lineBuf2->ptr[0] == '\n' || lineBuf2->ptr[0] == '\r' ||
                lineBuf2->ptr[0] == '\0')
            {
                /*
                 * iseos(uf->stream) = TRUE;
                 */
                break;
            }
        }
#endif /* USE_NNTP */
        if (src)
            lineBuf2->Puts(src);
        linelen += lineBuf2->Size();
        if (w3mApp::Instance().w3m_dump & DUMP_EXTRA)
            printf("W3m-in-progress: %s\n", convert_size2(linelen, current_content_length, TRUE));
        if (w3mApp::Instance().w3m_dump & DUMP_SOURCE)
            continue;
        showProgress(&linelen, &trbyte);
        if (frame_source)
            continue;
        lineBuf2 =
            convertLine(uf->scheme, lineBuf2, PAGER_MODE, &charset, doc_charset);
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
    auto b = loadHTMLString(page);
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
 * 
 * Dispatch URL, return Buffer
 * 
 */
BufferPtr
loadGeneralFile(const URL &url, const URL *_current, HttpReferrerPolicy referer,
                LoadFlags flag, FormList *form)
{
    // this->scheme = url.scheme;
    // this->url = url.ToStr()->ptr;
    // // url.is_nocache = (flag & RG_NOCACHE);
    // this->ext = filename_extension(url.path.c_str(), 1);
    if (url.scheme == SCM_HTTP || url.scheme == SCM_HTTPS)
    {
        //
        // HTTP
        //
        HttpRequest hr(referer, form);
        TextList *extra_header = newTextList();
        auto f = URLFile::OpenHttp(url, _current, referer, flag, form, &hr);
        HttpContext http;
        return http.Get(f, url, flag);
    }
    else if (url.scheme == SCM_LOCAL)
    {
        auto cgi = LocalCGI(url.real_file);
        if (cgi.check_local_cgi())
        {
            //
            // local CGI
            //
            // * "file:///$LIB/w3mbookmark"
            // or
            // * /cgi-bin/w3mbookmark
            //
            URLFilePtr uf;
            {
                FILE *f = nullptr;
                if (form && form->body)
                {
                    /* local CGI: POST */
                    f = localcgi_post(const_cast<char *>(url.real_file.c_str()), const_cast<char *>(url.query.c_str()), form, referer);
                }
                else
                {
                    /* lodal CGI: GET */
                    f = localcgi_get(const_cast<char *>(url.real_file.c_str()), const_cast<char *>(url.query.c_str()), referer);
                }
                auto stream = newFileStream(f, fclose);
                uf = URLFile::OpenStream(SCM_LOCAL, stream);
            }

            assert(uf->stream);
            {
                uf->is_cgi = TRUE;
                // TODO:
                // url.scheme =
                uf->scheme = SCM_LOCAL_CGI;
            }
            // // auto b = NULL;
            if (uf->is_cgi)
            {
                /* local CGI */
                // searchHeader = TRUE;
                // searchHeader_through = FALSE;
            }

            // if (searchHeader)
            // {
            //     searchHeader = w3mApp::Instance().SearchHeader = FALSE;
            //     if (t_buf == NULL)
            //         t_buf = newBuffer(INIT_BUFFER_WIDTH());
            //     readHeader(&f, t_buf, searchHeader_through, &pu);
            //     if (f.is_cgi && (p = checkHeader(t_buf, "Location:")) != NULL &&
            //         checkRedirection(&pu))
            //     {
            //         /* document moved */
            //         tpath = wc_conv_strict(remove_space(p), w3mApp::Instance().InnerCharset, w3mApp::Instance().DocumentCharset)->ptr;
            //         request = NULL;
            //         f.Close();
            //         add_auth_cookie_flag = 0;
            //         *current = pu;
            //         t_buf = newBuffer(INIT_BUFFER_WIDTH());
            //         t_buf->bufferprop |= BP_REDIRECTED;
            //         status = HTST_NORMAL;
            //         goto load_doc;
            //     }
            //     t = checkContentType(t_buf);
            //     if (t == NULL)
            //         t = "text/plain";
            // }
            // else if (w3mApp::Instance().DefaultType.size())
            // {
            //     t = Strnew(w3mApp::Instance().DefaultType)->ptr;
            //     w3mApp::Instance().DefaultType.clear();
            // }
            // else
            {
                // local CGI
                auto t = guessContentType(url.path);
                if (t == NULL)
                    t = "text/plain";
                auto real_type = t;
                if (uf->guess_type)
                    t = uf->guess_type;
            }

            HttpContext http;
            return http.Get(uf, url, flag);
        }
        else
        {
            //
            // local file
            //
            // auto f = URLFile::openURL(url, _current, referer, flag, form, extra_header, &hr, &status);
            auto uf = URLFile::OpenFile(url.real_file);
            if (uf->stream == NULL)
            {
                // TODO:
                assert(false);
                return nullptr;
                // if (dir_exist(const_cast<char *>(url.real_file.c_str())))
                // {
                //     add_index_file(&url, uf);
                //     if (uf->stream == NULL)
                //         return uf;
                // }

                // else if (document_root != NULL)
                // {
                //     // TODO:
                //     assert(false);
                //     // tmp = Strnew(document_root);
                //     // if (tmp->Back() != '/' && url.path[0] != '/')
                //     //     tmp->Push('/');
                //     // tmp->Push(url.path);
                //     // p = cleanupName(tmp->ptr);
                //     // q = cleanupName(file_unquote(p));
                //     // if (dir_exist(q))
                //     // {
                //     //     url.path = p;
                //     //     url.real_file = q;
                //     //     add_index_file(pu, this);
                //     //     if (uf.stream == NULL)
                //     //     {
                //     //         return;
                //     //     }
                //     // }
                //     // else
                //     // {
                //     //     examineFile(q);
                //     //     if (uf.stream)
                //     //     {
                //     //         url.path = p;
                //     //         url.real_file = q;
                //     //     }
                //     // }
                // }
            }
            // TODO:
            // if (this->stream == NULL && retryAsHttp && url[0] != '/')
            // {
            //     if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN)
            //     {
            //         /* retry it as "http://" */
            //         u = Strnew_m_charp("http://", url, NULL)->ptr;
            //         goto retry;
            //     }
            // }

            HttpContext http;
            return http.Get(uf, url, flag);
        }
    }
    else
    {
        // not implemened;
        assert(false);
        return nullptr;
    }

    // LoaderFunc proc = loadBuffer;
    // char *p;
    // BufferPtr t_buf = NULL;
    // int searchHeader = w3mApp::Instance().SearchHeader;
    // int searchHeader_through = TRUE;
    // int add_auth_cookie_flag;
    // Str tmp;

    // URL *auth_pu;

    // auto tpath = path;
    // MySignalHandler prevtrap = NULL;

    // HttpContext http;

    // std::shared_ptr<URL> current;
    // if (_current)
    // {
    //     current = std::make_shared<URL>();
    //     *current = *_current;
    // }

    // auto content_charset = WC_CES_NONE;

    // auto t = "text/plain";
    // const char *real_type = nullptr;
    // Str page = NULL;
    // CharacterEncodingScheme charset = WC_CES_US_ASCII;
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
    //     }
    //     if (page && page->Size() > 0)
    //     {
    //         return LoadPage(page, charset, pu, t);
    //     }
    //     return NULL;
    // }

    // if (status == HTST_MISSING)
    // {
    //     // TRAP_OFF;
    //     f.Close();
    //     return NULL;
    // }

    // /* openURL() succeeded */
    // if (SETJMP(AbortLoading) != 0)
    // {
    //     /* transfer interrupted */
    //     TRAP_OFF;
    //     f.Close();
    //     return NULL;
    // }

    // BufferPtr t_buf = nullptr;
    // if (w3mApp::Instance().header_string.size())
    //     w3mApp::Instance().header_string.clear();
    if (url.scheme == SCM_HTTP || url.scheme == SCM_HTTPS)
    {

        // int add_auth_cookie_flag = 0;
        // Str realm = NULL;
        // Str uname = NULL;
        // Str pwd = NULL;
        // if (add_auth_cookie_flag && realm && uname && pwd)
        // {
        //     /* If authorization is required and passed */
        //     add_auth_user_passwd(&pu, qstr_unquote(realm)->ptr, uname, pwd,
        //                          0);
        //     add_auth_cookie_flag = 0;
        // }
        // if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL &&
        //     http_response_code == 401)
        // {
        //     /* Authentication needed */
        //     struct http_auth hauth;
        //     if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL && (realm = get_auth_param(hauth.param, "realm")) != NULL)
        //     {
        //         auth_pu = &pu;
        //         getAuthCookie(&hauth, "Authorization:", extra_header,
        //                       auth_pu, &hr, request, &uname, &pwd);
        //         if (uname == NULL)
        //         {
        //             /* abort */
        //             TRAP_OFF;
        //             goto page_loaded;
        //         }
        //         f.Close();
        //         add_auth_cookie_flag = 1;
        //         status = HTST_NORMAL;
        //         goto load_doc;
        //     }
        // }
        // if ((p = checkHeader(t_buf, "Proxy-Authenticate:")) != NULL &&
        //     http_response_code == 407)
        // {
        //     /* Authentication needed */
        //     struct http_auth hauth;
        //     if (findAuthentication(&hauth, t_buf, "Proxy-Authenticate:") != NULL && (realm = get_auth_param(hauth.param, "realm")) != NULL)
        //     {
        //         auth_pu = schemeToProxy(pu.scheme);
        //         getAuthCookie(&hauth, "Proxy-Authorization:",
        //                       extra_header, auth_pu, &hr, request,
        //                       &uname, &pwd);
        //         if (uname == NULL)
        //         {
        //             /* abort */
        //             TRAP_OFF;
        //             goto page_loaded;
        //         }
        //         f.Close();
        //         add_auth_cookie_flag = 1;
        //         status = HTST_NORMAL;
        //         add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
        //         goto load_doc;
        //     }
        // }
        /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

        // if (status == HTST_CONNECT)
        // {
        //     assert(false);
        //     // goto load_doc;
        // }

        // f.modtime = mymktime(checkHeader(t_buf, "Last-Modified:"));
    }

    return nullptr;
}
