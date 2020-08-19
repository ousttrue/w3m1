#pragma once
#include <string>
#include <string_view>
#include "config.h"
#include "version.h"
#include <wc.h>
#include "symbol.h"
#include "stream/url.h"
#include "frontend/buffer.h"

struct TextList;
struct Hist;
class Terminal;

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

enum DnsOrderTypes
{
    DNS_ORDER_UNSPEC = 0,
    DNS_ORDER_INET_INET6 = 1,
    DNS_ORDER_INET6_INET = 2,
    DNS_ORDER_INET_ONLY = 4,
    DNS_ORDER_INET6_ONLY = 6,
};

enum MailtoOption
{
    MAILTO_OPTIONS_IGNORE = 1,
    MAILTO_OPTIONS_USE_MAILTO_URL = 2,
};

enum DisplayInsDel
{
    DISPLAY_INS_DEL_SIMPLE = 0,
    DISPLAY_INS_DEL_NORMAL = 1,
    DISPLAY_INS_DEL_FONTIFY = 2,
};

enum DefaultUrlTypes
{
    DEFAULT_URL_EMPTY = 0,
    DEFAULT_URL_CURRENT = 1,
    DEFAULT_URL_LINK = 2,
};

enum AcceptBadCookieTypes
{
    ACCEPT_BAD_COOKIE_DISCARD = 0,
    ACCEPT_BAD_COOKIE_ACCEPT = 1,
    ACCEPT_BAD_COOKIE_ASK = 2,
};

class w3mApp
{
    w3mApp();
    ~w3mApp();

    w3mApp(const w3mApp &) = delete;
    w3mApp &operator=(const w3mApp &) = delete;

    Terminal *m_term = nullptr;

public:
    // const
    static inline const std::string_view w3m_version = CURRENT_VERSION;
    static inline const std::string_view DOWNLOAD_LIST_TITLE = "Download List Panel";

    // rc.cpp
    //
    // Display Settings
    //
    int Tabstop = 8;
    int IndentIncr = 4;
    double pixel_per_char = 7.0;
    double pixel_per_line = 14.0;
    bool RenderFrame = false;
    bool TargetSelf = false;
    int open_tab_blank = false;
    int open_tab_dl_list = false;
    bool displayLink = false;
    bool displayLinkNumber = false;
    bool DecodeURL = false;
    bool displayLineInfo = false;
    bool UseExternalDirBuffer = true;
    std::string DirBufferCommand = "file:///$LIB/dirlist.cgi"; // CGI_EXTENSION;
    bool UseDictCommand = false;
    std::string DictCommand = "file:///$LIB/w3mdict.cgi"; // CGI_EXTENSION;
    bool multicolList = false;
    GraphicCharTypes UseGraphicChar = GRAPHIC_CHAR_CHARSET;
    bool FoldTextarea = false;
    DisplayInsDel displayInsDel = DISPLAY_INS_DEL_NORMAL;
    bool ignore_null_img_alt = true;
    bool view_unseenobject = false;
    bool displayImage = true;
    int pseudoInlines = true;
    bool autoImage = true;
    int maxLoadImage = 4;
    bool useExtImageViewer = true;
    double image_scale = 100;
    std::string Imgdisplay = IMGDISPLAY;
    bool image_map_list = true;
    bool FoldLine = false;
    bool showLineNum = false;
    bool show_srch_str = true;
    bool label_topline = false;
    bool nextpage_topline = false;
    //
    //
    //

    // globals
    int CurrentPid = -1;
    std::string CurrentDir;
    TextList *fileToDelete = nullptr;
    bool fmInitialized = false;
    std::string document_root;
    std::string personal_document_root;
    std::string cgi_bin;
    std::string index_file;
    std::string CurrentCmdData;
    bool confirm_on_quit = true;
    bool use_mark = false;
    bool emacs_like_lineedit = false;
    bool vi_prec_num = false;
    bool retryAsHttp = true;
    std::string Editor = DEF_EDITOR;
    std::string Mailer = DEF_MAILER;
    MailtoOption MailtoOptions = MAILTO_OPTIONS_IGNORE;
    std::string ExtBrowser = DEF_EXT_BROWSER;
    std::string ExtBrowser2;
    std::string ExtBrowser3;
    bool BackgroundExtViewer = true;
    bool disable_secret_security_check = false;
    std::string passwd_file = PASSWD_FILE;
    std::string pre_form_file = PRE_FORM_FILE;
    std::string ftppasswd;
    bool ftppass_hostnamegen = true;
    bool do_download = false;
    std::string image_source;
    std::string UserAgent;
    bool NoSendReferer = false;
    std::string AcceptLang;
    std::string AcceptEncoding;
    std::string AcceptMedia;
    bool IgnoreCase = true;
    bool WrapSearch = false;
    DefaultUrlTypes DefaultURLString = DEFAULT_URL_EMPTY;
    bool MarkAllPages = false;
    std::string mailcap_files = USER_MAILCAP ", " SYS_MAILCAP;
    std::string mimetypes_files = USER_MIMETYPES ", " SYS_MIMETYPES;
    std::string urimethodmap_files = USER_URIMETHODMAP ", " SYS_URIMETHODMAP;
    bool SearchConv = true;
    bool SimplePreserveSpace = false;
    bool no_rc_dir = false;
    std::string rc_dir;
    bool reverse_mouse = false;
    bool relative_wheel_scroll = false;
    int fixed_wheel_scroll_count = 5;
    int relative_wheel_scroll_ratio = 30;
    std::string tmp_dir;
    bool default_use_cookie = true;
    bool show_cookie = true;
    AcceptBadCookieTypes accept_bad_cookie = ACCEPT_BAD_COOKIE_DISCARD;
    std::string cookie_reject_domains;
    std::string cookie_accept_domains;
    std::string cookie_avoid_wrong_number_of_dots;
    TextList *Cookie_reject_domains = nullptr;
    TextList *Cookie_accept_domains = nullptr;
    TextList *Cookie_avoid_wrong_number_of_dots_domains = nullptr;
    bool is_redisplay = false;
    bool clear_buffer = true;
    bool use_lessopen = false;
    std::string keymap_file = KEYMAP_FILE;
    int FollowRedirection = 10;

    // ssl
    bool ssl_verify_server = false;
    std::string ssl_cert_file;
    std::string ssl_key_file;
    std::string ssl_ca_path;
    std::string ssl_ca_file;
    bool ssl_path_modified = false;
    std::string ssl_forbid_method;

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
    char *NO_proxy = nullptr;
    bool NOproxy_netaddr = true;
    int DNS_order = DNS_ORDER_UNSPEC;
    bool NoCache = false;

    // frontend

    bool ShowEffect = true;
    bool PermitSaveToPipe = false;
    bool DecodeCTE = false;
    bool AutoUncompress = false;
    bool PreserveTimestamp = true;
    bool ArgvIsURL = false;
    bool MetaRefresh = false;
    bool QuietMessage = false;
    bool TrapSignal = true;
    int close_tab_back = false;

    bool useColor = true;
    int basic_color = 8;  /* don't change */
    int anchor_color = 4; /* blue  */
    int image_color = 2;  /* green */
    int form_color = 1;   /* red   */
    int bg_color = 8;     /* don't change */
    int mark_color = 6;   /* cyan */
    bool useActiveColor = false;
    int active_color = 6; /* cyan */
    bool useVisitedColor = false;
    int visited_color = 5; /* magenta  */

    // Maximum line kept as pager
    int PagerMax = 10000;
    bool SearchHeader = false;
    bool WrapDefault = false;
    bool use_mouse = true;
    bool squeezeBlankLine = false;
    bool Do_not_use_ti_te = false;
    std::string displayTitleTerm;

    // image
    bool set_pixel_per_char = false;
    bool set_pixel_per_line = false;
    bool activeImage = false;

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

public:
    // 最終的にはこれを使わず引数経由で得るようにする
    static w3mApp &Instance()
    {
        static w3mApp w3m;
        return w3m;
    }

    int Main(int argc, char **argv);

    void _quitfm(int confirm);

    Terminal *term()
    {
        return m_term;
    }

    bool UseProxy(const URL &url);

private:
    bool check_no_proxy(std::string_view domain);
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
