#define MAINPROGRAM

#include "config.h"
#include <gc.h>
#include <locale.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "make_array.h"
#include "w3m.h"
#include "cli.h"
#include "backend.h"
#include "indep.h"
#include "gc_helper.h"
#include "textlist.h"
#include "charset.h"
#include "rc.h"
#include "register_commands.h"
#include "history.h"

#include "symbol.h"
#include "myctype.h"
#include "file.h"
#include "public.h"
#include "html/parsetag.h"
#include "html/form.h"
#include "html/image.h"
#include "html/html_processor.h"
#include "stream/loader.h"
#include "stream/istream.h"
#include "stream/cookie.h"
#include "frontend/mouse.h"
#include "frontend/lineinput.h"
#include "frontend/terms.h"
#include "frontend/event.h"
#include "frontend/tabbar.h"
#include "frontend/display.h"
#include "frontend/terms.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAXIMUM_COLS 1024
#define DEFAULT_COLS 80

static inline volatile bool s_need_resize_screen = false;
static void resize_hook(SIGNAL_ARG)
{
    s_need_resize_screen = true;
    mySignal(SIGWINCH, resize_hook);
}
int need_resize_screen()
{
    return s_need_resize_screen;
}
void set_need_resize_screen(int need)
{
    s_need_resize_screen = need;
}
void resize_screen()
{
    s_need_resize_screen = false;
    setlinescols();
    setupscreen();
    if (GetCurrentTab())
        displayCurrentbuf(B_FORCE_REDRAW);
}

//
// Download
//
struct DownloadList
{
    pid_t pid;
    char *url;
    char *save;
    char *lock;
    clen_t size;
    time_t time;
    int running;
    int err;
    DownloadList *next;
    DownloadList *prev;
};
static DownloadList *FirstDL = nullptr;
static DownloadList *LastDL = nullptr;
static void sig_chld(int signo)
{
    int p_stat;
    pid_t pid;

#ifdef HAVE_WAITPID
    while ((pid = waitpid(-1, &p_stat, WNOHANG)) > 0)
#elif HAVE_WAIT3
    while ((pid = wait3(&p_stat, WNOHANG, NULL)) > 0)
#else
    if ((pid = wait(&p_stat)) > 0)
#endif
    {
        DownloadList *d;

        if (WIFEXITED(p_stat))
        {
            for (d = FirstDL; d != NULL; d = d->next)
            {
                if (d->pid == pid)
                {
                    d->err = WEXITSTATUS(p_stat);
                    break;
                }
            }
        }
    }
    mySignal(SIGCHLD, sig_chld);
    return;
}

void stopDownload()
{
    DownloadList *d;

    if (!FirstDL)
        return;
    for (d = FirstDL; d != NULL; d = d->next)
    {
        if (!d->running)
            continue;
#ifndef __MINGW32_VERSION
        kill(d->pid, SIGKILL);
#endif
        unlink(d->lock);
    }
}

void download_action(struct parsed_tagarg *arg)
{
    DownloadList *d;
    pid_t pid;

    for (; arg; arg = arg->next)
    {
        if (!strncmp(arg->arg, "stop", 4))
        {
            pid = (pid_t)atoi(&arg->arg[4]);
#ifndef __MINGW32_VERSION
            kill(pid, SIGKILL);
#endif
        }
        else if (!strncmp(arg->arg, "ok", 2))
            pid = (pid_t)atoi(&arg->arg[2]);
        else
            continue;
        for (d = FirstDL; d; d = d->next)
        {
            if (d->pid == pid)
            {
                unlink(d->lock);
                if (d->prev)
                    d->prev->next = d->next;
                else
                    FirstDL = d->next;
                if (d->next)
                    d->next->prev = d->prev;
                else
                    LastDL = d->prev;
                break;
            }
        }
    }
    ldDL(&w3mApp::Instance());
}

int checkDownloadList()
{
    DownloadList *d;
    struct stat st;

    if (!FirstDL)
        return false;
    for (d = FirstDL; d != NULL; d = d->next)
    {
        if (d->running && !lstat(d->lock, &st))
            return true;
    }
    return false;
}

static int s_add_download_list = false;
int add_download_list()
{
    return s_add_download_list;
}
void set_add_download_list(int add)
{
    s_add_download_list = add;
}

void addDownloadList(pid_t pid, char *url, char *save, char *lock, clen_t size)
{
    DownloadList *d;

    d = New(DownloadList);
    d->pid = pid;
    d->url = url;
    if (save[0] != '/' && save[0] != '~')
        save = Strnew_m_charp(w3mApp::Instance().CurrentDir, "/", save, NULL)->ptr;
    d->save = expandPath(save);
    d->lock = lock;
    d->size = size;
    d->time = time(0);
    d->running = true;
    d->err = 0;
    d->next = NULL;
    d->prev = LastDL;
    if (LastDL)
        LastDL->next = d;
    else
        FirstDL = d;
    LastDL = d;
    set_add_download_list(true);
}

BufferPtr DownloadListBuffer(w3mApp *w3m)
{
    DownloadList *d;
    Str src = NULL;
    struct stat st;
    time_t cur_time;
    int duration, rate, eta;
    size_t size;

    if (!FirstDL)
        return NULL;
    cur_time = time(0);
    /* FIXME: gettextize? */
    src = Strnew_m_charp("<html><head><title>", w3m->DOWNLOAD_LIST_TITLE,
                         "</title></head>\n<body><h1 align=center>", w3m->DOWNLOAD_LIST_TITLE, "</h1>\n"
                                                                                               "<form method=internal action=download><hr>\n");
    for (d = LastDL; d != NULL; d = d->prev)
    {
        if (lstat(d->lock, &st))
            d->running = false;
        src->Push("<pre>\n");
        src->Push(Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url),
                          html_quote(conv_from_system(d->save))));
        duration = cur_time - d->time;
        if (!stat(d->save, &st))
        {
            size = st.st_size;
            if (!d->running)
            {
                if (!d->err)
                    d->size = size;
                duration = st.st_mtime - d->time;
            }
        }
        else
            size = 0;
        if (d->size)
        {
            int i, l = COLS - 6;
            if (size < d->size)
                i = 1.0 * l * size / d->size;
            else
                i = l;
            l -= i;
            while (i-- > 0)
                src->Push('#');
            while (l-- > 0)
                src->Push('_');
            src->Push('\n');
        }
        if ((d->running || d->err) && size < d->size)
            src->Push(Sprintf("  %s / %s bytes (%d%%)",
                              convert_size3(size), convert_size3(d->size),
                              (int)(100.0 * size / d->size)));
        else
            src->Push(Sprintf("  %s bytes loaded", convert_size3(size)));
        if (duration > 0)
        {
            rate = size / duration;
            src->Push(Sprintf("  %02d:%02d:%02d  rate %s/sec",
                              duration / (60 * 60), (duration / 60) % 60,
                              duration % 60, convert_size(rate, 1)));
            if (d->running && size < d->size && rate)
            {
                eta = (d->size - size) / rate;
                src->Push(Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60),
                                  (eta / 60) % 60, eta % 60));
            }
        }
        src->Push('\n');
        if (!d->running)
        {
            src->Push(Sprintf("<input type=submit name=ok%d value=OK>",
                              d->pid));
            switch (d->err)
            {
            case 0:
                if (size < d->size)
                    src->Push(" Download ended but probably not complete");
                else
                    src->Push(" Download complete");
                break;
            case 1:
                src->Push(" Error: could not open destination file");
                break;
            case 2:
                src->Push(" Error: could not write to file (disk full)");
                break;
            default:
                src->Push(" Error: unknown reason");
            }
        }
        else
            src->Push(Sprintf("<input type=submit name=stop%d value=STOP>",
                              d->pid));
        src->Push("\n</pre><hr>\n");
    }
    src->Push("</form></body></html>");
    return loadHTMLString({}, src);
}

//
//
//

// TODO
#define GC_WARN_KEEP_MAX (20)
static void wrap_GC_warn_proc(char *msg, GC_word arg)
{
    if (w3mApp::Instance().fmInitialized)
    {
        /* *INDENT-OFF* */
        static struct
        {
            char *msg;
            GC_word arg;
        } msg_ring[GC_WARN_KEEP_MAX];
        /* *INDENT-ON* */
        static int i = 0;
        static int n = 0;
        static int lock = 0;
        int j;

        j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
        msg_ring[j].msg = msg;
        msg_ring[j].arg = arg;

        if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
            ++n;
        else
            ++i;

        if (!lock)
        {
            lock = 1;

            for (; n > 0; --n, ++i)
            {
                i %= sizeof(msg_ring) / sizeof(msg_ring[0]);

                printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
                sleep_till_anykey(1, 1);
            }

            lock = 0;
        }
    }
    else
    {
        fprintf(stderr, msg, (unsigned long)arg);
    }
}

w3mApp::w3mApp()
{
    // GC
    GC_INIT();
    GC_set_warn_proc(wrap_GC_warn_proc);

    setlocale(LC_ALL, "");

    CurrentPid = (int)getpid();
    CurrentDir = currentdir();

    NO_proxy_domains = newTextList();
    fileToDelete = newTextList();

    m_term = new Terminal();
}

w3mApp::~w3mApp()
{
    delete m_term;

#ifdef USE_MIGEMO
    init_migemo(); /* close pipe to migemo */
#endif

    stopDownload();

    DeleteAllTabs();
    // deleteFiles();
    {
        char *f;
        while ((f = popText(fileToDelete)) != NULL)
            unlink(f);
    }

    // exit(i);
}

static const char *get_non_null_env()
{
    return "";
}

template <typename... ARGS>
static const char *get_non_null_env(const char *name, ARGS... args)
{
    auto value = getenv(name);
    if (non_null(value))
    {
        return value;
    }
    return get_non_null_env(args...);
}

std::string w3mApp::make_optional_header_string(const char *s)
{
    if (strchr(s, '\n') || strchr(s, '\r'))
        return "";

    const char *p;
    for (p = s; *p && *p != ':'; p++)
        ;
    if (*p != ':' || p == s)
        return "";

    auto hs = Strnew_size(strlen(s) + 3);
    hs->CopyFrom(s, p - s);
    if (hs->ICaseCmp("content-type") == 0)
        override_content_type = true;
    hs->Push(": ");
    if (*(++p))
    {                    /* not null header */
        SKIP_BLANKS(&p); /* skip white spaces */
        hs->Push(p);
    }
    hs->Push("\r\n");
    return hs->ptr;
}

int w3mApp::Main(const URL &url)
{
    std::string_view Locale = get_non_null_env("LC_ALL", "LC_CTYPE", "LANG");
    if (Locale.size())
    {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        DocumentCharset = wc_guess_locale_charset(Locale, DocumentCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }

    //
    // load rc settings
    //
    init_rc();

    register_commands();

    auto LoadHist = newHist();
    auto SaveHist = newHist();
    auto ShellHist = newHist();
    auto TextHist = newHist();
    auto URLHist = newHist();

    if (FollowLocale && Locale.size())
    {
        DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
        SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }
    BookmarkCharset = DocumentCharset;

    //
    // load env
    //
    if (HTTP_proxy.empty())
    {
        HTTP_proxy = get_non_null_env("HTTP_PROXY", "http_proxy", "HTTP_proxy");
    }

    //
    //
    //

    ClearCurrentKey();
    if (BookmarkFile.empty())
    {
        BookmarkFile = rcFile(BOOKMARK);
    }

    {
        // terminal
        fmInit();

        mySignal(SIGWINCH, resize_hook);

        setlinescols();
        setupscreen();
    }

    sync_with_option();
    initCookie();

    if (UseHistory)
        loadHistory(URLHist);

    wtf_init(DocumentCharset, DisplayCharset);
    mySignal(SIGCHLD, sig_chld);
    mySignal(SIGPIPE, SigPipe);

    //
    // load initial buffer
    //
    GetCurrentTab()->Push(url);

    SearchHeader = false;
    DefaultType.clear();
    UseContentCharset = true;

    mainloop();

    return 0;
}

// TODO:
extern char *CurrentCmdData;

void w3mApp::mainloop()
{
    for (;;)
    {
        if (add_download_list())
        {
            set_add_download_list(false);
            ldDL(&w3mApp::Instance());
        }

        auto tab = GetCurrentTab();
        auto buf = tab->GetCurrentBuffer();
        if (buf->submit)
        {
            auto a = buf->submit;
            buf->submit = NULL;
            buf->Goto(a->start);
            _followForm(true);
            continue;
        }

        /* event processing */
        if (ProcessEvent())
        {
            continue;
        }

        /* get keypress event */

        if (buf->event)
        {
            if (buf->event->status != AL_UNSET)
            {
                SetCurrentAlarm(buf->event);
                if (CurrentAlarm()->sec == 0)
                { /* refresh (0sec) */
                    buf->event = NULL;
                    ClearCurrentKey();
                    ClearCurrentKeyData();
                    CurrentCmdData = (const char *)CurrentAlarm()->data;
                    CurrentAlarm()->cmd(&w3mApp::Instance());
                    CurrentCmdData.clear();
                    continue;
                }
            }
            else
                buf->event = NULL;
        }
        if (!buf->event)
            SetCurrentAlarm(DefaultAlarm());

        DisableMouseAction();
        if (use_mouse)
            mouse_active();

        if (CurrentAlarm()->sec > 0)
        {
            mySignal(SIGALRM, SigAlarm);
            alarm(CurrentAlarm()->sec);
        }

        mySignal(SIGWINCH, resize_hook);

        if (activeImage && displayImage && GetCurrentTab()->GetCurrentBuffer()->img &&
            !GetCurrentTab()->GetCurrentBuffer()->image_loaded)
        {
            do
            {

                if (need_resize_screen())
                    resize_screen();

                loadImage(GetCurrentTab()->GetCurrentBuffer(), IMG_FLAG_NEXT);
            } while (sleep_till_anykey(1, 0) <= 0);
        }
        else
        // ここで入力をブロックする
        {
            do
            {
                if (need_resize_screen())
                    resize_screen();
            } while (sleep_till_anykey(1, 0) <= 0);
        }
        auto c = getch();

        if (CurrentAlarm()->sec > 0)
        {
            alarm(0);
        }

        if (use_mouse)
        {
            mouse_inactive();
        }

        DispatchKey(c);
    }
}

void w3mApp::_quitfm(int confirm)
{
    const char *ans = "y";
    if (checkDownloadList())
        /* FIXME: gettextize? */
        ans = inputChar("Download process retains. "
                        "Do you want to exit w3m? (y/n)");
    else if (confirm)
        /* FIXME: gettextize? */
        ans = inputChar("Do you want to exit w3m? (y/n)");
    if (!(ans && TOLOWER(*ans) == 'y'))
    {
        displayCurrentbuf(B_NORMAL);
        return;
    }

    term_title(""); /* XXX */

    if (activeImage)
        termImage();

    fmTerm();

    save_cookies();
    if (UseHistory && SaveURLHist)
        saveHistory(URLHist, URLHistSize);

    exit(0);
}

static bool domain_match(const char *pat, const char *domain)
{
    if (domain == NULL)
        return 0;
    if (*pat == '.')
        pat++;
    for (;;)
    {
        if (!strcasecmp(pat, domain))
            return 1;
        domain = strchr(domain, '.');
        if (domain == NULL)
            return 0;
        domain++;
    }
}

bool w3mApp::check_no_proxy(std::string_view domain)
{
    TextListItem *tl;
    int ret = 0;
    MySignalHandler prevtrap = NULL;

    if (this->NO_proxy_domains == NULL || this->NO_proxy_domains->nitem == 0 ||
        domain == NULL)
        return 0;
    for (tl = this->NO_proxy_domains->first; tl != NULL; tl = tl->next)
    {
        if (domain_match(tl->ptr, domain.data()))
            return 1;
    }
    if (!NOproxy_netaddr)
    {
        return 0;
    }
    /* 
     * to check noproxy by network addr
     */

    auto success = TrapJmp([&]() {
        {
#ifndef INET6
            struct hostent *he;
            int n;
            unsigned char **h_addr_list;
            char addr[4 * 16], buf[5];

            he = gethostbyname(domain);
            if (!he)
            {
                ret = 0;
                goto end;
            }
            for (h_addr_list = (unsigned char **)he->h_addr_list; *h_addr_list;
                 h_addr_list++)
            {
                sprintf(addr, "%d", h_addr_list[0][0]);
                for (n = 1; n < he->h_length; n++)
                {
                    sprintf(buf, ".%d", h_addr_list[0][n]);
                    addr->Push(buf);
                }
                for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next)
                {
                    if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0)
                    {
                        ret = 1;
                        goto end;
                    }
                }
            }
#else  /* INET6 */
            int error;
            struct addrinfo hints;
            struct addrinfo *res, *res0;
            char addr[4 * 16];
            int *af;

            for (af = ai_family_order_table[DNS_order];; af++)
            {
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = *af;
                error = getaddrinfo(domain.data(), NULL, &hints, &res0);
                if (error)
                {
                    if (*af == PF_UNSPEC)
                    {
                        break;
                    }
                    /* try next */
                    continue;
                }
                for (res = res0; res != NULL; res = res->ai_next)
                {
                    switch (res->ai_family)
                    {
                    case AF_INET:
                        inet_ntop(AF_INET,
                                  &((struct sockaddr_in *)res->ai_addr)->sin_addr,
                                  addr, sizeof(addr));
                        break;
                    case AF_INET6:
                        inet_ntop(AF_INET6,
                                  &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, addr, sizeof(addr));
                        break;
                    default:
                        /* unknown */
                        continue;
                    }
                    for (tl = this->NO_proxy_domains->first; tl != NULL; tl = tl->next)
                    {
                        if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0)
                        {
                            freeaddrinfo(res0);
                            ret = 1;
                            return true;
                        }
                    }
                }
                freeaddrinfo(res0);
                if (*af == PF_UNSPEC)
                {
                    break;
                }
            }
#endif /* INET6 */
        }

        return true;
    });

    if (!success)
    {
        ret = 0;
    }

    return ret;
}

bool w3mApp::UseProxy(const URL &url)
{
    if (!this->use_proxy)
    {
        return false;
    }
    if (url.scheme == SCM_HTTPS)
    {
        if (this->HTTPS_proxy.empty())
        {
            return false;
        }
    }
    else if (url.scheme == SCM_HTTP)
    {
        if (this->HTTP_proxy.empty())
        {
            return false;
        }
    }
    else
    {
        assert(false);
        return false;
    }

    if (url.host.empty())
    {
        return false;
    }
    if (check_no_proxy(url.host))
    {
        return false;
    }

    return true;
}
