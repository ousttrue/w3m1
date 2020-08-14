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
#include "transport/loader.h"
#include "transport/istream.h"
#include "transport/cookie.h"
#include "frontend/mouse.h"
#include "frontend/lineinput.h"
#include "frontend/terms.h"
#include "frontend/event.h"
#include "frontend/tabbar.h"
#include "frontend/display.h"
#include "frontend/terms.h"

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
    s_need_resize_screen = FALSE;
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
        return FALSE;
    for (d = FirstDL; d != NULL; d = d->next)
    {
        if (d->running && !lstat(d->lock, &st))
            return TRUE;
    }
    return FALSE;
}

static int s_add_download_list = FALSE;
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
    d->running = TRUE;
    d->err = 0;
    d->next = NULL;
    d->prev = LastDL;
    if (LastDL)
        LastDL->next = d;
    else
        FirstDL = d;
    LastDL = d;
    set_add_download_list(TRUE);
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
            d->running = FALSE;
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
    return loadHTMLString(src);
}

//
//
//

// TODO
extern char fmInitialized;
#define GC_WARN_KEEP_MAX (20)
static void wrap_GC_warn_proc(char *msg, GC_word arg)
{
    if (fmInitialized)
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
    free_ssl_ctx();
    
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
        override_content_type = TRUE;
    hs->Push(": ");
    if (*(++p))
    {                    /* not null header */
        SKIP_BLANKS(&p); /* skip white spaces */
        hs->Push(p);
    }
    hs->Push("\r\n");
    return hs->ptr;
}

int w3mApp::Main(int argc, char **argv)
{
    // int i;
    // char **load_argv;
    // FormList *request;
    // int load_argc = 0;

    // Str err_msg;

    // BookmarkFile = NULL;
    // config_file = NULL;

    /* argument search 1 */
    bool show_params_p = false;
    for (int i = 1; i < argc; i++)
    {
        if (*argv[i] == '-')
        {
            if (!strcmp("-config", argv[i]))
            {
                argv[i] = "-dummy";
                if (++i >= argc)
                    usage(show_params_p);
                config_file = argv[i];
                argv[i] = "-dummy";
            }
            else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]))
            {
                help(show_params_p);
            }
            else if (!strcmp("-V", argv[i]) || !strcmp("-version", argv[i]))
            {
                fversion(stdout);
                exit(0);
            }
        }
    }

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

    // if (!non_null(HTTPS_proxy) &&
    //     ((p = getenv("HTTPS_PROXY")) ||
    //      (p = getenv("https_proxy")) || (p = getenv("HTTPS_proxy"))))
    //     HTTPS_proxy = p;
    // if (HTTPS_proxy == NULL && non_null(HTTP_proxy))
    //     HTTPS_proxy = HTTP_proxy;

    // if (!non_null(FTP_proxy) &&
    //     ((p = getenv("FTP_PROXY")) ||
    //      (p = getenv("ftp_proxy")) || (p = getenv("FTP_proxy"))))
    //     FTP_proxy = p;
    // if (!non_null(NO_proxy) &&
    //     ((p = getenv("NO_PROXY")) ||
    //      (p = getenv("no_proxy")) || (p = getenv("NO_proxy"))))
    //     NO_proxy = p;

    // if (!non_null(NNTP_server) && (p = getenv("NNTPSERVER")) != NULL)
    //     NNTP_server = p;
    // if (!non_null(NNTP_mode) && (p = getenv("NNTPMODE")) != NULL)
    //     NNTP_mode = p;

    // if (!non_null(Editor) && (p = getenv("EDITOR")) != NULL)
    //     Editor = p;
    // if (!non_null(Mailer) && (p = getenv("MAILER")) != NULL)
    //     Mailer = p;

    /* argument search 2 */
    auto auto_detect = WcOption.auto_detect;
    std::string default_type;
    std::string post_file;
    auto search_header = false;
    auto visual_start = false;
    auto open_new_tab = false;
    auto load_bookmark = false;
    std::string line_str;
    std::vector<std::string> load_argv;
    for (int i = 1; i < argc;)
    {
        if (*argv[i] == '-')
        {
            if (!strcmp("-t", argv[i]))
            {
                if (++i >= argc)
                    usage(show_params_p);
                if (atoi(argv[i]) > 0)
                    Tabstop = atoi(argv[i]);
            }
            else if (!strcmp("-r", argv[i]))
            {
                ShowEffect = false;
            }
            else if (!strcmp("-l", argv[i]))
            {
                if (++i >= argc)
                    usage(show_params_p);
                if (atoi(argv[i]) > 0)
                    PagerMax = atoi(argv[i]);
            }
            else if (!strcmp("-s", argv[i]))
            {
                DisplayCharset = WC_CES_SHIFT_JIS;
            }
            else if (!strcmp("-j", argv[i]))
            {
                DisplayCharset = WC_CES_ISO_2022_JP;
            }
            else if (!strcmp("-e", argv[i]))
            {
                DisplayCharset = WC_CES_EUC_JP;
            }
            else if (!strncmp("-I", argv[i], 2))
            {
                const char *p;
                if (argv[i][2] != '\0')
                    p = argv[i] + 2;
                else
                {
                    if (++i >= argc)
                        usage(show_params_p);
                    p = argv[i];
                }
                DocumentCharset = wc_guess_charset_short(p, DocumentCharset);
                WcOption.auto_detect = WC_OPT_DETECT_OFF;
                UseContentCharset = FALSE;
            }
            else if (!strncmp("-O", argv[i], 2))
            {
                const char *p;
                if (argv[i][2] != '\0')
                    p = argv[i] + 2;
                else
                {
                    if (++i >= argc)
                        usage(show_params_p);
                    p = argv[i];
                }
                DisplayCharset = wc_guess_charset_short(p, DisplayCharset);
            }
            else if (!strcmp("-graph", argv[i]))
            {
                UseGraphicChar = GRAPHIC_CHAR_DEC;
            }
            else if (!strcmp("-no-graph", argv[i]))
            {
                UseGraphicChar = GRAPHIC_CHAR_ASCII;
            }
            else if (!strcmp("-T", argv[i]))
            {
                if (++i >= argc)
                    usage(show_params_p);
                DefaultType = default_type = argv[i];
            }
            else if (!strcmp("-m", argv[i]))
            {
                SearchHeader = search_header = TRUE;
            }
            else if (!strcmp("-v", argv[i]))
            {
                visual_start = TRUE;
            }
            else if (!strcmp("-N", argv[i]))
            {
                open_new_tab = TRUE;
            }
            else if (!strcmp("-M", argv[i]))
            {
                useColor = FALSE;
            }
            else if (!strcmp("-B", argv[i]))
            {
                load_bookmark = TRUE;
            }
            else if (!strcmp("-bookmark", argv[i]))
            {
                if (++i >= argc)
                    usage(show_params_p);
                BookmarkFile = argv[i];
                if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/')
                {
                    Str tmp = Strnew(CurrentDir);
                    if (tmp->Back() != '/')
                        tmp->Push('/');
                    tmp->Push(BookmarkFile);
                    BookmarkFile = cleanupName(tmp->c_str());
                }
            }
            else if (!strcmp("-F", argv[i]))
            {
                RenderFrame = TRUE;
            }
            else if (!strcmp("-W", argv[i]))
            {
                WrapDefault = !WrapDefault;
            }
            else if (!strcmp("-dump", argv[i]))
            {
                w3m_dump = DUMP_BUFFER;
            }
            else if (!strcmp("-dump_source", argv[i]))
            {
                w3m_dump = DUMP_SOURCE;
            }
            else if (!strcmp("-dump_head", argv[i]))
            {
                w3m_dump = DUMP_HEAD;
            }
            else if (!strcmp("-dump_both", argv[i]))
            {
                w3m_dump = (DUMP_HEAD | DUMP_SOURCE);
            }
            else if (!strcmp("-dump_extra", argv[i]))
            {
                w3m_dump = (DUMP_HEAD | DUMP_SOURCE | DUMP_EXTRA);
            }
            else if (!strcmp("-halfdump", argv[i]))
            {
                w3m_dump = DUMP_HALFDUMP;
            }
            else if (!strcmp("-backend", argv[i]))
            {
                w3m_backend = TRUE;
            }
            else if (!strcmp("-backend_batch", argv[i]))
            {
                w3m_backend = TRUE;
                if (++i >= argc)
                    usage(show_params_p);
                if (!backend_batch_commands)
                    backend_batch_commands = newTextList();
                pushText(backend_batch_commands, argv[i]);
            }
            else if (!strcmp("-cols", argv[i]))
            {
                if (++i >= argc)
                    usage(show_params_p);
                COLS = atoi(argv[i]);
                if (COLS > MAXIMUM_COLS)
                {
                    COLS = MAXIMUM_COLS;
                }
            }
            else if (!strcmp("-ppc", argv[i]))
            {
                double ppc;
                if (++i >= argc)
                    usage(show_params_p);
                ppc = atof(argv[i]);
                if (ppc >= MINIMUM_PIXEL_PER_CHAR &&
                    ppc <= MAXIMUM_PIXEL_PER_CHAR)
                {
                    pixel_per_char = ppc;
                    set_pixel_per_char = TRUE;
                }
            }
            else if (!strcmp("-ppl", argv[i]))
            {
                double ppc;
                if (++i >= argc)
                    usage(show_params_p);
                ppc = atof(argv[i]);
                if (ppc >= MINIMUM_PIXEL_PER_CHAR &&
                    ppc <= MAXIMUM_PIXEL_PER_CHAR * 2)
                {
                    pixel_per_line = ppc;
                    set_pixel_per_line = TRUE;
                }
            }
            else if (!strcmp("-num", argv[i]))
            {
                showLineNum = TRUE;
            }
            else if (!strcmp("-no-proxy", argv[i]))
            {
                use_proxy = FALSE;
            }
            else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i]))
            {
                set_param_option(Sprintf("dns_order=%c", argv[i][1])->c_str());
            }
            else if (!strcmp("-post", argv[i]))
            {
                if (++i >= argc)
                    usage(show_params_p);
                post_file = argv[i];
            }
            else if (!strcmp("-header", argv[i]))
            {
                if (++i >= argc)
                    usage(show_params_p);
                auto hs = make_optional_header_string(argv[i]);
                if (hs.size())
                {
                    header_string += hs;
                }
                while (argv[i][0])
                {
                    argv[i][0] = '\0';
                    argv[i]++;
                }
            }
            else if (!strcmp("-no-mouse", argv[i]))
            {
                use_mouse = FALSE;
            }
            else if (!strcmp("-no-cookie", argv[i]))
            {
                use_cookie = FALSE;
                accept_cookie = FALSE;
            }
            else if (!strcmp("-cookie", argv[i]))
            {
                use_cookie = TRUE;
                accept_cookie = TRUE;
            }
            else if (!strcmp("-S", argv[i]))
            {
                squeezeBlankLine = TRUE;
            }
            else if (!strcmp("-X", argv[i]))
            {
                Do_not_use_ti_te = TRUE;
            }
            else if (!strcmp("-title", argv[i]))
            {
                displayTitleTerm = getenv("TERM");
            }
            else if (!strncmp("-title=", argv[i], 7))
            {
                displayTitleTerm = argv[i] + 7;
            }
            else if (!strcmp("-o", argv[i]) ||
                     !strcmp("-show-option", argv[i]))
            {
                if (!strcmp("-show-option", argv[i]) || ++i >= argc ||
                    !strcmp(argv[i], "?"))
                {
                    show_params(stdout);
                    exit(0);
                }
                if (!set_param_option(argv[i]))
                {
                    /* option set failed */
                    /* FIXME: gettextize? */
                    fprintf(stderr, "%s: bad option\n", argv[i]);
                    show_params_p = 1;
                    usage(show_params_p);
                }
            }
            else if (!strcmp("-dummy", argv[i]))
            {
                /* do nothing */
            }
            else if (!strcmp("-debug", argv[i]))
            {
                w3m_debug = TRUE;
            }
            else if (!strcmp("-reqlog", argv[i]))
            {
                w3m_reqlog = rcFile("request.log");
            }
            else
            {
                usage(show_params_p);
            }
        }
        else if (*argv[i] == '+')
        {
            line_str = argv[i] + 1;
        }
        else
        {
            // URL LIST
            load_argv.push_back(argv[i]);
        }
        i++;
    }

    //
    //
    //

    ClearCurrentKey();
    if (BookmarkFile.empty())
    {
        BookmarkFile = rcFile(BOOKMARK);
    }

    if (!isatty(1) && !w3m_dump)
    {
        /* redirected output */
        w3m_dump = DUMP_BUFFER;
    }
    if (w3m_dump)
    {
        if (COLS == 0)
            COLS = DEFAULT_COLS;
    }

    if (w3m_dump)
    {
        if ((w3m_dump & DUMP_HALFDUMP) && displayImage)
        {
            activeImage = TRUE;
        }

        mySignal(SIGINT, SIG_IGN);
    }
    else if (w3m_backend)
    {
        backend();
    }
    else
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
    auto err_msg = Strnew();
    BufferPtr newbuf = NULL;
    if (load_argv.empty())
    {
        /* no URL specified */
        char *p = nullptr;
        if (!isatty(0))
        {
            // not terminal as pipe
            auto redin = newFileStream(fdopen(dup(0), "rb"), pclose);
            newbuf = openGeneralPagerBuffer(redin);
            dup2(1, 0);
        }
        else if (load_bookmark)
        {
            newbuf = loadGeneralFile(URL::ParsePath(BookmarkFile), NULL, HttpReferrerPolicy::NoReferer, RG_NONE, NULL);
            if (!newbuf)
            {
                err_msg->Push("w3m: Can't load bookmark.\n");
            }
        }
        else if (visual_start)
        {
            /* FIXME: gettextize? */
            Str s_page;
            s_page =
                Strnew("<title>W3M startup page</title><center><b>Welcome to ");
            s_page->Push("<a href='http://w3m.sourceforge.net/'>");
            Strcat_m_charp(s_page,
                           "w3m</a>!<p><p>This is w3m version ",
                           w3m_version,
                           "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
                           NULL);
            newbuf = loadHTMLString(s_page);
            if (newbuf == NULL)
                err_msg->Push("w3m: Can't load string.\n");
            else
                newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
        }
        else if ((p = getenv("HTTP_HOME")) || (p = getenv("WWW_HOME")))
        {
            newbuf = loadGeneralFile(URL::Parse(p), NULL, HttpReferrerPolicy::NoReferer, RG_NONE, NULL);
            if (newbuf == NULL)
                err_msg->Push(Sprintf("w3m: Can't load %s.\n", p));
            else
                pushHashHist(URLHist, newbuf->currentURL.ToStr()->c_str());
        }
        else
        {
            if (fmInitialized)
                fmTerm();
            usage(show_params_p);
        }
        if (newbuf == NULL)
        {
            if (fmInitialized)
                fmTerm();
            if (err_msg->Size())
                fprintf(stderr, "%s", err_msg->c_str());
            exit(2);
        }
    }
    else
    {
        bool isFirst = true;
        for (auto &arg : load_argv)
        {
            SearchHeader = search_header;
            DefaultType = default_type;

            FormList *request = nullptr;
            if (w3m_dump == DUMP_HEAD)
            {
                auto request = New(FormList);
                request->method = FORM_METHOD_HEAD;
                newbuf = loadGeneralFile(URL::Parse(arg), NULL, HttpReferrerPolicy::NoReferer, RG_NONE, request);
            }
            else
            {
                if (post_file.size() && isFirst)
                {
                    // POST
                    isFirst = false;
                    FILE *fp;
                    if (post_file == "-")
                    {
                        // post stdin
                        fp = stdin;
                    }
                    else
                    {
                        fp = fopen(post_file.c_str(), "r");
                    }
                    if (fp == NULL)
                    {
                        /* FIXME: gettextize? */
                        err_msg->Push(
                            Sprintf("w3m: Can't open %s.\n", post_file));
                        continue;
                    }

                    auto body = Strfgetall(fp);
                    if (fp != stdin)
                        fclose(fp);
                    auto request = newFormList(NULL, "post", NULL, NULL, NULL, NULL, NULL);
                    request->body = const_cast<char *>(body->c_str());
                    request->boundary = NULL;
                    request->length = body->Size();
                    newbuf = loadGeneralFile(URL::Parse(arg), NULL, HttpReferrerPolicy::NoReferer, RG_NONE, request);
                }
                else
                {
                    // GET
                    newbuf = loadGeneralFile(URL::Parse(arg), NULL, HttpReferrerPolicy::NoReferer, RG_NONE, nullptr);
                }
            }
            if (!newbuf)
            {
                /* FIXME: gettextize? */
                err_msg->Push(Sprintf("w3m: Can't load %s.\n", arg.c_str()));
                continue;
            }

            switch (newbuf->real_scheme)
            {
            case SCM_MAILTO:
                break;
            case SCM_LOCAL:
            case SCM_LOCAL_CGI:
                unshiftHist(LoadHist, conv_from_system(arg));
            default:
                pushHashHist(URLHist, newbuf->currentURL.ToStr()->c_str());
                break;
            }

            if (newbuf->pagerSource ||
                (newbuf->real_scheme == SCM_LOCAL && newbuf->header_source.size() &&
                 newbuf->currentURL.path != "-"))
            {
                newbuf->search_header = search_header;
            }

            //
            // set newbuf to tab
            //
            if (open_new_tab)
            {
                auto tab = CreateTabSetCurrent();
                tab->Push(newbuf);
            }
            else
            {
                GetCurrentTab()->Push(newbuf);
            }

            if (!w3m_dump || w3m_dump == DUMP_BUFFER)
            {
                // frame ?
                if (GetCurrentTab()->GetCurrentBuffer()->frameset != NULL && RenderFrame)
                    rFrame(&w3mApp::Instance());
            }

            if (w3m_dump)
            {
                do_dump(this, GetCurrentTab()->GetCurrentBuffer());
            }
            else
            {
                GetCurrentTab()->SetCurrentBuffer(newbuf);
            }
        }
    }

    if (w3m_dump)
    {
        // dump
        if (err_msg->Size())
        {
            fprintf(stderr, "%s", err_msg->c_str());
        }
        save_cookies();
        exit(0);
    }

    if (add_download_list())
    {
        set_add_download_list(FALSE);
        SetCurrentTab(GetLastTab());
        if (!GetCurrentTab()->GetFirstBuffer())
        {
            auto buf = newBuffer(INIT_BUFFER_WIDTH());
            buf->bufferprop = BP_INTERNAL | BP_NO_URL;
            buf->buffername = DOWNLOAD_LIST_TITLE;
            GetCurrentTab()->Push(buf);
        }
        else
        {
            GetCurrentTab()->SetCurrentBuffer(GetCurrentTab()->GetFirstBuffer());
        }
        ldDL(&w3mApp::Instance());
    }
    else
    {
        SetCurrentTab(GetFirstTab());
    }

    if (!GetFirstTab() || !GetCurrentTab()->GetFirstBuffer())
    {
        if (fmInitialized)
            fmTerm();
        if (err_msg->Size())
            fprintf(stderr, "%s", err_msg->c_str());
        exit(2);
    }

    if (err_msg->Size())
    {
        disp_message_nsec(err_msg->c_str(), FALSE, 1, TRUE, FALSE);
    }

    SearchHeader = FALSE;
    DefaultType.clear();
    UseContentCharset = TRUE;
    WcOption.auto_detect = auto_detect;

    GetCurrentTab()->SetCurrentBuffer(GetCurrentTab()->GetFirstBuffer());
    displayCurrentbuf(B_FORCE_REDRAW);
    if (line_str.size())
    {
        _goLine(line_str);
    }

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
            set_add_download_list(FALSE);
            ldDL(&w3mApp::Instance());
        }

        auto tab = GetCurrentTab();
        auto buf = tab->GetCurrentBuffer();
        if (buf->submit)
        {
            auto a = buf->submit;
            buf->submit = NULL;
            buf->Goto(a->start);
            _followForm(TRUE);
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
                    CurrentCmdData = (char *)CurrentAlarm()->data;
                    CurrentAlarm()->cmd(&w3mApp::Instance());
                    CurrentCmdData = NULL;
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
    char *ans = "y";

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
