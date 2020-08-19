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
#include "stream/url.h"

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

#define TMPF_DFL 0
#define TMPF_SRC 1
#define TMPF_FRAME 2
#define TMPF_CACHE 3
#define TMPF_COOKIE 4
#define MAX_TMPF_TYPE 5


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


extern const char *graph_symbol[];
extern const char *graph2_symbol[];
#define N_GRAPH_SYMBOL 32

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
#undef init

global TextLineList *backend_halfdump_buf;

#endif /* not FM_H */
