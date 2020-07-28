#include "loader.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "http_request.h"
#include "etc.h"
#include "auth.h"
#include "mimetypes.h"
#include "mimehead.h"
#include "myctype.h"
#include "compression.h"
#include "cookie.h"
#include "public.h"
#include "image.h"
#include "display.h"
#include "terms.h"

#include <assert.h>
#include <memory>

#include <setjmp.h>
#include <signal.h>
static JMP_BUF AbortLoading;
static void   KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

///
/// HTTP
///
 int http_response_code;
 long long current_content_length;
 int frame_source = 0;

//
// HTTP redirection
//
static std::vector<ParsedURL> g_puv;
static void clearRedirection()
{
    g_puv.clear();
}
static bool checkRedirection(ParsedURL *pu)
{
    assert(pu);
    Str tmp;

    if (g_puv.size() >= FollowRedirection)
    {
        /* FIXME: gettextize? */
        auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                           FollowRedirection, pu->ToStr()->ptr);
        disp_err_message(tmp->ptr, FALSE);
        return false;
    }

    g_puv.push_back(*pu);
    return true;
}

///
/// charset
///
 wc_ces content_charset = 0;
 wc_ces meta_charset = 0;
void SetMetaCharset(wc_ces ces)
{
    meta_charset = ces;
}
static char *check_charset(char *p);
static char *check_accept_charset(char *p);

static BufferPtr
loadSomething(URLFile *f,
              char *path,
              BufferPtr (*loadproc)(URLFile *, BufferPtr), BufferPtr defaultbuf)
{
    BufferPtr buf;

    if ((buf = loadproc(f, defaultbuf)) == NULL)
        return NULL;

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
    if (f->scheme == SCM_LOCAL && buf->sourcefile == NULL)
        buf->sourcefile = path;
    return buf;
}

/* 
 * loadFile: load file to buffer
 */
BufferPtr
loadFile(char *path)
{
    BufferPtr buf;
    URLFile uf(SCM_LOCAL, NULL);
    examineFile(path, &uf);
    if (uf.stream == NULL)
        return NULL;
    buf = newBuffer(INIT_BUFFER_WIDTH);
    current_content_length = 0;
    content_charset = 0;
    buf = loadSomething(&uf, path, loadBuffer, buf);
    uf.Close();
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
#ifdef USE_M17N
    if ((p = strcasestr(p, "charset")) != NULL)
    {
        p += 7;
        SKIP_BLANKS(p);
        if (*p == '=')
        {
            p++;
            SKIP_BLANKS(p);
            if (*p == '"')
                p++;
            content_charset = wc_guess_charset(p, 0);
        }
    }
#endif
    return r->ptr;
}

void readHeader(URLFile *uf, BufferPtr newBuf, int thru, ParsedURL *pu)
{
    char *p, *q;
#ifdef USE_COOKIE
    char *emsg;
#endif
    char c;
    Str lineBuf2 = NULL;
    Str tmp;
    TextList *headerlist;
#ifdef USE_M17N
    wc_ces charset = WC_CES_US_ASCII, mime_charset;
#endif
    char *tmpf;
    FILE *src = NULL;
    Lineprop *propBuffer;

    headerlist = newBuf->document_header = newTextList();
    if (uf->scheme == SCM_HTTP
#ifdef USE_SSL
        || uf->scheme == SCM_HTTPS
#endif /* USE_SSL */
    )
        http_response_code = -1;
    else
        http_response_code = 0;

    if (thru && !newBuf->header_source
#ifdef USE_IMAGE
        && !image_source
#endif
    )
    {
        tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
        src = fopen(tmpf, "w");
        if (src)
            newBuf->header_source = tmpf;
    }
    while ((tmp = uf->StrmyISgets())->Size())
    {
#ifdef USE_NNTP
        if (uf->scheme == SCM_NEWS && tmp->ptr[0] == '.')
            tmp->Delete(0, 1);
#endif
        if (w3m_reqlog)
        {
            FILE *ff;
            ff = fopen(w3m_reqlog, "a");
            tmp->Puts(ff);
            fclose(ff);
        }
        if (src)
            tmp->Puts(src);
        cleanup_line(tmp, HEADER_MODE);
        if (tmp->ptr[0] == '\n' || tmp->ptr[0] == '\r' || tmp->ptr[0] == '\0')
        {
            if (!lineBuf2)
                /* there is no header */
                break;
            /* last header */
        }
        else if (!(w3m_dump & DUMP_HEAD))
        {
            if (lineBuf2)
            {
                lineBuf2->Push(tmp);
            }
            else
            {
                lineBuf2 = tmp;
            }
            c = uf->Getc();
            uf->UndoGetc();
            if (c == ' ' || c == '\t')
                /* header line is continued */
                continue;
            lineBuf2 = decodeMIME(lineBuf2, &mime_charset);
            lineBuf2 = convertLine(NULL, lineBuf2, RAW_MODE,
                                   mime_charset ? &mime_charset : &charset,
                                   mime_charset ? mime_charset
                                                : DocumentCharset);
            /* separated with line and stored */
            tmp = Strnew_size(lineBuf2->Size());
            for (p = lineBuf2->ptr; *p; p = q)
            {
                for (q = p; *q && *q != '\r' && *q != '\n'; q++)
                    ;
                lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer,
                                     NULL);
                tmp->Push(lineBuf2);
                if (thru)
                    addnewline(newBuf, lineBuf2->ptr, propBuffer, NULL,
                               lineBuf2->Size(), FOLD_BUFFER_WIDTH, -1);
                for (; *q && (*q == '\r' || *q == '\n'); q++)
                    ;
            }
#ifdef USE_IMAGE
            if (thru && activeImage && displayImage)
            {
                Str src = NULL;
                if (!strncasecmp(tmp->ptr, "X-Image-URL:", 12))
                {
                    tmpf = &tmp->ptr[12];
                    SKIP_BLANKS(tmpf);
                    src = Strnew_m_charp("<img src=\"", html_quote(tmpf),
                                         "\" alt=\"X-Image-URL\">", NULL);
                }
#ifdef USE_XFACE
                else if (!strncasecmp(tmp->ptr, "X-Face:", 7))
                {
                    tmpf = xface2xpm(&tmp->ptr[7]);
                    if (tmpf)
                        src = Strnew_m_charp("<img src=\"file:",
                                             html_quote(tmpf),
                                             "\" alt=\"X-Face\"",
                                             " width=48 height=48>", NULL);
                }
#endif
                if (src)
                {
                    Line *l;
                    wc_ces old_charset = newBuf->document_charset;
                    URLFile f(SCM_LOCAL, newStrStream(src));
                    loadHTMLstream(&f, newBuf, NULL, TRUE);
                    for (l = newBuf->lastLine; l && l->real_linenumber;
                         l = l->prev)
                        l->real_linenumber = 0;
#ifdef USE_M17N
                    newBuf->document_charset = old_charset;
#endif
                }
            }
#endif
            lineBuf2 = tmp;
        }
        else
        {
            lineBuf2 = tmp;
        }
        if ((uf->scheme == SCM_HTTP
#ifdef USE_SSL
             || uf->scheme == SCM_HTTPS
#endif /* USE_SSL */
             ) &&
            http_response_code == -1)
        {
            p = lineBuf2->ptr;
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
        if (!strncasecmp(lineBuf2->ptr, "content-transfer-encoding:", 26))
        {
            p = lineBuf2->ptr + 26;
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
        else if (!strncasecmp(lineBuf2->ptr, "content-encoding:", 17))
        {
            struct compression_decoder *d;
            p = lineBuf2->ptr + 17;
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
        else if (use_cookie && accept_cookie &&
                 pu && check_cookie_accept_domain(pu->host) &&
                 (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
                  !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12)))
        {
            readHeaderCookie(pu, lineBuf2);
        }
        else if (!strncasecmp(lineBuf2->ptr, "w3m-control:", 12) &&
                 uf->scheme == SCM_LOCAL_CGI)
        {
            Str funcname = Strnew();

            p = lineBuf2->ptr + 12;
            SKIP_BLANKS(p);
            while (*p && !IS_SPACE(*p))
                funcname->Push(*(p++));
            SKIP_BLANKS(p);
            Command f = getFuncList(funcname->ptr);
            if (f)
            {
                tmp = Strnew(p);
                tmp->StripRight();
                pushEvent(f, tmp->ptr);
            }
        }
        if (headerlist)
            pushText(headerlist, lineBuf2->ptr);
        lineBuf2 = NULL;
    }
    if (thru)
        addnewline(newBuf, "", propBuffer, NULL, 0, -1, -1);
    if (src)
        fclose(src);
}

/* 
 * loadGeneralFile: load file to buffer
 */
BufferPtr
loadGeneralFile(char *path, const ParsedURL *_current, char *referer,
                int flag, FormList *request)
{
    ParsedURL pu;
    BufferPtr b = NULL;
    auto proc = loadBuffer;
    char *tpath;
    char *p;
    BufferPtr t_buf = NULL;
    int searchHeader = SearchHeader;
    int searchHeader_through = TRUE;
    TextList *extra_header = newTextList();
    Str uname = NULL;
    Str pwd = NULL;
    Str realm = NULL;
    int add_auth_cookie_flag;
    unsigned char status = HTST_NORMAL;
    URLOption url_option;
    Str tmp;
    Str page = NULL;
#ifdef USE_M17N
    wc_ces charset = WC_CES_US_ASCII;
#endif
    HRequest hr(referer, request);
    ParsedURL *auth_pu;

    tpath = path;
    MySignalHandler prevtrap = NULL;
    add_auth_cookie_flag = 0;

    clearRedirection();
load_doc:
    TRAP_OFF;
    url_option.referer = referer;
    url_option.flag = flag;

    std::shared_ptr<ParsedURL> current;
    if (_current)
    {
        current = std::make_shared<ParsedURL>();
        *current = *_current;
    }

    URLFile f(SCM_MISSING, NULL);
    f.openURL(tpath, &pu, current.get(), &url_option, request, extra_header,
              &hr, &status);
    content_charset = 0;

    auto t = "text/plain";
    const char *real_type = nullptr;
    if (f.stream == NULL)
    {
        switch (f.scheme)
        {
        case SCM_LOCAL:
        {
            struct stat st;
            if (stat(pu.real_file.c_str(), &st) < 0)
                return NULL;
            if (S_ISDIR(st.st_mode))
            {
                if (UseExternalDirBuffer)
                {
                    Str cmd = Sprintf("%s?dir=%s#current",
                                      DirBufferCommand, pu.file);
                    b = loadGeneralFile(cmd->ptr, NULL, NO_REFERER, 0,
                                        NULL);
                    if (b != NULL)
                    {
                        b->currentURL = pu;
                        b->filename = b->currentURL.real_file;
                    }
                    return b;
                }
                else
                {
                    page = loadLocalDir(const_cast<char *>(pu.real_file.c_str()));
                    t = "local:directory";
#ifdef USE_M17N
                    charset = SystemCharset;
#endif
                }
            }
        }
        break;
        case SCM_FTPDIR:
            page = loadFTPDir(&pu, &charset);
            t = "ftp:directory";
            break;
#ifdef USE_NNTP
        case SCM_NEWS_GROUP:
            page = loadNewsgroup(&pu, &charset);
            t = "news:group";
            break;
#endif
        case SCM_UNKNOWN:
#ifdef USE_EXTERNAL_URI_LOADER
            tmp = searchURIMethods(&pu);
            if (tmp != NULL)
            {
                b = loadGeneralFile(tmp->ptr, current.get(), referer, flag, request);
                if (b != NULL)
                    b->currentURL = pu;
                return b;
            }
#endif
            /* FIXME: gettextize? */
            disp_err_message(Sprintf("Unknown URI: %s",
                                     pu.ToStr()->ptr)
                                 ->ptr,
                             FALSE);
            break;
        }
        if (page && page->Size() > 0)
            goto page_loaded;
        return NULL;
    }

    if (status == HTST_MISSING)
    {
        TRAP_OFF;
        f.Close();
        return NULL;
    }

    /* openURL() succeeded */
    if (SETJMP(AbortLoading) != 0)
    {
        /* transfer interrupted */
        TRAP_OFF;
        f.Close();
        return NULL;
    }

    b = NULL;
    if (f.is_cgi)
    {
        /* local CGI */
        searchHeader = TRUE;
        searchHeader_through = FALSE;
    }
    if (header_string)
        header_string = NULL;
    TRAP_ON;
    if (pu.scheme == SCM_HTTP ||
#ifdef USE_SSL
        pu.scheme == SCM_HTTPS ||
#endif /* USE_SSL */
        ((
#ifdef USE_GOPHER
             (pu.scheme == SCM_GOPHER && non_null(GOPHER_proxy)) ||
#endif /* USE_GOPHER */
             (pu.scheme == SCM_FTP && non_null(FTP_proxy))) &&
         !Do_not_use_proxy && !check_no_proxy(const_cast<char *>(pu.host.c_str()))))
    {

        if (fmInitialized)
        {
            term_cbreak();
            /* FIXME: gettextize? */
            message(Sprintf("%s contacted. Waiting for reply...", pu.host.c_str())->ptr, 0, 0);
            refresh();
        }
        if (t_buf == NULL)
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
#if 0 /* USE_SSL */
        if (IStype(f.stream) == IST_SSL) {
            Str s = ssl_get_certificate(f.stream, pu.host);
            if (s == NULL)
                return NULL;
            else
                t_buf->ssl_certificate = s->ptr;
        }
#endif
        readHeader(&f, t_buf, FALSE, &pu);
        if (((http_response_code >= 301 && http_response_code <= 303) || http_response_code == 307) && (p = checkHeader(t_buf, "Location:")) != NULL && checkRedirection(&pu))
        {
            /* document moved */
            /* 301: Moved Permanently */
            /* 302: Found */
            /* 303: See Other */
            /* 307: Temporary Redirect (HTTP/1.1) */
            tpath = url_quote_conv(p, DocumentCharset);
            request = NULL;
            f.Close();
            *current = pu;
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
            t_buf->bufferprop |= BP_REDIRECTED;
            status = HTST_NORMAL;
            goto load_doc;
        }
        t = checkContentType(t_buf);
        if (t == NULL && pu.file.size())
        {
            if (!((http_response_code >= 400 && http_response_code <= 407) ||
                  (http_response_code >= 500 && http_response_code <= 505)))
                t = guessContentType(pu.file);
        }
        if (t == NULL)
            t = "text/plain";
        if (add_auth_cookie_flag && realm && uname && pwd)
        {
            /* If authorization is required and passed */
            add_auth_user_passwd(&pu, qstr_unquote(realm)->ptr, uname, pwd,
                                 0);
            add_auth_cookie_flag = 0;
        }
        if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL &&
            http_response_code == 401)
        {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL && (realm = get_auth_param(hauth.param, "realm")) != NULL)
            {
                auth_pu = &pu;
                getAuthCookie(&hauth, "Authorization:", extra_header,
                              auth_pu, &hr, request, &uname, &pwd);
                if (uname == NULL)
                {
                    /* abort */
                    TRAP_OFF;
                    goto page_loaded;
                }
                f.Close();
                add_auth_cookie_flag = 1;
                status = HTST_NORMAL;
                goto load_doc;
            }
        }
        if ((p = checkHeader(t_buf, "Proxy-Authenticate:")) != NULL &&
            http_response_code == 407)
        {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf, "Proxy-Authenticate:") != NULL && (realm = get_auth_param(hauth.param, "realm")) != NULL)
            {
                auth_pu = schemeToProxy(pu.scheme);
                getAuthCookie(&hauth, "Proxy-Authorization:",
                              extra_header, auth_pu, &hr, request,
                              &uname, &pwd);
                if (uname == NULL)
                {
                    /* abort */
                    TRAP_OFF;
                    goto page_loaded;
                }
                f.Close();
                add_auth_cookie_flag = 1;
                status = HTST_NORMAL;
                add_auth_user_passwd(auth_pu, qstr_unquote(realm)->ptr, uname, pwd, 1);
                goto load_doc;
            }
        }
        /* XXX: RFC2617 3.2.3 Authentication-Info: ? */

        if (status == HTST_CONNECT)
        {
            goto load_doc;
        }

        f.modtime = mymktime(checkHeader(t_buf, "Last-Modified:"));
    }
#ifdef USE_NNTP
    else if (pu.scheme == SCM_NEWS || pu.scheme == SCM_NNTP)
    {
        if (t_buf == NULL)
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
        readHeader(&f, t_buf, TRUE, &pu);
        t = checkContentType(t_buf);
        if (t == NULL)
            t = "text/plain";
    }
#endif /* USE_NNTP */
#ifdef USE_GOPHER
    else if (pu.scheme == SCM_GOPHER)
    {
        switch (*pu.file)
        {
        case '0':
            t = "text/plain";
            break;
        case '1':
        case 'm':
            page = loadGopherDir(&f, &pu, &charset);
            t = "gopher:directory";
            TRAP_OFF;
            goto page_loaded;
        case 's':
            t = "audio/basic";
            break;
        case 'g':
            t = "image/gif";
            break;
        case 'h':
            t = "text/html";
            break;
        }
    }
#endif /* USE_GOPHER */
    else if (pu.scheme == SCM_FTP)
    {
        check_compression(path, &f);
        if (f.compression != CMP_NOCOMPRESS)
        {
            auto t1 = uncompressed_file_type(pu.file.c_str(), NULL);
            real_type = f.guess_type;
            if (t1)
                t = t1;
            else
                t = real_type;
        }
        else
        {
            real_type = guessContentType(pu.file);
            if (real_type == NULL)
                real_type = "text/plain";
            t = real_type;
        }
#if 0
        if (!strncasecmp(t, "application/", 12)) {
            char *tmpf = tmpfname(TMPF_DFL, NULL)->ptr;
            current_content_length = 0;
            if (save2tmp(f, tmpf) < 0)
                f.Close();
            else {
                f.Close();
                TRAP_OFF;
                doFileMove(tmpf, guess_save_name(t_buf, pu.file));
            }
            return nullptr;
        }
#endif
    }
    else if (pu.scheme == SCM_DATA)
    {
        t = f.guess_type;
    }
    else if (searchHeader)
    {
        searchHeader = SearchHeader = FALSE;
        if (t_buf == NULL)
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
        readHeader(&f, t_buf, searchHeader_through, &pu);
        if (f.is_cgi && (p = checkHeader(t_buf, "Location:")) != NULL &&
            checkRedirection(&pu))
        {
            /* document moved */
            tpath = url_quote_conv(remove_space(p), DocumentCharset);
            request = NULL;
            f.Close();
            add_auth_cookie_flag = 0;
            *current = pu;
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
            t_buf->bufferprop |= BP_REDIRECTED;
            status = HTST_NORMAL;
            goto load_doc;
        }
#ifdef AUTH_DEBUG
        if ((p = checkHeader(t_buf, "WWW-Authenticate:")) != NULL)
        {
            /* Authentication needed */
            struct http_auth hauth;
            if (findAuthentication(&hauth, t_buf, "WWW-Authenticate:") != NULL && (realm = get_auth_param(hauth.param, "realm")) != NULL)
            {
                auth_pu = &pu;
                getAuthCookie(&hauth, "Authorization:", extra_header,
                              auth_pu, &hr, request, &uname, &pwd);
                if (uname == NULL)
                {
                    /* abort */
                    TRAP_OFF;
                    goto page_loaded;
                }
                f.Close();
                add_auth_cookie_flag = 1;
                status = HTST_NORMAL;
                goto load_doc;
            }
        }
#endif /* defined(AUTH_DEBUG) */
        t = checkContentType(t_buf);
        if (t == NULL)
            t = "text/plain";
    }
    else if (DefaultType)
    {
        t = DefaultType;
        DefaultType = NULL;
    }
    else
    {
        t = guessContentType(pu.file);
        if (t == NULL)
            t = "text/plain";
        real_type = t;
        if (f.guess_type)
            t = f.guess_type;
    }

    /* XXX: can we use guess_type to give the type to loadHTMLstream
     *      to support default utf8 encoding for XHTML here? */
    f.guess_type = t;

page_loaded:
    if (page)
    {
        FILE *src;
#ifdef USE_IMAGE
        if (image_source)
            return NULL;
#endif
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
        {
            Str s;
            s = wc_Str_conv_strict(page, InnerCharset, charset);
            s->Puts(src);
            fclose(src);
        }
        if (do_download)
        {
            char *file;
            if (!src)
                return NULL;
            file = guess_filename(pu.file);
#ifdef USE_GOPHER
            if (f.scheme == SCM_GOPHER)
                file = Sprintf("%s.html", file)->ptr;
#endif
#ifdef USE_NNTP
            if (f.scheme == SCM_NEWS_GROUP)
                file = Sprintf("%s.html", file)->ptr;
#endif
            doFileMove(tmp->ptr, file);
            return nullptr;
        }
        b = loadHTMLString(page);
        if (b)
        {
            b->currentURL = pu;
            b->real_scheme = pu.scheme;
            b->real_type = t;
            if (src)
                b->sourcefile = tmp->ptr;
#ifdef USE_M17N
            b->document_charset = charset;
#endif
        }
        return b;
    }

    if (real_type == NULL)
        real_type = t;
    proc = loadBuffer;

    *GetCurBaseUrl() = pu;

    current_content_length = 0;
    if ((p = checkHeader(t_buf, "Content-Length:")) != NULL)
        current_content_length = strtoclen(p);
    if (do_download)
    {
        /* download only */
        char *file;
        TRAP_OFF;
        if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
            f.stream = newEncodedStream(f.stream, f.encoding);
        if (pu.scheme == SCM_LOCAL)
        {
            struct stat st;
            if (PreserveTimestamp && !stat(pu.real_file.c_str(), &st))
                f.modtime = st.st_mtime;
            file = conv_from_system(guess_save_name(NULL, pu.real_file.c_str()));
        }
        else
            file = guess_save_name(t_buf, pu.file);
        if (f.DoFileSave(file, current_content_length) == 0)
            f.HalfClose();
        else
            f.Close();
        return nullptr;
    }

    if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress && !(w3m_dump & DUMP_EXTRA))
    {
        pu.real_file = uncompress_stream(&f, true);
    }
    else if (f.compression != CMP_NOCOMPRESS)
    {
        if (!(w3m_dump & DUMP_SOURCE) &&
            (w3m_dump & ~DUMP_FRAME || is_text_type(t) || searchExtViewer(t)))
        {
            if (t_buf == NULL)
                t_buf = newBuffer(INIT_BUFFER_WIDTH);
            t_buf->sourcefile = uncompress_stream(&f, true);
            uncompressed_file_type(pu.file.c_str(), &f.ext);
        }
        else
        {
            t = compress_application_type(f.compression);
            f.compression = CMP_NOCOMPRESS;
        }
    }
#ifdef USE_IMAGE
    if (image_source)
    {
        BufferPtr b = NULL;
        if (IStype(f.stream) != IST_ENCODED)
            f.stream = newEncodedStream(f.stream, f.encoding);
        if (save2tmp(f, image_source) == 0)
        {
            b = newBuffer(INIT_BUFFER_WIDTH);
            b->sourcefile = image_source;
            b->real_type = t;
        }
        f.Close();
        TRAP_OFF;
        return b;
    }
#endif

    if (is_html_type(t))
        proc = loadHTMLBuffer;
    else if (is_plain_text_type(t))
        proc = loadBuffer;
#ifdef USE_IMAGE
    else if (activeImage && displayImage && !useExtImageViewer &&
             !(w3m_dump & ~DUMP_FRAME) && !strncasecmp(t, "image/", 6))
        proc = loadImageBuffer;
#endif
    else if (w3m_backend)
        ;
    else if (!(w3m_dump & ~DUMP_FRAME) || is_dump_text_type(t))
    {
        if (!do_download && doExternal(f,
                                       pu.real_file.size() ? const_cast<char *>(pu.real_file.c_str()) : const_cast<char *>(pu.file.c_str()),
                                       t, &b, t_buf))
        {
            if (b)
            {
                b->real_scheme = f.scheme;
                b->real_type = real_type;
                if (b->currentURL.host.empty() && b->currentURL.file.empty())
                    b->currentURL = pu;
            }
            f.Close();
            TRAP_OFF;
            return b;
        }
        else
        {
            TRAP_OFF;
            if (pu.scheme == SCM_LOCAL)
            {
                f.Close();
                _doFileCopy(const_cast<char *>(pu.real_file.c_str()),
                            conv_from_system(guess_save_name(NULL, pu.real_file)), TRUE);
            }
            else
            {
                if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
                    f.stream = newEncodedStream(f.stream, f.encoding);
                if (f.DoFileSave(guess_save_name(t_buf, pu.file), current_content_length) == 0)
                    f.HalfClose();
                else
                    f.Close();
            }
            return nullptr;
        }
    }
    else if (w3m_dump & DUMP_FRAME)
        return NULL;

    if (flag & RG_FRAME)
    {
        if (t_buf == NULL)
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
        t_buf->bufferprop |= BP_FRAME;
    }
#ifdef USE_SSL
    if (t_buf)
        t_buf->ssl_certificate = f.ssl_certificate;
#endif
    frame_source = flag & RG_FRAME_SRC;
    b = loadSomething(&f, pu.real_file.size() ? const_cast<char *>(pu.real_file.c_str()) : const_cast<char *>(pu.file.c_str()), proc, t_buf);
    f.Close();
    frame_source = 0;
    if (b)
    {
        b->real_scheme = f.scheme;
        b->real_type = real_type;
        if (b->currentURL.host.empty() && b->currentURL.file.empty())
            b->currentURL = pu;
        if (is_html_type(t))
            b->type = "text/html";
        else if (w3m_backend)
        {
            Str s = Strnew(t);
            b->type = s->ptr;
        }
#ifdef USE_IMAGE
        else if (proc == loadImageBuffer)
            b->type = "text/html";
#endif
        else
            b->type = "text/plain";
        if (pu.label.size())
        {
            if (proc == loadHTMLBuffer)
            {
                auto a = searchURLLabel(b, const_cast<char *>(pu.label.c_str()));
                if (a != NULL)
                {
                    gotoLine(b, a->start.line);
                    if (label_topline)
                        b->topLine = lineSkip(b, b->topLine,
                                              b->currentLine->linenumber - b->topLine->linenumber, FALSE);
                    b->pos = a->start.pos;
                    arrangeCursor(b);
                }
            }
            else
            { /* plain text */
                int l = atoi(pu.label.c_str());
                gotoRealLine(b, l);
                b->pos = 0;
                arrangeCursor(b);
            }
        }
    }
    if (header_string)
        header_string = NULL;
#ifdef USE_NNTP
    if (f.scheme == SCM_NNTP || f.scheme == SCM_NEWS)
        reAnchorNewsheader(b);
#endif
    preFormUpdateBuffer(b);
    TRAP_OFF;
    return b;
}
