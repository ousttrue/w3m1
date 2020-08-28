#include <plog/Log.h>
#include <signal.h>
#include "config.h"
#include <unistd.h>
#include <signal.h>
#include <asio.hpp>
#include "indep.h"
#include "textlist.h"
#include "rc.h"
#include "register_commands.h"
#include "history.h"
#include "download_list.h"
#include "myctype.h"
#include "public.h"
#include "html/image.h"
#include "frontend/lineinput.h"
#include "stream/cookie.h"
#include "frontend/mouse.h"
#include "frontend/event.h"
#include "frontend/tabbar.h"
#include "frontend/display.h"
#include "frontend/terminal.h"
#include "frontend/screen.h"

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

    Screen::Instance().Setup();
}

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
                Terminal::sleep_till_anykey(1, 1);
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
}

w3mApp::~w3mApp()
{
    LOGI << "quit...";

    stopDownload();

    for (auto &f : fileToDelete)
    {
        unlink(f.c_str());
    }
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
    //
    //

    if (BookmarkFile.empty())
    {
        BookmarkFile = rcFile(BOOKMARK);
    }

    {
        // terminal
        fmInit();

        mySignal(SIGWINCH, resize_hook);

        Screen::Instance().Setup();
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

    mainloop2();

    return 0;
}

// TODO:
extern char *CurrentCmdData;

class Asio
{
    asio::io_context context;
    asio::posix::stream_descriptor tty;
    char byteArray[1];
    asio::signal_set signals;

public:
    Asio()
        : tty(context, Terminal::tty()), signals(context, SIGINT, SIGTERM, SIGWINCH)
    {
    }

    void Quit()
    {
        signals.cancel();
    }

    void ReadTty()
    {
        auto buffer = asio::buffer(byteArray);
        auto callback = std::bind(&Asio::OnReadTty, this, std::placeholders::_1, std::placeholders::_2);
        tty.async_read_some(buffer, callback);
    }

    void OnReadTty(asio::error_code ec, std::size_t n)
    {
        if (n == 0)
        {
            LOGE << "error. read zero";
            return;
        }

        // getch
        auto c = byteArray[0];

        // dispatch
        auto tab = GetCurrentTab();
        auto buf = GetCurrentBuffer();

        DispatchKey(c);

        if (w3mApp::Instance().IsQuit())
        {
            // quit
            Quit();
            return;
        }

        // auto currentTab = GetCurrentTab();
        // auto currentBuf = GetCurrentBuffer();
        // if (s_need_resize_screen)
        // {
        //     displayBuffer(currentBuf, B_FORCE_REDRAW);
        // }
        // else if (currentTab != tab)
        // {
        //     // different tab
        //     displayBuffer(currentBuf, B_FORCE_REDRAW);
        // }
        // else if (currentBuf != buf)
        // {
        //     // different buf
        //     displayBuffer(currentBuf, B_FORCE_REDRAW);
        // }
        // else
        // {
        //     // same buf
        //     auto drawType = currentBuf->Update();
        //     displayBuffer(currentBuf, drawType);
        // }
        displayBuffer();

        // next read
        ReadTty();
    }

    void OnSignal(const asio::error_code &error, int signal)
    {
        LOGI << "signal: " << signal;
    }

    void Signal()
    {
        // Construct a signal set registered for process termination.
        auto callback = std::bind(&Asio::OnSignal, this, std::placeholders::_1, std::placeholders::_2);
        signals.async_wait(callback);
    }

    void Run()
    {
        context.run();
    }
};

void w3mApp::mainloop2()
{
    // first draw
    displayBuffer();
    
    Asio asio;
    asio.ReadTty();
    asio.Signal();

    asio.Run();
}

void w3mApp::mainloop()
{
    for (;;)
    {
        if (add_download_list())
        {
            set_add_download_list(false);
            ldDL(&w3mApp::Instance(), {});
        }

        auto tab = GetCurrentTab();
        auto buf = GetCurrentBuffer();
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
                    CurrentAlarm()->cmd(&w3mApp::Instance(), {
                        data : (const char *)CurrentAlarm()->data
                    });
                    continue;
                }
            }
            else
                buf->event = NULL;
        }
        if (!buf->event)
            SetCurrentAlarm(DefaultAlarm());

        DisableMouseAction();
        Terminal::mouse_on();

        if (CurrentAlarm()->sec > 0)
        {
            mySignal(SIGALRM, SigAlarm);
            alarm(CurrentAlarm()->sec);
        }

        mySignal(SIGWINCH, resize_hook);

        // if (activeImage && displayImage && GetCurrentBuffer()->img &&
        //     !GetCurrentBuffer()->image_loaded)
        // {
        //     do
        //     {

        //         if (need_resize_screen())
        //             resize_screen();

        //         ImageManager::Instance().loadImage(GetCurrentBuffer(), IMG_FLAG_NEXT);
        //     } while (Terminal::sleep_till_anykey(1, 0) <= 0);
        // }
        // else
        // ここで入力をブロックする
        {
            do
            {
                if (need_resize_screen())
                    resize_screen();
            } while (Terminal::sleep_till_anykey(1, 0) <= 0);
        }
        auto c = Terminal::getch();

        if (CurrentAlarm()->sec > 0)
        {
            alarm(0);
        }

        Terminal::mouse_off();

        DispatchKey(c);
    }
}

void w3mApp::Quit(bool confirm)
{
    std::string_view ans = "y";
    if (checkDownloadList())
    {
        /* FIXME: gettextize? */
        ans = inputChar("Download process retains. "
                        "Do you want to exit w3m? (y/n)");
    }
    else if (confirm)
    {
        /* FIXME: gettextize? */
        ans = inputChar("Do you want to exit w3m? (y/n)");
    }

    if (ans.empty() || TOLOWER(ans[0]) != 'y')
    {
        // displayCurrentbuf(B_NORMAL);
        return;
    }

    //
    // quit
    //
    m_quit = true;

    fmTerm();

    if (UseHistory && SaveURLHist)
    {
        saveHistory(URLHist, URLHistSize);
    }
}
