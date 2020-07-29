#include "fm.h"
#include "html/table.h"
#include "indep.h"
#include "etc.h"
#include "commands.h"
#include "html/frame.h"
#include "file.h"
#include "html/anchor.h"
#include "public.h"
#include "symbol.h"
#include "html/map.h"
#include "frontend/display.h"
#include "myctype.h"
#include "html/html.h"
#include "html/parsetagx.h"
#include "transport/local.h"
#include "regex.h"
#include "dispatcher.h"
#include "transport/url.h"
#include "entity.h"
#include "http/cookie.h"
#include "frontend/terms.h"
#include "html/image.h"
#include "ctrlcode.h"
#include "mimehead.h"
#include "mimetypes.h"
#include "html/tagstack.h"
#include "http/http_request.h"
#include "transport/urlfile.h"
#include "http/auth.h"
#include "http/compression.h"
#include "rc.h"
#include "transport/loader.h"
#include "html/html_processor.h"
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
    uf.Close();
    return buf;
}

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

static ParsedURL g_cur_baseURL = {};
ParsedURL *GetCurBaseUrl()
{
    return &g_cur_baseURL;
}

int is_dump_text_type(const char *type)
{
    struct mailcap *mcap;
    return (type && (mcap = searchExtViewer(type)) &&
            (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

int is_text_type(const char *type)
{
    return (type == NULL || type[0] == '\0' ||
            strncasecmp(type, "text/", 5) == 0 ||
            (strncasecmp(type, "application/", 12) == 0 &&
             strstr(type, "xhtml") != NULL) ||
            strncasecmp(type, "message/", sizeof("message/") - 1) == 0);
}

int is_plain_text_type(const char *type)
{
    return ((type && strcasecmp(type, "text/plain") == 0) ||
            (is_text_type(type) && !is_dump_text_type(type)));
}

int is_html_type(std::string_view type)
{
    return type == "text/html" || type == "application/xhtml+xml";
}

int setModtime(char *path, time_t modtime)
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
                uf->Close();
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

static int
same_url_p(ParsedURL *pu1, ParsedURL *pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port &&
            (pu1->host.size() ? pu2->host.size() ? pu1->host == pu2->host : 0 : 1) &&
            (pu1->file.size() ? pu2->file.size() ? pu1->file == pu2->file : 0 : 1));
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

void addnewline(BufferPtr buf, char *line, Lineprop *prop, Linecolor *color, int pos,
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

static const char *_size_unit[] = {"b", "kb", "Mb", "Gb", "Tb",
                                   "Pb", "Eb", "Zb", "Bb", "Yb", NULL};

char *
convert_size(clen_t size, int usefloat)
{
    float csize;
    int sizepos = 0;
    const char **sizes = _size_unit;

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
    const char **sizes = _size_unit;
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

    auto current_content_length = GetCurrentContentLength();
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

    tmp = pu->ToStr();
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
        uf->Close();
        TRAP_OFF;
        return NULL;
    }
    uf->Close();
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
            uf.Close();
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
            if (buf->filename.size())
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
            setup_child(FALSE, 0, uf.FileNo());
            if (save2tmp(uf, tmpf->ptr) < 0)
                exit(1);
            uf.Close();
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

int doFileMove(char *tmpf, char *defstr)
{
    int ret = doFileCopy(tmpf, defstr);
    unlink(tmpf);
    return ret;
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
    uf.Close();
    is_redisplay = FALSE;
}
#endif

char *guess_filename(std::string_view file)
{
    char *p = NULL, *s;

    if (file.size())
        p = mybasename(file.data());
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
guess_save_name(BufferPtr buf, std::string_view path)
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
