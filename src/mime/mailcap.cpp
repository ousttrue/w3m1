#include <stdio.h>
#include <errno.h>
#include <string_view_util.h>
#include "mailcap.h"
#include "config.h"
#include "indep.h"
#include "w3m.h"
#include "file.h"

#define MCSTAT_REPNAME 0x01
#define MCSTAT_REPTYPE 0x02
#define MCSTAT_REPPARAM 0x04

enum MailcapFlags
{
    MAILCAP_NONE = 0x00,
    MAILCAP_NEEDSTERMINAL = 0x01,
    MAILCAP_COPIOUSOUTPUT = 0x02,
    MAILCAP_HTMLOUTPUT = 0x04,
};
struct Mailcap
{
    std::string type;
    std::string viewer;
    MailcapFlags flags = MAILCAP_NONE;
    std::string test;
    std::string nametemplate;
    std::string edit;

    int match(std::string_view src)
    {
        auto [sa, sb] = svu::split(src, '/');
        if (sb.empty())
        {
            return 0;
        }

        auto [a, b] = svu::split(type, '/');
        if (b.empty())
        {
            return 0;
        }

        if (!svu::ic_eq(sa, a))
        {
            return 0;
        }

        int level = this->flags & MAILCAP_HTMLOUTPUT ? 1 : 0;

        if (b[0] == '*')
        {
            return 10 + level;
        }

        if (!svu::ic_eq(sb, b))
        {
            return 0;
        }

        return 20 + level;
    }
};
static Mailcap DefaultMailcap[] = {
    {"image/*", DEF_IMAGE_VIEWER " %s", MAILCAP_NONE},
    {"audio/basic", DEF_AUDIO_PLAYER " %s", MAILCAP_NONE},
};

static std::vector<std::string> mailcap_list;

using MailcapList = std::vector<Mailcap>;
static std::vector<MailcapList> UserMailcap;

Mailcap *searchExtViewer(std::string_view type);

Str unquote_mailcap(const char *qstr, const char *type, char *name, char *attr, int *mc_stat);

Mailcap *searchMailcap(tcb::span<Mailcap> list, std::string_view type)
{
    int level = 0;
    Mailcap *mcap = NULL;

    for (auto &mp : list)
    {
        auto i = mp.match(type.data());
        if (i > level)
        {
            if (mp.test.size())
            {
                Str command = unquote_mailcap(mp.test.data(), type.data(), NULL, NULL, NULL);
                if (system(command->ptr) != 0)
                    continue;
            }
            level = i;
            mcap = &mp;
        }
    }
    return mcap;
}

static int matchMailcapAttr(const char *p, const char *attr, int len, Str *value)
{
    int quoted;
    const char *q = nullptr;

    if (strncasecmp(p, attr, len) == 0)
    {
        p += len;
        SKIP_BLANKS(&p);
        if (value)
        {
            *value = Strnew();
            if (*p == '=')
            {
                p++;
                SKIP_BLANKS(&p);
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

static std::shared_ptr<Mailcap> extractMailcapEntry(std::string_view mcap_entry)
{
    auto p = svu::strip_left(mcap_entry);
    auto k = -1;
    auto j = 0;
    for (; p[j] && p[j] != ';'; j++)
    {
        if (!IS_SPACE(p[j]))
            k = j;
    }

    auto mcap = std::make_shared<Mailcap>();
    mcap->type = allocStr(p.data(), (k >= 0) ? k + 1 : j);
    if (!p[j])
        return nullptr;
    p.remove_prefix(j + 1);
    p = svu::strip_left(p);

    k = -1;
    auto quoted = 0;
    for (j = 0; p[j] && (quoted || p[j] != ';'); j++)
    {
        if (quoted || !IS_SPACE(p[j]))
            k = j;
        if (quoted)
            quoted = 0;
        else if (p[j] == '\\')
            quoted = 1;
    }
    mcap->viewer = allocStr(p.data(), (k >= 0) ? k + 1 : j);
    p.remove_prefix(j);

    while (p[0] == ';')
    {
        p.remove_prefix(1);
        p = svu::strip_left(p);
        Str tmp = nullptr;
        if (matchMailcapAttr(p.data(), "needsterminal", 13, NULL))
        {
            mcap->flags |= MAILCAP_NEEDSTERMINAL;
        }
        else if (matchMailcapAttr(p.data(), "copiousoutput", 13, NULL))
        {
            mcap->flags |= MAILCAP_COPIOUSOUTPUT;
        }
        else if (matchMailcapAttr(p.data(), "x-htmloutput", 12, NULL) ||
                 matchMailcapAttr(p.data(), "htmloutput", 10, NULL))
        {
            mcap->flags |= MAILCAP_HTMLOUTPUT;
        }
        else if (matchMailcapAttr(p.data(), "test", 4, &tmp))
        {
            mcap->test = allocStr(tmp->ptr, tmp->Size());
        }
        else if (matchMailcapAttr(p.data(), "nametemplate", 12, &tmp))
        {
            mcap->nametemplate = allocStr(tmp->ptr, tmp->Size());
        }
        else if (matchMailcapAttr(p.data(), "edit", 4, &tmp))
        {
            mcap->edit = allocStr(tmp->ptr, tmp->Size());
        }
        quoted = 0;
        while (p.size() && (quoted || p[0] != ';'))
        {
            if (quoted)
                quoted = 0;
            else if (p[0] == '\\')
                quoted = 1;
            p.remove_prefix(1);
        }
    }
    return mcap;
}

static std::vector<Mailcap> loadMailcap(const std::string &filename)
{
    std::vector<Mailcap> mcap;
    auto f = fopen(expandPath(filename.c_str()), "r");
    if (!f)
    {
        return mcap;
    }

    auto i = 0;
    Str tmp;
    while (tmp = Strfgets(f), tmp->Size() > 0)
    {
        if (tmp->ptr[0] != '#')
            i++;
    }
    fseek(f, 0, 0);

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
        auto mp = extractMailcapEntry(tmp->ptr);
        if (mp)
        {
            mcap.push_back(*mp);
        }
    }
    fclose(f);
    return mcap;
}

void initMailcap()
{
    if (w3mApp::Instance().mailcap_files.size())
    {
        auto splitter = svu::splitter(w3mApp::Instance().mailcap_files.c_str(), [](char c) -> bool {
            return IS_SPACE(c) || c == ',';
        });
        for (auto s : splitter)
        {
            mailcap_list.push_back(std::string(s));
            UserMailcap.push_back(loadMailcap(mailcap_list.back()));
        }
    }
}

char *
acceptableMimeTypes()
{
    static Str types = NULL;

    if (types != NULL)
        return types->ptr;

    /* generate acceptable media types */
    std::unordered_map<std::string, int> mhash;
    mhash.insert(std::make_pair("text", 1));
    mhash.insert(std::make_pair("image", 1));
    std::vector<std::string> l;
    l.push_back("image");
    for (int i = 0; i < mailcap_list.size(); i++)
    {
        auto &list = UserMailcap[i];
        if (list.empty())
            continue;
        char *mt;
        for (auto j = 0; j < list.size(); ++j)
        {
            auto mp = &list[j];
            auto p = strchr(mp->type.c_str(), '/');
            if (p == NULL)
                continue;
            mt = allocStr(mp->type.c_str(), p - mp->type.c_str());
            auto found = mhash.find(mt);
            if (found == mhash.end())
            {
                l.push_back(mt);
                mhash.insert(std::make_pair(mt, 1));
            }
        }
    }
    types = Strnew();
    types->Push("text/html, text/*;q=0.5");
    while (l.size())
    {
        auto p = l.back();
        l.pop_back();
        types->Push(", ");
        types->Push(p);
        types->Push("/*");
    }
    return types->ptr;
}

Mailcap *searchExtViewer(std::string_view type)
{
    for (int i = 0; i < mailcap_list.size(); i++)
    {
        auto p = searchMailcap(UserMailcap[i], type);
        if (p)
        {
            return p;
        }
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

BufferPtr doExternal(const URL &url, const InputStreamPtr &stream, std::string_view type)
{
    Str tmpf, command;
    Mailcap *mcap;
    int mc_stat;
    BufferPtr buf = NULL;
    char *header, *src = NULL;
    // auto ext = uf->ext;

    if (!(mcap = searchExtViewer(type)))
        return 0;

    // TODO:
    //     if (mcap->nametemplate)
    //     {
    //         tmpf = unquote_mailcap(mcap->nametemplate, NULL, "", NULL, NULL);
    //         if (tmpf->ptr[0] == '.')
    //             ext = tmpf->ptr;
    //     }
    //     tmpf = tmpfname(TMPF_DFL, (ext && *ext) ? ext : NULL);

    //     // if (uf->stream->type() != IST_ENCODED)
    //     //     uf->stream = newEncodedStream(uf->stream, uf->encoding);
    //     // header = checkHeader(defaultbuf, "Content-Type:");
    //     // if (header)
    //     //     header = conv_to_system(header);
    //     command = unquote_mailcap(mcap->viewer, type, tmpf->ptr, header, &mc_stat);
    // #ifndef __EMX__
    //     if (!(mc_stat & MCSTAT_REPNAME))
    //     {
    //         Str tmp = Sprintf("(%s) < %s", command->ptr, shell_quote(tmpf->ptr));
    //         command = tmp;
    //     }
    // #endif

    // #ifdef HAVE_SETPGRP
    //     if (!(mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT)) &&
    //         !(mcap->flags & MAILCAP_NEEDSTERMINAL) && BackgroundExtViewer)
    //     {
    //         Terminal::flush();
    //         if (!fork())
    //         {
    //             setup_child(false, 0, uf->stream->FD());
    //             if (save2tmp(uf, tmpf->ptr) < 0)
    //                 exit(1);
    //             myExec(command->ptr);
    //         }
    //         return nullptr;
    //     }
    //     else
    // #endif
    //     {
    //         if (save2tmp(uf, tmpf->ptr) < 0)
    //         {
    //             return NULL;
    //         }
    //     }
    //     if (mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT))
    //     {
    //         buf = Buffer::Create(INIT_BUFFER_WIDTH());
    //         if (buf->sourcefile.size())
    //             src = Strnew(buf->sourcefile)->ptr;
    //         else
    //             src = tmpf->ptr;
    //         buf->sourcefile.clear();
    //         buf->mailcap = mcap;
    //     }
    //     if (mcap->flags & MAILCAP_HTMLOUTPUT)
    //     {
    //         buf = loadcmdout(command->ptr, loadHTMLBuffer);
    //         if (buf)
    //         {
    //             buf->type = "text/html";
    //             buf->mailcap_source = Strnew(buf->sourcefile)->ptr;
    //             buf->sourcefile = src;
    //         }
    //     }
    //     else if (mcap->flags & MAILCAP_COPIOUSOUTPUT)
    //     {
    //         buf = loadcmdout(command->ptr, loadBuffer);
    //         if (buf)
    //         {
    //             buf->type = "text/plain";
    //             buf->mailcap_source = Strnew(buf->sourcefile)->ptr;
    //             buf->sourcefile = src;
    //         }
    //     }
    //     else
    //     {
    //         if (mcap->flags & MAILCAP_NEEDSTERMINAL || !BackgroundExtViewer)
    //         {
    //             fmTerm();
    //             mySystem(command->ptr, 0);
    //             fmInit();
    //             if (GetCurrentTab() && GetCurrentBuffer())
    //                 displayCurrentbuf(B_FORCE_REDRAW);
    //         }
    //         else
    //         {
    //             mySystem(command->ptr, 1);
    //         }
    //         buf = nullptr;
    //     }
    //     if (buf)
    //     {
    //         buf->filename = path;
    //         if (buf->buffername.empty() || buf->buffername[0] == '\0')
    //             buf->buffername = conv_from_system(lastFileName(path));
    //         buf->edit = mcap->edit;
    //         buf->mailcap = mcap;
    //     }

    return buf;
}

bool is_dump_text_type(std::string_view type)
{
    auto mcap = searchExtViewer(type);
    if (!mcap)
    {
        return false;
    }
    return mcap->flags & (MAILCAP_HTMLOUTPUT | MAILCAP_COPIOUSOUTPUT);
}
