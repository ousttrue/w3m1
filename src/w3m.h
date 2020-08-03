#pragma once
#include <string>
#include <string_view>
#include "config.h"
#include "version.h"
#include "ces.h"
#include "conv.h"
#include "ucs.h"
#include "utf8.h"
#include "symbol.h"
#include "transport/url.h"
#include "frontend/buffer.h"

struct TextList;
struct Hist;

enum DumpFlags
{
    DUMP_NONE = 0x00,
    DUMP_BUFFER = 0x01,
    DUMP_HEAD = 0x02,
    DUMP_SOURCE = 0x04,
    DUMP_EXTRA = 0x08,
    DUMP_HALFDUMP = 0x10,
    DUMP_FRAME = 0x20,
};

class w3mApp
{
    w3mApp();
    ~w3mApp();

    w3mApp(const w3mApp &) = delete;
    w3mApp &operator=(const w3mApp &) = delete;

public:
    // const
    static inline const std::string_view w3m_version = CURRENT_VERSION;
    static inline const std::string_view DOWNLOAD_LIST_TITLE = "Download List Panel";

    // globals
    int CurrentPid = -1;
    std::string CurrentDir;
    TextList *fileToDelete = nullptr;

    // files settings
    std::string config_file;
    std::string BookmarkFile;

    // defalt content-type
    std::string DefaultType;

    // http
    bool override_content_type = false;
    std::string header_string;
    bool use_cookie = false;
    bool accept_cookie = false;

    // character encoding
    bool FollowLocale = true;
    CharacterEncodingScheme InnerCharset = WC_CES_WTF; /* Don't change */
    CharacterEncodingScheme DisplayCharset = DISPLAY_CHARSET;
    CharacterEncodingScheme DocumentCharset = DOCUMENT_CHARSET;
    CharacterEncodingScheme SystemCharset = SYSTEM_CHARSET;
    CharacterEncodingScheme BookmarkCharset = SYSTEM_CHARSET;
    bool UseContentCharset = true;
    GraphicCharTypes UseGraphicChar = GRAPHIC_CHAR_CHARSET;
    bool ExtHalfdump = false;

    // proxy
    bool use_proxy = true;
    TextList *NO_proxy_domains = nullptr;
    std::string HTTP_proxy;
    URL HTTP_proxy_parsed;
    std::string HTTPS_proxy;
    URL HTTPS_proxy_parsed;
    std::string FTP_proxy;
    URL FTP_proxy_parsed;

    // frontend
    bool FoldLine = false;
    bool showLineNum = false;

    int Tabstop = 8;
    bool ShowEffect = true;
    // Maximum line kept as pager
    int PagerMax = 10000;
    bool SearchHeader = false;
    bool useColor = true;
    bool RenderFrame = false;
    bool WrapDefault = false;
    bool use_mouse = true;
    bool squeezeBlankLine = false;
    bool Do_not_use_ti_te = false;
    std::string displayTitleTerm;

    // image
    double pixel_per_char = 7.0;
    bool set_pixel_per_char = false;
    double pixel_per_line = 14.0;
    bool set_pixel_per_line = false;
    double image_scale = 100;
    bool activeImage = false;
    bool displayImage = true;

    // hittory
    bool UseHistory = true;
    bool SaveURLHist = true;
    Hist *LoadHist = nullptr;
    Hist *SaveHist = nullptr;
    Hist *URLHist = nullptr;
    int URLHistSize = 100;
    Hist *ShellHist = nullptr;

    // lineeditor
    Hist *TextHist = nullptr;

    // backend mode
    bool w3m_backend = false;
    TextList *backend_batch_commands = nullptr;

    // debug & logging
    bool w3m_debug = false;
    DumpFlags w3m_dump = DUMP_NONE;
    bool w3m_halfload = false;
    std::string w3m_reqlog;

public:
    // 使いわないのが目標
    static w3mApp &Instance()
    {
        static w3mApp w3m;
        return w3m;
    }

    int Main(int argc, char **argv);

    void _quitfm(int confirm);

private:
    void mainloop();
    std::string make_optional_header_string(const char *s);
};

// keymap などで起動される関数。
// この関数内で、global 変数 Currentbuf 等へのアクセスを避ける( w3m から取れるようにする)
using Command = void (*)(w3mApp *w3m);

inline Str Str_conv_to_system(Str x)
{
    return wc_Str_conv_strict(x, w3mApp::Instance().InnerCharset, w3mApp::Instance().SystemCharset);
}
inline char *conv_to_system(const char *x)
{
    return wc_conv_strict(x, w3mApp::Instance().InnerCharset, w3mApp::Instance().SystemCharset)->ptr;
}
inline Str Str_conv_from_system(Str x)
{
    return wc_Str_conv(x, w3mApp::Instance().SystemCharset, w3mApp::Instance().InnerCharset);
}
inline char *conv_from_system(std::string_view x)
{
    return wc_conv(x.data(), w3mApp::Instance().SystemCharset, w3mApp::Instance().InnerCharset)->ptr;
}
inline Str Str_conv_to_halfdump(Str x)
{
    return w3mApp::Instance().ExtHalfdump ? wc_Str_conv((x), w3mApp::Instance().InnerCharset, w3mApp::Instance().DisplayCharset) : (x);
}
inline int RELATIVE_WIDTH(int w)
{
    return (w >= 0) ? (int)(w / w3mApp::Instance().pixel_per_char) : w;
}

BufferPtr DownloadListBuffer(w3mApp *w3m);

void resize_screen();
int need_resize_screen();
