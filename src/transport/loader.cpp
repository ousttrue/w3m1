#include "transport/loader.h"
#include "fm.h"
#include "indep.h"
#include "file.h"
#include "http/http_request.h"
#include "etc.h"
#include "http/auth.h"
#include "mimetypes.h"
#include "mimehead.h"
#include "myctype.h"
#include "http/compression.h"
#include "http/cookie.h"
#include "public.h"
#include "html/image.h"
#include "frontend/display.h"
#include "frontend/terms.h"

#include <assert.h>
#include <memory>

#include <setjmp.h>
#include <signal.h>
static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
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
char *
check_charset(char *p)
{
    return wc_guess_charset(p, 0) ? p : NULL;
}

char *
check_accept_charset(char *ac)
{
    char *s = ac, *e;

    while (*s)
    {
        while (*s && (IS_SPACE(*s) || *s == ','))
            s++;
        if (!*s)
            break;
        e = s;
        while (*e && !(IS_SPACE(*e) || *e == ','))
            e++;
        if (wc_guess_charset(Strnew_charp_n(s, e - s)->ptr, 0))
            return ac;
        s = e;
    }
    return NULL;
}

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
 * loadBuffer: read file and make new buffer
 */
BufferPtr
loadBuffer(URLFile *uf, BufferPtr newBuf)
{
    FILE *src = NULL;
#ifdef USE_M17N
    wc_ces charset = WC_CES_US_ASCII;
    wc_ces doc_charset = DocumentCharset;
#endif
    Str lineBuf2;
    char pre_lbuf = '\0';
    int nlines;
    Str tmpf;
    clen_t linelen = 0, trbyte = 0;
    Lineprop *propBuffer = NULL;
#ifdef USE_ANSI_COLOR
    Linecolor *colorBuffer = NULL;
#endif
    MySignalHandler prevtrap = NULL;

    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);
    lineBuf2 = Strnew();

    if (SETJMP(AbortLoading) != 0)
    {
        goto _end;
    }
    TRAP_ON;

    if (newBuf->sourcefile == NULL &&
        (uf->scheme != SCM_LOCAL || newBuf->mailcap))
    {
        tmpf = tmpfname(TMPF_SRC, NULL);
        src = fopen(tmpf->ptr, "w");
        if (src)
            newBuf->sourcefile = tmpf->ptr;
    }

    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (content_charset && UseContentCharset)
        doc_charset = content_charset;

    nlines = 0;
    if (IStype(uf->stream) != IST_ENCODED)
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
    while ((lineBuf2 = StrmyISgets(uf->stream))->Size())
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
        if (w3m_dump & DUMP_EXTRA)
            printf("W3m-in-progress: %s\n", convert_size2(linelen, current_content_length, TRUE));
        if (w3m_dump & DUMP_SOURCE)
            continue;
        showProgress(&linelen, &trbyte);
        if (frame_source)
            continue;
        lineBuf2 =
            convertLine(uf, lineBuf2, PAGER_MODE, &charset, doc_charset);
        if (squeezeBlankLine)
        {
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n')
            {
                ++nlines;
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        ++nlines;
        lineBuf2->StripRight();
        lineBuf2 = checkType(lineBuf2, &propBuffer, NULL);
        addnewline(newBuf, lineBuf2->ptr, propBuffer, colorBuffer,
                   lineBuf2->Size(), FOLD_BUFFER_WIDTH, nlines);
    }
_end:
    TRAP_OFF;
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->trbyte = trbyte + linelen;
#ifdef USE_M17N
    newBuf->document_charset = charset;
#endif
    if (src)
        fclose(src);

    return newBuf;
}

static int
_MoveFile(const char *path1, const char *path2)
{
    InputStream *f1;
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
        ISclose(f1);
        return -1;
    }
    current_content_length = 0;
    buf = Strnew_size(SAVE_BUF_SIZE);
    while (ISread(f1, buf, SAVE_BUF_SIZE))
    {
        buf->Puts(f2);
        linelen += buf->Size();
        showProgress(&linelen, &trbyte);
    }
    ISclose(f1);
    if (is_pipe)
        pclose(f2);
    else
        fclose(f2);
    return 0;
}

static
int checkCopyFile(const char *path1, const char *path2)
{
    struct stat st1, st2;

    if (*path2 == '|' && PermitSaveToPipe)
        return 0;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
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
                              defstr, IN_COMMAND, SaveHist);
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

int doFileCopy(const char *tmpf, const char *defstr)
{
    return _doFileCopy(tmpf, defstr, FALSE);
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

int save2tmp(URLFile uf, char *tmpf)
{
    FILE *ff;
    int check;
    clen_t linelen = 0, trbyte = 0;
    MySignalHandler prevtrap = NULL;
    static JMP_BUF env_bak;

    ff = fopen(tmpf, "wb");
    if (ff == NULL)
    {
        /* fclose(f); */
        return -1;
    }
    bcopy(AbortLoading, env_bak, sizeof(JMP_BUF));
    if (SETJMP(AbortLoading) != 0)
    {
        goto _end;
    }
    TRAP_ON;
    check = 0;
#ifdef USE_NNTP
    if (uf.scheme == SCM_NEWS)
    {
        char c;
        while (c = uf.Getc(), !iseos(uf.stream))
        {
            if (c == '\n')
            {
                if (check == 0)
                    check++;
                else if (check == 3)
                    break;
            }
            else if (c == '.' && check == 1)
                check++;
            else if (c == '\r' && check == 2)
                check++;
            else
                check = 0;
            putc(c, ff);
            linelen += sizeof(c);
            showProgress(&linelen, &trbyte);
        }
    }
    else
#endif /* USE_NNTP */
    {
        Str buf = Strnew_size(SAVE_BUF_SIZE);
        while (uf.Read(buf, SAVE_BUF_SIZE))
        {
            if (buf->Puts(ff) != buf->Size())
            {
                bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
                TRAP_OFF;
                fclose(ff);
                current_content_length = 0;
                return -2;
            }
            linelen += buf->Size();
            showProgress(&linelen, &trbyte);
        }
    }
_end:
    bcopy(env_bak, AbortLoading, sizeof(JMP_BUF));
    TRAP_OFF;
    fclose(ff);
    current_content_length = 0;
    return 0;
}
