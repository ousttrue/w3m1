/* $Id: etc.c,v 1.81 2007/05/23 15:06:05 inu Exp $ */

#include "fm.h"

#include "indep.h"
#include "file.h"
#ifndef __MINGW32_VERSION
#include <pwd.h>
#endif
#include "myctype.h"
#include "html/html.h"
#include "transport/local.h"
#include "html/tagtable.h"
#include "frontend/terms.h"
#include "frontend/display.h"
#include "frontend/buffer.h"
#include "transport/url.h"

// #include <sys/socket.h>
#include <netdb.h>

#include <fcntl.h>
#include <sys/types.h>

#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <signal.h>

#ifdef __WATT32__
#define read(a, b, c) read_s(a, b, c)
#define close(x) close_s(x)
#endif /* __WATT32__ */

struct auth_pass
{
    int bad;
    int is_proxy;
    Str host;
    int port;
    /*    Str file; */
    Str realm;
    Str uname;
    Str pwd;
    struct auth_pass *next;
};

struct auth_pass *passwords = NULL;

int columnSkip(BufferPtr buf, int offset)
{
    int i, maxColumn;
    int column = buf->currentColumn + offset;
    int nlines = buf->LINES + 1;
    Line *l;

    maxColumn = 0;
    for (i = 0, l = buf->topLine; i < nlines && l != NULL; i++, l = l->next)
    {
        if (l->width < 0)
            l->CalcWidth();
        if (l->width - 1 > maxColumn)
            maxColumn = l->width - 1;
    }
    maxColumn -= buf->COLS - 1;
    if (column < maxColumn)
        maxColumn = column;
    if (maxColumn < 0)
        maxColumn = 0;

    if (buf->currentColumn == maxColumn)
        return 0;
    buf->currentColumn = maxColumn;
    return 1;
}

int columnPos(Line *line, int column)
{
    int i;

    for (i = 1; i < line->len; i++)
    {
        if (line->COLPOS(i) > column)
            break;
    }
    for (i--; i > 0 && line->propBuf[i] & PC_WCHAR2; i--)
        ;
    return i;
}

Line *
lineSkip(BufferPtr buf, Line *line, int offset, int last)
{
    int i;
    Line *l;

    l = currentLineSkip(buf, line, offset, last);
    if (!nextpage_topline)
        for (i = buf->LINES - 1 - (buf->lastLine->linenumber - l->linenumber);
             i > 0 && l->prev != NULL; i--, l = l->prev)
            ;
    return l;
}

Line *
currentLineSkip(BufferPtr buf, Line *line, int offset, int last)
{
    int i, n;
    Line *l = line;

    if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE))
    {
        n = line->linenumber + offset + buf->LINES;
        if (buf->lastLine->linenumber < n)
            getNextPage(buf, n - buf->lastLine->linenumber);
        while ((last || (buf->lastLine->linenumber < n)) &&
               (getNextPage(buf, 1) != NULL))
            ;
        if (last)
            l = buf->lastLine;
    }

    if (offset == 0)
        return l;
    if (offset > 0)
        for (i = 0; i < offset && l->next != NULL; i++, l = l->next)
            ;
    else
        for (i = 0; i < -offset && l->prev != NULL; i++, l = l->prev)
            ;
    return l;
}

#define MAX_CMD_LEN 128

int gethtmlcmd(char **s)
{
    char cmdstr[MAX_CMD_LEN];
    char *p = cmdstr;
    char *save = *s;

    (*s)++;
    /* first character */
    if (IS_ALNUM(**s) || **s == '_' || **s == '/')
    {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    else
        return HTML_UNKNOWN;
    if (p[-1] == '/')
        SKIP_BLANKS(*s);
    while ((IS_ALNUM(**s) || **s == '_') && p - cmdstr < MAX_CMD_LEN)
    {
        *(p++) = TOLOWER(**s);
        (*s)++;
    }
    if (p - cmdstr == MAX_CMD_LEN)
    {
        /* buffer overflow: perhaps caused by bad HTML source */
        *s = save + 1;
        return HTML_UNKNOWN;
    }
    *p = '\0';

    /* hash search */
    //     extern Hash_si tagtable;
    //     int cmd = getHash_si(&tagtable, cmdstr, HTML_UNKNOWN);
    int cmd = GetTag(cmdstr, HTML_UNKNOWN);
    while (**s && **s != '>')
        (*s)++;
    if (**s == '>')
        (*s)++;
    return cmd;
}




char *
lastFileName(char *path)
{
    char *p, *q;

    p = q = path;
    while (*p != '\0')
    {
        if (*p == '/')
            q = p + 1;
        p++;
    }

    return allocStr(q, -1);
}

#ifdef USE_INCLUDED_SRAND48
static unsigned long R1 = 0x1234abcd;
static unsigned long R2 = 0x330e;
#define A1 0x5deec
#define A2 0xe66d
#define C 0xb

void srand48(long seed)
{
    R1 = (unsigned long)seed;
    R2 = 0x330e;
}

long lrand48(void)
{
    R1 = (A1 * R1 << 16) + A1 * R2 + A2 * R1 + ((A2 * R2 + C) >> 16);
    R2 = (A2 * R2 + C) & 0xffff;
    return (long)(R1 >> 1);
}
#endif



char *
mydirname(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    if (s != p)
        p--;
    while (s != p && *p == '/')
        p--;
    while (s != p && *p != '/')
        p--;
    if (*p != '/')
        return ".";
    while (s != p && *p == '/')
        p--;
    return allocStr(s, strlen(s) - strlen(p) + 1);
}

#ifndef HAVE_STRERROR
char *
strerror(int errno)
{
    extern char *sys_errlist[];
    return sys_errlist[errno];
}
#endif /* not HAVE_STRERROR */

#ifndef HAVE_SYS_ERRLIST
char **sys_errlist;

prepare_sys_errlist()
{
    int i, n;

    i = 1;
    while (strerror(i) != NULL)
        i++;
    n = i;
    sys_errlist = New_N(char *, n);
    sys_errlist[0] = "";
    for (i = 1; i < n; i++)
        sys_errlist[i] = strerror(i);
}
#endif /* not HAVE_SYS_ERRLIST */




/*
 * RFC2617: 1.2 Access Authentication Framework
 *
 * The realm value (case-sensitive), in combination with the canonical root
 * URL (the absoluteURI for the server whose abs_path is empty; see section
 * 5.1.2 of RFC2616 ) of the server being accessed, defines the protection
 * space. These realms allow the protected resources on a server to be
 * partitioned into a set of protection spaces, each with its own
 * authentication scheme and/or authorization database.
 *
 */
static void
add_auth_pass_entry(const struct auth_pass *ent, int netrc, int override)
{
    if ((ent->host || netrc) /* netrc accept default (host == NULL) */
        && (ent->is_proxy || ent->realm || netrc) && ent->uname && ent->pwd)
    {
        struct auth_pass *newent = New(struct auth_pass);
        memcpy(newent, ent, sizeof(struct auth_pass));
        if (override)
        {
            newent->next = passwords;
            passwords = newent;
        }
        else
        {
            if (passwords == NULL)
                passwords = newent;
            else if (passwords->next == NULL)
                passwords->next = newent;
            else
            {
                struct auth_pass *ep = passwords;
                for (; ep->next; ep = ep->next)
                    ;
                ep->next = newent;
            }
        }
    }
    /* ignore invalid entries */
}

static struct auth_pass *
find_auth_pass_entry(char *host, int port, char *realm, char *uname,
                     int is_proxy)
{
    struct auth_pass *ent;
    for (ent = passwords; ent != NULL; ent = ent->next)
    {
        if (ent->is_proxy == is_proxy && (ent->bad != TRUE) && (!ent->host || ent->host->ICaseCmp(host) == 0) && (!ent->port || ent->port == port) && (!ent->uname || !uname || ent->uname->Cmp(uname) == 0) && (!ent->realm || !realm || ent->realm->Cmp(realm) == 0))
            return ent;
    }
    return NULL;
}

int find_auth_user_passwd(ParsedURL *pu, char *realm,
                          Str *uname, Str *pwd, int is_proxy)
{
    if (pu->user.size() && pu->pass.size())
    {
        *uname = Strnew(pu->user);
        *pwd = Strnew(pu->pass);
        return 1;
    }
    auto ent = find_auth_pass_entry(const_cast<char*>(pu->host.c_str()), pu->port, realm, const_cast<char*>(pu->user.c_str()), is_proxy);
    if (ent)
    {
        *uname = ent->uname;
        *pwd = ent->pwd;
        return 1;
    }
    return 0;
}

void add_auth_user_passwd(ParsedURL *pu, char *realm, Str uname, Str pwd,
                          int is_proxy)
{
    struct auth_pass ent;
    memset(&ent, 0, sizeof(ent));

    ent.is_proxy = is_proxy;
    ent.host = Strnew(pu->host);
    ent.port = pu->port;
    ent.realm = Strnew(realm);
    ent.uname = uname;
    ent.pwd = pwd;
    add_auth_pass_entry(&ent, 0, 1);
}

void invalidate_auth_user_passwd(ParsedURL *pu, char *realm, Str uname, Str pwd,
                                 int is_proxy)
{
    struct auth_pass *ent;
    ent = find_auth_pass_entry(const_cast<char*>(pu->host.c_str()), pu->port, realm, NULL, is_proxy);
    if (ent)
    {
        ent->bad = TRUE;
    }
    return;
}

/* passwd */
/*
 * machine <host>
 * host <host>
 * port <port>
 * proxy
 * path <file>	; not used
 * realm <realm>
 * login <login>
 * passwd <passwd>
 * password <passwd>
 */

static Str
next_token(Str arg)
{
    Str narg = NULL;
    char *p, *q;
    if (arg == NULL || arg->Size() == 0)
        return NULL;
    p = arg->ptr;
    q = p;
    SKIP_NON_BLANKS(q);
    if (*q != '\0')
    {
        *q++ = '\0';
        SKIP_BLANKS(q);
        if (*q != '\0')
            narg = Strnew(q);
    }
    return narg;
}

static void
parsePasswd(FILE *fp, int netrc)
{
    struct auth_pass ent;
    Str line = NULL;

    bzero(&ent, sizeof(struct auth_pass));
    while (1)
    {
        Str arg = NULL;
        char *p;

        if (line == NULL || line->Size() == 0)
            line = Strfgets(fp);
        if (line->Size() == 0)
            break;
        line->Strip();
        p = line->ptr;
        if (*p == '#' || *p == '\0')
        {
            line = NULL;
            continue; /* comment or empty line */
        }
        arg = next_token(line);

        if (!strcmp(p, "machine") || !strcmp(p, "host") || (netrc && !strcmp(p, "default")))
        {
            add_auth_pass_entry(&ent, netrc, 0);
            bzero(&ent, sizeof(struct auth_pass));
            if (netrc)
                ent.port = 21; /* XXX: getservbyname("ftp"); ? */
            if (strcmp(p, "default") != 0)
            {
                line = next_token(arg);
                ent.host = arg;
            }
            else
            {
                line = arg;
            }
        }
        else if (!netrc && !strcmp(p, "port") && arg)
        {
            line = next_token(arg);
            ent.port = atoi(arg->ptr);
        }
        else if (!netrc && !strcmp(p, "proxy"))
        {
            ent.is_proxy = 1;
            line = arg;
        }
        else if (!netrc && !strcmp(p, "path"))
        {
            line = next_token(arg);
            /* ent.file = arg; */
        }
        else if (!netrc && !strcmp(p, "realm"))
        {
            /* XXX: rest of line becomes arg for realm */
            line = NULL;
            ent.realm = arg;
        }
        else if (!strcmp(p, "login"))
        {
            line = next_token(arg);
            ent.uname = arg;
        }
        else if (!strcmp(p, "password") || !strcmp(p, "passwd"))
        {
            line = next_token(arg);
            ent.pwd = arg;
        }
        else if (netrc && !strcmp(p, "machdef"))
        {
            while ((line = Strfgets(fp))->Size() != 0)
            {
                if (*line->ptr == '\n')
                    break;
            }
            line = NULL;
        }
        else if (netrc && !strcmp(p, "account"))
        {
            /* ignore */
            line = next_token(arg);
        }
        else
        {
            /* ignore rest of line */
            line = NULL;
        }
    }
    add_auth_pass_entry(&ent, netrc, 0);
}

/* FIXME: gettextize? */
#define FILE_IS_READABLE_MSG "SECURITY NOTE: file %s must not be accessible by others"

FILE *
openSecretFile(char *fname)
{
    char *efname;
    struct stat st;

    if (fname == NULL)
        return NULL;
    efname = expandPath(fname);
    if (stat(efname, &st) < 0)
        return NULL;

    /* check permissions, if group or others readable or writable,
     * refuse it, because it's insecure.
     *
     * XXX: disable_secret_security_check will introduce some
     *    security issues, but on some platform such as Windows
     *    it's not possible (or feasible) to disable group|other
     *    readable and writable.
     *   [w3m-dev 03368][w3m-dev 03369][w3m-dev 03370]
     */
    if (disable_secret_security_check)
        /* do nothing */;
    else if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0)
    {
        if (fmInitialized)
        {
            message(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, 0, 0);
            refresh();
        }
        else
        {
            fputs(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, stderr);
            fputc('\n', stderr);
        }
        sleep(2);
        return NULL;
    }

    return fopen(efname, "r");
}

void loadPasswd(void)
{
    FILE *fp;

    passwords = NULL;
    fp = openSecretFile(passwd_file);
    if (fp != NULL)
    {
        parsePasswd(fp, 0);
        fclose(fp);
    }

    /* for FTP */
    fp = openSecretFile("~/.netrc");
    if (fp != NULL)
    {
        parsePasswd(fp, 1);
        fclose(fp);
    }
    return;
}

/* get last modified time */
char *
last_modified(BufferPtr buf)
{
    TextListItem *ti;
    struct stat st;

    if (buf->document_header)
    {
        for (ti = buf->document_header->first; ti; ti = ti->next)
        {
            if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0)
            {
                return ti->ptr + 15;
            }
        }
        return "unknown";
    }
    else if (buf->currentURL.scheme == SCM_LOCAL)
    {
        if (stat(buf->currentURL.file.c_str(), &st) < 0)
            return "unknown";
        return ctime(&st.st_mtime);
    }
    return "unknown";
}

static char roman_num1[] = {
    'i',
    'x',
    'c',
    'm',
    '*',
};
static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static Str
romanNum2(int l, int n)
{
    Str s = Strnew();

    switch (n)
    {
    case 1:
    case 2:
    case 3:
        for (; n > 0; n--)
            s->Push(roman_num1[l]);
        break;
    case 4:
        s->Push(roman_num1[l]);
        s->Push(roman_num5[l]);
        break;
    case 5:
    case 6:
    case 7:
    case 8:
        s->Push(roman_num5[l]);
        for (n -= 5; n > 0; n--)
            s->Push(roman_num1[l]);
        break;
    case 9:
        s->Push(roman_num1[l]);
        s->Push(roman_num1[l + 1]);
        break;
    }
    return s;
}

Str romanNumeral(int n)
{
    Str r = Strnew();

    if (n <= 0)
        return r;
    if (n >= 4000)
    {
        r->Push("**");
        return r;
    }
    r->Push(romanNum2(3, n / 1000));
    r->Push(romanNum2(2, (n % 1000) / 100));
    r->Push(romanNum2(1, (n % 100) / 10));
    r->Push(romanNum2(0, n % 10));

    return r;
}

Str romanAlphabet(int n)
{
    Str r = Strnew();
    int l;
    char buf[14];

    if (n <= 0)
        return r;

    l = 0;
    while (n)
    {
        buf[l++] = 'a' + (n - 1) % 26;
        n = (n - 1) / 26;
    }
    l--;
    for (; l >= 0; l--)
        r->Push(buf[l]);

    return r;
}

#ifndef SIGIOT
#define SIGIOT SIGABRT
#endif /* not SIGIOT */

static void
reset_signals(void)
{
#ifdef SIGHUP
    mySignal(SIGHUP, SIG_DFL); /* terminate process */
#endif
    mySignal(SIGINT, SIG_DFL); /* terminate process */
#ifdef SIGQUIT
    mySignal(SIGQUIT, SIG_DFL); /* terminate process */
#endif
    mySignal(SIGTERM, SIG_DFL); /* terminate process */
    mySignal(SIGILL, SIG_DFL);  /* create core image */
    mySignal(SIGIOT, SIG_DFL);  /* create core image */
    mySignal(SIGFPE, SIG_DFL);  /* create core image */
#ifdef SIGBUS
    mySignal(SIGBUS, SIG_DFL); /* create core image */
#endif                         /* SIGBUS */
#ifdef SIGCHLD
    mySignal(SIGCHLD, SIG_IGN);
#endif
#ifdef SIGPIPE
    mySignal(SIGPIPE, SIG_IGN);
#endif
}

#ifndef FOPEN_MAX
#define FOPEN_MAX 1024 /* XXX */
#endif

static void
close_all_fds_except(int i, int f)
{
    switch (i)
    { /* fall through */
    case 0:
        dup2(open(DEV_NULL_PATH, O_RDONLY), 0);
    case 1:
        dup2(open(DEV_NULL_PATH, O_WRONLY), 1);
    case 2:
        dup2(open(DEV_NULL_PATH, O_WRONLY), 2);
    }
    /* close all other file descriptors (socket, ...) */
    for (i = 3; i < FOPEN_MAX; i++)
    {
        if (i != f)
            close(i);
    }
}

void setup_child(int child, int i, int f)
{
    reset_signals();
    mySignal(SIGINT, SIG_IGN);
#ifndef __MINGW32_VERSION
    if (!child)
        SETPGRP();
#endif /* __MINGW32_VERSION */
    close_tty();
    close_all_fds_except(i, f);
    QuietMessage = TRUE;
    fmInitialized = FALSE;
    TrapSignal = FALSE;
}


void myExec(char *command)
{
    mySignal(SIGINT, SIG_DFL);
    execl("/bin/sh", "sh", "-c", command, NULL);
    exit(127);
}

void mySystem(char *command, int background)
{
#ifndef __MINGW32_VERSION
    if (background)
    {
#ifndef __EMX__
        flush_tty();
        if (!fork())
        {
            setup_child(FALSE, 0, -1);
            myExec(command);
        }
#else
        Str cmd = Strnew("start /f ");
        cmd->Push(command);
        system(cmd->ptr);
#endif
    }
    else
#endif /* __MINGW32_VERSION */
        system(command);
}

Str myExtCommand(char *cmd, char *arg, int redirect)
{
    Str tmp = NULL;
    char *p;
    int set_arg = FALSE;

    for (p = cmd; *p; p++)
    {
        if (*p == '%' && *(p + 1) == 's' && !set_arg)
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            tmp->Push(arg);
            set_arg = TRUE;
            p++;
        }
        else
        {
            if (tmp)
                tmp->Push(*p);
        }
    }
    if (!set_arg)
    {
        if (redirect)
            tmp = Strnew_m_charp("(", cmd, ") < ", arg, NULL);
        else
            tmp = Strnew_m_charp(cmd, " ", arg, NULL);
    }
    return tmp;
}

Str myEditor(char *cmd, char *file, int line)
{
    Str tmp = NULL;
    char *p;
    int set_file = FALSE, set_line = FALSE;

    for (p = cmd; *p; p++)
    {
        if (*p == '%' && *(p + 1) == 's' && !set_file)
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            tmp->Push(file);
            set_file = TRUE;
            p++;
        }
        else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0)
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            tmp->Push(Sprintf("%d", line));
            set_line = TRUE;
            p++;
        }
        else
        {
            if (tmp)
                tmp->Push(*p);
        }
    }
    if (!set_file)
    {
        if (tmp == NULL)
            tmp = Strnew(cmd);
        if (!set_line && line > 1 && strcasestr(cmd, "vi"))
            tmp->Push(Sprintf(" +%d", line));
        Strcat_m_charp(tmp, " ", file, NULL);
    }
    return tmp;
}

#ifdef __MINGW32_VERSION
char *
expandName(char *name)
{
    return getenv("HOME");
}
#else
char *
expandName(char *name)
{
    char *p;
    struct passwd *passent, *getpwnam(const char *);
    Str extpath = NULL;

    if (name == NULL)
        return NULL;
    p = name;
    if (*p == '/')
    {
        if ((*(p + 1) == '~' && IS_ALPHA(*(p + 2))) && personal_document_root)
        {
            char *q;
            p += 2;
            q = strchr(p, '/');
            if (q)
            { /* /~user/dir... */
                passent = getpwnam(allocStr(p, q - p));
                p = q;
            }
            else
            { /* /~user */
                passent = getpwnam(p);
                p = "";
            }
            if (!passent)
                goto rest;
            extpath = Strnew_m_charp(passent->pw_dir, "/",
                                     personal_document_root, NULL);
            if (*personal_document_root == '\0' && *p == '/')
                p++;
        }
        else
            goto rest;
        if (extpath->Cmp("/") == 0 && *p == '/')
            p++;
        extpath->Push(p);
        return extpath->ptr;
    }
    else
        return expandPath(p);
rest:
    return name;
}
#endif


char *
url_unquote_conv(const char *url, wc_ces charset)
{
    uint8_t old_auto_detect = WcOption.auto_detect;
    Str tmp = Strnew(url)->UrlDecode(FALSE, TRUE);
    if (!charset || charset == WC_CES_US_ASCII)
        charset = SystemCharset;
    WcOption.auto_detect = WC_OPT_DETECT_ON;
    tmp = convertLine(NULL, tmp, RAW_MODE, &charset, charset);
    WcOption.auto_detect = old_auto_detect;
    return tmp->ptr;
}

static const char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp",
    "src",
    "frame",
    "cache",
    "cookie",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str tmpfname(int type, const char *ext)
{
    Str tmpf;
    tmpf = Sprintf("%s/w3m%s%d-%d%s",
                   tmp_dir,
                   tmpf_base[type],
                   CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");
    pushText(fileToDelete, tmpf->ptr);
    return tmpf;
}

