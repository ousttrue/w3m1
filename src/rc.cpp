#include "fm.h"
#include "urimethod.h"
#include "gc_helper.h"
#include "rc.h"

#include "symbol.h"
#include "myctype.h"
#include "indep.h"
#include "dispatcher.h"
#include "frontend/mouse.h"
#include "frontend/menu.h"
#include "html/image.h"
#include "html/html_processor.h"
#include "commands.h"
#include "file.h"
#include "frontend/display.h"
#include "frontend/buffer.h"
#include "stream/compression.h"
#include "stream/cookie.h"
#include "mime/mimetypes.h"
#include "mime/mailcap.h"
#include "html/parsetag.h"
#include "make_array.h"
#include "frontend/terms.h"
#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <assert.h>
#include <any>
#include <string>
#include <string_view>
#include "charset.h"

enum ParamTypes
{
    P_INT,
    P_SHORT,
    P_CHARINT,
    P_CHAR,
    P_STRING,
    P_SSLPATH,
    P_COLOR,
    P_CODE,
    P_PIXELS,
    P_NZINT,
    P_SCALE,
};

enum ParamInputType
{
    PI_TEXT,
    PI_ONOFF,
    PI_SEL_C,
    PI_CODE,
};

static int str_to_color(std::string_view value)
{
    if (value.empty())
        return 8; /* terminal */

    switch (TOLOWER(value[0]))
    {
    case '0':
        return 0; /* black */
    case '1':
    case 'r':
        return 1; /* red */
    case '2':
    case 'g':
        return 2; /* green */
    case '3':
    case 'y':
        return 3; /* yellow */
    case '4':
        return 4; /* blue */
    case '5':
    case 'm':
        return 5; /* magenta */
    case '6':
    case 'c':
        return 6; /* cyan */
    case '7':
    case 'w':
        return 7; /* white */
    case '8':
    case 't':
        return 8; /* terminal */
    case 'b':
        if (!strncasecmp(value.data(), "blu", 3))
            return 4; /* blue */
        else
            return 0; /* black */
    }
    return 8; /* terminal */
}

struct Param
{
    std::string_view name;
    ParamTypes type;
    ParamInputType inputtype;
    std::any value;

    std::string_view comment;
    void *select;

    Param(std::string_view n, ParamTypes t, ParamInputType it, int v, std::string_view c, void *s = nullptr)
        : name(n), type(t), inputtype(it), comment(c), select(s)
    {
        switch (type)
        {
        case P_INT:
        case P_NZINT:
            value = (int)v;
            break;
        case P_SHORT:
            value = (short)v;
            break;
        case P_CHARINT:
            value = (char)v;
            break;
        case P_CHAR:
            value = (char)v;
            break;
        case P_COLOR:
            value = (int)v;
            break;
        case P_CODE:
            value = (CharacterEncodingScheme)v;
            break;
        case P_PIXELS:
        case P_SCALE:
            assert(false);
            // value = (double)v;
            break;
        case P_STRING:
        case P_SSLPATH:
            // value = std::string(v);
            assert(false);
            break;
        }
    }

    Param(std::string_view n, ParamTypes t, ParamInputType it, CharacterEncodingScheme v, std::string_view c, void *s = nullptr)
        : name(n), type(t), inputtype(it), comment(c), select(s)
    {
        assert(t == P_CODE);
        value = v;
    }

    Param(std::string_view n, ParamTypes t, ParamInputType it, double v, std::string_view c, void *s = nullptr)
        : name(n), type(t), inputtype(it), comment(c), select(s)
    {
        switch (type)
        {
        case P_INT:
        case P_NZINT:
        case P_SHORT:
        case P_CHARINT:
        case P_CHAR:
        case P_COLOR:
        case P_CODE:
            assert(false);
            break;
        case P_PIXELS:
        case P_SCALE:
            value = (double)v;
            break;
        case P_STRING:
        case P_SSLPATH:
            // value = std::string(v);
            assert(false);
            break;
        }
    }
    Param(std::string_view n, ParamTypes t, ParamInputType it, char *v, std::string_view c, void *s = nullptr)
        : name(n), type(t), inputtype(it), comment(c), select(s)
    {
        switch (type)
        {
        case P_INT:
        case P_NZINT:
        case P_SHORT:
        case P_CHARINT:
        case P_CHAR:
        case P_COLOR:
        case P_CODE:
            assert(false);
            break;
        case P_PIXELS:
        case P_SCALE:
            assert(false);
            // value = (double)v;
            break;
        case P_STRING:
        case P_SSLPATH:
            value = v ? std::string(v) : "";
            break;
        }
    }
    Param(std::string_view n, ParamTypes t, ParamInputType it, const std::string &v, std::string_view c, void *s = nullptr)
        : name(n), type(t), inputtype(it), comment(c), select(s)
    {
        switch (type)
        {
        case P_INT:
        case P_NZINT:
        case P_SHORT:
        case P_CHARINT:
        case P_CHAR:
        case P_COLOR:
        case P_CODE:
            assert(false);
            break;
        case P_PIXELS:
        case P_SCALE:
            assert(false);
            // value = (double)v;
            break;
        case P_STRING:
        case P_SSLPATH:
            value = v;
            break;
        }
    }

    template <typename T>
    T Value() const
    {
        if (!value.has_value())
        {
            return {};
        }
        return std::any_cast<T>(value);
    }

    int Value(std::string_view src)
    {
        // double ppc;
        switch (this->type)
        {
        case P_INT:
            if (atoi(src.data()) >= 0)
            {
                this->value = (int)((this->inputtype == PI_ONOFF)
                                        ? str_to_bool(src.data(), std::any_cast<int>(this->value))
                                        : atoi(src.data()));
            }
            break;

        case P_NZINT:
            if (atoi(src.data()) > 0)
            {
                this->value = atoi(src.data());
            }
            break;

        case P_SHORT:
            this->value = (short)((this->inputtype == PI_ONOFF)
                                      ? str_to_bool(src.data(), this->Value<short>())
                                      : atoi(src.data()));

            break;

        case P_CHARINT:
            this->value = (char)((this->inputtype == PI_ONOFF)
                                     ? str_to_bool(src.data(), this->Value<char>())
                                     : atoi(src.data()));
            break;

        case P_CHAR:
            this->value = src[0];
            break;

        case P_STRING:
            this->value = std::string(src);
            break;

        case P_SSLPATH:
            if (src.size())
                this->value = std::string(rcFile(src.data()));
            else
                this->value = std::string("");
            ssl_path_modified = 1;
            break;

        case P_COLOR:
            this->value = str_to_color(src);
            break;

        case P_CODE:
            this->value = wc_guess_charset_short(src.data(), this->Value<CharacterEncodingScheme>());
            break;

        case P_PIXELS:
        {
            double ppc = atof(src.data());
            if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2)
            {
                this->value = ppc;
            }
            break;
        }

        case P_SCALE:
        {
            double ppc = atof(src.data());
            if (ppc >= 10 && ppc <= 1000)
            {
                this->value = ppc;
            }
            break;
        }
        }
        return 1;
    }

    Str to_str() const
    {
        switch (type)
        {
        case P_INT:
        case P_COLOR:
        case P_NZINT:
            return Sprintf("%d", std::any_cast<int>(value));
        case P_CODE:
            return Sprintf("%d", std::any_cast<CharacterEncodingScheme>(value));
        case P_SHORT:
            return Sprintf("%d", std::any_cast<short>(value));
        case P_CHARINT:
            return Sprintf("%d", std::any_cast<char>(value));
        case P_CHAR:
            return Sprintf("%c", std::any_cast<char>(value));
        case P_STRING:
        case P_SSLPATH:
            /*  SystemCharset -> InnerCharset */
            return Strnew(conv_from_system(std::any_cast<std::string>(value)));
        case P_PIXELS:
        case P_SCALE:
            return Sprintf("%g", std::any_cast<double>(value));
        }
        /* not reached */
        return NULL;
    }
};
std::unordered_map<std::string_view, Param *> RC_search_table;

/* FIXME: gettextize here */
#ifdef USE_M17N
static CharacterEncodingScheme OptionCharset = WC_CES_US_ASCII; /* FIXME: charset of source code */
static int OptionEncode = FALSE;
#endif

#define CMT_HELPER N_("External Viewer Setup")
#define CMT_TABSTOP N_("Tab width in characters")
#define CMT_INDENT_INCR N_("Indent for HTML rendering")
#define CMT_PIXEL_PER_CHAR N_("Number of pixels per character (4.0...32.0)")
#define CMT_PIXEL_PER_LINE N_("Number of pixels per line (4.0...64.0)")
#define CMT_PAGERLINE N_("Number of remembered lines when used as a pager")
#define CMT_HISTORY N_("Use URL history")
#define CMT_HISTSIZE N_("Number of remembered URL")
#define CMT_SAVEHIST N_("Save URL history")
#define CMT_FRAME N_("Render frames automatically")
#define CMT_ARGV_IS_URL N_("Treat argument without scheme as URL")
#define CMT_TSELF N_("Use _self as default target")
#define CMT_OPEN_TAB_BLANK N_("Open link on new tab if target is _blank or _new")
#define CMT_OPEN_TAB_DL_LIST N_("Open download list panel on new tab")
#define CMT_DISPLINK N_("Display link URL automatically")
#define CMT_DISPLINKNUMBER N_("Display link numbers")
#define CMT_DECODE_URL N_("Display decoded URL")
#define CMT_DISPLINEINFO N_("Display current line number")
#define CMT_DISP_IMAGE N_("Display inline images")
#define CMT_PSEUDO_INLINES N_("Display pseudo-ALTs for inline images with no ALT or TITLE string")
#ifdef USE_IMAGE
#define CMT_AUTO_IMAGE N_("Load inline images automatically")
#define CMT_MAX_LOAD_IMAGE N_("Maximum processes for parallel image loading")
#define CMT_EXT_IMAGE_VIEWER N_("Use external image viewer")
#define CMT_IMAGE_SCALE N_("Scale of image (%)")
#define CMT_IMGDISPLAY N_("External command to display image")
#define CMT_IMAGE_MAP_LIST N_("Use link list of image map")
#endif
#define CMT_MULTICOL N_("Display file names in multi-column format")
#define CMT_ALT_ENTITY N_("Use ASCII equivalents to display entities")
#define CMT_GRAPHIC_CHAR N_("Character type for border of table and menu")
#define CMT_FOLD_TEXTAREA N_("Fold lines in TEXTAREA")
#define CMT_DISP_INS_DEL N_("Display INS, DEL, S and STRIKE element")
#define CMT_COLOR N_("Display with color")
#define CMT_B_COLOR N_("Color of normal character")
#define CMT_A_COLOR N_("Color of anchor")
#define CMT_I_COLOR N_("Color of image link")
#define CMT_F_COLOR N_("Color of form")
#define CMT_ACTIVE_STYLE N_("Enable coloring of active link")
#define CMT_C_COLOR N_("Color of currently active link")
#define CMT_VISITED_ANCHOR N_("Use visited link color")
#define CMT_V_COLOR N_("Color of visited link")
#define CMT_BG_COLOR N_("Color of background")
#define CMT_MARK_COLOR N_("Color of mark")
#define CMT_USE_PROXY N_("Use proxy")
#define CMT_HTTP_PROXY N_("URL of HTTP proxy host")
#ifdef USE_SSL
#define CMT_HTTPS_PROXY N_("URL of HTTPS proxy host")
#endif /* USE_SSL */
#ifdef USE_GOPHER
#define CMT_GOPHER_PROXY N_("URL of GOPHER proxy host")
#endif /* USE_GOPHER */
#define CMT_FTP_PROXY N_("URL of FTP proxy host")
#define CMT_NO_PROXY N_("Domains to be accessed directly (no proxy)")
#define CMT_NOPROXY_NETADDR N_("Check noproxy by network address")
#define CMT_NO_CACHE N_("Disable cache")
#ifdef USE_NNTP
#define CMT_NNTP_SERVER N_("News server")
#define CMT_NNTP_MODE N_("Mode of news server")
#define CMT_MAX_NEWS N_("Number of news messages")
#endif
#define CMT_DNS_ORDER N_("Order of name resolution")
#define CMT_DROOT N_("Directory corresponding to / (document root)")
#define CMT_PDROOT N_("Directory corresponding to /~user")
#define CMT_CGIBIN N_("Directory corresponding to /cgi-bin")
#define CMT_CONFIRM_QQ N_("Confirm when quitting with q")
#define CMT_CLOSE_TAB_BACK N_("Close tab if buffer is last when back")
#ifdef USE_MARK
#define CMT_USE_MARK N_("Enable mark operations")
#endif
#define CMT_EMACS_LIKE_LINEEDIT N_("Enable Emacs-style line editing")
#define CMT_VI_PREC_NUM N_("Enable vi-like numeric prefix")
#define CMT_LABEL_TOPLINE N_("Move cursor to top line when going to label")
#define CMT_NEXTPAGE_TOPLINE N_("Move cursor to top line when moving to next page")
#define CMT_FOLD_LINE N_("Fold lines of plain text file")
#define CMT_SHOW_NUM N_("Show line numbers")
#define CMT_SHOW_SRCH_STR N_("Show search string")
#define CMT_MIMETYPES N_("List of mime.types files")
#define CMT_MAILCAP N_("List of mailcap files")
#define CMT_URIMETHODMAP N_("List of urimethodmap files")
#define CMT_EDITOR N_("Editor")
#define CMT_MAILER N_("Mailer")
#define CMT_MAILTO_OPTIONS N_("How to call Mailer for mailto URLs with options")
#define CMT_EXTBRZ N_("External Browser")
#define CMT_EXTBRZ2 N_("Second External Browser")
#define CMT_EXTBRZ3 N_("Third External Browser")
#define CMT_DISABLE_SECRET_SECURITY_CHECK N_("Disable secret file security check")
#define CMT_PASSWDFILE N_("Password file")
#define CMT_PRE_FORM_FILE N_("File for setting form on loading")
#define CMT_FTPPASS N_("Password for anonymous FTP (your mail address)")
#define CMT_FTPPASS_HOSTNAMEGEN N_("Generate domain part of password for FTP")
#define CMT_USERAGENT N_("User-Agent identification string")
#define CMT_ACCEPTENCODING N_("Accept-Encoding header")
#define CMT_ACCEPTMEDIA N_("Accept header")
#define CMT_ACCEPTLANG N_("Accept-Language header")
#define CMT_MARK_ALL_PAGES N_("Treat URL-like strings as links in all pages")
#define CMT_WRAP N_("Wrap search")
#define CMT_VIEW_UNSEENOBJECTS N_("Display unseen objects (e.g. bgimage tag)")
#define CMT_AUTO_UNCOMPRESS N_("Uncompress compressed data automatically when downloading")
#ifdef __EMX__
#define CMT_BGEXTVIEW N_("Run external viewer in a separate session")
#else
#define CMT_BGEXTVIEW N_("Run external viewer in the background")
#endif
#define CMT_EXT_DIRLIST N_("Use external program for directory listing")
#define CMT_DIRLIST_CMD N_("URL of directory listing command")
#ifdef USE_DICT
#define CMT_USE_DICTCOMMAND N_("Enable dictionary lookup through CGI")
#define CMT_DICTCOMMAND N_("URL of dictionary lookup command")
#endif /* USE_DICT */
#define CMT_IGNORE_NULL_IMG_ALT N_("Display link name for images lacking ALT")
#define CMT_IFILE N_("Index file for directories")
#define CMT_RETRY_HTTP N_("Prepend http:// to URL automatically")
#define CMT_DEFAULT_URL N_("Default value for open-URL command")
#define CMT_DECODE_CTE N_("Decode Content-Transfer-Encoding when saving")
#define CMT_PRESERVE_TIMESTAMP N_("Preserve timestamp when saving")
#ifdef USE_MOUSE
#define CMT_MOUSE N_("Enable mouse")
#define CMT_REVERSE_MOUSE N_("Scroll in reverse direction of mouse drag")
#define CMT_RELATIVE_WHEEL_SCROLL N_("Behavior of wheel scroll speed")
#define CMT_RELATIVE_WHEEL_SCROLL_RATIO N_("(A only)Scroll by # (%) of screen")
#define CMT_FIXED_WHEEL_SCROLL_COUNT N_("(B only)Scroll by # lines")
#endif /* USE_MOUSE */
#define CMT_CLEAR_BUF N_("Free memory of undisplayed buffers")
#define CMT_NOSENDREFERER N_("Suppress `Referer:' header")
#define CMT_IGNORE_CASE N_("Search case-insensitively")
#define CMT_USE_LESSOPEN N_("Use LESSOPEN")
#ifdef USE_SSL
#ifdef USE_SSL_VERIFY
#define CMT_SSL_VERIFY_SERVER N_("Perform SSL server verification")
#define CMT_SSL_CERT_FILE N_("PEM encoded certificate file of client")
#define CMT_SSL_KEY_FILE N_("PEM encoded private key file of client")
#define CMT_SSL_CA_PATH N_("Path to directory for PEM encoded certificates of CAs")
#define CMT_SSL_CA_FILE N_("File consisting of PEM encoded certificates of CAs")
#endif /* USE_SSL_VERIFY */
#define CMT_SSL_FORBID_METHOD N_("List of forbidden SSL methods (2: SSLv2, 3: SSLv3, t:TLSv1)")
#endif /* USE_SSL */
#ifdef USE_COOKIE
#define CMT_USECOOKIE N_("Enable cookie processing")
#define CMT_SHOWCOOKIE N_("Print a message when receiving a cookie")
#define CMT_ACCEPTCOOKIE N_("Accept cookies")
#define CMT_ACCEPTBADCOOKIE N_("Action to be taken on invalid cookie")
#define CMT_COOKIE_REJECT_DOMAINS N_("Domains to reject cookies from")
#define CMT_COOKIE_ACCEPT_DOMAINS N_("Domains to accept cookies from")
#define CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS N_("Domains to avoid [wrong number of dots]")
#endif
#define CMT_FOLLOW_REDIRECTION N_("Number of redirections to follow")
#define CMT_META_REFRESH N_("Enable processing of meta-refresh tag")

#ifdef USE_MIGEMO
#define CMT_USE_MIGEMO N_("Enable Migemo (Roma-ji search)")
#define CMT_MIGEMO_COMMAND N_("Migemo command")
#endif /* USE_MIGEMO */

#ifdef USE_M17N
#define CMT_DISPLAY_CHARSET N_("Display charset")
#define CMT_DOCUMENT_CHARSET N_("Default document charset")
#define CMT_AUTO_DETECT N_("Automatic charset detect when loading")
#define CMT_SYSTEM_CHARSET N_("System charset")
#define CMT_FOLLOW_LOCALE N_("System charset follows locale(LC_CTYPE)")
#define CMT_EXT_HALFDUMP N_("Output halfdump with display charset")
#define CMT_USE_WIDE N_("Use multi column characters")
#define CMT_USE_COMBINING N_("Use combining characters")
#define CMT_EAST_ASIAN_WIDTH N_("Use double width for some Unicode characters")
#define CMT_USE_LANGUAGE_TAG N_("Use Unicode language tags")
#define CMT_UCS_CONV N_("Charset conversion using Unicode map")
#define CMT_PRE_CONV N_("Charset conversion when loading")
#define CMT_SEARCH_CONV N_("Adjust search string for document charset")
#define CMT_FIX_WIDTH_CONV N_("Fix character width when conversion")
#define CMT_USE_GB12345_MAP N_("Use GB 12345 Unicode map instead of GB 2312's")
#define CMT_USE_JISX0201 N_("Use JIS X 0201 Roman for ISO-2022-JP")
#define CMT_USE_JISC6226 N_("Use JIS C 6226:1978 for ISO-2022-JP")
#define CMT_USE_JISX0201K N_("Use JIS X 0201 Katakana")
#define CMT_USE_JISX0212 N_("Use JIS X 0212:1990 (Supplemental Kanji)")
#define CMT_USE_JISX0213 N_("Use JIS X 0213:2000 (2000JIS)")
#define CMT_STRICT_ISO2022 N_("Strict ISO-2022-JP/KR/CN")
#define CMT_GB18030_AS_UCS N_("Treat 4 bytes char. of GB18030 as Unicode")
#define CMT_SIMPLE_PRESERVE_SPACE N_("Simple Preserve space")
#endif

#define CMT_KEYMAP_FILE N_("keymap file")

struct sel_c
{
    int value;
    char *cvalue;
    char *text;
};

#ifdef USE_COLOR
static struct sel_c colorstr[] = {
    {0, "black", N_("black")},
    {1, "red", N_("red")},
    {2, "green", N_("green")},
    {3, "yellow", N_("yellow")},
    {4, "blue", N_("blue")},
    {5, "magenta", N_("magenta")},
    {6, "cyan", N_("cyan")},
    {7, "white", N_("white")},
    {8, "terminal", N_("terminal")},
    {0, NULL, NULL}};
#endif /* USE_COLOR */

#if 1 /* ANSI-C ? */
#define N_STR(x) #x
#define N_S(x) (x), N_STR(x)
#else /* for traditional cpp? */
static char n_s[][2] = {
    {'0', 0},
    {'1', 0},
    {'2', 0},
};
#define N_S(x) (x), n_s[(x)]
#endif

static struct sel_c defaulturls[] = {
    {N_S(DEFAULT_URL_EMPTY), N_("none")},
    {N_S(DEFAULT_URL_CURRENT), N_("current URL")},
    {N_S(DEFAULT_URL_LINK), N_("link URL")},
    {0, NULL, NULL}};

static struct sel_c displayinsdel[] = {
    {N_S(DISPLAY_INS_DEL_SIMPLE), N_("simple")},
    {N_S(DISPLAY_INS_DEL_NORMAL), N_("use tag")},
    {N_S(DISPLAY_INS_DEL_FONTIFY), N_("fontify")},
    {0, NULL, NULL}};

#ifdef USE_MOUSE
static struct sel_c wheelmode[] = {
    {TRUE, "1", N_("A:relative to screen height")},
    {FALSE, "0", N_("B:fixed speed")},
    {0, NULL, NULL}};
#endif /* MOUSE */

#ifdef INET6
static struct sel_c dnsorders[] = {
    {N_S(DNS_ORDER_UNSPEC), N_("unspecified")},
    {N_S(DNS_ORDER_INET_INET6), N_("inet inet6")},
    {N_S(DNS_ORDER_INET6_INET), N_("inet6 inet")},
    {N_S(DNS_ORDER_INET_ONLY), N_("inet only")},
    {N_S(DNS_ORDER_INET6_ONLY), N_("inet6 only")},
    {0, NULL, NULL}};
#endif /* INET6 */

#ifdef USE_COOKIE
static struct sel_c badcookiestr[] = {
    {N_S(ACCEPT_BAD_COOKIE_DISCARD), N_("discard")},
#if 0
    {N_S(ACCEPT_BAD_COOKIE_ACCEPT), N_("accept")},
#endif
    {N_S(ACCEPT_BAD_COOKIE_ASK), N_("ask")},
    {0, NULL, NULL}};
#endif /* USE_COOKIE */

static struct sel_c mailtooptionsstr[] = {
#ifdef USE_W3MMAILER
    {N_S(MAILTO_OPTIONS_USE_W3MMAILER), N_("use internal mailer instead")},
#endif
    {N_S(MAILTO_OPTIONS_IGNORE), N_("ignore options and use only the address")},
    {N_S(MAILTO_OPTIONS_USE_MAILTO_URL), N_("use full mailto URL")},
    {0, NULL, NULL}};

#ifdef USE_M17N
static wc_ces_list *display_charset_str = NULL;
static wc_ces_list *document_charset_str = NULL;
static wc_ces_list *system_charset_str = NULL;
static struct sel_c auto_detect_str[] = {
    {N_S(WC_OPT_DETECT_OFF), N_("OFF")},
    {N_S(WC_OPT_DETECT_ISO_2022), N_("Only ISO 2022")},
    {N_S(WC_OPT_DETECT_ON), N_("ON")},
    {0, NULL, NULL}};
#endif

static struct sel_c graphic_char_str[] = {
    {N_S(GRAPHIC_CHAR_ASCII), N_("ASCII")},
    {N_S(GRAPHIC_CHAR_CHARSET), N_("charset specific")},
    {N_S(GRAPHIC_CHAR_DEC), N_("DEC special graphics")},
    {0, NULL, NULL}};

struct ParamSection
{
    std::string name;
    std::vector<Param> params;
};
auto sections = make_array(
    ParamSection{N_("Display Settings"),
                 {
                     {"tabstop", P_NZINT, PI_TEXT, w3mApp::Instance().Tabstop, CMT_TABSTOP},
                     {"indent_incr", P_NZINT, PI_TEXT, IndentIncr, CMT_INDENT_INCR},
                     {"pixel_per_char", P_PIXELS, PI_TEXT, w3mApp::Instance().pixel_per_char, CMT_PIXEL_PER_CHAR},
                     {"pixel_per_line", P_PIXELS, PI_TEXT, w3mApp::Instance().pixel_per_line, CMT_PIXEL_PER_LINE},
                     {"frame", P_CHARINT, PI_ONOFF, w3mApp::Instance().RenderFrame, CMT_FRAME},
                     {"target_self", P_CHARINT, PI_ONOFF, TargetSelf, CMT_TSELF},
                     {"open_tab_blank", P_INT, PI_ONOFF, open_tab_blank, CMT_OPEN_TAB_BLANK},
                     {"open_tab_dl_list", P_INT, PI_ONOFF, open_tab_dl_list, CMT_OPEN_TAB_DL_LIST},
                     {"display_link", P_INT, PI_ONOFF, displayLink, CMT_DISPLINK},
                     {"display_link_number", P_INT, PI_ONOFF, displayLinkNumber, CMT_DISPLINKNUMBER},
                     {"decode_url", P_INT, PI_ONOFF, DecodeURL, CMT_DECODE_URL},
                     {"display_lineinfo", P_INT, PI_ONOFF, displayLineInfo, CMT_DISPLINEINFO},
                     {"ext_dirlist", P_INT, PI_ONOFF, UseExternalDirBuffer, CMT_EXT_DIRLIST},
                     {"dirlist_cmd", P_STRING, PI_TEXT, DirBufferCommand, CMT_DIRLIST_CMD},
                     {"use_dictcommand", P_INT, PI_ONOFF, UseDictCommand, CMT_USE_DICTCOMMAND},
                     {"dictcommand", P_STRING, PI_TEXT, DictCommand, CMT_DICTCOMMAND},
                     {"multicol", P_INT, PI_ONOFF, multicolList, CMT_MULTICOL},
                     //  {"alt_entity", P_CHARINT, PI_ONOFF, UseAltEntity, CMT_ALT_ENTITY},
                     {"graphic_char", P_CHARINT, PI_SEL_C, w3mApp::Instance().UseGraphicChar, CMT_GRAPHIC_CHAR, (void *)graphic_char_str},
                     {"fold_textarea", P_CHARINT, PI_ONOFF, FoldTextarea, CMT_FOLD_TEXTAREA},
                     {"display_ins_del", P_INT, PI_SEL_C, displayInsDel, CMT_DISP_INS_DEL, displayinsdel},
                     {"ignore_null_img_alt", P_INT, PI_ONOFF, ignore_null_img_alt, CMT_IGNORE_NULL_IMG_ALT},
                     {"view_unseenobject", P_INT, PI_ONOFF, view_unseenobject, CMT_VIEW_UNSEENOBJECTS},
                     /* XXX: emacs-w3m force to off display_image even if image options off */
                     {"display_image", P_INT, PI_ONOFF, w3mApp::Instance().displayImage, CMT_DISP_IMAGE},
                     {"pseudo_inlines", P_INT, PI_ONOFF, pseudoInlines, CMT_PSEUDO_INLINES},
                     {"auto_image", P_INT, PI_ONOFF, autoImage, CMT_AUTO_IMAGE},
                     {"max_load_image", P_INT, PI_TEXT, maxLoadImage, CMT_MAX_LOAD_IMAGE},
                     {"ext_image_viewer", P_INT, PI_ONOFF, useExtImageViewer, CMT_EXT_IMAGE_VIEWER},
                     {"image_scale", P_SCALE, PI_TEXT, w3mApp::Instance().image_scale, CMT_IMAGE_SCALE},
                     {"imgdisplay", P_STRING, PI_TEXT, Imgdisplay, CMT_IMGDISPLAY},
                     {"image_map_list", P_INT, PI_ONOFF, image_map_list, CMT_IMAGE_MAP_LIST},
                     {"fold_line", P_INT, PI_ONOFF, w3mApp::Instance().FoldLine, CMT_FOLD_LINE},
                     {"show_lnum", P_INT, PI_ONOFF, w3mApp::Instance().showLineNum, CMT_SHOW_NUM},
                     {"show_srch_str", P_INT, PI_ONOFF, show_srch_str, CMT_SHOW_SRCH_STR},
                     {"label_topline", P_INT, PI_ONOFF, label_topline, CMT_LABEL_TOPLINE},
                     {"nextpage_topline", P_INT, PI_ONOFF, nextpage_topline, CMT_NEXTPAGE_TOPLINE},
                 }},
    ParamSection{N_("Color Settings"),
                 {
                     {"color", P_INT, PI_ONOFF, w3mApp::Instance().useColor, CMT_COLOR},
                     {"basic_color", P_COLOR, PI_SEL_C, basic_color, CMT_B_COLOR, (void *)colorstr},
                     {"anchor_color", P_COLOR, PI_SEL_C, anchor_color, CMT_A_COLOR, (void *)colorstr},
                     {"image_color", P_COLOR, PI_SEL_C, image_color, CMT_I_COLOR, (void *)colorstr},
                     {"form_color", P_COLOR, PI_SEL_C, form_color, CMT_F_COLOR, (void *)colorstr},
                     {"mark_color", P_COLOR, PI_SEL_C, mark_color, CMT_MARK_COLOR, (void *)colorstr},
                     {"bg_color", P_COLOR, PI_SEL_C, bg_color, CMT_BG_COLOR, (void *)colorstr},
                     {"active_style", P_INT, PI_ONOFF, useActiveColor, CMT_ACTIVE_STYLE},
                     {"active_color", P_COLOR, PI_SEL_C, active_color, CMT_C_COLOR, (void *)colorstr},
                     {"visited_anchor", P_INT, PI_ONOFF, useVisitedColor, CMT_VISITED_ANCHOR},
                     {"visited_color", P_COLOR, PI_SEL_C, visited_color, CMT_V_COLOR, (void *)colorstr},
                 }},
    ParamSection{N_("Miscellaneous Settings"),
                 {
                     {"pagerline", P_NZINT, PI_TEXT, w3mApp::Instance().PagerMax, CMT_PAGERLINE},
                     {"use_history", P_INT, PI_ONOFF, w3mApp::Instance().UseHistory, CMT_HISTORY},
                     {"history", P_INT, PI_TEXT, w3mApp::Instance().URLHistSize, CMT_HISTSIZE},
                     {"save_hist", P_INT, PI_ONOFF, w3mApp::Instance().SaveURLHist, CMT_SAVEHIST},
                     {"confirm_qq", P_INT, PI_ONOFF, confirm_on_quit, CMT_CONFIRM_QQ},
                     {"close_tab_back", P_INT, PI_ONOFF, close_tab_back, CMT_CLOSE_TAB_BACK},
                     {"mark", P_INT, PI_ONOFF, use_mark, CMT_USE_MARK},
                     {"emacs_like_lineedit", P_INT, PI_ONOFF, emacs_like_lineedit, CMT_EMACS_LIKE_LINEEDIT},
                     {"vi_prec_num", P_INT, PI_ONOFF, vi_prec_num, CMT_VI_PREC_NUM},
                     {"mark_all_pages", P_INT, PI_ONOFF, MarkAllPages, CMT_MARK_ALL_PAGES},
                     {"wrap_search", P_INT, PI_ONOFF, w3mApp::Instance().WrapDefault, CMT_WRAP},
                     {"ignorecase_search", P_INT, PI_ONOFF, IgnoreCase, CMT_IGNORE_CASE},
                     //  {"use_migemo", P_INT, PI_ONOFF, use_migemo, CMT_USE_MIGEMO},
                     //  {"migemo_command", P_STRING, PI_TEXT, migemo_command, CMT_MIGEMO_COMMAND},
                     {"use_mouse", P_INT, PI_ONOFF, w3mApp::Instance().use_mouse, CMT_MOUSE},
                     {"reverse_mouse", P_INT, PI_ONOFF, reverse_mouse, CMT_REVERSE_MOUSE},
                     {"relative_wheel_scroll", P_INT, PI_SEL_C, relative_wheel_scroll, CMT_RELATIVE_WHEEL_SCROLL, (void *)wheelmode},
                     {"relative_wheel_scroll_ratio", P_INT, PI_TEXT, relative_wheel_scroll_ratio, CMT_RELATIVE_WHEEL_SCROLL_RATIO},
                     {"fixed_wheel_scroll_count", P_INT, PI_TEXT, fixed_wheel_scroll_count, CMT_FIXED_WHEEL_SCROLL_COUNT},
                     {"clear_buffer", P_INT, PI_ONOFF, clear_buffer, CMT_CLEAR_BUF},
                     {"decode_cte", P_CHARINT, PI_ONOFF, DecodeCTE, CMT_DECODE_CTE},
                     {"auto_uncompress", P_CHARINT, PI_ONOFF, AutoUncompress, CMT_AUTO_UNCOMPRESS},
                     {"preserve_timestamp", P_CHARINT, PI_ONOFF, PreserveTimestamp, CMT_PRESERVE_TIMESTAMP},
                     {"keymap_file", P_STRING, PI_TEXT, keymap_file, CMT_KEYMAP_FILE},
                 }},
    ParamSection{N_("Directory Settings"),
                 {
                     {"document_root", P_STRING, PI_TEXT, document_root, CMT_DROOT},
                     {"personal_document_root", P_STRING, PI_TEXT, personal_document_root, CMT_PDROOT},
                     {"cgi_bin", P_STRING, PI_TEXT, cgi_bin, CMT_CGIBIN},
                     {"index_file", P_STRING, PI_TEXT, index_file, CMT_IFILE},
                 }},
    ParamSection{N_("External Program Settings"),
                 {
                     {"mime_types", P_STRING, PI_TEXT, mimetypes_files, CMT_MIMETYPES},
                     {"mailcap", P_STRING, PI_TEXT, mailcap_files, CMT_MAILCAP},
                     {"urimethodmap", P_STRING, PI_TEXT, urimethodmap_files, CMT_URIMETHODMAP},
                     {"editor", P_STRING, PI_TEXT, Editor, CMT_EDITOR},
                     {"mailto_options", P_INT, PI_SEL_C, MailtoOptions, CMT_MAILTO_OPTIONS, (void *)mailtooptionsstr},
                     {"mailer", P_STRING, PI_TEXT, Mailer, CMT_MAILER},
                     {"extbrowser", P_STRING, PI_TEXT, ExtBrowser, CMT_EXTBRZ},
                     {"extbrowser2", P_STRING, PI_TEXT, ExtBrowser2, CMT_EXTBRZ2},
                     {"extbrowser3", P_STRING, PI_TEXT, ExtBrowser3, CMT_EXTBRZ3},
                     {"bgextviewer", P_INT, PI_ONOFF, BackgroundExtViewer, CMT_BGEXTVIEW},
                     {"use_lessopen", P_INT, PI_ONOFF, use_lessopen, CMT_USE_LESSOPEN},
                 }},
    ParamSection{N_("Network Settings"),
                 {
                     {"passwd_file", P_STRING, PI_TEXT, passwd_file, CMT_PASSWDFILE},
                     {"disable_secret_security_check", P_INT, PI_ONOFF, disable_secret_security_check, CMT_DISABLE_SECRET_SECURITY_CHECK},
                     {"ftppasswd", P_STRING, PI_TEXT, ftppasswd, CMT_FTPPASS},
                     {"ftppass_hostnamegen", P_INT, PI_ONOFF, ftppass_hostnamegen, CMT_FTPPASS_HOSTNAMEGEN},
                     {"pre_form_file", P_STRING, PI_TEXT, pre_form_file, CMT_PRE_FORM_FILE},
                     {"user_agent", P_STRING, PI_TEXT, UserAgent, CMT_USERAGENT},
                     {"no_referer", P_INT, PI_ONOFF, NoSendReferer, CMT_NOSENDREFERER},
                     {"accept_language", P_STRING, PI_TEXT, AcceptLang, CMT_ACCEPTLANG},
                     {"accept_encoding", P_STRING, PI_TEXT, AcceptEncoding, CMT_ACCEPTENCODING},
                     {"accept_media", P_STRING, PI_TEXT, AcceptMedia, CMT_ACCEPTMEDIA},
                     {"argv_is_url", P_CHARINT, PI_ONOFF, ArgvIsURL, CMT_ARGV_IS_URL},
                     {"retry_http", P_INT, PI_ONOFF, retryAsHttp, CMT_RETRY_HTTP},
                     {"default_url", P_INT, PI_SEL_C, DefaultURLString, CMT_DEFAULT_URL, (void *)defaulturls},
                     {"follow_redirection", P_INT, PI_TEXT, FollowRedirection, CMT_FOLLOW_REDIRECTION},
                     {"meta_refresh", P_CHARINT, PI_ONOFF, MetaRefresh, CMT_META_REFRESH},
                     {"dns_order", P_INT, PI_SEL_C, DNS_order, CMT_DNS_ORDER, (void *)dnsorders},
                     {"nntpserver", P_STRING, PI_TEXT, NNTP_server, CMT_NNTP_SERVER},
                     {"nntpmode", P_STRING, PI_TEXT, NNTP_mode, CMT_NNTP_MODE},
                     {"max_news", P_INT, PI_TEXT, MaxNewsMessage, CMT_MAX_NEWS},
                 }},
    ParamSection{N_("Proxy Settings"),
                 {
                     {"use_proxy", P_CHARINT, PI_ONOFF, w3mApp::Instance().use_proxy, CMT_USE_PROXY},
                     {"http_proxy", P_STRING, PI_TEXT, w3mApp::Instance().HTTP_proxy, CMT_HTTP_PROXY},
                     {"https_proxy", P_STRING, PI_TEXT, w3mApp::Instance().HTTPS_proxy, CMT_HTTPS_PROXY},
                     {"ftp_proxy", P_STRING, PI_TEXT, w3mApp::Instance().FTP_proxy, CMT_FTP_PROXY},
                     {"no_proxy", P_STRING, PI_TEXT, NO_proxy, CMT_NO_PROXY},
                     {"noproxy_netaddr", P_INT, PI_ONOFF, NOproxy_netaddr, CMT_NOPROXY_NETADDR},
                     {"no_cache", P_CHARINT, PI_ONOFF, NoCache, CMT_NO_CACHE},
                 }},
    ParamSection{N_("SSL Settings"),
                 {
                     {"ssl_forbid_method", P_STRING, PI_TEXT, ssl_forbid_method, CMT_SSL_FORBID_METHOD},
                     {"ssl_verify_server", P_INT, PI_ONOFF, ssl_verify_server, CMT_SSL_VERIFY_SERVER},
                     {"ssl_cert_file", P_SSLPATH, PI_TEXT, ssl_cert_file, CMT_SSL_CERT_FILE},
                     {"ssl_key_file", P_SSLPATH, PI_TEXT, ssl_key_file, CMT_SSL_KEY_FILE},
                     {"ssl_ca_path", P_SSLPATH, PI_TEXT, ssl_ca_path, CMT_SSL_CA_PATH},
                     {"ssl_ca_file", P_SSLPATH, PI_TEXT, ssl_ca_file, CMT_SSL_CA_FILE},
                 }},
    ParamSection{N_("Cookie Settings"),
                 {
                     {"use_cookie", P_INT, PI_ONOFF, w3mApp::Instance().use_cookie, CMT_USECOOKIE},
                     {"show_cookie", P_INT, PI_ONOFF, show_cookie, CMT_SHOWCOOKIE},
                     {"accept_cookie", P_INT, PI_ONOFF, w3mApp::Instance().accept_cookie, CMT_ACCEPTCOOKIE},
                     {"accept_bad_cookie", P_INT, PI_SEL_C, accept_bad_cookie, CMT_ACCEPTBADCOOKIE, (void *)badcookiestr},
                     {"cookie_reject_domains", P_STRING, PI_TEXT, cookie_reject_domains, CMT_COOKIE_REJECT_DOMAINS},
                     {"cookie_accept_domains", P_STRING, PI_TEXT, cookie_accept_domains, CMT_COOKIE_ACCEPT_DOMAINS},
                     {"cookie_avoid_wrong_number_of_dots", P_STRING, PI_TEXT, cookie_avoid_wrong_number_of_dots, CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS},
                 }},
    ParamSection{N_("Charset Settings"),
                 {
                     {"display_charset", P_CODE, PI_CODE, w3mApp::Instance().DisplayCharset, CMT_DISPLAY_CHARSET, display_charset_str},
                     {"document_charset", P_CODE, PI_CODE, w3mApp::Instance().DocumentCharset, CMT_DOCUMENT_CHARSET, document_charset_str},
                     {"auto_detect", P_CHARINT, PI_SEL_C, WcOption.auto_detect, CMT_AUTO_DETECT, (void *)auto_detect_str},
                     {"system_charset", P_CODE, PI_CODE, w3mApp::Instance().SystemCharset, CMT_SYSTEM_CHARSET, system_charset_str},
                     {"follow_locale", P_CHARINT, PI_ONOFF, w3mApp::Instance().FollowLocale, CMT_FOLLOW_LOCALE},
                     {"ext_halfdump", P_CHARINT, PI_ONOFF, w3mApp::Instance().ExtHalfdump, CMT_EXT_HALFDUMP},
                     {"use_wide", P_CHARINT, PI_ONOFF, WcOption.use_wide, CMT_USE_WIDE},
                     {"use_combining", P_CHARINT, PI_ONOFF, WcOption.use_combining, CMT_USE_COMBINING},
                     {"east_asian_width", P_CHARINT, PI_ONOFF, WcOption.east_asian_width, CMT_EAST_ASIAN_WIDTH},
                     {"use_language_tag", P_CHARINT, PI_ONOFF, WcOption.use_language_tag, CMT_USE_LANGUAGE_TAG},
                     {"ucs_conv", P_CHARINT, PI_ONOFF, WcOption.ucs_conv, CMT_UCS_CONV},
                     {"pre_conv", P_CHARINT, PI_ONOFF, WcOption.pre_conv, CMT_PRE_CONV},
                     {"search_conv", P_CHARINT, PI_ONOFF, SearchConv, CMT_SEARCH_CONV},
                     {"fix_width_conv", P_CHARINT, PI_ONOFF, WcOption.fix_width_conv, CMT_FIX_WIDTH_CONV},
                     {"use_gb12345_map", P_CHARINT, PI_ONOFF, WcOption.use_gb12345_map, CMT_USE_GB12345_MAP},
                     {"use_jisx0201", P_CHARINT, PI_ONOFF, WcOption.use_jisx0201, CMT_USE_JISX0201},
                     {"use_jisc6226", P_CHARINT, PI_ONOFF, WcOption.use_jisc6226, CMT_USE_JISC6226},
                     {"use_jisx0201k", P_CHARINT, PI_ONOFF, WcOption.use_jisx0201k, CMT_USE_JISX0201K},
                     {"use_jisx0212", P_CHARINT, PI_ONOFF, WcOption.use_jisx0212, CMT_USE_JISX0212},
                     {"use_jisx0213", P_CHARINT, PI_ONOFF, WcOption.use_jisx0213, CMT_USE_JISX0213},
                     {"strict_iso2022", P_CHARINT, PI_ONOFF, WcOption.strict_iso2022, CMT_STRICT_ISO2022},
                     {"gb18030_as_ucs", P_CHARINT, PI_ONOFF, WcOption.gb18030_as_ucs, CMT_GB18030_AS_UCS},
                     {"simple_preserve_space", P_CHARINT, PI_ONOFF, SimplePreserveSpace, CMT_SIMPLE_PRESERVE_SPACE},
                 }});

static void create_option_search_table()
{
    for (auto &section : sections)
    {
        for (auto &param : section.params)
        {
            RC_search_table.insert(std::make_pair(param.name, &param));
        }
    }
}

/* show parameter with bad options invokation */
void show_params(FILE *fp)
{
    fputs("\nconfiguration parameters\n", fp);
    for (auto j = 0; j < sections.size(); ++j)
    {
        auto &section = sections[j];
        std::string_view cmt;
        if (!OptionEncode)
            cmt =
                wc_conv(_(section.name.c_str()), OptionCharset,
                        w3mApp::Instance().InnerCharset)
                    ->ptr;
        else
            cmt = section.name;
        fprintf(fp, "  section[%d]: %s\n", j, conv_to_system(cmt.data()));

        for (auto &param : section.params)
        {
            std::string_view t;
            switch (param.type)
            {
            case P_INT:
            case P_SHORT:
            case P_CHARINT:
            case P_NZINT:
                t = (param.inputtype == PI_ONOFF)
                        ? "bool"
                        : "number";
                break;
            case P_CHAR:
                t = "char";
                break;
            case P_STRING:
                t = "string";
                break;
            case P_SSLPATH:
                t = "path";
                break;
            case P_COLOR:
                t = "color";
                break;
            case P_CODE:
                t = "charset";
                break;
            case P_PIXELS:
                t = "number";
                break;
            case P_SCALE:
                t = "percent";
                break;
            }
            if (!OptionEncode)
                cmt = wc_conv(_(param.comment.data()),
                              OptionCharset, w3mApp::Instance().InnerCharset)
                          ->ptr;
            else
                cmt = param.comment;
            int l = 30 - (param.name.size() + t.size());
            if (l < 0)
                l = 1;
            fprintf(fp, "    -o %s=<%s>%*s%s\n",
                    param.name.data(), t.data(), l, " ",
                    conv_to_system(cmt.data()));
        }
    }
}

int str_to_bool(const char *value, int old)
{
    if (value == NULL)
        return 1;
    switch (TOLOWER(*value))
    {
    case '0':
    case 'f': /* false */
    case 'n': /* no */
    case 'u': /* undef */
        return 0;
    case 'o':
        if (TOLOWER(value[1]) == 'f') /* off */
            return 0;
        return 1; /* on */
    case 't':
        if (TOLOWER(value[1]) == 'o') /* toggle */
            return !old;
        return 1; /* true */
    case '!':
    case 'r': /* reverse */
    case 'x': /* exchange */
        return !old;
    }
    return 1;
}

static int
set_param(std::string_view name, std::string_view value)
{
    auto found = RC_search_table.find(name);
    if (found == RC_search_table.end())
    {
        return 0;
    }
    auto p = found->second;
    assert(p);

    return p->Value(value);
}

int set_param_option(const char *option)
{
    Str tmp = Strnew();
    auto p = option;
    char *q;

    while (*p && !IS_SPACE(*p) && *p != '=')
        tmp->Push(*p++);
    while (*p && IS_SPACE(*p))
        p++;
    if (*p == '=')
    {
        p++;
        while (*p && IS_SPACE(*p))
            p++;
    }
    ToLower(tmp);
    if (set_param(tmp->ptr, p))
        goto option_assigned;
    q = tmp->ptr;
    if (!strncmp(q, "no", 2))
    { /* -o noxxx, -o no-xxx, -o no_xxx */
        q += 2;
        if (*q == '-' || *q == '_')
            q++;
    }
    else if (tmp->ptr[0] == '-') /* -o -xxx */
        q++;
    else
        return 0;
    if (set_param(q, "0"))
        goto option_assigned;
    return 0;
option_assigned:
    return 1;
}

char *
get_param_option(char *name)
{
    auto found = RC_search_table.find(name);
    if (found == RC_search_table.end())
    {
        return "";
    }
    return found->second->to_str()->ptr;
}

static void
interpret_rc(FILE *f)
{
    Str line;
    Str tmp;
    char *p;

    for (;;)
    {
        line = Strfgets(f);
        if (line->Size() == 0) /* end of file */
            break;
        Strip(line);
        if (line->Size() == 0) /* blank line */
            continue;
        if (line->ptr[0] == '#') /* comment */
            continue;
        tmp = Strnew();
        p = line->ptr;
        while (*p && !IS_SPACE(*p))
            tmp->Push(*p++);
        while (*p && IS_SPACE(*p))
            p++;
        ToLower(tmp);
        set_param(tmp->ptr, p);
    }
}

#define set_no_proxy(domains) (w3mApp::Instance().NO_proxy_domains = make_domain_list(domains))
void parse_proxy()
{
    if (w3mApp::Instance().HTTP_proxy.size())
        w3mApp::Instance().HTTP_proxy_parsed = URL::Parse(w3mApp::Instance().HTTP_proxy, NULL);

    if (w3mApp::Instance().HTTPS_proxy.size())
        w3mApp::Instance().HTTPS_proxy_parsed = URL::Parse(w3mApp::Instance().HTTPS_proxy, NULL);

    if (w3mApp::Instance().FTP_proxy.size())
        w3mApp::Instance().FTP_proxy_parsed = URL::Parse(w3mApp::Instance().FTP_proxy, NULL);
    if (non_null(NO_proxy))
        set_no_proxy(NO_proxy);
}

void parse_cookie()
{
    if (non_null(cookie_reject_domains))
        Cookie_reject_domains = make_domain_list(cookie_reject_domains);
    if (non_null(cookie_accept_domains))
        Cookie_accept_domains = make_domain_list(cookie_accept_domains);
    if (non_null(cookie_avoid_wrong_number_of_dots))
        Cookie_avoid_wrong_number_of_dots_domains = make_domain_list(cookie_avoid_wrong_number_of_dots);
}

#ifdef __EMX__
static int
do_mkdir(const char *dir, long mode)
{
    char *r, abs[_MAX_PATH];
    size_t n;

    _abspath(abs, rc_dir, _MAX_PATH); /* Translate '\\' to '/' */

    if (!(n = strlen(abs)))
        return -1;

    if (*(r = abs + n - 1) == '/') /* Ignore tailing slash if it is */
        *r = 0;

    return mkdir(abs, mode);
}
#else /* not __EMX__ */
#ifdef __MINGW32_VERSION
#define do_mkdir(dir, mode) mkdir(dir)
#else
#define do_mkdir(dir, mode) mkdir(dir, mode)
#endif /* not __MINW32_VERSION */
#endif /* not __EMX__ */

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

int find_auth_user_passwd(URL *pu, char *realm,
                          Str *uname, Str *pwd, int is_proxy)
{
    if (pu->userinfo.name.size() && pu->userinfo.pass.size())
    {
        *uname = Strnew(pu->userinfo.name);
        *pwd = Strnew(pu->userinfo.pass);
        return 1;
    }
    auto ent = find_auth_pass_entry(const_cast<char *>(pu->host.c_str()), pu->port, realm, const_cast<char *>(pu->userinfo.name.c_str()), is_proxy);
    if (ent)
    {
        *uname = ent->uname;
        *pwd = ent->pwd;
        return 1;
    }
    return 0;
}

void add_auth_user_passwd(URL *pu, char *realm, Str uname, Str pwd,
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

void invalidate_auth_user_passwd(URL *pu, char *realm, Str uname, Str pwd,
                                 int is_proxy)
{
    struct auth_pass *ent;
    ent = find_auth_pass_entry(const_cast<char *>(pu->host.c_str()), pu->port, realm, NULL, is_proxy);
    if (ent)
    {
        ent->bad = TRUE;
    }
    return;
}

static Str
next_token(Str arg)
{
    Str narg = NULL;
    char *p, *q;
    if (arg == NULL || arg->Size() == 0)
        return NULL;
    p = arg->ptr;
    q = p;
    SKIP_NON_BLANKS(&q);
    if (*q != '\0')
    {
        *q++ = '\0';
        SKIP_BLANKS(&q);
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
        Strip(line);
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

static void loadPasswd(void)
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

void sync_with_option(void)
{
    if (w3mApp::Instance().PagerMax < ::LINES)
        w3mApp::Instance().PagerMax = ::LINES;
    WrapSearch = w3mApp::Instance().WrapDefault;
    parse_proxy();
#ifdef USE_COOKIE
    parse_cookie();
#endif
    initMailcap();
    initMimeTypes();
#ifdef USE_EXTERNAL_URI_LOADER
    initURIMethods();
#endif
#ifdef USE_MIGEMO
    init_migemo();
#endif
#ifdef USE_IMAGE
    if (fmInitialized && w3mApp::Instance().displayImage)
        initImage();
#else
    displayImage = FALSE; /* XXX */
#endif
    loadPasswd();
    loadPreForm();

    if (AcceptLang == NULL || *AcceptLang == '\0')
    {
        /* TRANSLATORS: 
        * AcceptLang default: this is used in Accept-Language: HTTP request 
        * header. For example, ja.po should translate it as
        * "ja;q=1.0, en;q=0.5" like that.
        */
        AcceptLang = _("en;q=1.0");
    }
    if (AcceptEncoding == NULL || *AcceptEncoding == '\0')
        AcceptEncoding = acceptableEncoding();
    if (AcceptMedia == NULL || *AcceptMedia == '\0')
        AcceptMedia = acceptableMimeTypes();

    if (fmInitialized)
    {
        initKeymap(FALSE);
#ifdef USE_MOUSE
        initMouseAction();
#endif /* MOUSE */
#ifdef USE_MENU
        initMenu();
#endif /* MENU */
    }
}

void init_rc(void)
{
    int i;
    struct stat st;
    FILE *f;

    if (rc_dir != NULL)
        goto open_rc;

    rc_dir = expandPath(RC_DIR);
    i = strlen(rc_dir);
    if (i > 1 && rc_dir[i - 1] == '/')
        rc_dir[i - 1] = '\0';

#ifdef USE_M17N
    display_charset_str = wc_get_ces_list();
    document_charset_str = display_charset_str;
    system_charset_str = display_charset_str;
#endif

    if (stat(rc_dir, &st) < 0)
    {
        if (errno == ENOENT)
        { /* no directory */
            if (do_mkdir(rc_dir, 0700) < 0)
            {
                fprintf(stderr, "Can't create config directory (%s)!", rc_dir);
                goto rc_dir_err;
            }
            else
            {
                stat(rc_dir, &st);
            }
        }
        else
        {
            fprintf(stderr, "Can't open config directory (%s)!", rc_dir);
            goto rc_dir_err;
        }
    }
    if (!S_ISDIR(st.st_mode))
    {
        /* not a directory */
        fprintf(stderr, "%s is not a directory!", rc_dir);
        goto rc_dir_err;
    }
    if (!(st.st_mode & S_IWUSR))
    {
        fprintf(stderr, "%s is not writable!", rc_dir);
        goto rc_dir_err;
    }
    no_rc_dir = FALSE;
    tmp_dir = rc_dir;

    if (w3mApp::Instance().config_file.empty())
        w3mApp::Instance().config_file = rcFile(CONFIG_FILE);

    create_option_search_table();

open_rc:
    /* open config file */
    if ((f = fopen(etcFile(W3MCONFIG), "rt")) != NULL)
    {
        interpret_rc(f);
        fclose(f);
    }
    if ((f = fopen(confFile(CONFIG_FILE), "rt")) != NULL)
    {
        interpret_rc(f);
        fclose(f);
    }
    if (w3mApp::Instance().config_file.size() && (f = fopen(w3mApp::Instance().config_file.c_str(), "rt")) != NULL)
    {
        interpret_rc(f);
        fclose(f);
    }
    return;

rc_dir_err:
    no_rc_dir = TRUE;
    if (((tmp_dir = getenv("TMPDIR")) == NULL || *tmp_dir == '\0') &&
        ((tmp_dir = getenv("TMP")) == NULL || *tmp_dir == '\0') &&
        ((tmp_dir = getenv("TEMP")) == NULL || *tmp_dir == '\0'))
        tmp_dir = "/tmp";
    create_option_search_table();
    goto open_rc;
}

static char optionpanel_src1[] =
    "<html><head><title>Option Setting Panel</title></head><body>\
<h1 align=center>Option Setting Panel<br>(w3m version %s)</b></h1>\
<form method=post action=\"file:///$LIB/" W3MHELPERPANEL_CMDNAME "\">\
<input type=hidden name=mode value=panel>\
<input type=hidden name=cookie value=\"%s\">\
<input type=submit value=\"%s\">\
</form><br>\
<form method=internal action=option>";

static Str optionpanel_str = NULL;

BufferPtr
load_option_panel(void)
{
    Str src;
    struct sel_c *s = nullptr;
    wc_ces_list *c = nullptr;
    int x, i;
    Str tmp;
    BufferPtr buf;

    if (optionpanel_str == NULL)
        optionpanel_str = Sprintf(optionpanel_src1, w3mApp::w3m_version.data(),
                                  html_quote(localCookie()->ptr), _(CMT_HELPER));
    if (!OptionEncode)
    {
        optionpanel_str =
            wc_Str_conv(optionpanel_str, OptionCharset, w3mApp::Instance().InnerCharset);
        for (auto &section : sections)
        {
            section.name =
                wc_conv(_(section.name.c_str()), OptionCharset, w3mApp::Instance().InnerCharset)
                    ->ptr;

            for (auto &param : section.params)
            {
                param.comment =
                    wc_conv(_(param.comment.data()), OptionCharset,
                            w3mApp::Instance().InnerCharset)
                        ->ptr;
                if (param.inputtype == PI_SEL_C

                    && param.select != colorstr

                )
                {
                    for (s = (struct sel_c *)param.select; s->text != NULL; s++)
                    {
                        s->text =
                            wc_conv(_(s->text), OptionCharset,
                                    w3mApp::Instance().InnerCharset)
                                ->ptr;
                    }
                }
            }
        }

        for (s = colorstr; s->text; s++)
            s->text = wc_conv(_(s->text), OptionCharset,
                              w3mApp::Instance().InnerCharset)
                          ->ptr;

        OptionEncode = TRUE;
    }
    src = optionpanel_str->Clone();

    src->Push("<table><tr><td>");
    for (auto &section : sections)
    {
        Strcat_m_charp(src, "<h1>", section.name, "</h1>", NULL);

        src->Push("<table width=100% cellpadding=0>");
        for (auto &param : section.params)
        {
            Strcat_m_charp(src, "<tr><td>", param.comment, NULL);
            src->Push(Sprintf("</td><td width=%d>",
                              (int)(28 * w3mApp::Instance().pixel_per_char)));
            switch (param.inputtype)
            {
            case PI_TEXT:
                Strcat_m_charp(src, "<input type=text name=",
                               param.name,
                               " value=\"",
                               html_quote(param.to_str()), "\">", NULL);
                break;
            case PI_ONOFF:
                x = atoi(param.to_str()->ptr);
                Strcat_m_charp(src, "<input type=radio name=",
                               param.name,
                               " value=1",
                               (x ? " checked" : ""),
                               ">YES&nbsp;&nbsp;<input type=radio name=",
                               param.name,
                               " value=0", (x ? "" : " checked"), ">NO", NULL);
                break;
            case PI_SEL_C:
                tmp = param.to_str();
                Strcat_m_charp(src, "<select name=", param.name, ">", NULL);
                for (s = (struct sel_c *)param.select; s->text != NULL; s++)
                {
                    src->Push("<option value=");
                    src->Push(Sprintf("%s\n", s->cvalue));
                    if ((param.type != P_CHAR && s->value == atoi(tmp->ptr)) ||
                        (param.type == P_CHAR && (char)s->value == *(tmp->ptr)))
                        src->Push(" selected");
                    src->Push('>');
                    src->Push(s->text);
                }
                src->Push("</select>");
                break;
#ifdef USE_M17N
            case PI_CODE:
                tmp = param.to_str();
                Strcat_m_charp(src, "<select name=", param.name, ">", NULL);
                if (param.select)
                { // TODO:
                    for (c = *(wc_ces_list **)param.select; c->desc != NULL; c++)
                    {
                        src->Push("<option value=");
                        src->Push(Sprintf("%s\n", c->name));
                        if (c->id == atoi(tmp->ptr))
                            src->Push(" selected");
                        src->Push('>');
                        src->Push(c->desc);
                    }
                }
                src->Push("</select>");
                break;
#endif
            }
            src->Push("</td></tr>\n");
        }
        src->Push(
            "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>");
        src->Push("</table><hr width=50%>");
    }
    src->Push("</table></form></body></html>");
    buf = loadHTMLString(src);
#ifdef USE_M17N
    if (buf)
        buf->document_charset = OptionCharset;
#endif
    return buf;
}

void panel_set_option(struct parsed_tagarg *arg)
{
    FILE *f = NULL;
    char *p;

    if (w3mApp::Instance().config_file.empty())
    {
        disp_message("There's no config file... config not saved", FALSE);
    }
    else
    {
        f = fopen(w3mApp::Instance().config_file.c_str(), "wt");
        if (f == NULL)
        {
            disp_message("Can't write option!", FALSE);
        }
    }
    while (arg)
    {
        /*  InnerCharset -> SystemCharset */
        if (arg->value)
        {
            p = conv_to_system(arg->value);
            if (set_param(arg->arg, p))
            {
                if (f)
                    fprintf(f, "%s %s\n", arg->arg, p);
            }
        }
        arg = arg->next;
    }
    if (f)
        fclose(f);
    sync_with_option();
    backBf(&w3mApp::Instance());
}

char *
rcFile(const char *base)
{
    if (base &&
        (base[0] == '/' ||
         (base[0] == '.' && (base[1] == '/' || (base[1] == '.' && base[2] == '/'))) || (base[0] == '~' && base[1] == '/')))
        /* /file, ./file, ../file, ~/file */
        return expandPath(base);
    return expandPath(Strnew_m_charp(rc_dir, "/", base)->ptr);
}

char *
auxbinFile(const char *base)
{
    return expandPath(Strnew_m_charp(w3m_auxbin_dir(), "/", base, NULL)->ptr);
}

#if 0 /* not used */
char *
libFile(char *base)
{
    return expandPath(Strnew_m_charp(w3m_lib_dir(), "/", base, NULL)->ptr);
}
#endif

char *
etcFile(char *base)
{
    return expandPath(Strnew_m_charp(w3m_etc_dir(), "/", base, NULL)->ptr);
}

char *
confFile(char *base)
{
    return expandPath(Strnew_m_charp(w3m_conf_dir(), "/", base, NULL)->ptr);
}

#ifndef USE_HELP_CGI
char *
helpFile(char *base)
{
    return expandPath(Strnew_m_charp(w3m_help_dir(), "/", base, NULL)->ptr);
}
#endif
