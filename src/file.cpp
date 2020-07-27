/* $Id: file.c,v 1.265 2010/12/15 10:50:24 htrb Exp $ */

#include "fm.h"
#include "table.h"
#include "indep.h"
#include "etc.h"
#include "commands.h"
#include "frame.h"
#include "file.h"
#include "anchor.h"
#include "public.h"
#include "symbol.h"
#include "map.h"
#include "display.h"
#include "myctype.h"
#include "html.h"
#include "parsetagx.h"
#include "local.h"
#include "regex.h"
#include "dispatcher.h"
#include "url.h"
#include "entity.h"
#include "cookie.h"
#include "terms.h"
#include "image.h"
#include "ctrlcode.h"
#include "mimehead.h"
#include "mimetypes.h"
#include "tagstack.h"
#include "http_request.h"
#include "urlfile.h"
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
/* foo */

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif /* not max */
#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */

static int frame_source = 0;

static char *guess_filename(char *file);
static int _MoveFile(char *path1, char *path2);
static void uncompress_stream(URLFile *uf, char **src);
static FILE *lessopen_stream(char *path);
static BufferPtr loadcmdout(char *cmd,
                            BufferPtr (*loadproc)(URLFile *, BufferPtr),
                            BufferPtr defaultbuf);
#ifndef USE_ANSI_COLOR
#define addnewline(a, b, c, d, e, f, g) _addnewline(a, b, c, e, f, g)
#endif
static void addnewline(BufferPtr buf, char *line, Lineprop *prop,
                       Linecolor *color, int pos, int width, int nlines);
static void addLink(BufferPtr buf, struct parsed_tag *tag);

static JMP_BUF AbortLoading;

static ParsedURL g_cur_baseURL = {};
ParsedURL *GetCurBaseUrl()
{
    return &g_cur_baseURL;
}

static char cur_document_charset;

static Str cur_select;
static Str select_str;
static int select_is_multiple;
static int n_selectitem;
static Str cur_option;
static Str cur_option_value;
static Str cur_option_label;
static int cur_option_selected;
static int cur_status;
#ifdef MENU_SELECT
/* menu based <select>  */
FormSelectOption *select_option;
static int max_select = MAX_SELECT;
static int n_select;
static int cur_option_maxwidth;
#endif /* MENU_SELECT */

static Str cur_textarea;
Str *textarea_str;
static int cur_textarea_size;
static int cur_textarea_rows;
static int cur_textarea_readonly;
static int n_textarea;
static int ignore_nl_textarea;
static int max_textarea = MAX_TEXTAREA;

static int http_response_code;

#ifdef USE_M17N
static wc_ces content_charset = 0;
static wc_ces meta_charset = 0;
void SetMetaCharset(wc_ces ces)
{
    meta_charset = ces;
}
static char *check_charset(char *p);
static char *check_accept_charset(char *p);
#endif

#define FORMSTACK_SIZE 10
#define FRAMESTACK_SIZE 10

#ifdef USE_NNTP
#define Str_news_endline(s) ((s)->ptr[0] == '.' && ((s)->ptr[1] == '\n' || (s)->ptr[1] == '\r' || (s)->ptr[1] == '\0'))
#endif /* USE_NNTP */

#define INITIAL_FORM_SIZE 10
static FormList **forms;
static int *form_stack;
static int form_max = -1;
static int forms_size = 0;
#define cur_form_id ((form_sp >= 0) ? form_stack[form_sp] : -1)
static int form_sp = 0;

static clen_t current_content_length;

static int cur_hseq;
int GetCurHSeq()
{
    return cur_hseq;
}
void SetCurHSeq(int seq)
{
    cur_hseq = seq;
}

static int cur_iseq;

#ifdef USE_COOKIE
/* This array should be somewhere else */
/* FIXME: gettextize? */
const char *violations[COO_EMAX] = {
    "internal error",
    "tail match failed",
    "wrong number of dots",
    "RFC 2109 4.3.2 rule 1",
    "RFC 2109 4.3.2 rule 2.1",
    "RFC 2109 4.3.2 rule 2.2",
    "RFC 2109 4.3.2 rule 3",
    "RFC 2109 4.3.2 rule 4",
    "RFC XXXX 4.3.2 rule 5"};
#endif

/* *INDENT-OFF* */
static struct compression_decoder
{
    CompressionTypes type;
    const char *ext;
    const char *mime_type;
    int auxbin_p;
    const char *cmd;
    const char *name;
    const char *encoding;
    const char *encodings[4];
} compression_decoders[] = {
    {CMP_COMPRESS, ".gz", "application/x-gzip", 0, GUNZIP_CMDNAME, GUNZIP_NAME, "gzip", {"gzip", "x-gzip", NULL}},
    {CMP_COMPRESS, ".Z", "application/x-compress", 0, GUNZIP_CMDNAME, GUNZIP_NAME, "compress", {"compress", "x-compress", NULL}},
    {CMP_BZIP2, ".bz2", "application/x-bzip", 0, BUNZIP2_CMDNAME, BUNZIP2_NAME, "bzip, bzip2", {"x-bzip", "bzip", "bzip2", NULL}},
    {CMP_DEFLATE, ".deflate", "application/x-deflate", 1, INFLATE_CMDNAME, INFLATE_NAME, "deflate", {"deflate", "x-deflate", NULL}},
    {CMP_NOCOMPRESS, NULL, NULL, 0, NULL, NULL, NULL, {NULL}},
};
/* *INDENT-ON* */

#define SAVE_BUF_SIZE 1536

static void
    KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

static void
UFhalfclose(URLFile *f)
{
    switch (f->scheme)
    {
    case SCM_FTP:
        closeFTP();
        break;
#ifdef USE_NNTP
    case SCM_NEWS:
    case SCM_NNTP:
        closeNews();
        break;
#endif
    default:
        UFclose(f);
        break;
    }
}

int currentLn(BufferPtr buf)
{
    if (buf->currentLine)
        /*     return buf->currentLine->real_linenumber + 1;      */
        return buf->currentLine->linenumber + 1;
    else
        return 1;
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
        buf->buffername = checkHeader(buf, "Subject:");
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

int dir_exist(char *path)
{
    struct stat stbuf;

    if (path == NULL || *path == '\0')
        return 0;
    if (stat(path, &stbuf) == -1)
        return 0;
    return IS_DIRECTORY(stbuf.st_mode);
}

static int
is_dump_text_type(const char *type)
{
    struct mailcap *mcap;
    return (type && (mcap = searchExtViewer(type)) &&
            (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

static int
is_text_type(const char *type)
{
    return (type == NULL || type[0] == '\0' ||
            strncasecmp(type, "text/", 5) == 0 ||
            (strncasecmp(type, "application/", 12) == 0 &&
             strstr(type, "xhtml") != NULL) ||
            strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

static int
is_plain_text_type(const char *type)
{
    return ((type && strcasecmp(type, "text/plain") == 0) ||
            (is_text_type(type) && !is_dump_text_type(type)));
}

int is_html_type(const char *type)
{
    return (type && (strcasecmp(type, "text/html") == 0 ||
                     strcasecmp(type, "application/xhtml+xml") == 0));
}

static void
check_compression(char *path, URLFile *uf)
{
    int len;
    struct compression_decoder *d;

    if (path == NULL)
        return;

    len = strlen(path);
    uf->compression = CMP_NOCOMPRESS;
    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        int elen;
        if (d->ext == NULL)
            continue;
        elen = strlen(d->ext);
        if (len > elen && strcasecmp(&path[len - elen], d->ext) == 0)
        {
            uf->compression = d->type;
            uf->guess_type = d->mime_type;
            break;
        }
    }
}

static const char *
compress_application_type(int compression)
{
    struct compression_decoder *d;

    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        if (d->type == compression)
            return d->mime_type;
    }
    return NULL;
}

static const char *
uncompressed_file_type(const char *path, const char **ext)
{
    int len, slen;
    Str fn;
    struct compression_decoder *d;

    if (path == NULL)
        return NULL;

    slen = 0;
    len = strlen(path);
    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        if (d->ext == NULL)
            continue;
        slen = strlen(d->ext);
        if (len > slen && strcasecmp(&path[len - slen], d->ext) == 0)
            break;
    }
    if (d->type == CMP_NOCOMPRESS)
        return NULL;

    fn = Strnew(path);
    fn->Pop(slen);
    if (ext)
        *ext = filename_extension(fn->ptr, 0);
    auto t0 = guessContentType(fn->ptr);
    if (t0 == NULL)
        t0 = "text/plain";
    return t0;
}

static int
setModtime(char *path, time_t modtime)
{
    struct utimbuf t;
    struct stat st;

    if (stat(path, &st) == 0)
        t.actime = st.st_atime;
    else
        t.actime = time(NULL);
    t.modtime = modtime;
    return utime(path, &t);
}

void examineFile(char *path, URLFile *uf)
{
    struct stat stbuf;

    uf->guess_type = NULL;
    if (path == NULL || *path == '\0' ||
        stat(path, &stbuf) == -1 || NOT_REGULAR(stbuf.st_mode))
    {
        uf->stream = NULL;
        return;
    }
    uf->stream = openIS(path);
    if (!do_download)
    {
        if (use_lessopen && getenv("LESSOPEN") != NULL)
        {
            FILE *fp;
            uf->guess_type = guessContentType(path);
            if (uf->guess_type == NULL)
                uf->guess_type = "text/plain";
            if (is_html_type(uf->guess_type))
                return;
            if ((fp = lessopen_stream(path)))
            {
                UFclose(uf);
                uf->stream = newFileStream(fp, (FileStreamCloseFunc)pclose);
                uf->guess_type = "text/plain";
                return;
            }
        }
        check_compression(path, uf);
        if (uf->compression != CMP_NOCOMPRESS)
        {
            const char *ext = uf->ext;
            auto t0 = uncompressed_file_type(path, &ext);
            uf->guess_type = t0;
            uf->ext = ext;
            uncompress_stream(uf, NULL);
            return;
        }
    }
}

#define S_IXANY (S_IXUSR | S_IXGRP | S_IXOTH)

int check_command(const char *cmd, int auxbin_p)
{
    static char *path = NULL;
    Str dirs;
    char *p, *np;
    Str pathname;
    struct stat st;

    if (path == NULL)
        path = getenv("PATH");
    if (auxbin_p)
        dirs = Strnew(w3m_auxbin_dir());
    else
        dirs = Strnew(path);
    for (p = dirs->ptr; p != NULL; p = np)
    {
        np = strchr(p, PATH_SEPARATOR);
        if (np)
            *np++ = '\0';
        pathname = Strnew();
        pathname->Push(p);
        pathname->Push('/');
        pathname->Push(cmd);
        if (stat(pathname->ptr, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXANY) != 0)
            return 1;
    }
    return 0;
}

char *
acceptableEncoding()
{
    static Str encodings = NULL;
    struct compression_decoder *d;
    TextList *l;
    char *p;

    if (encodings != NULL)
        return encodings->ptr;
    l = newTextList();
    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        if (check_command(d->cmd, d->auxbin_p))
        {
            pushText(l, d->encoding);
        }
    }
    encodings = Strnew();
    while ((p = popText(l)) != NULL)
    {
        if (encodings->Size())
            encodings->Push(", ");
        encodings->Push(p);
    }
    return encodings->ptr;
}

/* 
 * convert line
 */
#ifdef USE_M17N
Str convertLine(URLFile *uf, Str line, int mode, wc_ces *charset,
                wc_ces doc_charset)
#else
Str convertLine0(URLFile *uf, Str line, int mode)
#endif
{

    line = wc_Str_conv_with_detect(line, charset, doc_charset, InnerCharset);

    if (mode != RAW_MODE)
        cleanup_line(line, mode);

    if (uf && uf->scheme == SCM_NEWS)
        line->StripRight();

    return line;
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
#ifdef USE_M17N
    content_charset = 0;
#endif
    buf = loadSomething(&uf, path, loadBuffer, buf);
    UFclose(&uf);
    return buf;
}

int matchattr(char *p, const char *attr, int len, Str *value)
{
    int quoted;
    char *q = NULL;

    if (strncasecmp(p, attr, len) == 0)
    {
        p += len;
        SKIP_BLANKS(p);
        if (value)
        {
            *value = Strnew();
            if (*p == '=')
            {
                p++;
                SKIP_BLANKS(p);
                quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';'))
                {
                    if (!IS_SPACE(*p))
                        q = p;
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    else
                        (*value)->Push(*p);
                    p++;
                }
                if (q)
                    (*value)->Pop(p - q - 1);
            }
            return 1;
        }
        else
        {
            if (IS_ENDT(*p))
            {
                return 1;
            }
        }
    }
    return 0;
}

#ifdef USE_IMAGE
#ifdef USE_XFACE
static char *
xface2xpm(char *xface)
{
    Image image;
    ImageCache *cache;
    FILE *f;
    struct stat st;

    SKIP_BLANKS(xface);
    image.url = xface;
    image.ext = ".xpm";
    image.width = 48;
    image.height = 48;
    image.cache = NULL;
    cache = getImage(&image, NULL, IMG_FLAG_AUTO);
    if (cache->loaded & IMG_FLAG_LOADED && !stat(cache->file, &st))
        return cache->file;
    cache->loaded = IMG_FLAG_ERROR;

    f = popen(Sprintf("%s > %s", shell_quote(auxbinFile(XFACE2XPM)),
                      shell_quote(cache->file))
                  ->ptr,
              "w");
    if (!f)
        return NULL;
    fputs(xface, f);
    pclose(f);
    if (stat(cache->file, &st) || !st.st_size)
        return NULL;
    cache->loaded = IMG_FLAG_LOADED | IMG_FLAG_DONT_REMOVE;
    cache->index = 0;
    return cache->file;
}
#endif
#endif

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
    while ((tmp = StrmyUFgets(uf))->Size())
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
            c = UFgetc(uf);
            UFundogetc(uf);
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
#ifdef USE_COOKIE
        else if (use_cookie && accept_cookie &&
                 pu && check_cookie_accept_domain(pu->host) &&
                 (!strncasecmp(lineBuf2->ptr, "Set-Cookie:", 11) ||
                  !strncasecmp(lineBuf2->ptr, "Set-Cookie2:", 12)))
        {
            Str name = Strnew(), value = Strnew(), domain = NULL, path = NULL,
                comment = NULL, commentURL = NULL, port = NULL, tmp2;
            int version, quoted, flag = 0;
            time_t expires = (time_t)-1;

            q = NULL;
            if (lineBuf2->ptr[10] == '2')
            {
                p = lineBuf2->ptr + 12;
                version = 1;
            }
            else
            {
                p = lineBuf2->ptr + 11;
                version = 0;
            }
#ifdef DEBUG
            fprintf(stderr, "Set-Cookie: [%s]\n", p);
#endif /* DEBUG */
            SKIP_BLANKS(p);
            while (*p != '=' && !IS_ENDT(*p))
                name->Push(*(p++));
            name->StripRight();
            if (*p == '=')
            {
                p++;
                SKIP_BLANKS(p);
                quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';'))
                {
                    if (!IS_SPACE(*p))
                        q = p;
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    value->Push(*(p++));
                }
                if (q)
                    value->Pop(p - q - 1);
            }
            while (*p == ';')
            {
                p++;
                SKIP_BLANKS(p);
                if (matchattr(p, "expires", 7, &tmp2))
                {
                    /* version 0 */
                    expires = mymktime(tmp2->ptr);
                }
                else if (matchattr(p, "max-age", 7, &tmp2))
                {
                    /* XXX Is there any problem with max-age=0? (RFC 2109 ss. 4.2.1, 4.2.2 */
                    expires = time(NULL) + atol(tmp2->ptr);
                }
                else if (matchattr(p, "domain", 6, &tmp2))
                {
                    domain = tmp2;
                }
                else if (matchattr(p, "path", 4, &tmp2))
                {
                    path = tmp2;
                }
                else if (matchattr(p, "secure", 6, NULL))
                {
                    flag |= COO_SECURE;
                }
                else if (matchattr(p, "comment", 7, &tmp2))
                {
                    comment = tmp2;
                }
                else if (matchattr(p, "version", 7, &tmp2))
                {
                    version = atoi(tmp2->ptr);
                }
                else if (matchattr(p, "port", 4, &tmp2))
                {
                    /* version 1, Set-Cookie2 */
                    port = tmp2;
                }
                else if (matchattr(p, "commentURL", 10, &tmp2))
                {
                    /* version 1, Set-Cookie2 */
                    commentURL = tmp2;
                }
                else if (matchattr(p, "discard", 7, NULL))
                {
                    /* version 1, Set-Cookie2 */
                    flag |= COO_DISCARD;
                }
                quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';'))
                {
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    p++;
                }
            }
            if (pu && name->Size() > 0)
            {
                int err;
                if (show_cookie)
                {
                    if (flag & COO_SECURE)
                        disp_message_nsec("Received a secured cookie", FALSE, 1,
                                          TRUE, FALSE);
                    else
                        disp_message_nsec(Sprintf("Received cookie: %s=%s",
                                                  name->ptr, value->ptr)
                                              ->ptr,
                                          FALSE, 1, TRUE, FALSE);
                }
                err =
                    add_cookie(pu, name, value, expires, domain, path, flag,
                               comment, version, port, commentURL);
                if (err)
                {
                    char *ans = (accept_bad_cookie == ACCEPT_BAD_COOKIE_ACCEPT)
                                    ? (char *)"y"
                                    : NULL;
                    if (fmInitialized && (err & COO_OVERRIDE_OK) &&
                        accept_bad_cookie == ACCEPT_BAD_COOKIE_ASK)
                    {
                        Str msg = Sprintf("Accept bad cookie from %s for %s?",
                                          pu->host,
                                          ((domain && domain->ptr)
                                               ? domain->ptr
                                               : "<localdomain>"));
                        if (msg->Size() > COLS - 10)
                            msg->Pop(msg->Size() - (COLS - 10));
                        msg->Push(" (y/n)");
                        ans = inputAnswer(msg->ptr);
                    }
                    if (ans == NULL || TOLOWER(*ans) != 'y' ||
                        (err =
                             add_cookie(pu, name, value, expires, domain, path,
                                        flag | COO_OVERRIDE, comment, version,
                                        port, commentURL)))
                    {
                        err = (err & ~COO_OVERRIDE_OK) - 1;
                        if (err >= 0 && err < COO_EMAX)
                            emsg = Sprintf("This cookie was rejected "
                                           "to prevent security violation. [%s]",
                                           violations[err])
                                       ->ptr;
                        else
                            emsg =
                                "This cookie was rejected to prevent security violation.";
                        record_err_message(emsg);
                        if (show_cookie)
                            disp_message_nsec(emsg, FALSE, 1, TRUE, FALSE);
                    }
                    else if (show_cookie)
                        disp_message_nsec(Sprintf("Accepting invalid cookie: %s=%s",
                                                  name->ptr, value->ptr)
                                              ->ptr,
                                          FALSE,
                                          1, TRUE, FALSE);
                }
            }
        }
#endif /* USE_COOKIE */
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

char *
checkHeader(BufferPtr buf, char *field)
{
    int len;
    TextListItem *i;
    char *p;

    if (buf == NULL || field == NULL || buf->document_header == NULL)
        return NULL;
    len = strlen(field);
    for (i = buf->document_header->first; i != NULL; i = i->next)
    {
        if (!strncasecmp(i->ptr, field, len))
        {
            p = i->ptr + len;
            return remove_space(p);
        }
    }
    return NULL;
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

struct auth_param
{
    char *name;
    Str val;
};

struct http_auth
{
    int pri;
    char *scheme;
    struct auth_param *param;
    Str (*cred)(struct http_auth *ha, Str uname, Str pw, ParsedURL *pu,
                HRequest *hr, FormList *request);
};

enum
{
    AUTHCHR_NUL,
    AUTHCHR_SEP,
    AUTHCHR_TOKEN,
};

static int
skip_auth_token(char **pp)
{
    char *p;
    int first = AUTHCHR_NUL, typ;

    for (p = *pp;; ++p)
    {
        switch (*p)
        {
        case '\0':
            goto endoftoken;
        default:
            if ((unsigned char)*p > 037)
            {
                typ = AUTHCHR_TOKEN;
                break;
            }
            /* thru */
        case '\177':
        case '[':
        case ']':
        case '(':
        case ')':
        case '<':
        case '>':
        case '@':
        case ';':
        case ':':
        case '\\':
        case '"':
        case '/':
        case '?':
        case '=':
        case ' ':
        case '\t':
        case ',':
            typ = AUTHCHR_SEP;
            break;
        }

        if (!first)
            first = typ;
        else if (first != typ)
            break;
    }
endoftoken:
    *pp = p;
    return first;
}

static Str
extract_auth_val(char **q)
{
    unsigned char *qq = *(unsigned char **)q;
    int quoted = 0;
    Str val = Strnew();

    SKIP_BLANKS(qq);
    if (*qq == '"')
    {
        quoted = TRUE;
        val->Push(*qq++);
    }
    while (*qq != '\0')
    {
        if (quoted && *qq == '"')
        {
            val->Push(*qq++);
            break;
        }
        if (!quoted)
        {
            switch (*qq)
            {
            case '[':
            case ']':
            case '(':
            case ')':
            case '<':
            case '>':
            case '@':
            case ';':
            case ':':
            case '\\':
            case '"':
            case '/':
            case '?':
            case '=':
            case ' ':
            case '\t':
                qq++;
            case ',':
                goto end_token;
            default:
                if (*qq <= 037 || *qq == 0177)
                {
                    qq++;
                    goto end_token;
                }
            }
        }
        else if (quoted && *qq == '\\')
            val->Push(*qq++);
        val->Push(*qq++);
    }
end_token:
    *q = (char *)qq;
    return val;
}

static Str
qstr_unquote(Str s)
{
    char *p;

    if (s == NULL)
        return NULL;
    p = s->ptr;
    if (*p == '"')
    {
        Str tmp = Strnew();
        for (p++; *p != '\0'; p++)
        {
            if (*p == '\\')
                p++;
            tmp->Push(*p);
        }
        if (tmp->Back() == '"')
            tmp->Pop(1);
        return tmp;
    }
    else
        return s;
}

static char *
extract_auth_param(char *q, struct auth_param *auth)
{
    struct auth_param *ap;
    char *p;

    for (ap = auth; ap->name != NULL; ap++)
    {
        ap->val = NULL;
    }

    while (*q != '\0')
    {
        SKIP_BLANKS(q);
        for (ap = auth; ap->name != NULL; ap++)
        {
            size_t len;

            len = strlen(ap->name);
            if (strncasecmp(q, ap->name, len) == 0 &&
                (IS_SPACE(q[len]) || q[len] == '='))
            {
                p = q + len;
                SKIP_BLANKS(p);
                if (*p != '=')
                    return q;
                q = p + 1;
                ap->val = extract_auth_val(&q);
                break;
            }
        }
        if (ap->name == NULL)
        {
            /* skip unknown param */
            int token_type;
            p = q;
            if ((token_type = skip_auth_token(&q)) == AUTHCHR_TOKEN &&
                (IS_SPACE(*q) || *q == '='))
            {
                SKIP_BLANKS(q);
                if (*q != '=')
                    return p;
                q++;
                extract_auth_val(&q);
            }
            else
                return p;
        }
        if (*q != '\0')
        {
            SKIP_BLANKS(q);
            if (*q == ',')
                q++;
            else
                break;
        }
    }
    return q;
}

static Str
get_auth_param(struct auth_param *auth, char *name)
{
    struct auth_param *ap;
    for (ap = auth; ap->name != NULL; ap++)
    {
        if (strcasecmp(name, ap->name) == 0)
            return ap->val;
    }
    return NULL;
}

static Str
AuthBasicCred(struct http_auth *ha, Str uname, Str pw, ParsedURL *pu,
              HRequest *hr, FormList *request)
{
    Str s = uname->Clone();
    s->Push(':');
    s->Push(pw);
    return Strnew_m_charp("Basic ", encodeB(s->ptr)->ptr, NULL);
}

#ifdef USE_DIGEST_AUTH
#include <openssl/md5.h>

/* RFC2617: 3.2.2 The Authorization Request Header
 * 
 * credentials      = "Digest" digest-response
 * digest-response  = 1#( username | realm | nonce | digest-uri
 *                    | response | [ algorithm ] | [cnonce] |
 *                     [opaque] | [message-qop] |
 *                         [nonce-count]  | [auth-param] )
 *
 * username         = "username" "=" username-value
 * username-value   = quoted-string
 * digest-uri       = "uri" "=" digest-uri-value
 * digest-uri-value = request-uri   ; As specified by HTTP/1.1
 * message-qop      = "qop" "=" qop-value
 * cnonce           = "cnonce" "=" cnonce-value
 * cnonce-value     = nonce-value
 * nonce-count      = "nc" "=" nc-value
 * nc-value         = 8LHEX
 * response         = "response" "=" request-digest
 * request-digest = <"> 32LHEX <">
 * LHEX             =  "0" | "1" | "2" | "3" |
 *                     "4" | "5" | "6" | "7" |
 *                     "8" | "9" | "a" | "b" |
 *                     "c" | "d" | "e" | "f"
 */

#include <openssl/md5.h>

static Str
digest_hex(unsigned char *p)
{
    auto *h = "0123456789abcdef";
    Str tmp = Strnew_size(MD5_DIGEST_LENGTH * 2 + 1);
    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++, p++)
    {
        tmp->Push(h[(*p >> 4) & 0x0f]);
        tmp->Push(h[*p & 0x0f]);
    }
    return tmp;
}

enum
{
    QOP_NONE,
    QOP_AUTH,
    QOP_AUTH_INT,
};

static Str
AuthDigestCred(struct http_auth *ha, Str uname, Str pw, ParsedURL *pu,
               HRequest *hr, FormList *request)
{
    Str tmp, a1buf, a2buf, rd, s;
    unsigned char md5[MD5_DIGEST_LENGTH + 1];
    Str uri = hr->URI(pu);
    char nc[] = "00000001";

    Str algorithm = qstr_unquote(get_auth_param(ha->param, "algorithm"));
    Str nonce = qstr_unquote(get_auth_param(ha->param, "nonce"));
    Str cnonce /* = qstr_unquote(get_auth_param(ha->param, "cnonce")) */;
    /* cnonce is what client should generate. */
    Str qop = qstr_unquote(get_auth_param(ha->param, "qop"));

    static union {
        int r[4];
        unsigned char s[sizeof(int) * 4];
    } cnonce_seed;
    int qop_i = QOP_NONE;

    cnonce_seed.r[0] = rand();
    cnonce_seed.r[1] = rand();
    cnonce_seed.r[2] = rand();
    MD5((const unsigned char *)cnonce_seed.s, sizeof(cnonce_seed.s), md5);
    cnonce = digest_hex(md5);
    cnonce_seed.r[3]++;

    if (qop)
    {
        char *p;
        size_t i;

        p = qop->ptr;
        SKIP_BLANKS(p);

        for (;;)
        {
            if ((i = strcspn(p, " \t,")) > 0)
            {
                if (i == sizeof("auth-int") - sizeof("") && !strncasecmp(p, "auth-int", i))
                {
                    if (qop_i < QOP_AUTH_INT)
                        qop_i = QOP_AUTH_INT;
                }
                else if (i == sizeof("auth") - sizeof("") && !strncasecmp(p, "auth", i))
                {
                    if (qop_i < QOP_AUTH)
                        qop_i = QOP_AUTH;
                }
            }

            if (p[i])
            {
                p += i + 1;
                SKIP_BLANKS(p);
            }
            else
                break;
        }
    }

    /* A1 = unq(username-value) ":" unq(realm-value) ":" passwd */
    tmp = Strnew_m_charp(uname->ptr, ":",
                         qstr_unquote(get_auth_param(ha->param, "realm"))->ptr,
                         ":", pw->ptr, NULL);
    MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
    a1buf = digest_hex(md5);

    if (algorithm)
    {
        if (strcasecmp(algorithm->ptr, "MD5-sess") == 0)
        {
            /* A1 = H(unq(username-value) ":" unq(realm-value) ":" passwd)
             *      ":" unq(nonce-value) ":" unq(cnonce-value)
             */
            if (nonce == NULL)
                return NULL;
            tmp = Strnew_m_charp(a1buf->ptr, ":",
                                 qstr_unquote(nonce)->ptr,
                                 ":", qstr_unquote(cnonce)->ptr, NULL);
            MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
            a1buf = digest_hex(md5);
        }
        else if (strcasecmp(algorithm->ptr, "MD5") == 0)
            /* ok default */
            ;
        else
            /* unknown algorithm */
            return NULL;
    }

    /* A2 = Method ":" digest-uri-value */
    tmp = Strnew_m_charp(hr->Method()->ptr, ":", uri->ptr, NULL);
    if (qop_i == QOP_AUTH_INT)
    {
        /*  A2 = Method ":" digest-uri-value ":" H(entity-body) */
        if (request && request->body)
        {
            if (request->method == FORM_METHOD_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
            {
                FILE *fp = fopen(request->body, "r");
                if (fp != NULL)
                {
                    Str ebody;
                    ebody = Strfgetall(fp);
                    MD5((const unsigned char *)ebody->ptr, strlen(ebody->ptr), md5);
                }
                else
                {
                    MD5((const unsigned char *)"", 0, md5);
                }
            }
            else
            {
                MD5((const unsigned char *)request->body, request->length, md5);
            }
        }
        else
        {
            MD5((const unsigned char *)"", 0, md5);
        }
        tmp->Push(':');
        tmp->Push(digest_hex(md5));
    }
    MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
    a2buf = digest_hex(md5);

    if (qop_i >= QOP_AUTH)
    {
        /* request-digest  = <"> < KD ( H(A1),     unq(nonce-value)
         *                      ":" nc-value
         *                      ":" unq(cnonce-value)
         *                      ":" unq(qop-value)
         *                      ":" H(A2)
         *                      ) <">
         */
        if (nonce == NULL)
            return NULL;
        tmp = Strnew_m_charp(a1buf->ptr, ":", qstr_unquote(nonce)->ptr,
                             ":", nc,
                             ":", qstr_unquote(cnonce)->ptr,
                             ":", qop_i == QOP_AUTH ? "auth" : "auth-int",
                             ":", a2buf->ptr, NULL);
        MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
        rd = digest_hex(md5);
    }
    else
    {
        /* compatibility with RFC 2069
         * request_digest = KD(H(A1),  unq(nonce), H(A2))
         */
        tmp = Strnew_m_charp(a1buf->ptr, ":",
                             qstr_unquote(get_auth_param(ha->param, "nonce"))->ptr, ":", a2buf->ptr, NULL);
        MD5((const unsigned char *)tmp->ptr, strlen(tmp->ptr), md5);
        rd = digest_hex(md5);
    }

    /*
     * digest-response  = 1#( username | realm | nonce | digest-uri
     *                          | response | [ algorithm ] | [cnonce] |
     *                          [opaque] | [message-qop] |
     *                          [nonce-count]  | [auth-param] )
     */

    tmp = Strnew_m_charp("Digest username=\"", uname->ptr, "\"", NULL);
    Strcat_m_charp(tmp, ", realm=",
                   get_auth_param(ha->param, "realm")->ptr, NULL);
    Strcat_m_charp(tmp, ", nonce=",
                   get_auth_param(ha->param, "nonce")->ptr, NULL);
    Strcat_m_charp(tmp, ", uri=\"", uri->ptr, "\"", NULL);
    Strcat_m_charp(tmp, ", response=\"", rd->ptr, "\"", NULL);

    if (algorithm)
        Strcat_m_charp(tmp, ", algorithm=",
                       get_auth_param(ha->param, "algorithm")->ptr, NULL);

    if (cnonce)
        Strcat_m_charp(tmp, ", cnonce=\"", cnonce->ptr, "\"", NULL);

    if ((s = get_auth_param(ha->param, "opaque")) != NULL)
        Strcat_m_charp(tmp, ", opaque=", s->ptr, NULL);

    if (qop_i >= QOP_AUTH)
    {
        Strcat_m_charp(tmp, ", qop=",
                       qop_i == QOP_AUTH ? "auth" : "auth-int",
                       NULL);
        /* XXX how to count? */
        /* Since nonce is unique up to each *-Authenticate and w3m does not re-use *-Authenticate: headers,
           nonce-count should be always "00000001". */
        Strcat_m_charp(tmp, ", nc=", nc, NULL);
    }

    return tmp;
}
#endif

/* *INDENT-OFF* */
struct auth_param none_auth_param[] = {
    {NULL, NULL}};

struct auth_param basic_auth_param[] = {
    {"realm", NULL},
    {NULL, NULL}};

#ifdef USE_DIGEST_AUTH
/* RFC2617: 3.2.1 The WWW-Authenticate Response Header
 * challenge        =  "Digest" digest-challenge
 * 
 * digest-challenge  = 1#( realm | [ domain ] | nonce |
 *                       [ opaque ] |[ stale ] | [ algorithm ] |
 *                        [ qop-options ] | [auth-param] )
 *
 * domain            = "domain" "=" <"> URI ( 1*SP URI ) <">
 * URI               = absoluteURI | abs_path
 * nonce             = "nonce" "=" nonce-value
 * nonce-value       = quoted-string
 * opaque            = "opaque" "=" quoted-string
 * stale             = "stale" "=" ( "true" | "false" )
 * algorithm         = "algorithm" "=" ( "MD5" | "MD5-sess" |
 *                        token )
 * qop-options       = "qop" "=" <"> 1#qop-value <">
 * qop-value         = "auth" | "auth-int" | token
 */
struct auth_param digest_auth_param[] = {
    {"realm", NULL},
    {"domain", NULL},
    {"nonce", NULL},
    {"opaque", NULL},
    {"stale", NULL},
    {"algorithm", NULL},
    {"qop", NULL},
    {NULL, NULL}};
#endif
/* for RFC2617: HTTP Authentication */
struct http_auth www_auth[] = {
    {1, "Basic ", basic_auth_param, AuthBasicCred},
#ifdef USE_DIGEST_AUTH
    {10, "Digest ", digest_auth_param, AuthDigestCred},
#endif
    {
        0,
        NULL,
        NULL,
        NULL,
    }};
/* *INDENT-ON* */

static struct http_auth *
findAuthentication(struct http_auth *hauth, BufferPtr buf, char *auth_field)
{
    struct http_auth *ha;
    int len = strlen(auth_field), slen;
    TextListItem *i;
    char *p0, *p;

    bzero(hauth, sizeof(struct http_auth));
    for (i = buf->document_header->first; i != NULL; i = i->next)
    {
        if (strncasecmp(i->ptr, auth_field, len) == 0)
        {
            for (p = i->ptr + len; p != NULL && *p != '\0';)
            {
                SKIP_BLANKS(p);
                p0 = p;
                for (ha = &www_auth[0]; ha->scheme != NULL; ha++)
                {
                    slen = strlen(ha->scheme);
                    if (strncasecmp(p, ha->scheme, slen) == 0)
                    {
                        p += slen;
                        SKIP_BLANKS(p);
                        if (hauth->pri < ha->pri)
                        {
                            *hauth = *ha;
                            p = extract_auth_param(p, hauth->param);
                            break;
                        }
                        else
                        {
                            /* weak auth */
                            p = extract_auth_param(p, none_auth_param);
                        }
                    }
                }
                if (p0 == p)
                {
                    /* all unknown auth failed */
                    int token_type;
                    if ((token_type = skip_auth_token(&p)) == AUTHCHR_TOKEN && IS_SPACE(*p))
                    {
                        SKIP_BLANKS(p);
                        p = extract_auth_param(p, none_auth_param);
                    }
                    else
                        break;
                }
            }
        }
    }
    return hauth->scheme ? hauth : NULL;
}

static void
getAuthCookie(struct http_auth *hauth, char *auth_header,
              TextList *extra_header, ParsedURL *pu, HRequest *hr,
              FormList *request,
              Str *uname, Str *pwd)
{
    Str ss = NULL;
    Str tmp;
    TextListItem *i;
    int a_found;
    int auth_header_len = strlen(auth_header);
    char *realm = NULL;
    int proxy;

    if (hauth)
        realm = qstr_unquote(get_auth_param(hauth->param, "realm"))->ptr;

    if (!realm)
        return;

    a_found = FALSE;
    for (i = extra_header->first; i != NULL; i = i->next)
    {
        if (!strncasecmp(i->ptr, auth_header, auth_header_len))
        {
            a_found = TRUE;
            break;
        }
    }
    proxy = !strncasecmp("Proxy-Authorization:", auth_header,
                         auth_header_len);
    if (a_found)
    {
        /* This means that *-Authenticate: header is received after
         * Authorization: header is sent to the server. 
         */
        if (fmInitialized)
        {
            message("Wrong username or password", 0, 0);
            refresh();
        }
        else
            fprintf(stderr, "Wrong username or password\n");
        sleep(1);
        /* delete Authenticate: header from extra_header */
        delText(extra_header, (char *)i);
        invalidate_auth_user_passwd(pu, realm, *uname, *pwd, proxy);
    }
    *uname = NULL;
    *pwd = NULL;

    if (!a_found && find_auth_user_passwd(pu, realm, (Str *)uname, (Str *)pwd,
                                          proxy))
    {
        /* found username & password in passwd file */;
    }
    else
    {
        if (QuietMessage)
            return;
        /* input username and password */
        sleep(2);
        if (fmInitialized)
        {
            char *pp;
            term_raw();
            /* FIXME: gettextize? */
            if ((pp = inputStr(Sprintf("Username for %s: ", realm)->ptr,
                               NULL)) == NULL)
                return;
            *uname = Str_conv_to_system(Strnew(pp));
            if ((pp = inputLine(Sprintf("Password for %s: ", realm)->ptr, NULL,
                                IN_PASSWORD)) == NULL)
            {
                *uname = NULL;
                return;
            }
            *pwd = Str_conv_to_system(Strnew(pp));
            term_cbreak();
        }
        else
        {
            /*
             * If post file is specified as '-', stdin is closed at this
             * point.
             * In this case, w3m cannot read username from stdin.
             * So exit with error message.
             * (This is same behavior as lwp-request.)
             */
            if (feof(stdin) || ferror(stdin))
            {
                /* FIXME: gettextize? */
                fprintf(stderr, "w3m: Authorization required for %s\n",
                        realm);
                exit(1);
            }

            /* FIXME: gettextize? */
            printf(proxy ? "Proxy Username for %s: " : "Username for %s: ",
                   realm);
            fflush(stdout);
            *uname = Strfgets(stdin);
            (*uname)->StripRight();
#ifdef HAVE_GETPASSPHRASE
            *pwd = Strnew((char *)
                              getpassphrase(proxy ? "Proxy Password: " : "Password: "));
#else
#ifndef __MINGW32_VERSION
            *pwd = Strnew((char *)
                              getpass(proxy ? "Proxy Password: " : "Password: "));
#else
            term_raw();
            *pwd = Strnew((char *)
                              inputLine(proxy ? "Proxy Password: " : "Password: ", NULL, IN_PASSWORD));
            term_cbreak();
#endif /* __MINGW32_VERSION */
#endif
        }
    }
    ss = hauth->cred(hauth, *uname, *pwd, pu, hr, request);
    if (ss)
    {
        tmp = Strnew(auth_header);
        Strcat_m_charp(tmp, " ", ss->ptr, "\r\n", NULL);
        pushText(extra_header, tmp->ptr);
    }
    else
    {
        *uname = NULL;
        *pwd = NULL;
    }
    return;
}

static int
same_url_p(ParsedURL *pu1, ParsedURL *pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port &&
            (pu1->host.size() ? pu2->host.size() ? pu1->host == pu2->host : 0 : 1) && (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

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
                           FollowRedirection, parsedURL2Str(pu)->ptr);
        disp_err_message(tmp->ptr, FALSE);
        return false;
    }

    g_puv.push_back(*pu);
    return true;
}

Str getLinkNumberStr(int correction)
{
    return Sprintf("[%d]", cur_hseq + correction);
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
    HRequest hr;
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
            if (stat(pu.real_file, &st) < 0)
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
                        copyParsedURL(&b->currentURL, &pu);
                        b->filename = b->currentURL.real_file;
                    }
                    return b;
                }
                else
                {
                    page = loadLocalDir(pu.real_file);
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
                    copyParsedURL(&b->currentURL, &pu);
                return b;
            }
#endif
            /* FIXME: gettextize? */
            disp_err_message(Sprintf("Unknown URI: %s",
                                     parsedURL2Str(&pu)->ptr)
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
        UFclose(&f);
        return NULL;
    }

    /* openURL() succeeded */
    if (SETJMP(AbortLoading) != 0)
    {
        /* transfer interrupted */
        TRAP_OFF;
        UFclose(&f);
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
            message(Sprintf("%s contacted. Waiting for reply...", pu.host)->ptr, 0, 0);
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
            UFclose(&f);
            copyParsedURL(current.get(), &pu);
            t_buf = newBuffer(INIT_BUFFER_WIDTH);
            t_buf->bufferprop |= BP_REDIRECTED;
            status = HTST_NORMAL;
            goto load_doc;
        }
        t = checkContentType(t_buf);
        if (t == NULL && pu.file != NULL)
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
                UFclose(&f);
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
                UFclose(&f);
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
            auto t1 = uncompressed_file_type(pu.file, NULL);
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
                UFclose(&f);
            else {
                UFclose(&f);
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
            UFclose(&f);
            add_auth_cookie_flag = 0;
            copyParsedURL(current.get(), &pu);
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
                UFclose(&f);
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
            copyParsedURL(&b->currentURL, &pu);
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

    copyParsedURL(GetCurBaseUrl(), &pu);

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
            if (PreserveTimestamp && !stat(pu.real_file, &st))
                f.modtime = st.st_mtime;
            file = conv_from_system(guess_save_name(NULL, pu.real_file));
        }
        else
            file = guess_save_name(t_buf, pu.file);
        if (doFileSave(f, file) == 0)
            UFhalfclose(&f);
        else
            UFclose(&f);
        return nullptr;
    }

    if ((f.content_encoding != CMP_NOCOMPRESS) && AutoUncompress && !(w3m_dump & DUMP_EXTRA))
    {
        uncompress_stream(&f, &pu.real_file);
    }
    else if (f.compression != CMP_NOCOMPRESS)
    {
        if (!(w3m_dump & DUMP_SOURCE) &&
            (w3m_dump & ~DUMP_FRAME || is_text_type(t) || searchExtViewer(t)))
        {
            if (t_buf == NULL)
                t_buf = newBuffer(INIT_BUFFER_WIDTH);
            uncompress_stream(&f, &t_buf->sourcefile);
            uncompressed_file_type(pu.file, &f.ext);
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
        UFclose(&f);
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
                                       pu.real_file ? pu.real_file : pu.file,
                                       t, &b, t_buf))
        {
            if (b)
            {
                b->real_scheme = f.scheme;
                b->real_type = real_type;
                if (b->currentURL.host.empty() && b->currentURL.file == NULL)
                    copyParsedURL(&b->currentURL, &pu);
            }
            UFclose(&f);
            TRAP_OFF;
            return b;
        }
        else
        {
            TRAP_OFF;
            if (pu.scheme == SCM_LOCAL)
            {
                UFclose(&f);
                _doFileCopy(pu.real_file,
                            conv_from_system(guess_save_name(NULL, pu.real_file)), TRUE);
            }
            else
            {
                if (DecodeCTE && IStype(f.stream) != IST_ENCODED)
                    f.stream = newEncodedStream(f.stream, f.encoding);
                if (doFileSave(f, guess_save_name(t_buf, pu.file)) == 0)
                    UFhalfclose(&f);
                else
                    UFclose(&f);
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
    b = loadSomething(&f, pu.real_file ? pu.real_file : pu.file, proc, t_buf);
    UFclose(&f);
    frame_source = 0;
    if (b)
    {
        b->real_scheme = f.scheme;
        b->real_type = real_type;
        if (b->currentURL.host.empty() && b->currentURL.file == NULL)
            copyParsedURL(&b->currentURL, &pu);
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
        if (pu.label)
        {
            if (proc == loadHTMLBuffer)
            {
                auto a = searchURLLabel(b, pu.label);
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
                int l = atoi(pu.label);
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

#define TAG_IS(s, tag, len) \
    (strncasecmp(s, tag, len) == 0 && (s[len] == '>' || IS_SPACE((int)s[len])))

static int
is_period_char(unsigned char *ch)
{
    switch (*ch)
    {
    case ',':
    case '.':
    case ':':
    case ';':
    case '?':
    case '!':
    case ')':
    case ']':
    case '}':
    case '>':
        return 1;
    default:
        return 0;
    }
}

static int
is_beginning_char(unsigned char *ch)
{
    switch (*ch)
    {
    case '(':
    case '[':
    case '{':
    case '`':
    case '<':
        return 1;
    default:
        return 0;
    }
}

static int
is_word_char(unsigned char *ch)
{
    Lineprop ctype = get_mctype(ch);

#ifdef USE_M17N
    if (ctype & (PC_CTRL | PC_KANJI | PC_UNKNOWN))
        return 0;
    if (ctype & (PC_WCHAR1 | PC_WCHAR2))
        return 1;
#else
    if (ctype == PC_CTRL)
        return 0;
#endif

    if (IS_ALNUM(*ch))
        return 1;

    switch (*ch)
    {
    case ',':
    case '.':
    case ':':
    case '\"': /* " */
    case '\'':
    case '$':
    case '%':
    case '*':
    case '+':
    case '-':
    case '@':
    case '~':
    case '_':
        return 1;
    }
#ifdef USE_M17N
    if (*ch == NBSP_CODE)
        return 1;
#else
    if (*ch == TIMES_CODE || *ch == DIVIDE_CODE || *ch == ANSP_CODE)
        return 0;
    if (*ch >= AGRAVE_CODE || *ch == NBSP_CODE)
        return 1;
#endif
    return 0;
}

#ifdef USE_M17N
static int
is_combining_char(unsigned char *ch)
{
    Lineprop ctype = get_mctype(ch);

    if (ctype & PC_WCHAR2)
        return 1;
    return 0;
}
#endif

int is_boundary(unsigned char *ch1, unsigned char *ch2)
{
    if (!*ch1 || !*ch2)
        return 1;

    if (*ch1 == ' ' && *ch2 == ' ')
        return 0;

    if (*ch1 != ' ' && is_period_char(ch2))
        return 0;

    if (*ch2 != ' ' && is_beginning_char(ch1))
        return 0;

#ifdef USE_M17N
    if (is_combining_char(ch2))
        return 0;
#endif
    if (is_word_char(ch1) && is_word_char(ch2))
        return 0;

    return 1;
}

Str process_img(struct parsed_tag *tag, int width)
{
    char *p, *q, *r, *r2 = NULL, *s, *t;
#ifdef USE_IMAGE
    int w, i, nw, ni = 1, n, w0 = -1, i0 = -1;
    int align, xoffset, yoffset, top, bottom, ismap = 0;
    int use_image = activeImage && displayImage;
#else
    int w, i, nw, n;
#endif
    int pre_int = FALSE, ext_pre_int = FALSE;
    Str tmp = Strnew();

    if (!parsedtag_get_value(tag, ATTR_SRC, &p))
        return tmp;
    p = remove_space(p);
    q = NULL;
    parsedtag_get_value(tag, ATTR_ALT, &q);
    if (!pseudoInlines && (q == NULL || (*q == '\0' && ignore_null_img_alt)))
        return tmp;
    t = q;
    parsedtag_get_value(tag, ATTR_TITLE, &t);
    w = -1;
    if (parsedtag_get_value(tag, ATTR_WIDTH, &w))
    {
        if (w < 0)
        {
            if (width > 0)
                w = (int)(-width * pixel_per_char * w / 100 + 0.5);
            else
                w = -1;
        }
#ifdef USE_IMAGE
        if (use_image)
        {
            if (w > 0)
            {
                w = (int)(w * image_scale / 100 + 0.5);
                if (w == 0)
                    w = 1;
                else if (w > MAX_IMAGE_SIZE)
                    w = MAX_IMAGE_SIZE;
            }
        }
#endif
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        i = -1;
        if (parsedtag_get_value(tag, ATTR_HEIGHT, &i))
        {
            if (i > 0)
            {
                i = (int)(i * image_scale / 100 + 0.5);
                if (i == 0)
                    i = 1;
                else if (i > MAX_IMAGE_SIZE)
                    i = MAX_IMAGE_SIZE;
            }
            else
            {
                i = -1;
            }
        }
        align = -1;
        parsedtag_get_value(tag, ATTR_ALIGN, &align);
        ismap = 0;
        if (parsedtag_exists(tag, ATTR_ISMAP))
            ismap = 1;
    }
    else
#endif
        parsedtag_get_value(tag, ATTR_HEIGHT, &i);
    r = NULL;
    parsedtag_get_value(tag, ATTR_USEMAP, &r);
    if (parsedtag_exists(tag, ATTR_PRE_INT))
        ext_pre_int = TRUE;

    tmp = Strnew_size(128);
#ifdef USE_IMAGE
    if (use_image)
    {
        switch (align)
        {
        case ALIGN_LEFT:
            tmp->Push("<div_int align=left>");
            break;
        case ALIGN_CENTER:
            tmp->Push("<div_int align=center>");
            break;
        case ALIGN_RIGHT:
            tmp->Push("<div_int align=right>");
            break;
        }
    }
#endif
    if (r)
    {
        Str tmp2;
        r2 = strchr(r, '#');
        s = "<form_int method=internal action=map>";
        tmp2 = process_form(parse_tag(&s, TRUE));
        if (tmp2)
            tmp->Push(tmp2);
        tmp->Push(Sprintf("<input_alt fid=\"%d\" "
                          "type=hidden name=link value=\"",
                          cur_form_id));
        tmp->Push(html_quote((r2) ? r2 + 1 : r));
        tmp->Push(Sprintf("\"><input_alt hseq=\"%d\" fid=\"%d\" "
                          "type=submit no_effect=true>",
                          cur_hseq++, cur_form_id));
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        w0 = w;
        i0 = i;
        if (w < 0 || i < 0)
        {
            Image image;
            ParsedURL u;

            u.Parse2(wc_conv(p, InnerCharset, cur_document_charset)->ptr, GetCurBaseUrl());
            image.url = parsedURL2Str(&u)->ptr;
            if (!uncompressed_file_type(u.file, &image.ext))
                image.ext = filename_extension(u.file, TRUE);
            image.cache = NULL;
            image.width = w;
            image.height = i;

            image.cache = getImage(&image, GetCurBaseUrl(), IMG_FLAG_SKIP);
            if (image.cache && image.cache->width > 0 &&
                image.cache->height > 0)
            {
                w = w0 = image.cache->width;
                i = i0 = image.cache->height;
            }
            if (w < 0)
                w = 8 * pixel_per_char;
            if (i < 0)
                i = pixel_per_line;
        }
        nw = (w > 3) ? (int)((w - 3) / pixel_per_char + 1) : 1;
        ni = (i > 3) ? (int)((i - 3) / pixel_per_line + 1) : 1;
        tmp->Push(
            Sprintf("<pre_int><img_alt hseq=\"%d\" src=\"", cur_iseq++));
        pre_int = TRUE;
    }
    else
#endif
    {
        if (w < 0)
            w = 12 * pixel_per_char;
        nw = w ? (int)((w - 1) / pixel_per_char + 1) : 1;
        if (r)
        {
            tmp->Push("<pre_int>");
            pre_int = TRUE;
        }
        tmp->Push("<img_alt src=\"");
    }
    tmp->Push(html_quote(p));
    tmp->Push("\"");
    if (t)
    {
        tmp->Push(" title=\"");
        tmp->Push(html_quote(t));
        tmp->Push("\"");
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        if (w0 >= 0)
            tmp->Push(Sprintf(" width=%d", w0));
        if (i0 >= 0)
            tmp->Push(Sprintf(" height=%d", i0));
        switch (align)
        {
        case ALIGN_TOP:
            top = 0;
            bottom = ni - 1;
            yoffset = 0;
            break;
        case ALIGN_MIDDLE:
            top = ni / 2;
            bottom = top;
            if (top * 2 == ni)
                yoffset = (int)(((ni + 1) * pixel_per_line - i) / 2);
            else
                yoffset = (int)((ni * pixel_per_line - i) / 2);
            break;
        case ALIGN_BOTTOM:
            top = ni - 1;
            bottom = 0;
            yoffset = (int)(ni * pixel_per_line - i);
            break;
        default:
            top = ni - 1;
            bottom = 0;
            if (ni == 1 && ni * pixel_per_line > i)
                yoffset = 0;
            else
            {
                yoffset = (int)(ni * pixel_per_line - i);
                if (yoffset <= -2)
                    yoffset++;
            }
            break;
        }
        xoffset = (int)((nw * pixel_per_char - w) / 2);
        if (xoffset)
            tmp->Push(Sprintf(" xoffset=%d", xoffset));
        if (yoffset)
            tmp->Push(Sprintf(" yoffset=%d", yoffset));
        if (top)
            tmp->Push(Sprintf(" top_margin=%d", top));
        if (bottom)
            tmp->Push(Sprintf(" bottom_margin=%d", bottom));
        if (r)
        {
            tmp->Push(" usemap=\"");
            tmp->Push(html_quote((r2) ? r2 + 1 : r));
            tmp->Push("\"");
        }
        if (ismap)
            tmp->Push(" ismap");
    }
#endif
    tmp->Push(">");
    if (q != NULL && *q == '\0' && ignore_null_img_alt)
        q = NULL;
    if (q != NULL)
    {
        n = get_strwidth(q);
#ifdef USE_IMAGE
        if (use_image)
        {
            if (n > nw)
            {
                char *r;
                for (r = q, n = 0; r; r += get_mclen(r), n += get_mcwidth(r))
                {
                    if (n + get_mcwidth(r) > nw)
                        break;
                }
                tmp->Push(html_quote(Strnew_charp_n(q, r - q)->ptr));
            }
            else
                tmp->Push(html_quote(q));
        }
        else
#endif
            tmp->Push(html_quote(q));
        goto img_end;
    }
    if (w > 0 && i > 0)
    {
        /* guess what the image is! */
        if (w < 32 && i < 48)
        {
            /* must be an icon or space */
            n = 1;
            if (strcasestr(p, "space") || strcasestr(p, "blank"))
                tmp->Push("_");
            else
            {
                if (w * i < 8 * 16)
                    tmp->Push("*");
                else
                {
                    if (!pre_int)
                    {
                        tmp->Push("<pre_int>");
                        pre_int = TRUE;
                    }
                    push_symbol(tmp, IMG_SYMBOL, symbol_width, 1);
                    n = symbol_width;
                }
            }
            goto img_end;
        }
        if (w > 200 && i < 13)
        {
            /* must be a horizontal line */
            if (!pre_int)
            {
                tmp->Push("<pre_int>");
                pre_int = TRUE;
            }
            w = w / pixel_per_char / symbol_width;
            if (w <= 0)
                w = 1;
            push_symbol(tmp, HR_SYMBOL, symbol_width, w);
            n = w * symbol_width;
            goto img_end;
        }
    }
    for (q = p; *q; q++)
        ;
    while (q > p && *q != '/')
        q--;
    if (*q == '/')
        q++;
    tmp->Push('[');
    n = 1;
    p = q;
    for (; *q; q++)
    {
        if (!IS_ALNUM(*q) && *q != '_' && *q != '-')
        {
            break;
        }
        tmp->Push(*q);
        n++;
        if (n + 1 >= nw)
            break;
    }
    tmp->Push(']');
    n++;
img_end:
#ifdef USE_IMAGE
    if (use_image)
    {
        for (; n < nw; n++)
            tmp->Push(' ');
    }
#endif
    tmp->Push("</img_alt>");
    if (pre_int && !ext_pre_int)
        tmp->Push("</pre_int>");
    if (r)
    {
        tmp->Push("</input_alt>");
        process_n_form();
    }
#ifdef USE_IMAGE
    if (use_image)
    {
        switch (align)
        {
        case ALIGN_RIGHT:
        case ALIGN_CENTER:
        case ALIGN_LEFT:
            tmp->Push("</div_int>");
            break;
        }
    }
#endif
    return tmp;
}

Str process_anchor(struct parsed_tag *tag, char *tagbuf)
{
    if (parsedtag_need_reconstruct(tag))
    {
        parsedtag_set_value(tag, ATTR_HSEQ, Sprintf("%d", cur_hseq++)->ptr);
        return parsedtag2str(tag);
    }
    else
    {
        Str tmp = Sprintf("<a hseq=\"%d\"", cur_hseq++);
        tmp->Push(tagbuf + 2);
        return tmp;
    }
}

Str process_input(struct parsed_tag *tag)
{
    int i, w, v, x, y, z, iw, ih;
    char *q, *p, *r, *p2, *s;
    Str tmp = NULL;
    char *qq = "";
    int qlen = 0;

    if (cur_form_id < 0)
    {
        char *s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }
    if (tmp == NULL)
        tmp = Strnew();

    p = "text";
    parsedtag_get_value(tag, ATTR_TYPE, &p);
    q = NULL;
    parsedtag_get_value(tag, ATTR_VALUE, &q);
    r = "";
    parsedtag_get_value(tag, ATTR_NAME, &r);
    w = 20;
    parsedtag_get_value(tag, ATTR_SIZE, &w);
    i = 20;
    parsedtag_get_value(tag, ATTR_MAXLENGTH, &i);
    p2 = NULL;
    parsedtag_get_value(tag, ATTR_ALT, &p2);
    x = parsedtag_exists(tag, ATTR_CHECKED);
    y = parsedtag_exists(tag, ATTR_ACCEPT);
    z = parsedtag_exists(tag, ATTR_READONLY);

    v = formtype(p);
    if (v == FORM_UNKNOWN)
        return NULL;

    if (!q)
    {
        switch (v)
        {
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            q = "SUBMIT";
            break;
        case FORM_INPUT_RESET:
            q = "RESET";
            break;
            /* if no VALUE attribute is specified in 
             * <INPUT TYPE=CHECKBOX> tag, then the value "on" is used 
             * as a default value. It is not a part of HTML4.0 
             * specification, but an imitation of Netscape behaviour. 
             */
        case FORM_INPUT_CHECKBOX:
            q = "on";
        }
    }
    /* VALUE attribute is not allowed in <INPUT TYPE=FILE> tag. */
    if (v == FORM_INPUT_FILE)
        q = NULL;
    if (q)
    {
        qq = html_quote(q);
        qlen = get_strwidth(q);
    }

    tmp->Push("<pre_int>");
    switch (v)
    {
    case FORM_INPUT_PASSWORD:
    case FORM_INPUT_TEXT:
    case FORM_INPUT_FILE:
    case FORM_INPUT_CHECKBOX:
        if (displayLinkNumber)
            tmp->Push(getLinkNumberStr(0));
        tmp->Push('[');
        break;
    case FORM_INPUT_RADIO:
        if (displayLinkNumber)
            tmp->Push(getLinkNumberStr(0));
        tmp->Push('(');
    }
    tmp->Push(Sprintf("<input_alt hseq=\"%d\" fid=\"%d\" type=%s "
                      "name=\"%s\" width=%d maxlength=%d value=\"%s\"",
                      cur_hseq++, cur_form_id, p, html_quote(r), w, i, qq));
    if (x)
        tmp->Push(" checked");
    if (y)
        tmp->Push(" accept");
    if (z)
        tmp->Push(" readonly");
    tmp->Push('>');

    if (v == FORM_INPUT_HIDDEN)
        tmp->Push("</input_alt></pre_int>");
    else
    {
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            tmp->Push("<u>");
            break;
        case FORM_INPUT_IMAGE:
            s = NULL;
            parsedtag_get_value(tag, ATTR_SRC, &s);
            if (s)
            {
                tmp->Push(Sprintf("<img src=\"%s\"", html_quote(s)));
                if (p2)
                    tmp->Push(Sprintf(" alt=\"%s\"", html_quote(p2)));
                if (parsedtag_get_value(tag, ATTR_WIDTH, &iw))
                    tmp->Push(Sprintf(" width=\"%d\"", iw));
                if (parsedtag_get_value(tag, ATTR_HEIGHT, &ih))
                    tmp->Push(Sprintf(" height=\"%d\"", ih));
                tmp->Push(" pre_int>");
                tmp->Push("</input_alt></pre_int>");
                return tmp;
            }
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            if (displayLinkNumber)
                tmp->Push(getLinkNumberStr(-1));
            tmp->Push("[");
            break;
        }
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
            i = 0;
            if (q)
            {
                for (; i < qlen && i < w; i++)
                    tmp->Push('*');
            }
            for (; i < w; i++)
                tmp->Push(' ');
            break;
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            if (q)
                tmp->Push(textfieldrep(Strnew(q), w));
            else
            {
                for (i = 0; i < w; i++)
                    tmp->Push(' ');
            }
            break;
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
            if (p2)
                tmp->Push(html_quote(p2));
            else
                tmp->Push(qq);
            break;
        case FORM_INPUT_RESET:
            tmp->Push(qq);
            break;
        case FORM_INPUT_RADIO:
        case FORM_INPUT_CHECKBOX:
            if (x)
                tmp->Push('*');
            else
                tmp->Push(' ');
            break;
        }
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
            tmp->Push("</u>");
            break;
        case FORM_INPUT_IMAGE:
        case FORM_INPUT_SUBMIT:
        case FORM_INPUT_BUTTON:
        case FORM_INPUT_RESET:
            tmp->Push("]");
        }
        tmp->Push("</input_alt>");
        switch (v)
        {
        case FORM_INPUT_PASSWORD:
        case FORM_INPUT_TEXT:
        case FORM_INPUT_FILE:
        case FORM_INPUT_CHECKBOX:
            tmp->Push(']');
            break;
        case FORM_INPUT_RADIO:
            tmp->Push(')');
        }
        tmp->Push("</pre_int>");
    }
    return tmp;
}

Str process_select(struct parsed_tag *tag)
{
    Str tmp = NULL;
    char *p;

    if (cur_form_id < 0)
    {
        char *s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }

    p = "";
    parsedtag_get_value(tag, ATTR_NAME, &p);
    cur_select = Strnew(p);
    select_is_multiple = parsedtag_exists(tag, ATTR_MULTIPLE);

#ifdef MENU_SELECT
    if (!select_is_multiple)
    {
        select_str = Strnew("<pre_int>");
        if (displayLinkNumber)
            select_str->Push(getLinkNumberStr(0));
        select_str->Push(Sprintf("[<input_alt hseq=\"%d\" "
                                 "fid=\"%d\" type=select name=\"%s\" selectnumber=%d",
                                 cur_hseq++, cur_form_id, html_quote(p), n_select));
        select_str->Push(">");
        if (n_select == max_select)
        {
            max_select *= 2;
            select_option =
                New_Reuse(FormSelectOption, select_option, max_select);
        }
        select_option[n_select].first = NULL;
        select_option[n_select].last = NULL;
        cur_option_maxwidth = 0;
    }
    else
#endif /* MENU_SELECT */
        select_str = Strnew();
    cur_option = NULL;
    cur_status = R_ST_NORMAL;
    n_selectitem = 0;
    return tmp;
}

Str process_n_select(void)
{
    if (cur_select == NULL)
        return NULL;
    process_option();
#ifdef MENU_SELECT
    if (!select_is_multiple)
    {
        if (select_option[n_select].first)
        {
            FormItemList sitem;
            chooseSelectOption(&sitem, select_option[n_select].first);
            select_str->Push(textfieldrep(sitem.label, cur_option_maxwidth));
        }
        select_str->Push("</input_alt>]</pre_int>");
        n_select++;
    }
    else
#endif /* MENU_SELECT */
        select_str->Push("<br>");
    cur_select = NULL;
    n_selectitem = 0;
    return select_str;
}

void feed_select(char *str)
{
    Str tmp = Strnew();
    int prev_status = cur_status;
    static int prev_spaces = -1;
    char *p;

    if (cur_select == NULL)
        return;
    while (read_token(tmp, &str, &cur_status, 0, 0))
    {
        if (cur_status != R_ST_NORMAL || prev_status != R_ST_NORMAL)
            continue;
        p = tmp->ptr;
        if (tmp->ptr[0] == '<' && tmp->Back() == '>')
        {
            struct parsed_tag *tag;
            char *q;
            if (!(tag = parse_tag(&p, FALSE)))
                continue;
            switch (tag->tagid)
            {
            case HTML_OPTION:
                process_option();
                cur_option = Strnew();
                if (parsedtag_get_value(tag, ATTR_VALUE, &q))
                    cur_option_value = Strnew(q);
                else
                    cur_option_value = NULL;
                if (parsedtag_get_value(tag, ATTR_LABEL, &q))
                    cur_option_label = Strnew(q);
                else
                    cur_option_label = NULL;
                cur_option_selected = parsedtag_exists(tag, ATTR_SELECTED);
                prev_spaces = -1;
                break;
            case HTML_N_OPTION:
                /* do nothing */
                break;
            default:
                /* never happen */
                break;
            }
        }
        else if (cur_option)
        {
            while (*p)
            {
                if (IS_SPACE(*p) && prev_spaces != 0)
                {
                    p++;
                    if (prev_spaces > 0)
                        prev_spaces++;
                }
                else
                {
                    if (IS_SPACE(*p))
                        prev_spaces = 1;
                    else
                        prev_spaces = 0;
                    if (*p == '&')
                        cur_option->Push(getescapecmd(p).second);
                    else
                        cur_option->Push(*(p++));
                }
            }
        }
    }
}

void process_option(void)
{
    char begin_char = '[', end_char = ']';
    int len;

    if (cur_select == NULL || cur_option == NULL)
        return;
    while (cur_option->Size() > 0 && IS_SPACE(cur_option->Back()))
        cur_option->Pop(1);
    if (cur_option_value == NULL)
        cur_option_value = cur_option;
    if (cur_option_label == NULL)
        cur_option_label = cur_option;
#ifdef MENU_SELECT
    if (!select_is_multiple)
    {
        len = get_Str_strwidth(cur_option_label);
        if (len > cur_option_maxwidth)
            cur_option_maxwidth = len;
        addSelectOption(&select_option[n_select],
                        cur_option_value,
                        cur_option_label, cur_option_selected);
        return;
    }
#endif /* MENU_SELECT */
    if (!select_is_multiple)
    {
        begin_char = '(';
        end_char = ')';
    }
    select_str->Push(Sprintf("<br><pre_int>%c<input_alt hseq=\"%d\" "
                             "fid=\"%d\" type=%s name=\"%s\" value=\"%s\"",
                             begin_char, cur_hseq++, cur_form_id,
                             select_is_multiple ? "checkbox" : "radio",
                             html_quote(cur_select->ptr),
                             html_quote(cur_option_value->ptr)));
    if (cur_option_selected)
        select_str->Push(" checked>*</input_alt>");
    else
        select_str->Push("> </input_alt>");
    select_str->Push(end_char);
    select_str->Push(html_quote(cur_option_label->ptr));
    select_str->Push("</pre_int>");
    n_selectitem++;
}

Str process_textarea(struct parsed_tag *tag, int width)
{
    Str tmp = NULL;
    char *p;
#define TEXTAREA_ATTR_COL_MAX 4096
#define TEXTAREA_ATTR_ROWS_MAX 4096

    if (cur_form_id < 0)
    {
        char *s = "<form_int method=internal action=none>";
        tmp = process_form(parse_tag(&s, TRUE));
    }

    p = "";
    parsedtag_get_value(tag, ATTR_NAME, &p);
    cur_textarea = Strnew(p);
    cur_textarea_size = 20;
    if (parsedtag_get_value(tag, ATTR_COLS, &p))
    {
        cur_textarea_size = atoi(p);
        if (p[strlen(p) - 1] == '%')
            cur_textarea_size = width * cur_textarea_size / 100 - 2;
        if (cur_textarea_size <= 0)
        {
            cur_textarea_size = 20;
        }
        else if (cur_textarea_size > TEXTAREA_ATTR_COL_MAX)
        {
            cur_textarea_size = TEXTAREA_ATTR_COL_MAX;
        }
    }
    cur_textarea_rows = 1;
    if (parsedtag_get_value(tag, ATTR_ROWS, &p))
    {
        cur_textarea_rows = atoi(p);
        if (cur_textarea_rows <= 0)
        {
            cur_textarea_rows = 1;
        }
        else if (cur_textarea_rows > TEXTAREA_ATTR_ROWS_MAX)
        {
            cur_textarea_rows = TEXTAREA_ATTR_ROWS_MAX;
        }
    }
    cur_textarea_readonly = parsedtag_exists(tag, ATTR_READONLY);
    if (n_textarea >= max_textarea)
    {
        max_textarea *= 2;
        textarea_str = New_Reuse(Str, textarea_str, max_textarea);
    }
    textarea_str[n_textarea] = Strnew();
    ignore_nl_textarea = TRUE;

    return tmp;
}

Str process_n_textarea(void)
{
    Str tmp;
    int i;

    if (cur_textarea == NULL)
        return NULL;

    tmp = Strnew();
    tmp->Push(Sprintf("<pre_int>[<input_alt hseq=\"%d\" fid=\"%d\" "
                      "type=textarea name=\"%s\" size=%d rows=%d "
                      "top_margin=%d textareanumber=%d",
                      cur_hseq, cur_form_id,
                      html_quote(cur_textarea->ptr),
                      cur_textarea_size, cur_textarea_rows,
                      cur_textarea_rows - 1, n_textarea));
    if (cur_textarea_readonly)
        tmp->Push(" readonly");
    tmp->Push("><u>");
    for (i = 0; i < cur_textarea_size; i++)
        tmp->Push(' ');
    tmp->Push("</u></input_alt>]</pre_int>\n");
    cur_hseq++;
    n_textarea++;
    cur_textarea = NULL;

    return tmp;
}

void feed_textarea(char *str)
{
    if (cur_textarea == NULL)
        return;
    if (ignore_nl_textarea)
    {
        if (*str == '\r')
            str++;
        if (*str == '\n')
            str++;
    }
    ignore_nl_textarea = FALSE;
    while (*str)
    {
        if (*str == '&')
            textarea_str[n_textarea]->Push(getescapecmd(str).second);
        else if (*str == '\n')
        {
            textarea_str[n_textarea]->Push("\r\n");
            str++;
        }
        else if (*str != '\r')
            textarea_str[n_textarea]->Push(*(str++));
    }
}

#ifdef USE_M17N
static char *
check_charset(char *p)
{
    return wc_guess_charset(p, 0) ? p : NULL;
}

static char *
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
#endif

static Str
process_form_int(struct parsed_tag *tag, int fid)
{
    char *p, *q, *r, *s, *tg, *n;

    p = "get";
    parsedtag_get_value(tag, ATTR_METHOD, &p);
    q = "!CURRENT_URL!";
    parsedtag_get_value(tag, ATTR_ACTION, &q);
    r = NULL;
#ifdef USE_M17N
    if (parsedtag_get_value(tag, ATTR_ACCEPT_CHARSET, &r))
        r = check_accept_charset(r);
    if (!r && parsedtag_get_value(tag, ATTR_CHARSET, &r))
        r = check_charset(r);
#endif
    s = NULL;
    parsedtag_get_value(tag, ATTR_ENCTYPE, &s);
    tg = NULL;
    parsedtag_get_value(tag, ATTR_TARGET, &tg);
    n = NULL;
    parsedtag_get_value(tag, ATTR_NAME, &n);

    if (fid < 0)
    {
        form_max++;
        form_sp++;
        fid = form_max;
    }
    else
    { /* <form_int> */
        if (form_max < fid)
            form_max = fid;
        form_sp = fid;
    }
    if (forms_size == 0)
    {
        forms_size = INITIAL_FORM_SIZE;
        forms = New_N(FormList *, forms_size);
        form_stack = NewAtom_N(int, forms_size);
    }
    else if (forms_size <= form_max)
    {
        forms_size += form_max;
        forms = New_Reuse(FormList *, forms, forms_size);
        form_stack = New_Reuse(int, form_stack, forms_size);
    }
    form_stack[form_sp] = fid;

    if (w3m_halfdump)
    {
        Str tmp = Sprintf("<form_int fid=\"%d\" action=\"%s\" method=\"%s\"",
                          fid, html_quote(q), html_quote(p));
        if (s)
            tmp->Push(Sprintf(" enctype=\"%s\"", html_quote(s)));
        if (tg)
            tmp->Push(Sprintf(" target=\"%s\"", html_quote(tg)));
        if (n)
            tmp->Push(Sprintf(" name=\"%s\"", html_quote(n)));
#ifdef USE_M17N
        if (r)
            tmp->Push(Sprintf(" accept-charset=\"%s\"", html_quote(r)));
#endif
        tmp->Push(">");
        return tmp;
    }

    forms[fid] = newFormList(q, p, r, s, tg, n, NULL);
    return NULL;
}

Str process_form(struct parsed_tag *tag)
{
    return process_form_int(tag, -1);
}

Str process_n_form(void)
{
    if (form_sp >= 0)
        form_sp--;
    return NULL;
}

int getMetaRefreshParam(char *q, Str *refresh_uri)
{
    int refresh_interval;
    char *r;
    Str s_tmp = NULL;

    if (q == NULL || refresh_uri == NULL)
        return 0;

    refresh_interval = atoi(q);
    if (refresh_interval < 0)
        return 0;

    while (*q)
    {
        if (!strncasecmp(q, "url=", 4))
        {
            q += 4;
            if (*q == '\"') /* " */
                q++;
            r = q;
            while (*r && !IS_SPACE(*r) && *r != ';')
                r++;
            s_tmp = Strnew_charp_n(q, r - q);

            if (s_tmp->ptr[s_tmp->Size() - 1] == '\"')
            { /* " 
                                                                 */
                s_tmp->Pop(1);
                s_tmp->ptr[s_tmp->Size()] = '\0';
            }
            q = r;
        }
        while (*q && *q != ';')
            q++;
        if (*q == ';')
            q++;
        while (*q && *q == ' ')
            q++;
    }
    *refresh_uri = s_tmp;
    return refresh_interval;
}

#define PPUSH(p, c)      \
    {                    \
        outp[pos] = (p); \
        outc[pos] = (c); \
        pos++;           \
    }
#define PSIZE                                       \
    if (out_size <= pos + 1)                        \
    {                                               \
        out_size = pos * 3 / 2;                     \
        outc = New_Reuse(char, outc, out_size);     \
        outp = New_Reuse(Lineprop, outp, out_size); \
    }

static TextLineListItem *_tl_lp2;

static Str
textlist_feed()
{
    TextLine *p;
    if (_tl_lp2 != NULL)
    {
        p = _tl_lp2->ptr;
        _tl_lp2 = _tl_lp2->next;
        return p->line;
    }
    return NULL;
}

static int
ex_efct(int ex)
{
    int effect = 0;

    if (!ex)
        return 0;

    if (ex & PE_EX_ITALIC)
        effect |= PE_EX_ITALIC_E;

    if (ex & PE_EX_INSERT)
        effect |= PE_EX_INSERT_E;

    if (ex & PE_EX_STRIKE)
        effect |= PE_EX_STRIKE_E;

    return effect;
}

static void
HTMLlineproc2body(BufferPtr buf, Str (*feed)(), int llimit)
{
    static char *outc = NULL;
    static Lineprop *outp = NULL;
    static int out_size = 0;
    Anchor *a_href = NULL, *a_img = NULL, *a_form = NULL;
    char *p, *q, *r, *s, *t, *str;
    Lineprop mode, effect, ex_effect;
    int pos;
    int nlines;
#ifdef DEBUG
    FILE *debug = NULL;
#endif
    struct frameset *frameset_s[FRAMESTACK_SIZE];
    int frameset_sp = -1;
    union frameset_element *idFrame = NULL;
    char *id = NULL;
    int hseq, form_id;
    Str line;
    char *endp;
    char symbol = '\0';
    int internal = 0;
    Anchor **a_textarea = NULL;
#ifdef MENU_SELECT
    Anchor **a_select = NULL;
#endif

    if (out_size == 0)
    {
        out_size = LINELEN;
        outc = NewAtom_N(char, out_size);
        outp = NewAtom_N(Lineprop, out_size);
    }

    n_textarea = -1;
    if (!max_textarea)
    { /* halfload */
        max_textarea = MAX_TEXTAREA;
        textarea_str = New_N(Str, max_textarea);
        a_textarea = New_N(Anchor *, max_textarea);
    }
#ifdef MENU_SELECT
    n_select = -1;
    if (!max_select)
    { /* halfload */
        max_select = MAX_SELECT;
        select_option = New_N(FormSelectOption, max_select);
        a_select = New_N(Anchor *, max_select);
    }
#endif

#ifdef DEBUG
    if (w3m_debug)
        debug = fopen("zzzerr", "a");
#endif

    effect = 0;
    ex_effect = 0;
    nlines = 0;
    while ((line = feed()) != NULL)
    {
#ifdef DEBUG
        if (w3m_debug)
        {
            line->Puts(debug);
            fputc('\n', debug);
        }
#endif
        if (n_textarea >= 0 && *(line->ptr) != '<')
        { /* halfload */
            textarea_str[n_textarea]->Push(line);
            continue;
        }
    proc_again:
        if (++nlines == llimit)
            break;
        pos = 0;
#ifdef ENABLE_REMOVE_TRAILINGSPACES
        line->StripRight();
#endif
        str = line->ptr;
        endp = str + line->Size();
        while (str < endp)
        {
            PSIZE;
            mode = get_mctype(str);
            if ((effect | ex_efct(ex_effect)) & PC_SYMBOL && *str != '<')
            {
#ifdef USE_M17N
                char **buf = set_symbol(symbol_width0);
                int len;

                p = buf[(int)symbol];
                len = get_mclen(p);
                mode = get_mctype(p);
                PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--)
                    {
                        PSIZE;
                        PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                    }
                }
#else
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), SYMBOL_BASE + symbol);
#endif
                str += symbol_width;
            }
#ifdef USE_M17N
            else if (mode == PC_CTRL || mode == PC_UNDEF)
            {
#else
            else if (mode == PC_CTRL || IS_INTSPACE(*str))
            {
#endif
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str++;
            }
#ifdef USE_M17N
            else if (mode & PC_UNKNOWN)
            {
                PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                str += get_mclen(str);
            }
#endif
            else if (*str != '<' && *str != '&')
            {
#ifdef USE_M17N
                int len = get_mclen(str);
#endif
                PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
#ifdef USE_M17N
                if (--len)
                {
                    mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                    while (len--)
                    {
                        PSIZE;
                        PPUSH(mode | effect | ex_efct(ex_effect), *(str++));
                    }
                }
#endif
            }
            else if (*str == '&')
            {
                /* 
                 * & escape processing
                 */
                {
                    auto [pos, view] = getescapecmd(str);
                    str = const_cast<char *>(pos);
                    p = const_cast<char *>(view.data());
                }

                while (*p)
                {
                    PSIZE;
                    mode = get_mctype((unsigned char *)p);
#ifdef USE_M17N
                    if (mode == PC_CTRL || mode == PC_UNDEF)
                    {
#else
                    if (mode == PC_CTRL || IS_INTSPACE(*str))
                    {
#endif
                        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p++;
                    }
#ifdef USE_M17N
                    else if (mode & PC_UNKNOWN)
                    {
                        PPUSH(PC_ASCII | effect | ex_efct(ex_effect), ' ');
                        p += get_mclen(p);
                    }
#endif
                    else
                    {
#ifdef USE_M17N
                        int len = get_mclen(p);
#endif
                        PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
#ifdef USE_M17N
                        if (--len)
                        {
                            mode = (mode & ~PC_WCHAR1) | PC_WCHAR2;
                            while (len--)
                            {
                                PSIZE;
                                PPUSH(mode | effect | ex_efct(ex_effect), *(p++));
                            }
                        }
#endif
                    }
                }
            }
            else
            {
                /* tag processing */
                struct parsed_tag *tag;
                if (!(tag = parse_tag(&str, TRUE)))
                    continue;
                switch (tag->tagid)
                {
                case HTML_B:
                    effect |= PE_BOLD;
                    break;
                case HTML_N_B:
                    effect &= ~PE_BOLD;
                    break;
                case HTML_I:
                    ex_effect |= PE_EX_ITALIC;
                    break;
                case HTML_N_I:
                    ex_effect &= ~PE_EX_ITALIC;
                    break;
                case HTML_INS:
                    ex_effect |= PE_EX_INSERT;
                    break;
                case HTML_N_INS:
                    ex_effect &= ~PE_EX_INSERT;
                    break;
                case HTML_U:
                    effect |= PE_UNDER;
                    break;
                case HTML_N_U:
                    effect &= ~PE_UNDER;
                    break;
                case HTML_S:
                    ex_effect |= PE_EX_STRIKE;
                    break;
                case HTML_N_S:
                    ex_effect &= ~PE_EX_STRIKE;
                    break;
                case HTML_A:
                    if (renderFrameSet &&
                        parsedtag_get_value(tag, ATTR_FRAMENAME, &p))
                    {
                        p = url_quote_conv(p, buf->document_charset);
                        if (!idFrame || strcmp(idFrame->body->name, p))
                        {
                            idFrame = search_frame(renderFrameSet, p);
                            if (idFrame && idFrame->body->attr != F_BODY)
                                idFrame = NULL;
                        }
                    }
                    p = r = s = NULL;
                    q = buf->baseTarget;
                    t = "";
                    hseq = 0;
                    id = NULL;
                    if (parsedtag_get_value(tag, ATTR_NAME, &id))
                    {
                        id = url_quote_conv(id, buf->document_charset);
                        buf->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
                    }
                    if (parsedtag_get_value(tag, ATTR_HREF, &p))
                        p = url_quote_conv(remove_space(p),
                                           buf->document_charset);
                    if (parsedtag_get_value(tag, ATTR_TARGET, &q))
                        q = url_quote_conv(q, buf->document_charset);
                    if (parsedtag_get_value(tag, ATTR_REFERER, &r))
                        r = url_quote_conv(r, buf->document_charset);
                    parsedtag_get_value(tag, ATTR_TITLE, &s);
                    parsedtag_get_value(tag, ATTR_ACCESSKEY, &t);
                    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
                    if (hseq > 0)
                        buf->hmarklist =
                            putHmarker(buf->hmarklist, currentLn(buf),
                                       pos, hseq - 1);
                    else if (hseq < 0)
                    {
                        int h = -hseq - 1;
                        if (buf->hmarklist &&
                            h < buf->hmarklist->nmark &&
                            buf->hmarklist->marks[h].invalid)
                        {
                            buf->hmarklist->marks[h].pos = pos;
                            buf->hmarklist->marks[h].line = currentLn(buf);
                            buf->hmarklist->marks[h].invalid = 0;
                            hseq = -hseq;
                        }
                    }
                    if (id && idFrame)
                    {
                        auto a = Anchor::CreateName(id, currentLn(buf), pos);
                        idFrame->body->nameList.Put(a);
                    }
                    if (p)
                    {
                        effect |= PE_ANCHOR;
                        a_href = buf->href.Put(Anchor::CreateHref(p,
                                                                  q ? q : "",
                                                                  r ? r : "",
                                                                  s ? s : "",
                                                                  *t, currentLn(buf), pos));
                        a_href->hseq = ((hseq > 0) ? hseq : -hseq) - 1;
                        a_href->slave = (hseq > 0) ? FALSE : TRUE;
                    }
                    break;
                case HTML_N_A:
                    effect &= ~PE_ANCHOR;
                    if (a_href)
                    {
                        a_href->end.line = currentLn(buf);
                        a_href->end.pos = pos;
                        if (a_href->start == a_href->end)
                        {
                            if (buf->hmarklist &&
                                a_href->hseq < buf->hmarklist->nmark)
                                buf->hmarklist->marks[a_href->hseq].invalid = 1;
                            a_href->hseq = -1;
                        }
                        a_href = NULL;
                    }
                    break;

                case HTML_LINK:
                    addLink(buf, tag);
                    break;

                case HTML_IMG_ALT:
                    if (parsedtag_get_value(tag, ATTR_SRC, &p))
                    {
#ifdef USE_IMAGE
                        int w = -1, h = -1, iseq = 0, ismap = 0;
                        int xoffset = 0, yoffset = 0, top = 0, bottom = 0;
                        parsedtag_get_value(tag, ATTR_HSEQ, &iseq);
                        parsedtag_get_value(tag, ATTR_WIDTH, &w);
                        parsedtag_get_value(tag, ATTR_HEIGHT, &h);
                        parsedtag_get_value(tag, ATTR_XOFFSET, &xoffset);
                        parsedtag_get_value(tag, ATTR_YOFFSET, &yoffset);
                        parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
                        parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
                        if (parsedtag_exists(tag, ATTR_ISMAP))
                            ismap = 1;
                        q = NULL;
                        parsedtag_get_value(tag, ATTR_USEMAP, &q);
                        if (iseq > 0)
                        {
                            buf->imarklist = putHmarker(buf->imarklist,
                                                        currentLn(buf), pos,
                                                        iseq - 1);
                        }
#endif
                        s = NULL;
                        parsedtag_get_value(tag, ATTR_TITLE, &s);
                        p = url_quote_conv(remove_space(p),
                                           buf->document_charset);
                        a_img = buf->img.Put(Anchor::CreateImage(
                            p,
                            s ? s : "",
                            currentLn(buf), pos));
#ifdef USE_IMAGE
                        a_img->hseq = iseq;
                        a_img->image = NULL;
                        if (iseq > 0)
                        {
                            ParsedURL u;
                            Image *image;

                            u.Parse2(a_img->url, GetCurBaseUrl());
                            a_img->image = image = New(Image);
                            image->url = parsedURL2Str(&u)->ptr;
                            if (!uncompressed_file_type(u.file, &image->ext))
                                image->ext = filename_extension(u.file, TRUE);
                            image->cache = NULL;
                            image->width =
                                (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
                            image->height =
                                (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
                            image->xoffset = xoffset;
                            image->yoffset = yoffset;
                            image->y = currentLn(buf) - top;
                            if (image->xoffset < 0 && pos == 0)
                                image->xoffset = 0;
                            if (image->yoffset < 0 && image->y == 1)
                                image->yoffset = 0;
                            image->rows = 1 + top + bottom;
                            image->map = q;
                            image->ismap = ismap;
                            image->touch = 0;
                            image->cache = getImage(image, GetCurBaseUrl(),
                                                    IMG_FLAG_SKIP);
                        }
                        else if (iseq < 0)
                        {
                            BufferPoint *po = buf->imarklist->marks - iseq - 1;
                            auto a = buf->img.RetrieveAnchor(po->line, po->pos);
                            if (a)
                            {
                                a_img->url = a->url;
                                a_img->image = a->image;
                            }
                        }
#endif
                    }
                    effect |= PE_IMAGE;
                    break;
                case HTML_N_IMG_ALT:
                    effect &= ~PE_IMAGE;
                    if (a_img)
                    {
                        a_img->end.line = currentLn(buf);
                        a_img->end.pos = pos;
                    }
                    a_img = NULL;
                    break;
                case HTML_INPUT_ALT:
                {
                    FormList *form;
                    int top = 0, bottom = 0;
                    int textareanumber = -1;
#ifdef MENU_SELECT
                    int selectnumber = -1;
#endif
                    hseq = 0;
                    form_id = -1;

                    parsedtag_get_value(tag, ATTR_HSEQ, &hseq);
                    parsedtag_get_value(tag, ATTR_FID, &form_id);
                    parsedtag_get_value(tag, ATTR_TOP_MARGIN, &top);
                    parsedtag_get_value(tag, ATTR_BOTTOM_MARGIN, &bottom);
                    if (form_id < 0 || form_id > form_max || forms == NULL)
                        break; /* outside of <form>..</form> */
                    form = forms[form_id];
                    if (hseq > 0)
                    {
                        int hpos = pos;
                        if (*str == '[')
                            hpos++;
                        buf->hmarklist =
                            putHmarker(buf->hmarklist, currentLn(buf),
                                       hpos, hseq - 1);
                    }
                    if (!form->target)
                        form->target = buf->baseTarget;
                    if (a_textarea &&
                        parsedtag_get_value(tag, ATTR_TEXTAREANUMBER,
                                            &textareanumber))
                    {
                        if (textareanumber >= max_textarea)
                        {
                            max_textarea = 2 * textareanumber;
                            textarea_str = New_Reuse(Str, textarea_str,
                                                     max_textarea);
                            a_textarea = New_Reuse(Anchor *, a_textarea,
                                                   max_textarea);
                        }
                    }
#ifdef MENU_SELECT
                    if (a_select &&
                        parsedtag_get_value(tag, ATTR_SELECTNUMBER,
                                            &selectnumber))
                    {
                        if (selectnumber >= max_select)
                        {
                            max_select = 2 * selectnumber;
                            select_option = New_Reuse(FormSelectOption,
                                                      select_option,
                                                      max_select);
                            a_select = New_Reuse(Anchor *, a_select,
                                                 max_select);
                        }
                    }
#endif

                    auto fi = formList_addInput(form, tag);
                    if (fi)
                    {
                        Anchor a;
                        a.target = form->target ? form->target : "";
                        a.item = fi;
                        BufferPoint bp = {
                            line : currentLn(buf),
                            pos : pos
                        };
                        a.start = bp;
                        a.end = bp;
                        a_form = buf->formitem.Put(a);
                    }
                    else
                    {
                        a_form = nullptr;
                    }

                    if (a_textarea && textareanumber >= 0)
                        a_textarea[textareanumber] = a_form;
#ifdef MENU_SELECT
                    if (a_select && selectnumber >= 0)
                        a_select[selectnumber] = a_form;
#endif
                    if (a_form)
                    {
                        a_form->hseq = hseq - 1;
                        a_form->y = currentLn(buf) - top;
                        a_form->rows = 1 + top + bottom;
                        if (!parsedtag_exists(tag, ATTR_NO_EFFECT))
                            effect |= PE_FORM;
                        break;
                    }
                }
                case HTML_N_INPUT_ALT:
                    effect &= ~PE_FORM;
                    if (a_form)
                    {
                        a_form->end.line = currentLn(buf);
                        a_form->end.pos = pos;
                        if (a_form->start.line == a_form->end.line &&
                            a_form->start.pos == a_form->end.pos)
                            a_form->hseq = -1;
                    }
                    a_form = NULL;
                    break;
                case HTML_MAP:
                    if (parsedtag_get_value(tag, ATTR_NAME, &p))
                    {
                        MapList *m = New(MapList);
                        m->name = Strnew(p);
                        m->area = newGeneralList();
                        m->next = buf->maplist;
                        buf->maplist = m;
                    }
                    break;
                case HTML_N_MAP:
                    /* nothing to do */
                    break;
                case HTML_AREA:
                    if (buf->maplist == NULL) /* outside of <map>..</map> */
                        break;
                    if (parsedtag_get_value(tag, ATTR_HREF, &p))
                    {
                        MapArea *a;
                        p = url_quote_conv(remove_space(p),
                                           buf->document_charset);
                        t = NULL;
                        parsedtag_get_value(tag, ATTR_TARGET, &t);
                        q = "";
                        parsedtag_get_value(tag, ATTR_ALT, &q);
                        r = NULL;
                        s = NULL;
#ifdef USE_IMAGE
                        parsedtag_get_value(tag, ATTR_SHAPE, &r);
                        parsedtag_get_value(tag, ATTR_COORDS, &s);
#endif
                        a = newMapArea(p, t, q, r, s);
                        pushValue(buf->maplist->area, (void *)a);
                    }
                    break;
                case HTML_FRAMESET:
                    frameset_sp++;
                    if (frameset_sp >= FRAMESTACK_SIZE)
                        break;
                    frameset_s[frameset_sp] = newFrameSet(tag);
                    if (frameset_s[frameset_sp] == NULL)
                        break;
                    if (frameset_sp == 0)
                    {
                        if (buf->frameset == NULL)
                        {
                            buf->frameset = frameset_s[frameset_sp];
                        }
                        else
                            pushFrameTree(&(buf->frameQ),
                                          frameset_s[frameset_sp], NULL);
                    }
                    else
                        addFrameSetElement(frameset_s[frameset_sp - 1],
                                           *(union frameset_element *)&frameset_s[frameset_sp]);
                    break;
                case HTML_N_FRAMESET:
                    if (frameset_sp >= 0)
                        frameset_sp--;
                    break;
                case HTML_FRAME:
                    if (frameset_sp >= 0 && frameset_sp < FRAMESTACK_SIZE)
                    {
                        union frameset_element element;

                        element.body = newFrame(tag, buf);
                        addFrameSetElement(frameset_s[frameset_sp], element);
                    }
                    break;
                case HTML_BASE:
                    if (parsedtag_get_value(tag, ATTR_HREF, &p))
                    {
                        p = url_quote_conv(remove_space(p),
                                           buf->document_charset);
                        buf->baseURL.Parse(p, NULL);
                    }
                    if (parsedtag_get_value(tag, ATTR_TARGET, &p))
                        buf->baseTarget =
                            url_quote_conv(p, buf->document_charset);
                    break;
                case HTML_META:
                    p = q = NULL;
                    parsedtag_get_value(tag, ATTR_HTTP_EQUIV, &p);
                    parsedtag_get_value(tag, ATTR_CONTENT, &q);
                    if (p && q && !strcasecmp(p, "refresh") && MetaRefresh)
                    {
                        Str tmp = NULL;
                        int refresh_interval = getMetaRefreshParam(q, &tmp);
#ifdef USE_ALARM
                        if (tmp)
                        {
                            p = url_quote_conv(remove_space(tmp->ptr),
                                               buf->document_charset);
                            buf->event = setAlarmEvent(buf->event,
                                                       refresh_interval,
                                                       AL_IMPLICIT_ONCE,
                                                       &gorURL, p);
                        }
                        else if (refresh_interval > 0)
                            buf->event = setAlarmEvent(buf->event,
                                                       refresh_interval,
                                                       AL_IMPLICIT,
                                                       &reload, NULL);
#else
                        if (tmp && refresh_interval == 0)
                        {
                            p = url_quote_conv(remove_space(tmp->ptr),
                                               buf->document_charset);
                            pushEvent(FUNCNAME_gorURL, p);
                        }
#endif
                    }
                    break;
                case HTML_INTERNAL:
                    internal = HTML_INTERNAL;
                    break;
                case HTML_N_INTERNAL:
                    internal = HTML_N_INTERNAL;
                    break;
                case HTML_FORM_INT:
                    if (parsedtag_get_value(tag, ATTR_FID, &form_id))
                        process_form_int(tag, form_id);
                    break;
                case HTML_TEXTAREA_INT:
                    if (parsedtag_get_value(tag, ATTR_TEXTAREANUMBER,
                                            &n_textarea) &&
                        n_textarea < max_textarea)
                    {
                        textarea_str[n_textarea] = Strnew();
                    }
                    else
                        n_textarea = -1;
                    break;
                case HTML_N_TEXTAREA_INT:
                    if (n_textarea >= 0)
                    {
                        FormItemList *item = a_textarea[n_textarea]->item;
                        item->init_value = item->value =
                            textarea_str[n_textarea];
                    }
                    break;
#ifdef MENU_SELECT
                case HTML_SELECT_INT:
                    if (parsedtag_get_value(tag, ATTR_SELECTNUMBER, &n_select) && n_select < max_select)
                    {
                        select_option[n_select].first = NULL;
                        select_option[n_select].last = NULL;
                    }
                    else
                        n_select = -1;
                    break;
                case HTML_N_SELECT_INT:
                    if (n_select >= 0)
                    {
                        FormItemList *item = a_select[n_select]->item;
                        item->select_option = select_option[n_select].first;
                        chooseSelectOption(item, item->select_option);
                        item->init_selected = item->selected;
                        item->init_value = item->value;
                        item->init_label = item->label;
                    }
                    break;
                case HTML_OPTION_INT:
                    if (n_select >= 0)
                    {
                        int selected;
                        q = "";
                        parsedtag_get_value(tag, ATTR_LABEL, &q);
                        p = q;
                        parsedtag_get_value(tag, ATTR_VALUE, &p);
                        selected = parsedtag_exists(tag, ATTR_SELECTED);
                        addSelectOption(&select_option[n_select],
                                        Strnew(p), Strnew(q),
                                        selected);
                    }
                    break;
#endif
                case HTML_TITLE_ALT:
                    if (parsedtag_get_value(tag, ATTR_TITLE, &p))
                        buf->buffername = html_unquote(p);
                    break;
                case HTML_SYMBOL:
                    effect |= PC_SYMBOL;
                    if (parsedtag_get_value(tag, ATTR_TYPE, &p))
                        symbol = (char)atoi(p);
                    break;
                case HTML_N_SYMBOL:
                    effect &= ~PC_SYMBOL;
                    break;
                }
#ifdef ID_EXT
                id = NULL;
                if (parsedtag_get_value(tag, ATTR_ID, &id))
                {
                    id = url_quote_conv(id, buf->document_charset);
                    buf->name.Put(Anchor::CreateName(id, currentLn(buf), pos));
                }
                if (renderFrameSet &&
                    parsedtag_get_value(tag, ATTR_FRAMENAME, &p))
                {
                    p = url_quote_conv(p, buf->document_charset);
                    if (!idFrame || strcmp(idFrame->body->name, p))
                    {
                        idFrame = search_frame(renderFrameSet, p);
                        if (idFrame && idFrame->body->attr != F_BODY)
                            idFrame = NULL;
                    }
                }
                if (id && idFrame)
                {
                    auto a = Anchor::CreateName(id, currentLn(buf), pos);
                    idFrame->body->nameList.Put(a);
                }
#endif /* ID_EXT */
            }
        }
        /* end of processing for one line */
        if (!internal)
            addnewline(buf, outc, outp, NULL, pos, -1, nlines);
        if (internal == HTML_N_INTERNAL)
            internal = 0;
        if (str != endp)
        {
            line = line->Substr(str - line->ptr, endp - str);
            goto proc_again;
        }
    }
#ifdef DEBUG
    if (w3m_debug)
        fclose(debug);
#endif
    for (form_id = 1; form_id <= form_max; form_id++)
        forms[form_id]->next = forms[form_id - 1];
    buf->formlist = (form_max >= 0) ? forms[form_max] : NULL;
    if (n_textarea)
        addMultirowsForm(buf, buf->formitem);
#ifdef USE_IMAGE
    addMultirowsImg(buf, buf->img);
#endif
}

static void
addLink(BufferPtr buf, struct parsed_tag *tag)
{
    char *href = NULL, *title = NULL, *ctype = NULL, *rel = NULL, *rev = NULL;
    char type = LINK_TYPE_NONE;
    LinkList *l;

    parsedtag_get_value(tag, ATTR_HREF, &href);
    if (href)
        href = url_quote_conv(remove_space(href), buf->document_charset);
    parsedtag_get_value(tag, ATTR_TITLE, &title);
    parsedtag_get_value(tag, ATTR_TYPE, &ctype);
    parsedtag_get_value(tag, ATTR_REL, &rel);
    if (rel != NULL)
    {
        /* forward link type */
        type = LINK_TYPE_REL;
        if (title == NULL)
            title = rel;
    }
    parsedtag_get_value(tag, ATTR_REV, &rev);
    if (rev != NULL)
    {
        /* reverse link type */
        type = LINK_TYPE_REV;
        if (title == NULL)
            title = rev;
    }

    l = New(LinkList);
    l->url = href;
    l->title = title;
    l->ctype = ctype;
    l->type = type;
    l->next = NULL;
    if (buf->linklist)
    {
        LinkList *i;
        for (i = buf->linklist; i->next; i = i->next)
            ;
        i->next = l;
    }
    else
        buf->linklist = l;
}

void HTMLlineproc2(BufferPtr buf, TextLineList *tl)
{
    _tl_lp2 = tl->first;
    HTMLlineproc2body(buf, textlist_feed, -1);
}

static InputStream *_file_lp2;

static Str
file_feed()
{
    Str s;
    s = StrISgets(_file_lp2);
    if (s->Size() == 0)
    {
        ISclose(_file_lp2);
        return NULL;
    }
    return s;
}

void HTMLlineproc3(BufferPtr buf, InputStream *stream)
{
    _file_lp2 = stream;
    HTMLlineproc2body(buf, file_feed, -1);
}

extern char *NullLine;
extern Lineprop NullProp[];

#ifndef USE_ANSI_COLOR
#define addnewline2(a, b, c, d, e, f) _addnewline2(a, b, c, e, f)
#endif
static void
addnewline2(BufferPtr buf, char *line, Lineprop *prop, Linecolor *color, int pos,
            int nlines)
{
    Line *l;
    l = New(Line);
    l->next = NULL;
    l->lineBuf = line;
    l->propBuf = prop;
#ifdef USE_ANSI_COLOR
    l->colorBuf = color;
#endif
    l->len = pos;
    l->width = -1;
    l->size = pos;
    l->bpos = 0;
    l->bwidth = 0;
    l->prev = buf->currentLine;
    if (buf->currentLine)
    {
        l->next = buf->currentLine->next;
        buf->currentLine->next = l;
    }
    else
        l->next = NULL;
    if (buf->lastLine == NULL || buf->lastLine == buf->currentLine)
        buf->lastLine = l;
    buf->currentLine = l;
    if (buf->firstLine == NULL)
        buf->firstLine = l;
    l->linenumber = ++buf->allLine;
    if (nlines < 0)
    {
        /*     l->real_linenumber = l->linenumber;     */
        l->real_linenumber = 0;
    }
    else
    {
        l->real_linenumber = nlines;
    }
    l = NULL;
}

static void
addnewline(BufferPtr buf, char *line, Lineprop *prop, Linecolor *color, int pos,
           int width, int nlines)
{
    char *s;
    Lineprop *p;
#ifdef USE_ANSI_COLOR
    Linecolor *c;
#endif
    Line *l;
    int i, bpos, bwidth;

    if (pos > 0)
    {
        s = allocStr(line, pos);
        p = NewAtom_N(Lineprop, pos);
        bcopy((void *)prop, (void *)p, pos * sizeof(Lineprop));
    }
    else
    {
        s = NullLine;
        p = NullProp;
    }
#ifdef USE_ANSI_COLOR
    if (pos > 0 && color)
    {
        c = NewAtom_N(Linecolor, pos);
        bcopy((void *)color, (void *)c, pos * sizeof(Linecolor));
    }
    else
    {
        c = NULL;
    }
#endif
    addnewline2(buf, s, p, c, pos, nlines);
    if (pos <= 0 || width <= 0)
        return;
    bpos = 0;
    bwidth = 0;
    while (1)
    {
        l = buf->currentLine;
        l->bpos = bpos;
        l->bwidth = bwidth;
        i = columnLen(l, width);
        if (i == 0)
        {
            i++;
#ifdef USE_M17N
            while (i < l->len && p[i] & PC_WCHAR2)
                i++;
#endif
        }
        l->len = i;
        l->width = l->COLPOS(l->len);
        if (pos <= i)
            return;
        bpos += l->len;
        bwidth += l->width;
        s += i;
        p += i;
#ifdef USE_ANSI_COLOR
        if (c)
            c += i;
#endif
        pos -= i;
        addnewline2(buf, s, p, c, pos, nlines);
    }
}

/* 
 * loadHTMLBuffer: read file and make new buffer
 */
BufferPtr
loadHTMLBuffer(URLFile *f, BufferPtr newBuf)
{
    FILE *src = NULL;
    Str tmp;

    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);
    if (newBuf->sourcefile == NULL &&
        (f->scheme != SCM_LOCAL || newBuf->mailcap))
    {
        tmp = tmpfname(TMPF_SRC, ".html");
        src = fopen(tmp->ptr, "w");
        if (src)
            newBuf->sourcefile = tmp->ptr;
    }

    loadHTMLstream(f, newBuf, src, newBuf->bufferprop & BP_FRAME);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    if (src)
        fclose(src);

    return newBuf;
}

static char *_size_unit[] = {"b", "kb", "Mb", "Gb", "Tb",
                             "Pb", "Eb", "Zb", "Bb", "Yb", NULL};

char *
convert_size(clen_t size, int usefloat)
{
    float csize;
    int sizepos = 0;
    char **sizes = _size_unit;

    csize = (float)size;
    while (csize >= 999.495 && sizes[sizepos + 1])
    {
        csize = csize / 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? (char *)"%.3g%s" : (char *)"%.0f%s",
                   floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
        ->ptr;
}

char *
convert_size2(clen_t size1, clen_t size2, int usefloat)
{
    char **sizes = _size_unit;
    float csize, factor = 1;
    int sizepos = 0;

    csize = (float)((size1 > size2) ? size1 : size2);
    while (csize / factor >= 999.495 && sizes[sizepos + 1])
    {
        factor *= 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? (char *)"%.3g/%.3g%s" : (char *)"%.0f/%.0f%s",
                   floor(size1 / factor * 100.0 + 0.5) / 100.0,
                   floor(size2 / factor * 100.0 + 0.5) / 100.0,
                   sizes[sizepos])
        ->ptr;
}

void showProgress(clen_t *linelen, clen_t *trbyte)
{
    int i, j, rate, duration, eta, pos;
    static time_t last_time, start_time;
    time_t cur_time;
    Str messages;
    char *fmtrbyte, *fmrate;

    if (!fmInitialized)
        return;

    if (*linelen < 1024)
        return;
    if (current_content_length > 0)
    {
        double ratio;
        cur_time = time(0);
        if (*trbyte == 0)
        {
            move((LINES - 1), 0);
            clrtoeolx();
            start_time = cur_time;
        }
        *trbyte += *linelen;
        *linelen = 0;
        if (cur_time == last_time)
            return;
        last_time = cur_time;
        move((LINES - 1), 0);
        ratio = 100.0 * (*trbyte) / current_content_length;
        fmtrbyte = convert_size2(*trbyte, current_content_length, 1);
        duration = cur_time - start_time;
        if (duration)
        {
            rate = *trbyte / duration;
            fmrate = convert_size(rate, 1);
            eta = rate ? (current_content_length - *trbyte) / rate : -1;
            messages = Sprintf("%11s %3.0f%% "
                               "%7s/s "
                               "eta %02d:%02d:%02d     ",
                               fmtrbyte, ratio,
                               fmrate,
                               eta / (60 * 60), (eta / 60) % 60, eta % 60);
        }
        else
        {
            messages = Sprintf("%11s %3.0f%%                          ",
                               fmtrbyte, ratio);
        }
        addstr(messages->ptr);
        pos = 42;
        i = pos + (COLS - pos - 1) * (*trbyte) / current_content_length;
        move((LINES - 1), pos);
        standout();
        addch(' ');
        for (j = pos + 1; j <= i; j++)
            addch('|');
        standend();
        /* no_clrtoeol(); */
        refresh();
    }
    else
    {
        cur_time = time(0);
        if (*trbyte == 0)
        {
            move((LINES - 1), 0);
            clrtoeolx();
            start_time = cur_time;
        }
        *trbyte += *linelen;
        *linelen = 0;
        if (cur_time == last_time)
            return;
        last_time = cur_time;
        move((LINES - 1), 0);
        fmtrbyte = convert_size(*trbyte, 1);
        duration = cur_time - start_time;
        if (duration)
        {
            fmrate = convert_size(*trbyte / duration, 1);
            messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
        }
        else
        {
            messages = Sprintf("%7s loaded", fmtrbyte);
        }
        message(messages->ptr, 0, 0);
        refresh();
    }
}

/* 
 * loadHTMLString: read string and make new buffer
 */
BufferPtr
loadHTMLString(Str page)
{
    MySignalHandler prevtrap = NULL;
    BufferPtr newBuf;

    newBuf = newBuffer(INIT_BUFFER_WIDTH);
    if (SETJMP(AbortLoading) != 0)
    {
        TRAP_OFF;
        return NULL;
    }
    TRAP_ON;

    URLFile f(SCM_LOCAL, newStrStream(page));

#ifdef USE_M17N
    newBuf->document_charset = InnerCharset;
#endif
    loadHTMLstream(&f, newBuf, NULL, TRUE);
#ifdef USE_M17N
    newBuf->document_charset = WC_CES_US_ASCII;
#endif

    TRAP_OFF;
    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->type = "text/html";
    newBuf->real_type = newBuf->type;
    if (n_textarea)
        formResetBuffer(newBuf, newBuf->formitem);
    return newBuf;
}

#ifdef USE_GOPHER

/* 
 * loadGopherDir: get gopher directory
 */
Str loadGopherDir(URLFile *uf, ParsedURL *pu, wc_ces *charset)
{
    Str tmp;
    Str lbuf, name, file, host, port;
    char *p, *q;
    MySignalHandler prevtrap = NULL;
#ifdef USE_M17N
    wc_ces doc_charset = DocumentCharset;
#endif

    tmp = parsedURL2Str(pu);
    p = html_quote(tmp->ptr);
    tmp =
        convertLine(NULL, Strnew(file_unquote(tmp->ptr)), RAW_MODE,
                    charset, doc_charset);
    q = html_quote(tmp->ptr);
    tmp = Strnew_m_charp("<html>\n<head>\n<base href=\"", p, "\">\n<title>", q,
                         "</title>\n</head>\n<body>\n<h1>Index of ", q,
                         "</h1>\n<table>\n", NULL);

    if (SETJMP(AbortLoading) != 0)
        goto gopher_end;
    TRAP_ON;

    while (1)
    {
        if (lbuf = StrUFgets(uf), lbuf->length == 0)
            break;
        if (lbuf->ptr[0] == '.' &&
            (lbuf->ptr[1] == '\n' || lbuf->ptr[1] == '\r'))
            break;
        lbuf = convertLine(uf, lbuf, HTML_MODE, charset, doc_charset);
        p = lbuf->ptr;
        for (q = p; *q && *q != '\t'; q++)
            ;
        name = Strnew_charp_n(p, q - p);
        if (!*q)
            continue;
        p = q + 1;
        for (q = p; *q && *q != '\t'; q++)
            ;
        file = Strnew_charp_n(p, q - p);
        if (!*q)
            continue;
        p = q + 1;
        for (q = p; *q && *q != '\t'; q++)
            ;
        host = Strnew_charp_n(p, q - p);
        if (!*q)
            continue;
        p = q + 1;
        for (q = p; *q && *q != '\t' && *q != '\r' && *q != '\n'; q++)
            ;
        port = Strnew_charp_n(p, q - p);

        switch (name->ptr[0])
        {
        case '0':
            p = "[text file]";
            break;
        case '1':
            p = "[directory]";
            break;
        case 'm':
            p = "[message]";
            break;
        case 's':
            p = "[sound]";
            break;
        case 'g':
            p = "[gif]";
            break;
        case 'h':
            p = "[HTML]";
            break;
        default:
            p = "[unsupported]";
            break;
        }
        q = Strnew_m_charp("gopher://", host->ptr, ":", port->ptr,
                           "/", file->ptr, NULL)
                ->ptr;
        Strcat_m_charp(tmp, "<a href=\"",
                       html_quote(url_quote_conv(q, *charset)),
                       "\">", p, html_quote(name->ptr + 1), "</a>\n", NULL);
    }

gopher_end:
    TRAP_OFF;

    tmp->Push("</table>\n</body>\n</html>\n");
    return tmp;
}
#endif /* USE_GOPHER */

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
#ifdef USE_M17N
    if (newBuf->document_charset)
        charset = doc_charset = newBuf->document_charset;
    if (content_charset && UseContentCharset)
        doc_charset = content_charset;
#endif

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

#ifdef USE_IMAGE
BufferPtr
loadImageBuffer(URLFile *uf, BufferPtr newBuf)
{
    Image image;
    ImageCache *cache;
    Str tmp, tmpf;
    FILE *src = NULL;
    MySignalHandler prevtrap = NULL;
    struct stat st;

    loadImage(newBuf, IMG_FLAG_STOP);
    image.url = uf->url;
    image.ext = uf->ext;
    image.width = -1;
    image.height = -1;
    image.cache = NULL;
    cache = getImage(&image, GetCurBaseUrl(), IMG_FLAG_AUTO);
    if (!GetCurBaseUrl()->is_nocache && cache->loaded & IMG_FLAG_LOADED &&
        !stat(cache->file, &st))
        goto image_buffer;

    TRAP_ON;
    if (IStype(uf->stream) != IST_ENCODED)
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
    if (save2tmp(*uf, cache->file) < 0)
    {
        UFclose(uf);
        TRAP_OFF;
        return NULL;
    }
    UFclose(uf);
    TRAP_OFF;

    cache->loaded = IMG_FLAG_LOADED;
    cache->index = 0;

image_buffer:
    if (newBuf == NULL)
        newBuf = newBuffer(INIT_BUFFER_WIDTH);
    cache->loaded |= IMG_FLAG_DONT_REMOVE;
    if (newBuf->sourcefile == NULL && uf->scheme != SCM_LOCAL)
        newBuf->sourcefile = cache->file;

    tmp = Sprintf("<img src=\"%s\"><br><br>", html_quote(image.url));
    tmpf = tmpfname(TMPF_SRC, ".html");
    src = fopen(tmpf->ptr, "w");
    newBuf->mailcap_source = tmpf->ptr;

    URLFile f(SCM_LOCAL, newStrStream(tmp));
    loadHTMLstream(&f, newBuf, src, TRUE);
    if (src)
        fclose(src);

    newBuf->topLine = newBuf->firstLine;
    newBuf->lastLine = newBuf->currentLine;
    newBuf->currentLine = newBuf->firstLine;
    newBuf->image_flag = IMG_FLAG_AUTO;
    return newBuf;
}
#endif

static Str
conv_symbol(Line *l)
{
    Str tmp = NULL;
    char *p = l->lineBuf, *ep = p + l->len;
    Lineprop *pr = l->propBuf;
#ifdef USE_M17N
    int w;
    const char **symbol = NULL;
#else
    char **symbol = get_symbol();
#endif

    for (; p < ep; p++, pr++)
    {
        if (*pr & PC_SYMBOL)
        {
#ifdef USE_M17N
            char c = ((char)wtf_get_code((uint8_t *)p) & 0x7f) - SYMBOL_BASE;
            int len = get_mclen(p);
#else
            char c = *p - SYMBOL_BASE;
#endif
            if (tmp == NULL)
            {
                tmp = Strnew_size(l->len);
                tmp->CopyFrom(l->lineBuf, p - l->lineBuf);
#ifdef USE_M17N
                w = (*pr & PC_KANJI) ? 2 : 1;
                symbol = get_symbol(DisplayCharset, &w);
#endif
            }
            tmp->Push(symbol[(int)c]);
#ifdef USE_M17N
            p += len - 1;
            pr += len - 1;
#endif
        }
        else if (tmp != NULL)
            tmp->Push(*p);
    }
    if (tmp)
        return tmp;
    else
        return Strnew_charp_n(l->lineBuf, l->len);
}

/* 
 * saveBuffer: write buffer to file
 */
static void
_saveBuffer(BufferPtr buf, Line *l, FILE *f, int cont)
{
    Str tmp;
    int is_html = FALSE;
#ifdef USE_M17N
    int set_charset = !DisplayCharset;
    wc_ces charset = DisplayCharset ? DisplayCharset : WC_CES_US_ASCII;
#endif

    is_html = is_html_type(buf->type);

pager_next:
    for (; l != NULL; l = l->next)
    {
        if (is_html)
            tmp = conv_symbol(l);
        else
            tmp = Strnew_charp_n(l->lineBuf, l->len);
        tmp = wc_Str_conv(tmp, InnerCharset, charset);
        tmp->Puts(f);
        if (tmp->Back() != '\n' && !(cont && l->next && l->next->bpos))
            putc('\n', f);
    }
    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE))
    {
        l = getNextPage(buf, PagerMax);
#ifdef USE_M17N
        if (set_charset)
            charset = buf->document_charset;
#endif
        goto pager_next;
    }
}

void saveBuffer(BufferPtr buf, FILE *f, int cont)
{
    _saveBuffer(buf, buf->firstLine, f, cont);
}

void saveBufferBody(BufferPtr buf, FILE *f, int cont)
{
    Line *l = buf->firstLine;

    while (l != NULL && l->real_linenumber == 0)
        l = l->next;
    _saveBuffer(buf, l, f, cont);
}

static BufferPtr
loadcmdout(char *cmd,
           BufferPtr (*loadproc)(URLFile *, BufferPtr), BufferPtr defaultbuf)
{
    FILE *f, *popen(const char *, const char *);
    BufferPtr buf;

    if (cmd == NULL || *cmd == '\0')
        return NULL;
    f = popen(cmd, "r");
    if (f == NULL)
        return NULL;

    URLFile uf(SCM_UNKNOWN, newFileStream(f, (FileStreamCloseFunc)pclose));
    buf = loadproc(&uf, defaultbuf);
    UFclose(&uf);
    return buf;
}

/* 
 * getshell: execute shell command and get the result into a buffer
 */
BufferPtr
getshell(char *cmd)
{
    BufferPtr buf;

    buf = loadcmdout(cmd, loadBuffer, NULL);
    if (buf == NULL)
        return NULL;
    buf->filename = cmd;
    buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME,
                              conv_from_system(cmd))
                          ->ptr;
    return buf;
}

/* 
 * getpipe: execute shell command and connect pipe to the buffer
 */
BufferPtr
getpipe(char *cmd)
{
    FILE *f, *popen(const char *, const char *);
    BufferPtr buf;

    if (cmd == NULL || *cmd == '\0')
        return NULL;
    f = popen(cmd, "r");
    if (f == NULL)
        return NULL;
    buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->pagerSource = newFileStream(f, (FileStreamCloseFunc)pclose);
    buf->filename = cmd;
    buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME,
                              conv_from_system(cmd))
                          ->ptr;
    buf->bufferprop |= BP_PIPE;
#ifdef USE_M17N
    buf->document_charset = WC_CES_US_ASCII;
#endif
    return buf;
}

/* 
 * Open pager buffer
 */
BufferPtr
openPagerBuffer(InputStream *stream, BufferPtr buf)
{

    if (buf == NULL)
        buf = newBuffer(INIT_BUFFER_WIDTH);
    buf->pagerSource = stream;
    buf->buffername = getenv("MAN_PN");
    if (buf->buffername.empty())
        buf->buffername = PIPEBUFFERNAME;
    else
        buf->buffername = conv_from_system(buf->buffername.c_str());
    buf->bufferprop |= BP_PIPE;
#ifdef USE_M17N
    if (content_charset && UseContentCharset)
        buf->document_charset = content_charset;
    else
        buf->document_charset = WC_CES_US_ASCII;
#endif
    buf->currentLine = buf->firstLine;

    return buf;
}

BufferPtr
openGeneralPagerBuffer(InputStream *stream)
{
    BufferPtr buf;
    const char *t = "text/plain";
    BufferPtr t_buf = NULL;
    URLFile uf(SCM_UNKNOWN, stream);

    content_charset = 0;
    if (SearchHeader)
    {
        t_buf = newBuffer(INIT_BUFFER_WIDTH);
        readHeader(&uf, t_buf, TRUE, NULL);
        t = checkContentType(t_buf);
        if (t == NULL)
            t = "text/plain";
        if (t_buf)
        {
            t_buf->topLine = t_buf->firstLine;
            t_buf->currentLine = t_buf->lastLine;
        }
        SearchHeader = FALSE;
    }
    else if (DefaultType)
    {
        t = DefaultType;
        DefaultType = NULL;
    }
    if (is_html_type(t))
    {
        buf = loadHTMLBuffer(&uf, t_buf);
        buf->type = "text/html";
    }
    else if (is_plain_text_type(t))
    {
        if (IStype(stream) != IST_ENCODED)
            stream = newEncodedStream(stream, uf.encoding);
        buf = openPagerBuffer(stream, t_buf);
        buf->type = "text/plain";
    }
#ifdef USE_IMAGE
    else if (activeImage && displayImage && !useExtImageViewer &&
             !(w3m_dump & ~DUMP_FRAME) && !strncasecmp(t, "image/", 6))
    {
        GetCurBaseUrl()->Parse("-", NULL);
        buf = loadImageBuffer(&uf, t_buf);
        buf->type = "text/html";
    }
#endif
    else
    {
        if (doExternal(uf, "-", t, &buf, t_buf))
        {
            UFclose(&uf);
            if (buf == NULL)
                return buf;
        }
        else
        { /* unknown type is regarded as text/plain */
            if (IStype(stream) != IST_ENCODED)
                stream = newEncodedStream(stream, uf.encoding);
            buf = openPagerBuffer(stream, t_buf);
            buf->type = "text/plain";
        }
    }
    buf->real_type = t;
    buf->currentURL.scheme = SCM_LOCAL;
    buf->currentURL.file = "-";
    return buf;
}

Line *
getNextPage(BufferPtr buf, int plen)
{
    Line *top = buf->topLine, *last = buf->lastLine,
         *cur = buf->currentLine;
    int i;
    int nlines = 0;
    clen_t linelen = 0, trbyte = buf->trbyte;
    Str lineBuf2;
    char pre_lbuf = '\0';

    wc_ces charset;
    wc_ces doc_charset = DocumentCharset;
    uint8_t old_auto_detect = WcOption.auto_detect;

    int squeeze_flag = FALSE;
    Lineprop *propBuffer = NULL;

    Linecolor *colorBuffer = NULL;

    MySignalHandler prevtrap = NULL;

    if (buf->pagerSource == NULL)
        return NULL;

    if (last != NULL)
    {
        nlines = last->real_linenumber;
        pre_lbuf = *(last->lineBuf);
        if (pre_lbuf == '\0')
            pre_lbuf = '\n';
        buf->currentLine = last;
    }

    charset = buf->document_charset;
    if (buf->document_charset != WC_CES_US_ASCII)
        doc_charset = buf->document_charset;
    else if (UseContentCharset)
    {
        content_charset = 0;
        checkContentType(buf);
        if (content_charset)
            doc_charset = content_charset;
    }
    WcOption.auto_detect = buf->auto_detect;

    URLFile uf(SCM_UNKNOWN, NULL);
    if (SETJMP(AbortLoading) != 0)
    {
        goto pager_end;
    }
    TRAP_ON;

    for (i = 0; i < plen; i++)
    {
        lineBuf2 = StrmyISgets(buf->pagerSource);
        if (lineBuf2->Size() == 0)
        {
            /* Assume that `cmd == buf->filename' */
            if (buf->filename)
                buf->buffername = Sprintf("%s %s",
                                          CPIPEBUFFERNAME,
                                          conv_from_system(buf->filename))
                                      ->ptr;
            else if (getenv("MAN_PN") == NULL)
                buf->buffername = CPIPEBUFFERNAME;
            buf->bufferprop |= BP_CLOSE;
            break;
        }
        linelen += lineBuf2->Size();
        showProgress(&linelen, &trbyte);
        lineBuf2 =
            convertLine(&uf, lineBuf2, PAGER_MODE, &charset, doc_charset);
        if (squeezeBlankLine)
        {
            squeeze_flag = FALSE;
            if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n')
            {
                ++nlines;
                --i;
                squeeze_flag = TRUE;
                continue;
            }
            pre_lbuf = lineBuf2->ptr[0];
        }
        ++nlines;
        lineBuf2->StripRight();
        lineBuf2 = checkType(lineBuf2, &propBuffer, &colorBuffer);
        addnewline(buf, lineBuf2->ptr, propBuffer, colorBuffer,
                   lineBuf2->Size(), FOLD_BUFFER_WIDTH, nlines);
        if (!top)
        {
            top = buf->firstLine;
            cur = top;
        }
        if (buf->lastLine->real_linenumber - buf->firstLine->real_linenumber >= PagerMax)
        {
            Line *l = buf->firstLine;
            do
            {
                if (top == l)
                    top = l->next;
                if (cur == l)
                    cur = l->next;
                if (last == l)
                    last = NULL;
                l = l->next;
            } while (l && l->bpos);
            buf->firstLine = l;
            buf->firstLine->prev = NULL;
        }
    }
pager_end:
    TRAP_OFF;

    buf->trbyte = trbyte + linelen;
#ifdef USE_M17N
    buf->document_charset = charset;
    WcOption.auto_detect = old_auto_detect;
#endif
    buf->topLine = top;
    buf->currentLine = cur;
    if (!last)
        last = buf->firstLine;
    else if (last && (last->next || !squeeze_flag))
        last = last->next;
    return last;
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
        while (c = UFgetc(&uf), !iseos(uf.stream))
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
        while (UFread(&uf, buf, SAVE_BUF_SIZE))
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

int doExternal(URLFile uf, char *path, const char *type, BufferPtr *bufp,
               BufferPtr defaultbuf)
{
    Str tmpf, command;
    struct mailcap *mcap;
    int mc_stat;
    BufferPtr buf = NULL;
    char *header, *src = NULL;
    auto ext = uf.ext;

    if (!(mcap = searchExtViewer(type)))
        return 0;

    if (mcap->nametemplate)
    {
        tmpf = unquote_mailcap(mcap->nametemplate, NULL, "", NULL, NULL);
        if (tmpf->ptr[0] == '.')
            ext = tmpf->ptr;
    }
    tmpf = tmpfname(TMPF_DFL, (ext && *ext) ? ext : NULL);

    if (IStype(uf.stream) != IST_ENCODED)
        uf.stream = newEncodedStream(uf.stream, uf.encoding);
    header = checkHeader(defaultbuf, "Content-Type:");
    if (header)
        header = conv_to_system(header);
    command = unquote_mailcap(mcap->viewer, type, tmpf->ptr, header, &mc_stat);
#ifndef __EMX__
    if (!(mc_stat & MCSTAT_REPNAME))
    {
        Str tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf->ptr));
        command = tmp;
    }
#endif

#ifdef HAVE_SETPGRP
    if (!(mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) &&
        !(mcap->flags & MAILCAP_NEEDSTERMINAL) && BackgroundExtViewer)
    {
        flush_tty();
        if (!fork())
        {
            setup_child(FALSE, 0, UFfileno(&uf));
            if (save2tmp(uf, tmpf->ptr) < 0)
                exit(1);
            UFclose(&uf);
            myExec(command->ptr);
        }
        *bufp = nullptr;
        return 1;
    }
    else
#endif
    {
        if (save2tmp(uf, tmpf->ptr) < 0)
        {
            *bufp = NULL;
            return 1;
        }
    }
    if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT))
    {
        if (defaultbuf == NULL)
            defaultbuf = newBuffer(INIT_BUFFER_WIDTH);
        if (defaultbuf->sourcefile)
            src = defaultbuf->sourcefile;
        else
            src = tmpf->ptr;
        defaultbuf->sourcefile = NULL;
        defaultbuf->mailcap = mcap;
    }
    if (mcap->flags & MAILCAP_HTMLOUTPUT)
    {
        buf = loadcmdout(command->ptr, loadHTMLBuffer, defaultbuf);
        if (buf)
        {
            buf->type = "text/html";
            buf->mailcap_source = buf->sourcefile;
            buf->sourcefile = src;
        }
    }
    else if (mcap->flags & MAILCAP_COPIOUSOUTPUT)
    {
        buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
        if (buf)
        {
            buf->type = "text/plain";
            buf->mailcap_source = buf->sourcefile;
            buf->sourcefile = src;
        }
    }
    else
    {
        if (mcap->flags & MAILCAP_NEEDSTERMINAL || !BackgroundExtViewer)
        {
            fmTerm();
            mySystem(command->ptr, 0);
            fmInit();
            if (GetCurrentTab() && GetCurrentTab()->GetCurrentBuffer())
                displayCurrentbuf(B_FORCE_REDRAW);
        }
        else
        {
            mySystem(command->ptr, 1);
        }
        buf = nullptr;
    }
    if (buf)
    {
        buf->filename = path;
        if (buf->buffername.empty() || buf->buffername[0] == '\0')
            buf->buffername = conv_from_system(lastFileName(path));
        buf->edit = mcap->edit;
        buf->mailcap = mcap;
    }
    *bufp = buf;
    return 1;
}

static int
_MoveFile(char *path1, char *path2)
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

int _doFileCopy(char *tmpf, char *defstr, int download)
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

int doFileMove(char *tmpf, char *defstr)
{
    int ret = doFileCopy(tmpf, defstr);
    unlink(tmpf);
    return ret;
}

int doFileSave(URLFile uf, char *defstr)
{
#ifndef __MINGW32_VERSION
    Str msg;
    Str filen;
    char *p, *q;
    pid_t pid;
    char *lock;
    char *tmpf = NULL;
#if !(defined(HAVE_SYMLINK) && defined(HAVE_LSTAT))
    FILE *f;
#endif

    if (fmInitialized)
    {
        p = searchKeyData();
        if (p == NULL || *p == '\0')
        {
            /* FIXME: gettextize? */
            p = inputLineHist("(Download)Save file to: ",
                              defstr, IN_FILENAME, SaveHist);
            if (p == NULL || *p == '\0')
                return -1;
            p = conv_to_system(p);
        }
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(uf.stream, p) < 0)
        {
            /* FIXME: gettextize? */
            msg = Sprintf("Can't save. Load file and %s are identical.",
                          conv_from_system(p));
            disp_err_message(msg->ptr, FALSE);
            return -1;
        }
        /*
         * if (save2tmp(uf, p) < 0) {
         * msg = Sprintf("Can't save to %s", conv_from_system(p));
         * disp_err_message(msg->ptr, FALSE);
         * }
         */
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
            int err;
            if ((uf.content_encoding != CMP_NOCOMPRESS) && AutoUncompress)
            {
                uncompress_stream(&uf, &tmpf);
                if (tmpf)
                    unlink(tmpf);
            }
            setup_child(FALSE, 0, UFfileno(&uf));
            err = save2tmp(uf, p);
            if (err == 0 && PreserveTimestamp && uf.modtime != -1)
                setModtime(p, uf.modtime);
            UFclose(&uf);
            unlink(lock);
            if (err != 0)
                exit(-err);
            exit(0);
        }
        addDownloadList(pid, uf.url, p, lock, current_content_length);
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
        p = expandPath(q);
        if (checkOverWrite(p) < 0)
            return -1;
        if (checkSaveFile(uf.stream, p) < 0)
        {
            /* FIXME: gettextize? */
            printf("Can't save. Load file and %s are identical.", p);
            return -1;
        }
        if (uf.content_encoding != CMP_NOCOMPRESS && AutoUncompress)
        {
            uncompress_stream(&uf, &tmpf);
            if (tmpf)
                unlink(tmpf);
        }
        if (save2tmp(uf, p) < 0)
        {
            /* FIXME: gettextize? */
            printf("Can't save to %s\n", p);
            return -1;
        }
        if (PreserveTimestamp && uf.modtime != -1)
            setModtime(p, uf.modtime);
    }
#endif /* __MINGW32_VERSION */
    return 0;
}

int checkCopyFile(char *path1, char *path2)
{
    struct stat st1, st2;

    if (*path2 == '|' && PermitSaveToPipe)
        return 0;
    if ((stat(path1, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkSaveFile(InputStream *stream, char *path2)
{
    struct stat st1, st2;
    int des = ISfileno(stream);

    if (des < 0)
        return 0;
    if (*path2 == '|' && PermitSaveToPipe)
        return 0;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkOverWrite(char *path)
{
    struct stat st;
    char *ans;

    if (stat(path, &st) < 0)
        return 0;
    /* FIXME: gettextize? */
    ans = inputAnswer("File exists. Overwrite? (y/n)");
    if (ans && TOLOWER(*ans) == 'y')
        return 0;
    else
        return -1;
}

char *
inputAnswer(const char *prompt)
{
    char *ans;

    if (QuietMessage)
        return "n";
    if (fmInitialized)
    {
        term_raw();
        ans = inputChar(prompt);
    }
    else
    {
        printf("%s", prompt);
        fflush(stdout);
        ans = Strfgets(stdin)->ptr;
    }
    return ans;
}

static void
uncompress_stream(URLFile *uf, char **src)
{
#ifndef __MINGW32_VERSION
    pid_t pid1;
    FILE *f1;
    const char *expand_cmd = GUNZIP_CMDNAME;
    const char *expand_name = GUNZIP_NAME;
    char *tmpf = NULL;
    const char *ext = NULL;
    struct compression_decoder *d;

    if (IStype(uf->stream) != IST_ENCODED)
    {
        uf->stream = newEncodedStream(uf->stream, uf->encoding);
        uf->encoding = ENC_7BIT;
    }
    for (d = compression_decoders; d->type != CMP_NOCOMPRESS; d++)
    {
        if (uf->compression == d->type)
        {
            if (d->auxbin_p)
                expand_cmd = auxbinFile(d->cmd);
            else
                expand_cmd = d->cmd;
            expand_name = d->name;
            ext = d->ext;
            break;
        }
    }
    uf->compression = CMP_NOCOMPRESS;

    if (uf->scheme != SCM_LOCAL
#ifdef USE_IMAGE
        && !image_source
#endif
    )
    {
        tmpf = tmpfname(TMPF_DFL, ext)->ptr;
    }

    /* child1 -- stdout|f1=uf -> parent */
    pid1 = open_pipe_rw(&f1, NULL);
    if (pid1 < 0)
    {
        UFclose(uf);
        return;
    }
    if (pid1 == 0)
    {
        /* child */
        pid_t pid2;
        FILE *f2 = stdin;

        /* uf -> child2 -- stdout|stdin -> child1 */
        pid2 = open_pipe_rw(&f2, NULL);
        if (pid2 < 0)
        {
            UFclose(uf);
            exit(1);
        }
        if (pid2 == 0)
        {
            /* child2 */
            Str buf = Strnew_size(SAVE_BUF_SIZE);
            FILE *f = NULL;

            setup_child(TRUE, 2, UFfileno(uf));
            if (tmpf)
                f = fopen(tmpf, "wb");
            while (UFread(uf, buf, SAVE_BUF_SIZE))
            {
                if (buf->Puts(stdout) < 0)
                    break;
                if (f)
                    buf->Puts(f);
            }
            UFclose(uf);
            if (f)
                fclose(f);
            exit(0);
        }
        /* child1 */
        dup2(1, 2); /* stderr>&stdout */
        setup_child(TRUE, -1, -1);
        execlp(expand_cmd, expand_name, NULL);
        exit(1);
    }
    if (tmpf)
    {
        if (src)
            *src = tmpf;
        else
            uf->scheme = SCM_LOCAL;
    }
    UFhalfclose(uf);
    uf->stream = newFileStream(f1, (FileStreamCloseFunc)fclose);
#endif /* __MINGW32_VERSION */
}

static FILE *
lessopen_stream(char *path)
{
    char *lessopen;
    FILE *fp;

    lessopen = getenv("LESSOPEN");
    if (lessopen == NULL)
    {
        return NULL;
    }
    if (lessopen[0] == '\0')
    {
        return NULL;
    }

    if (lessopen[0] == '|')
    {
        /* pipe mode */
        Str tmpf;
        int c;

        ++lessopen;
        tmpf = Sprintf(lessopen, shell_quote(path));
        fp = popen(tmpf->ptr, "r");
        if (fp == NULL)
        {
            return NULL;
        }
        c = getc(fp);
        if (c == EOF)
        {
            fclose(fp);
            return NULL;
        }
        ungetc(c, fp);
    }
    else
    {
        /* filename mode */
        /* not supported m(__)m */
        fp = NULL;
    }
    return fp;
}

#if 0
void
reloadBuffer(BufferPtr buf)
{
    URLFile uf;

    if (buf->sourcefile == NULL || buf->pagerSource != NULL)
        return;
    init_stream(&uf, SCM_UNKNOWN, NULL);
    examineFile(buf->mailcap_source ? buf->mailcap_source : buf->sourcefile,
                &uf);
    if (uf.stream == NULL)
        return;
    is_redisplay = TRUE;
    buf->allLine = 0;
    buf->href = NULL;
    buf->name = NULL;
    buf->img = NULL;
    buf->formitem = NULL;
    buf->linklist = NULL;
    buf->maplist = NULL;
    if (buf->hmarklist)
        buf->hmarklist->nmark = 0;
    if (buf->imarklist)
        buf->imarklist->nmark = 0;
    if (is_html_type(buf->type))
        loadHTMLBuffer(&uf, buf);
    else
        loadBuffer(&uf, buf);
    UFclose(&uf);
    is_redisplay = FALSE;
}
#endif

static char *
guess_filename(char *file)
{
    char *p = NULL, *s;

    if (file != NULL)
        p = mybasename(file);
    if (p == NULL || *p == '\0')
        return DEF_SAVE_FILE;
    s = p;
    if (*p == '#')
        p++;
    while (*p != '\0')
    {
        if ((*p == '#' && *(p + 1) != '\0') || *p == '?')
        {
            *p = '\0';
            break;
        }
        p++;
    }
    return s;
}

char *
guess_save_name(BufferPtr buf, char *path)
{
    if (buf && buf->document_header)
    {
        Str name = NULL;
        char *p, *q;
        if ((p = checkHeader(buf, "Content-Disposition:")) != NULL &&
            (q = strcasestr(p, "filename")) != NULL &&
            (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
            matchattr(q, "filename", 8, &name))
            path = name->ptr;
        else if ((p = checkHeader(buf, "Content-Type:")) != NULL &&
                 (q = strcasestr(p, "name")) != NULL &&
                 (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
                 matchattr(q, "name", 4, &name))
            path = name->ptr;
    }
    return guess_filename(path);
}

/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */

static void
print_internal_information(struct html_feed_environ *henv)
{
    int i;
    Str s;
    TextLineList *tl = newTextLineList();

    s = Strnew("<internal>");
    pushTextLine(tl, newTextLine(s, 0));
    if (henv->title)
    {
        s = Strnew_m_charp("<title_alt title=\"",
                           html_quote(henv->title), "\">", NULL);
        pushTextLine(tl, newTextLine(s, 0));
    }
#if 0
    if (form_max >= 0) {
        FormList *fp;
        for (i = 0; i <= form_max; i++) {
            fp = forms[i];
            s = Sprintf("<form_int fid=\"%d\" action=\"%s\" method=\"%s\"",
                        i, html_quote(fp->action->ptr),
                        (fp->method == FORM_METHOD_POST) ? "post"
                        : ((fp->method ==
                            FORM_METHOD_INTERNAL) ? "internal" : "get"));
            if (fp->target)
                s->Push( Sprintf(" target=\"%s\"", html_quote(fp->target)));
            if (fp->enctype == FORM_ENCTYPE_MULTIPART)
                s->Push( " enctype=\"multipart/form-data\"");
#ifdef USE_M17N
            if (fp->charset)
                s->Push( Sprintf(" accept-charset=\"%s\"",
                                  html_quote(fp->charset)));
#endif
            s->Push( ">");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
#endif
#ifdef MENU_SELECT
    if (n_select > 0)
    {
        FormSelectOptionItem *ip;
        for (i = 0; i < n_select; i++)
        {
            s = Sprintf("<select_int selectnumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            for (ip = select_option[i].first; ip; ip = ip->next)
            {
                s = Sprintf("<option_int value=\"%s\" label=\"%s\"%s>",
                            html_quote(ip->value ? ip->value->ptr : ip->label->ptr),
                            html_quote(ip->label->ptr),
                            ip->checked ? " selected" : "");
                pushTextLine(tl, newTextLine(s, 0));
            }
            s = Strnew("</select_int>");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
#endif /* MENU_SELECT */
    if (n_textarea > 0)
    {
        for (i = 0; i < n_textarea; i++)
        {
            s = Sprintf("<textarea_int textareanumber=%d>", i);
            pushTextLine(tl, newTextLine(s, 0));
            s = Strnew(html_quote(textarea_str[i]->ptr));
            s->Push("</textarea_int>");
            pushTextLine(tl, newTextLine(s, 0));
        }
    }
    s = Strnew("</internal>");
    pushTextLine(tl, newTextLine(s, 0));

    if (henv->buf)
        appendTextLineList(henv->buf, tl);
    else if (henv->f)
    {
        TextLineListItem *p;
        for (p = tl->first; p; p = p->next)
            fprintf(henv->f, "%s\n", Str_conv_to_halfdump(p->ptr->line)->ptr);
    }
}

void loadHTMLstream(URLFile *f, BufferPtr newBuf, FILE *src, int internal)
{
    struct environment envs[MAX_ENV_LEVEL];
    clen_t linelen = 0;
    clen_t trbyte = 0;
    Str lineBuf2 = Strnew();
#ifdef USE_M17N
    wc_ces charset = WC_CES_US_ASCII;
    wc_ces doc_charset = DocumentCharset;
#endif
    struct html_feed_environ htmlenv1;
    struct readbuffer obuf;
#ifdef USE_IMAGE
    int image_flag;
#endif
    MySignalHandler prevtrap = NULL;

#ifdef USE_M17N
    if (fmInitialized && graph_ok())
    {
        symbol_width = symbol_width0 = 1;
    }
    else
    {
        symbol_width0 = 0;
        get_symbol(DisplayCharset, &symbol_width0);
        symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    }
#else
    symbol_width = symbol_width0 = 1;
#endif

    SetCurTitle(nullptr);
    n_textarea = 0;
    cur_textarea = NULL;
    max_textarea = MAX_TEXTAREA;
    textarea_str = New_N(Str, max_textarea);
#ifdef MENU_SELECT
    n_select = 0;
    max_select = MAX_SELECT;
    select_option = New_N(FormSelectOption, max_select);
#endif /* MENU_SELECT */
    cur_select = NULL;
    form_sp = -1;
    form_max = -1;
    forms_size = 0;
    forms = NULL;
    cur_hseq = 1;
#ifdef USE_IMAGE
    cur_iseq = 1;
    if (newBuf->image_flag)
        image_flag = newBuf->image_flag;
    else if (activeImage && displayImage && autoImage)
        image_flag = IMG_FLAG_AUTO;
    else
        image_flag = IMG_FLAG_SKIP;
    if (newBuf->currentURL.file)
    {
        copyParsedURL(GetCurBaseUrl(), baseURL(newBuf));
    }
#endif

    if (w3m_halfload)
    {
        newBuf->buffername = "---";
#ifdef USE_M17N
        newBuf->document_charset = InnerCharset;
#endif
        max_textarea = 0;
#ifdef MENU_SELECT
        max_select = 0;
#endif
        HTMLlineproc3(newBuf, f->stream);
        w3m_halfload = FALSE;
        return;
    }

    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, newBuf->width, 0);

    if (w3m_halfdump)
        htmlenv1.f = stdout;
    else
        htmlenv1.buf = newTextLineList();

    if (SETJMP(AbortLoading) != 0)
    {
        HTMLlineproc1("<br>Transfer Interrupted!<br>", &htmlenv1);
        goto phase2;
    }
    TRAP_ON;

#ifdef USE_M17N
    if (newBuf != NULL)
    {
        if (newBuf->bufferprop & BP_FRAME)
            charset = InnerCharset;
        else if (newBuf->document_charset)
            charset = doc_charset = newBuf->document_charset;
    }
    if (content_charset && UseContentCharset)
        doc_charset = content_charset;
    else if (f->guess_type && !strcasecmp(f->guess_type, "application/xhtml+xml"))
        doc_charset = WC_CES_UTF_8;
    meta_charset = 0;
#endif
#if 0
    do_blankline(&htmlenv1, &obuf, 0, 0, htmlenv1.limit);
    obuf.flag = RB_IGNORE_P;
#endif
    if (IStype(f->stream) != IST_ENCODED)
        f->stream = newEncodedStream(f->stream, f->encoding);
    while ((lineBuf2 = StrmyUFgets(f))->Size())
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
        if (w3m_dump & DUMP_EXTRA)
            printf("W3m-in-progress: %s\n", convert_size2(linelen, current_content_length, TRUE));
        if (w3m_dump & DUMP_SOURCE)
            continue;
        showProgress(&linelen, &trbyte);
        /*
         * if (frame_source)
         * continue;
         */
#ifdef USE_M17N
        if (meta_charset)
        { /* <META> */
            if (content_charset == 0 && UseContentCharset)
            {
                doc_charset = meta_charset;
                charset = WC_CES_US_ASCII;
            }
            meta_charset = 0;
        }
#endif
        lineBuf2 = convertLine(f, lineBuf2, HTML_MODE, &charset, doc_charset);
#if defined(USE_M17N) && defined(USE_IMAGE)
        cur_document_charset = charset;
#endif
        HTMLlineproc0(lineBuf2->ptr, &htmlenv1, internal);
    }
    if (obuf.status != R_ST_NORMAL)
    {
        obuf.status = R_ST_EOL;
        HTMLlineproc0("\n", &htmlenv1, internal);
    }
    obuf.status = R_ST_NORMAL;
    completeHTMLstream(&htmlenv1, &obuf);
    flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);
    if (htmlenv1.title)
        newBuf->buffername = htmlenv1.title;
    if (w3m_halfdump)
    {
        TRAP_OFF;
        print_internal_information(&htmlenv1);
        return;
    }
    if (w3m_backend)
    {
        TRAP_OFF;
        print_internal_information(&htmlenv1);
        backend_halfdump_buf = htmlenv1.buf;
        return;
    }
phase2:
    newBuf->trbyte = trbyte + linelen;
    TRAP_OFF;
#ifdef USE_M17N
    if (!(newBuf->bufferprop & BP_FRAME))
        newBuf->document_charset = charset;
#endif
#ifdef USE_IMAGE
    newBuf->image_flag = image_flag;
#endif
    HTMLlineproc2(newBuf, htmlenv1.buf);
}
