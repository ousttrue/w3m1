#include <stdio.h>
#include <errno.h>
#include "mailcap.h"
#include "fm.h"
#include "indep.h"
#include "gc_helper.h"
#include "myctype.h"
#include "file.h"
#include "mime/mimetypes.h"
#include "html/parsetag.h"
#include "html/html.h"
#include "transport/local.h"
#include "transport/loader.h"
#include "transport/url.h"
#include "frontend/tab.h"
#include "frontend/buffer.h"
#include "frontend/terms.h"
#include "frontend/display.h"
#include "frontend/tabbar.h"

static Mailcap DefaultMailcap[] = {
    {"image/*", DEF_IMAGE_VIEWER " %s", 0, NULL, NULL, NULL}, /* */
    {"audio/basic", DEF_AUDIO_PLAYER " %s", 0, NULL, NULL, NULL},
    {NULL, NULL, 0, NULL, NULL, NULL}};

static TextList *mailcap_list;
static Mailcap **UserMailcap;

int mailcapMatch(Mailcap *mcap, const char *type)
{
    auto cap = mcap->type;
    const char *p;
    for (p = cap; *p != '/'; p++)
    {
        if (TOLOWER(*p) != TOLOWER(*type))
            return 0;
        type++;
    }

    if (*type != '/')
        return 0;
    p++;
    type++;

    int level;
    if (mcap->flags & MAILCAP_HTMLOUTPUT)
        level = 1;
    else
        level = 0;
    if (*p == '*')
        return 10 + level;
    while (*p)
    {
        if (TOLOWER(*p) != TOLOWER(*type))
            return 0;
        p++;
        type++;
    }
    if (*type != '\0')
        return 0;
    return 20 + level;
}

Mailcap *
searchMailcap(Mailcap *table, const char *type)
{
    int level = 0;
    Mailcap *mcap = NULL;
    int i;

    if (table == NULL)
        return NULL;
    for (; table->type; table++)
    {
        i = mailcapMatch(table, type);
        if (i > level)
        {
            if (table->test)
            {
                Str command =
                    unquote_mailcap(table->test, type, NULL, NULL, NULL);
                if (system(command->ptr) != 0)
                    continue;
            }
            level = i;
            mcap = table;
        }
    }
    return mcap;
}

static int
matchMailcapAttr(char *p, char *attr, int len, Str *value)
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
                while (*p && (quoted || *p != ';'))
                {
                    if (quoted || !IS_SPACE(*p))
                        q = p;
                    if (quoted)
                        quoted = 0;
                    else if (*p == '\\')
                        quoted = 1;
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
            if (*p == '\0' || *p == ';')
            {
                return 1;
            }
        }
    }
    return 0;
}

static int
extractMailcapEntry(char *mcap_entry, Mailcap *mcap)
{
    int j, k;
    char *p;
    int quoted;
    Str tmp;

    bzero(mcap, sizeof(Mailcap));
    p = mcap_entry;
    SKIP_BLANKS(p);
    k = -1;
    for (j = 0; p[j] && p[j] != ';'; j++)
    {
        if (!IS_SPACE(p[j]))
            k = j;
    }
    mcap->type = allocStr(p, (k >= 0) ? k + 1 : j);
    if (!p[j])
        return 0;
    p += j + 1;

    SKIP_BLANKS(p);
    k = -1;
    quoted = 0;
    for (j = 0; p[j] && (quoted || p[j] != ';'); j++)
    {
        if (quoted || !IS_SPACE(p[j]))
            k = j;
        if (quoted)
            quoted = 0;
        else if (p[j] == '\\')
            quoted = 1;
    }
    mcap->viewer = allocStr(p, (k >= 0) ? k + 1 : j);
    p += j;

    while (*p == ';')
    {
        p++;
        SKIP_BLANKS(p);
        if (matchMailcapAttr(p, "needsterminal", 13, NULL))
        {
            mcap->flags |= MAILCAP_NEEDSTERMINAL;
        }
        else if (matchMailcapAttr(p, "copiousoutput", 13, NULL))
        {
            mcap->flags |= MAILCAP_COPIOUSOUTPUT;
        }
        else if (matchMailcapAttr(p, "x-htmloutput", 12, NULL) ||
                 matchMailcapAttr(p, "htmloutput", 10, NULL))
        {
            mcap->flags |= MAILCAP_HTMLOUTPUT;
        }
        else if (matchMailcapAttr(p, "test", 4, &tmp))
        {
            mcap->test = allocStr(tmp->ptr, tmp->Size());
        }
        else if (matchMailcapAttr(p, "nametemplate", 12, &tmp))
        {
            mcap->nametemplate = allocStr(tmp->ptr, tmp->Size());
        }
        else if (matchMailcapAttr(p, "edit", 4, &tmp))
        {
            mcap->edit = allocStr(tmp->ptr, tmp->Size());
        }
        quoted = 0;
        while (*p && (quoted || *p != ';'))
        {
            if (quoted)
                quoted = 0;
            else if (*p == '\\')
                quoted = 1;
            p++;
        }
    }
    return 1;
}

static Mailcap *
loadMailcap(char *filename)
{
    FILE *f;
    int i, n;
    Str tmp;
    Mailcap *mcap;

    f = fopen(expandPath(filename), "r");
    if (f == NULL)
        return NULL;
    i = 0;
    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        if (tmp->ptr[0] != '#')
            i++;
    }
    fseek(f, 0, 0);
    n = i;
    mcap = New_N(Mailcap, n + 1);
    i = 0;
    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        if (tmp->ptr[0] == '#')
            continue;
    redo:
        while (IS_SPACE(tmp->Back()))
            tmp->Pop(1);
        if (tmp->Back() == '\\')
        {
            /* continuation */
            tmp->Pop(1);
            tmp->Push(Strfgets(f));
            goto redo;
        }
        if (extractMailcapEntry(tmp->ptr, &mcap[i]))
            i++;
    }
    bzero(&mcap[i], sizeof(Mailcap));
    fclose(f);
    return mcap;
}

void initMailcap()
{
    TextListItem *tl;
    int i;

    if (non_null(mailcap_files))
        mailcap_list = make_domain_list(mailcap_files);
    else
        mailcap_list = NULL;
    if (mailcap_list == NULL)
        return;
    UserMailcap = New_N(Mailcap *, mailcap_list->nitem);
    for (i = 0, tl = mailcap_list->first; tl; i++, tl = tl->next)
        UserMailcap[i] = loadMailcap(tl->ptr);
}

char *
acceptableMimeTypes()
{
    static Str types = NULL;
    TextList *l;
    Hash_si *mhash;
    const char *p;
    int i;

    if (types != NULL)
        return types->ptr;

    /* generate acceptable media types */
    l = newTextList();
    mhash = newHash_si(16); /* XXX */
    /* pushText(l, "text"); */
    putHash_si(mhash, "text", 1);
    pushText(l, "image");
    putHash_si(mhash, "image", 1);
    for (i = 0; i < mailcap_list->nitem; i++)
    {
        Mailcap *mp = UserMailcap[i];
        char *mt;
        if (mp == NULL)
            continue;
        for (; mp->type; mp++)
        {
            p = strchr(mp->type, '/');
            if (p == NULL)
                continue;
            mt = allocStr(mp->type, p - mp->type);
            if (getHash_si(mhash, mt, 0) == 0)
            {
                pushText(l, mt);
                putHash_si(mhash, mt, 1);
            }
        }
    }
    types = Strnew();
    types->Push("text/html, text/*;q=0.5");
    while ((p = popText(l)) != NULL)
    {
        types->Push(", ");
        types->Push(p);
        types->Push("/*");
    }
    return types->ptr;
}

Mailcap *
searchExtViewer(const char *type)
{
    Mailcap *p;
    int i;

    if (mailcap_list == NULL)
        goto no_user_mailcap;

    for (i = 0; i < mailcap_list->nitem; i++)
    {
        if ((p = searchMailcap(UserMailcap[i], type)) != NULL)
            return p;
    }

no_user_mailcap:
    return searchMailcap(DefaultMailcap, type);
}

#define MC_NORMAL 0
#define MC_PREC 1
#define MC_PREC2 2
#define MC_QUOTED 3

#define MCF_SQUOTED (1 << 0)
#define MCF_DQUOTED (1 << 1)

Str quote_mailcap(const char *s, int flag)
{
    Str d;

    d = Strnew();

    for (;; ++s)
        switch (*s)
        {
        case '\0':
            goto end;
        case '$':
        case '`':
        case '"':
        case '\\':
            if (!(flag & MCF_SQUOTED))
                d->Push('\\');

            d->Push(*s);
            break;
        case '\'':
            if (flag & MCF_SQUOTED)
            {
                d->Push("'\\''");
                break;
            }
        default:
            if (!flag && !IS_ALNUM(*s))
                d->Push('\\');
        case '_':
        case '.':
        case ':':
        case '/':
            d->Push(*s);
            break;
        }
end:
    return d;
}

static Str
unquote_mailcap_loop(const char *qstr, const char *type, char *name, char *attr,
                     int *mc_stat, int flag0)
{
    Str str, tmp, test, then;
    const char *p;
    int status = MC_NORMAL, prev_status = MC_NORMAL, sp = 0, flag;

    if (mc_stat)
        *mc_stat = 0;

    if (qstr == NULL)
        return NULL;

    str = Strnew();
    tmp = test = then = NULL;

    for (flag = flag0, p = qstr; *p; p++)
    {
        if (status == MC_QUOTED)
        {
            if (prev_status == MC_PREC2)
                tmp->Push(*p);
            else
                str->Push(*p);
            status = prev_status;
            continue;
        }
        else if (*p == '\\')
        {
            prev_status = status;
            status = MC_QUOTED;
            continue;
        }
        switch (status)
        {
        case MC_NORMAL:
            if (*p == '%')
            {
                status = MC_PREC;
            }
            else
            {
                if (*p == '\'')
                {
                    if (!flag0 && flag & MCF_SQUOTED)
                        flag &= ~MCF_SQUOTED;
                    else if (!flag)
                        flag |= MCF_SQUOTED;
                }
                else if (*p == '"')
                {
                    if (!flag0 && flag & MCF_DQUOTED)
                        flag &= ~MCF_DQUOTED;
                    else if (!flag)
                        flag |= MCF_DQUOTED;
                }
                str->Push(*p);
            }
            break;
        case MC_PREC:
            if (IS_ALPHA(*p))
            {
                switch (*p)
                {
                case 's':
                    if (name)
                    {
                        str->Push(quote_mailcap(name, flag)->ptr);
                        if (mc_stat)
                            *mc_stat |= MCSTAT_REPNAME;
                    }
                    break;
                case 't':
                    if (type)
                    {
                        str->Push(quote_mailcap(type, flag)->ptr);
                        if (mc_stat)
                            *mc_stat |= MCSTAT_REPTYPE;
                    }
                    break;
                }
                status = MC_NORMAL;
            }
            else if (*p == '{')
            {
                status = MC_PREC2;
                test = then = NULL;
                tmp = Strnew();
            }
            else if (*p == '%')
            {
                str->Push(*p);
            }
            break;
        case MC_PREC2:
            if (sp > 0 || *p == '{')
            {
                tmp->Push(*p);

                switch (*p)
                {
                case '{':
                    ++sp;
                    break;
                case '}':
                    --sp;
                    break;
                default:
                    break;
                }
            }
            else if (*p == '}')
            {
                char *q;
                if (attr && (q = strcasestr(attr, tmp->ptr)) != NULL &&
                    (q == attr || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
                    matchattr(q, tmp->ptr, tmp->Size(), &tmp))
                {
                    str->Push(quote_mailcap(tmp->ptr, flag)->ptr);
                    if (mc_stat)
                        *mc_stat |= MCSTAT_REPPARAM;
                }
                status = MC_NORMAL;
            }
            else
            {
                tmp->Push(*p);
            }
            break;
        }
    }
    return str;
}

Str unquote_mailcap(const char *qstr, const char *type, char *name, char *attr, int *mc_stat)
{
    return unquote_mailcap_loop(qstr, type, name, attr, mc_stat, 0);
}

int is_dump_text_type(const char *type)
{
    Mailcap *mcap;
    return (type && (mcap = searchExtViewer(type)) &&
            (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)));
}

int doExternal(URLFile uf, char *path, const char *type, BufferPtr *bufp,
               BufferPtr defaultbuf)
{
    Str tmpf, command;
    Mailcap *mcap;
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
        if (defaultbuf->sourcefile.size())
            src = Strnew(defaultbuf->sourcefile)->ptr;
        else
            src = tmpf->ptr;
        defaultbuf->sourcefile.clear();
        defaultbuf->mailcap = mcap;
    }
    if (mcap->flags & MAILCAP_HTMLOUTPUT)
    {
        buf = loadcmdout(command->ptr, loadHTMLBuffer, defaultbuf);
        if (buf)
        {
            buf->type = "text/html";
            buf->mailcap_source = Strnew(buf->sourcefile)->ptr;
            buf->sourcefile = src;
        }
    }
    else if (mcap->flags & MAILCAP_COPIOUSOUTPUT)
    {
        buf = loadcmdout(command->ptr, loadBuffer, defaultbuf);
        if (buf)
        {
            buf->type = "text/plain";
            buf->mailcap_source = Strnew(buf->sourcefile)->ptr;
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
