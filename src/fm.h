/* $Id: fm.h,v 1.149 2010/08/20 09:47:09 htrb Exp $ */
/* 
 * w3m: WWW wo Miru utility
 * 
 * by A.ITO  Feb. 1995
 * 
 * You can use,copy,modify and distribute this program without any permission.
 */

#ifndef FM_H
#define FM_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif
#include <string_view>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "config.h"
#include "history.h"
#include "transport/url.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#if !HAVE_SETLOCALE
#define setlocale(category, locale) /* empty */
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#define N_(String) (String)
#else
#undef bindtextdomain
#define bindtextdomain(Domain, Directory) /* empty */
#undef textdomain
#define textdomain(Domain) /* empty */
#define _(Text) Text
#define N_(Text) Text
#define gettext(Text) Text
#endif

#ifdef MAINPROGRAM
#define global
#define init(x) = (x)
#else /* not MAINPROGRAM */
#define global extern
#define init(x)
#endif /* not MAINPROGRAM */

#define SAVE_BUF_SIZE 1536

/* 
 * Constants.
 */

#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

#ifdef FALSE
#undef FALSE
#endif

#ifdef TRUE
#undef TRUE
#endif

#define FALSE 0
#define TRUE 1

#define SHELLBUFFERNAME "*Shellout*"
#define PIPEBUFFERNAME "*stream*"
#define CPIPEBUFFERNAME "*stream(closed)*"
#ifdef USE_DICT
#define DICTBUFFERNAME "*dictionary*"
#endif /* USE_DICT */

/* Effect ( standout/underline ) */
#define P_EFFECT 0x40ff
#define PE_NORMAL 0x00
#define PE_MARK 0x01
#define PE_UNDER 0x02
#define PE_STAND 0x04
#define PE_BOLD 0x08
#define PE_ANCHOR 0x10
#define PE_EMPH 0x08
#define PE_IMAGE 0x20
#define PE_FORM 0x40
#define PE_ACTIVE 0x80
#define PE_VISITED 0x4000

/* Extra effect */
#define PE_EX_ITALIC 0x01
#define PE_EX_INSERT 0x02
#define PE_EX_STRIKE 0x04

#define PE_EX_ITALIC_E PE_UNDER
#define PE_EX_INSERT_E PE_UNDER
#define PE_EX_STRIKE_E PE_STAND

#define CharType(c) ((c)&P_CHARTYPE)
#define CharEffect(c) ((c) & (P_EFFECT | PC_SYMBOL))
#define SetCharType(v, c) ((v) = (((v) & ~P_CHARTYPE) | (c)))

/* Search Result */
#define SR_FOUND 0x1
#define SR_NOTFOUND 0x2
#define SR_WRAPPED 0x4

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

/* Completion status. */
#define CPL_OK 0
#define CPL_AMBIG 1
#define CPL_FAIL 2
#define CPL_MENU 3

#define CPL_NEVER 0x0
#define CPL_OFF 0x1
#define CPL_ON 0x2
#define CPL_ALWAYS 0x4
#define CPL_URL 0x8


#define IMG_FLAG_SKIP 1
#define IMG_FLAG_AUTO 2


#define IMG_FLAG_UNLOADED 0
#define IMG_FLAG_LOADED 1
#define IMG_FLAG_ERROR 2
#define IMG_FLAG_DONT_REMOVE 4

/* 
 * Macros.
 */

#define free(x) GC_free(x) /* let GC do it. */

/* 
 * Types.
 */

#define COPY_BUFROOT(dstbuf, srcbuf)       \
    {                                      \
        (dstbuf)->rootX = (srcbuf)->rootX; \
        (dstbuf)->rootY = (srcbuf)->rootY; \
        (dstbuf)->COLS = (srcbuf)->COLS;   \
        (dstbuf)->LINES = (srcbuf)->LINES; \
    }

#define COPY_BUFPOSITION(dstbuf, srcbuf)                   \
    {                                                      \
        (dstbuf)->topLine = (srcbuf)->topLine;             \
        (dstbuf)->currentLine = (srcbuf)->currentLine;     \
        (dstbuf)->pos = (srcbuf)->pos;                     \
        (dstbuf)->cursorX = (srcbuf)->cursorX;             \
        (dstbuf)->cursorY = (srcbuf)->cursorY;             \
        (dstbuf)->visualpos = (srcbuf)->visualpos;         \
        (dstbuf)->currentColumn = (srcbuf)->currentColumn; \
    }

/* status flags */
#define R_ST_NORMAL 0  /* normal */
#define R_ST_TAG0 1    /* within tag, just after < */
#define R_ST_TAG 2     /* within tag */
#define R_ST_QUOTE 3   /* within single quote */
#define R_ST_DQUOTE 4  /* within double quote */
#define R_ST_EQL 5     /* = */
#define R_ST_AMP 6     /* within ampersand quote */
#define R_ST_EOL 7     /* end of file */
#define R_ST_CMNT1 8   /* <!  */
#define R_ST_CMNT2 9   /* <!- */
#define R_ST_CMNT 10   /* within comment */
#define R_ST_NCMNT1 11 /* comment - */
#define R_ST_NCMNT2 12 /* comment -- */
#define R_ST_NCMNT3 13 /* comment -- space */
#define R_ST_IRRTAG 14 /* within irregular tag */
#define R_ST_VALUE 15  /* within tag attribule value */

#define ST_IS_REAL_TAG(s) ((s) == R_ST_TAG || (s) == R_ST_TAG0 || (s) == R_ST_EQL || (s) == R_ST_VALUE)

/* is this '<' really means the beginning of a tag? */
#define REALLY_THE_BEGINNING_OF_A_TAG(p) \
    (IS_ALPHA(p[1]) || p[1] == '/' || p[1] == '!' || p[1] == '?' || p[1] == '\0' || p[1] == '_')

/* modes for align() */

#define ALIGN_CENTER 0
#define ALIGN_LEFT 1
#define ALIGN_RIGHT 2
#define ALIGN_MIDDLE 4
#define ALIGN_TOP 5
#define ALIGN_BOTTOM 6

#define VALIGN_MIDDLE 0
#define VALIGN_TOP 1
#define VALIGN_BOTTOM 2

#define HTST_UNKNOWN 255
#define HTST_MISSING 254
#define HTST_NORMAL 0
#define HTST_CONNECT 1

#define TMPF_DFL 0
#define TMPF_SRC 1
#define TMPF_FRAME 2
#define TMPF_CACHE 3
#define TMPF_COOKIE 4
#define MAX_TMPF_TYPE 5


/* 
 * Globals.
 */

global int IndentIncr init(4);

global char TargetSelf init(FALSE);
global char PermitSaveToPipe init(FALSE);
global char DecodeCTE init(FALSE);
global char AutoUncompress init(FALSE);
global char PreserveTimestamp init(TRUE);
global char ArgvIsURL init(FALSE);
global char MetaRefresh init(FALSE);

global char fmInitialized init(FALSE);
global char QuietMessage init(FALSE);
global char TrapSignal init(TRUE);

global char *NO_proxy init(NULL);
global int NOproxy_netaddr init(TRUE);
#ifdef INET6
#define DNS_ORDER_UNSPEC 0
#define DNS_ORDER_INET_INET6 1
#define DNS_ORDER_INET6_INET 2
#define DNS_ORDER_INET_ONLY 4
#define DNS_ORDER_INET6_ONLY 6
global int DNS_order init(DNS_ORDER_UNSPEC);
extern int ai_family_order_table[7][3]; /* XXX */
#endif                                  /* INET6 */

global char NoCache init(FALSE);



#ifdef USE_NNTP
global char *NNTP_server init(NULL);
global char *NNTP_mode init(NULL);
global int MaxNewsMessage init(50);
#endif

global char *document_root init(NULL);
global char *personal_document_root init(NULL);
global char *cgi_bin init(NULL);
global char *index_file init(NULL);

global int open_tab_blank init(FALSE);
global int open_tab_dl_list init(FALSE);
global int close_tab_back init(FALSE);
global int TabCols init(10);

global char *CurrentCmdData;

global int basic_color init(8);  /* don't change */
global int anchor_color init(4); /* blue  */
global int image_color init(2);  /* green */
global int form_color init(1);   /* red   */

global int bg_color init(8);   /* don't change */
global int mark_color init(6); /* cyan */

global int useActiveColor init(FALSE);
global int active_color init(6); /* cyan */
global int useVisitedColor init(FALSE);
global int visited_color init(5); /* magenta  */

global int confirm_on_quit init(TRUE);
#ifdef USE_MARK
global int use_mark init(FALSE);
#endif
global int emacs_like_lineedit init(FALSE);
global int vi_prec_num init(FALSE);
global int label_topline init(FALSE);
global int nextpage_topline init(FALSE);

global int displayLink init(FALSE);
global int displayLinkNumber init(FALSE);
global int displayLineInfo init(FALSE);
global int DecodeURL init(FALSE);
global int retryAsHttp init(TRUE);
global int show_srch_str init(TRUE);
#ifdef USE_IMAGE
global char *Imgdisplay init(IMGDISPLAY);
global int autoImage init(TRUE);
global int useExtImageViewer init(TRUE);
global int maxLoadImage init(4);
global int image_map_list init(TRUE);
#else
global int displayImage init(FALSE); /* XXX: emacs-w3m use display_image=off */
#endif
global int pseudoInlines init(TRUE);
global char *Editor init(DEF_EDITOR);
#ifdef USE_W3MMAILER
global char *Mailer init(NULL);
#else
global char *Mailer init(DEF_MAILER);
#endif
#ifdef USE_W3MMAILER
#define MAILTO_OPTIONS_USE_W3MMAILER 0
#endif
#define MAILTO_OPTIONS_IGNORE 1
#define MAILTO_OPTIONS_USE_MAILTO_URL 2
global int MailtoOptions init(MAILTO_OPTIONS_IGNORE);
global char *ExtBrowser init(DEF_EXT_BROWSER);
global char *ExtBrowser2 init(NULL);
global char *ExtBrowser3 init(NULL);
global int BackgroundExtViewer init(TRUE);
global int disable_secret_security_check init(FALSE);
global char *passwd_file init(PASSWD_FILE);
global char *pre_form_file init(PRE_FORM_FILE);
global char *ftppasswd init(NULL);
global int ftppass_hostnamegen init(TRUE);
global int do_download init(FALSE);
#ifdef USE_IMAGE
global char *image_source init(NULL);
#endif
global char *UserAgent init(NULL);
global int NoSendReferer init(FALSE);
global char *AcceptLang init(NULL);
global char *AcceptEncoding init(NULL);
global char *AcceptMedia init(NULL);

global int IgnoreCase init(TRUE);
global int WrapSearch init(FALSE);

global int UseExternalDirBuffer init(TRUE);
global char *DirBufferCommand init("file:///$LIB/dirlist" CGI_EXTENSION);
#ifdef USE_DICT
global int UseDictCommand init(FALSE);
global char *DictCommand init("file:///$LIB/w3mdict" CGI_EXTENSION);
#endif /* USE_DICT */
global int ignore_null_img_alt init(TRUE);
#define DISPLAY_INS_DEL_SIMPLE 0
#define DISPLAY_INS_DEL_NORMAL 1
#define DISPLAY_INS_DEL_FONTIFY 2
global int displayInsDel init(DISPLAY_INS_DEL_NORMAL);
global int FoldTextarea init(FALSE);

#define DEFAULT_URL_EMPTY 0
#define DEFAULT_URL_CURRENT 1
#define DEFAULT_URL_LINK 2
global int DefaultURLString init(DEFAULT_URL_EMPTY);
global int MarkAllPages init(FALSE);

#ifdef USE_MIGEMO
global int use_migemo init(FALSE);
global int migemo_active init(0);
global char *migemo_command init(DEF_MIGEMO_COMMAND);
#endif /* USE_MIGEMO */

global char *mailcap_files init(USER_MAILCAP ", " SYS_MAILCAP);
global char *mimetypes_files init(USER_MIMETYPES ", " SYS_MIMETYPES);
#ifdef USE_EXTERNAL_URI_LOADER
global char *urimethodmap_files init(USER_URIMETHODMAP ", " SYS_URIMETHODMAP);
#endif

global int multicolList init(FALSE);



global char SearchConv init(TRUE);
global char SimplePreserveSpace init(FALSE);






global char UseAltEntity init(TRUE);

extern const char *graph_symbol[];
extern const char *graph2_symbol[];
extern int symbol_width;
extern int symbol_width0;
#define N_GRAPH_SYMBOL 32
#define SYMBOL_BASE 0x20
global int no_rc_dir init(FALSE);
global char *rc_dir init(NULL);
global char *tmp_dir;

extern int mouseActive;
global int reverse_mouse init(FALSE);
global int relative_wheel_scroll init(FALSE);
global int fixed_wheel_scroll_count init(5);
global int relative_wheel_scroll_ratio init(30);
#define LIMIT_MOUSE_MENU 100

global int default_use_cookie init(TRUE);
global int show_cookie init(TRUE);
#define ACCEPT_BAD_COOKIE_DISCARD 0
#define ACCEPT_BAD_COOKIE_ACCEPT 1
#define ACCEPT_BAD_COOKIE_ASK 2
global int accept_bad_cookie init(ACCEPT_BAD_COOKIE_DISCARD);
global char *cookie_reject_domains init(NULL);
global char *cookie_accept_domains init(NULL);
global char *cookie_avoid_wrong_number_of_dots init(NULL);
global TextList *Cookie_reject_domains;
global TextList *Cookie_accept_domains;
global TextList *Cookie_avoid_wrong_number_of_dots_domains;

#ifdef USE_IMAGE
global int view_unseenobject init(FALSE);
#else
global int view_unseenobject init(TRUE);
#endif

#if defined(USE_SSL) && defined(USE_SSL_VERIFY)
global int ssl_verify_server init(FALSE);
global char *ssl_cert_file init(NULL);
global char *ssl_key_file init(NULL);
global char *ssl_ca_path init(NULL);
global char *ssl_ca_file init(NULL);
global int ssl_path_modified init(FALSE);
#endif /* defined(USE_SSL) && \
        * defined(USE_SSL_VERIFY) */
#ifdef USE_SSL
global char *ssl_forbid_method init(NULL);
#endif

global int is_redisplay init(FALSE);
global int clear_buffer init(TRUE);

global int use_lessopen init(FALSE);

global char *keymap_file init(KEYMAP_FILE);

global int FollowRedirection init(10);

global TextLineList *backend_halfdump_buf;

#endif /* not FM_H */
