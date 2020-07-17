extern "C" {

#define MAINPROGRAM
#include "fm.h"
#include "public.h"
#include <signal.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <time.h>
#include "terms.h"
#include "myctype.h"
#include "regex.h"
#ifdef USE_M17N
#include "wc.h"
#include "wtf.h"
#ifdef USE_UNICODE
#include "ucs.h"
#endif
#endif
#ifdef USE_MOUSE
#ifdef USE_GPM
#include <gpm.h>
#endif				/* USE_GPM */
#if defined(USE_GPM) || defined(USE_SYSMOUSE)
extern int do_getch();
#define getch()	do_getch()
#endif				/* defined(USE_GPM) || defined(USE_SYSMOUSE) */
#endif

#ifdef __MINGW32_VERSION
#include <winsock.h>

WSADATA WSAData;
#endif

#define DSTR_LEN	256

Hist *LoadHist;
Hist *SaveHist;
Hist *URLHist;
Hist *ShellHist;
Hist *TextHist;

typedef struct _Event {
    int cmd;
    void *data;
    struct _Event *next;
} Event;
static Event *CurrentEvent = NULL;
static Event *LastEvent = NULL;


#ifdef SIGWINCH
static int need_resize_screen = FALSE;
static MySignalHandler resize_hook(SIGNAL_ARG);
static void resize_screen(void);
#endif

#ifdef SIGPIPE
static MySignalHandler SigPipe(SIGNAL_ARG);
#endif





static void keyPressEventProc(int c);
int show_params_p = 0;
void show_params(FILE * fp);
static int add_download_list = FALSE;
void set_buffer_environ(Buffer *);
static void save_buffer_position(Buffer *buf);

#define help() fusage(stdout, 0)
#define usage() fusage(stderr, 1)

static void
fversion(FILE * f)
{
    fprintf(f, "w3m version %s, options %s\n", w3m_version,
#if LANG == JA
	    "lang=ja"
#else
	    "lang=en"
#endif
#ifdef USE_M17N
	    ",m17n"
#endif
#ifdef USE_IMAGE
	    ",image"
#endif
#ifdef USE_COLOR
	    ",color"
#ifdef USE_ANSI_COLOR
	    ",ansi-color"
#endif
#endif
#ifdef USE_MOUSE
	    ",mouse"
#ifdef USE_GPM
	    ",gpm"
#endif
#ifdef USE_SYSMOUSE
	    ",sysmouse"
#endif
#endif
#ifdef USE_MENU
	    ",menu"
#endif
#ifdef USE_COOKIE
	    ",cookie"
#endif
#ifdef USE_SSL
	    ",ssl"
#ifdef USE_SSL_VERIFY
	    ",ssl-verify"
#endif
#endif
#ifdef USE_EXTERNAL_URI_LOADER
	    ",external-uri-loader"
#endif
#ifdef USE_W3MMAILER
	    ",w3mmailer"
#endif
#ifdef USE_NNTP
	    ",nntp"
#endif
#ifdef USE_GOPHER
	    ",gopher"
#endif
#ifdef INET6
	    ",ipv6"
#endif
#ifdef USE_ALARM
	    ",alarm"
#endif
#ifdef USE_MARK
	    ",mark"
#endif
#ifdef USE_MIGEMO
	    ",migemo"
#endif
	);
}

static void
fusage(FILE * f, int err)
{
    fversion(f);
    /* FIXME: gettextize? */
    fprintf(f, "usage: w3m [options] [URL or filename]\noptions:\n");
    fprintf(f, "    -t tab           set tab width\n");
    fprintf(f, "    -r               ignore backspace effect\n");
    fprintf(f, "    -l line          # of preserved line (default 10000)\n");
#ifdef USE_M17N
    fprintf(f, "    -I charset       document charset\n");
    fprintf(f, "    -O charset       display/output charset\n");
    fprintf(f, "    -e               EUC-JP\n");
    fprintf(f, "    -s               Shift_JIS\n");
    fprintf(f, "    -j               JIS\n");
#endif
    fprintf(f, "    -B               load bookmark\n");
    fprintf(f, "    -bookmark file   specify bookmark file\n");
    fprintf(f, "    -T type          specify content-type\n");
    fprintf(f, "    -m               internet message mode\n");
    fprintf(f, "    -v               visual startup mode\n");
#ifdef USE_COLOR
    fprintf(f, "    -M               monochrome display\n");
#endif				/* USE_COLOR */
    fprintf(f,
	    "    -N               open URL of command line on each new tab\n");
    fprintf(f, "    -F               automatically render frame\n");
    fprintf(f,
	    "    -cols width      specify column width (used with -dump)\n");
    fprintf(f,
	    "    -ppc count       specify the number of pixels per character (4.0...32.0)\n");
#ifdef USE_IMAGE
    fprintf(f,
	    "    -ppl count       specify the number of pixels per line (4.0...64.0)\n");
#endif
    fprintf(f, "    -dump            dump formatted page into stdout\n");
    fprintf(f,
	    "    -dump_head       dump response of HEAD request into stdout\n");
    fprintf(f, "    -dump_source     dump page source into stdout\n");
    fprintf(f, "    -dump_both       dump HEAD and source into stdout\n");
    fprintf(f,
	    "    -dump_extra      dump HEAD, source, and extra information into stdout\n");
    fprintf(f, "    -post file       use POST method with file content\n");
    fprintf(f, "    -header string   insert string as a header\n");
    fprintf(f, "    +<num>           goto <num> line\n");
    fprintf(f, "    -num             show line number\n");
    fprintf(f, "    -no-proxy        don't use proxy\n");
#ifdef INET6
    fprintf(f, "    -4               IPv4 only (-o dns_order=4)\n");
    fprintf(f, "    -6               IPv6 only (-o dns_order=6)\n");
#endif
#ifdef USE_MOUSE
    fprintf(f, "    -no-mouse        don't use mouse\n");
#endif				/* USE_MOUSE */
#ifdef USE_COOKIE
    fprintf(f,
	    "    -cookie          use cookie (-no-cookie: don't use cookie)\n");
#endif				/* USE_COOKIE */
    fprintf(f, "    -graph           use DEC special graphics for border of table and menu\n");
    fprintf(f, "    -no-graph        use ACII character for border of table and menu\n");
    fprintf(f, "    -S               squeeze multiple blank lines\n");
    fprintf(f, "    -W               toggle wrap search mode\n");
    fprintf(f, "    -X               don't use termcap init/deinit\n");
    fprintf(f,
	    "    -title[=TERM]    set buffer name to terminal title string\n");
    fprintf(f, "    -o opt=value     assign value to config option\n");
    fprintf(f, "    -show-option     print all config options\n");
    fprintf(f, "    -config file     specify config file\n");
    fprintf(f, "    -help            print this usage message\n");
    fprintf(f, "    -version         print w3m version\n");
    fprintf(f, "    -reqlog          write request logfile\n");
    fprintf(f, "    -debug           DO NOT USE\n");
    if (show_params_p)
	show_params(f);
    exit(err);
}

#ifdef USE_M17N
#ifdef __EMX__
static char *getCodePage(void);
#endif
#endif

static GC_warn_proc orig_GC_warn_proc = NULL;
#define GC_WARN_KEEP_MAX (20)

static void
wrap_GC_warn_proc(char *msg, GC_word arg)
{
    if (fmInitialized) {
	/* *INDENT-OFF* */
	static struct {
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

	if (!lock) {
	    lock = 1;

	    for (; n > 0; --n, ++i) {
		i %= sizeof(msg_ring) / sizeof(msg_ring[0]);

		printf(msg_ring[i].msg,	(unsigned long)msg_ring[i].arg);
		sleep_till_anykey(1, 1);
	    }

	    lock = 0;
	}
    }
    else if (orig_GC_warn_proc)
	orig_GC_warn_proc(msg, arg);
    else
	fprintf(stderr, msg, (unsigned long)arg);
}

#ifdef SIGCHLD
static void
sig_chld(int signo)
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

	if (WIFEXITED(p_stat)) {
	    for (d = FirstDL; d != NULL; d = d->next) {
		if (d->pid == pid) {
		    d->err = WEXITSTATUS(p_stat);
		    break;
		}
	    }
	}
    }
    mySignal(SIGCHLD, sig_chld);
    return;
}
#endif

Str
make_optional_header_string(char *s)
{
    char *p;
    Str hs;

    if (strchr(s, '\n') || strchr(s, '\r'))
	return NULL;
    for (p = s; *p && *p != ':'; p++) ;
    if (*p != ':' || p == s)
	return NULL;
    hs = Strnew_size(strlen(s) + 3);
    Strcopy_charp_n(hs, s, p - s);
    if (!Strcasecmp_charp(hs, "content-type"))
	override_content_type = TRUE;
    Strcat_charp(hs, ": ");
    if (*(++p)) {		/* not null header */
	SKIP_BLANKS(p);		/* skip white spaces */
	Strcat_charp(hs, p);
    }
    Strcat_charp(hs, "\r\n");
    return hs;
}

int
main(int argc, char **argv, char **envp)
{
    Buffer *newbuf = NULL;
    char *p, c;
    int i;
    InputStream redin;
    char *line_str = NULL;
    char **load_argv;
    FormList *request;
    int load_argc = 0;
    int load_bookmark = FALSE;
    int visual_start = FALSE;
    int open_new_tab = FALSE;
    char search_header = FALSE;
    char *default_type = NULL;
    char *post_file = NULL;
    Str err_msg;
#ifdef USE_M17N
    char *Locale = NULL;
    wc_uint8 auto_detect;
#ifdef __EMX__
    wc_ces CodePage;
#endif
#endif
    GC_INIT();
#if defined(ENABLE_NLS) || (defined(USE_M17N) && defined(HAVE_LANGINFO_CODESET))
    setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

#ifndef HAVE_SYS_ERRLIST
    prepare_sys_errlist();
#endif				/* not HAVE_SYS_ERRLIST */

    NO_proxy_domains = newTextList();
    fileToDelete = newTextList();

    load_argv = New_N(char *, argc - 1);
    load_argc = 0;

    CurrentDir = currentdir();
    CurrentPid = (int)getpid();
    BookmarkFile = NULL;
    config_file = NULL;

    /* argument search 1 */
    for (i = 1; i < argc; i++) {
	if (*argv[i] == '-') {
	    if (!strcmp("-config", argv[i])) {
		argv[i] = "-dummy";
		if (++i >= argc)
		    usage();
		config_file = argv[i];
		argv[i] = "-dummy";
	    }
	    else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]))
		help();
	    else if (!strcmp("-V", argv[i]) || !strcmp("-version", argv[i])) {
		fversion(stdout);
		exit(0);
	    }
	}
    }

#ifdef USE_M17N
    if (non_null(Locale = getenv("LC_ALL")) ||
	non_null(Locale = getenv("LC_CTYPE")) ||
	non_null(Locale = getenv("LANG"))) {
	DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
	DocumentCharset = wc_guess_locale_charset(Locale, DocumentCharset);
	SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }
#ifdef __EMX__
    CodePage = wc_guess_charset(getCodePage(), 0);
    if (CodePage)
	DisplayCharset = DocumentCharset = SystemCharset = CodePage;
#endif
#endif

    /* initializations */
    init_rc();

    LoadHist = newHist();
    SaveHist = newHist();
    ShellHist = newHist();
    TextHist = newHist();
    URLHist = newHist();

#ifdef USE_M17N
    if (FollowLocale && Locale) {
	DisplayCharset = wc_guess_locale_charset(Locale, DisplayCharset);
	SystemCharset = wc_guess_locale_charset(Locale, SystemCharset);
    }
    auto_detect = WcOption.auto_detect;
    BookmarkCharset = DocumentCharset;
#endif

    if (!non_null(HTTP_proxy) &&
	((p = getenv("HTTP_PROXY")) ||
	 (p = getenv("http_proxy")) || (p = getenv("HTTP_proxy"))))
	HTTP_proxy = p;
#ifdef USE_SSL
    if (!non_null(HTTPS_proxy) &&
	((p = getenv("HTTPS_PROXY")) ||
	 (p = getenv("https_proxy")) || (p = getenv("HTTPS_proxy"))))
	HTTPS_proxy = p;
    if (HTTPS_proxy == NULL && non_null(HTTP_proxy))
	HTTPS_proxy = HTTP_proxy;
#endif				/* USE_SSL */
#ifdef USE_GOPHER
    if (!non_null(GOPHER_proxy) &&
	((p = getenv("GOPHER_PROXY")) ||
	 (p = getenv("gopher_proxy")) || (p = getenv("GOPHER_proxy"))))
	GOPHER_proxy = p;
#endif				/* USE_GOPHER */
    if (!non_null(FTP_proxy) &&
	((p = getenv("FTP_PROXY")) ||
	 (p = getenv("ftp_proxy")) || (p = getenv("FTP_proxy"))))
	FTP_proxy = p;
    if (!non_null(NO_proxy) &&
	((p = getenv("NO_PROXY")) ||
	 (p = getenv("no_proxy")) || (p = getenv("NO_proxy"))))
	NO_proxy = p;
#ifdef USE_NNTP
    if (!non_null(NNTP_server) && (p = getenv("NNTPSERVER")) != NULL)
	NNTP_server = p;
    if (!non_null(NNTP_mode) && (p = getenv("NNTPMODE")) != NULL)
	NNTP_mode = p;
#endif

    if (!non_null(Editor) && (p = getenv("EDITOR")) != NULL)
	Editor = p;
    if (!non_null(Mailer) && (p = getenv("MAILER")) != NULL)
	Mailer = p;

    /* argument search 2 */
    i = 1;
    while (i < argc) {
	if (*argv[i] == '-') {
	    if (!strcmp("-t", argv[i])) {
		if (++i >= argc)
		    usage();
		if (atoi(argv[i]) > 0)
		    Tabstop = atoi(argv[i]);
	    }
	    else if (!strcmp("-r", argv[i]))
		ShowEffect = FALSE;
	    else if (!strcmp("-l", argv[i])) {
		if (++i >= argc)
		    usage();
		if (atoi(argv[i]) > 0)
		    PagerMax = atoi(argv[i]);
	    }
#ifdef USE_M17N
	    else if (!strcmp("-s", argv[i]))
		DisplayCharset = WC_CES_SHIFT_JIS;
	    else if (!strcmp("-j", argv[i]))
		DisplayCharset = WC_CES_ISO_2022_JP;
	    else if (!strcmp("-e", argv[i]))
		DisplayCharset = WC_CES_EUC_JP;
	    else if (!strncmp("-I", argv[i], 2)) {
		if (argv[i][2] != '\0')
		    p = argv[i] + 2;
		else {
		    if (++i >= argc)
			usage();
		    p = argv[i];
		}
		DocumentCharset = wc_guess_charset_short(p, DocumentCharset);
		WcOption.auto_detect = WC_OPT_DETECT_OFF;
		UseContentCharset = FALSE;
	    }
	    else if (!strncmp("-O", argv[i], 2)) {
		if (argv[i][2] != '\0')
		    p = argv[i] + 2;
		else {
		    if (++i >= argc)
			usage();
		    p = argv[i];
		}
		DisplayCharset = wc_guess_charset_short(p, DisplayCharset);
	    }
#endif
	    else if (!strcmp("-graph", argv[i]))
		UseGraphicChar = GRAPHIC_CHAR_DEC;
	    else if (!strcmp("-no-graph", argv[i]))
		UseGraphicChar = GRAPHIC_CHAR_ASCII;
	    else if (!strcmp("-T", argv[i])) {
		if (++i >= argc)
		    usage();
		DefaultType = default_type = argv[i];
	    }
	    else if (!strcmp("-m", argv[i]))
		SearchHeader = search_header = TRUE;
	    else if (!strcmp("-v", argv[i]))
		visual_start = TRUE;
	    else if (!strcmp("-N", argv[i]))
		open_new_tab = TRUE;
#ifdef USE_COLOR
	    else if (!strcmp("-M", argv[i]))
		useColor = FALSE;
#endif				/* USE_COLOR */
	    else if (!strcmp("-B", argv[i]))
		load_bookmark = TRUE;
	    else if (!strcmp("-bookmark", argv[i])) {
		if (++i >= argc)
		    usage();
		BookmarkFile = argv[i];
		if (BookmarkFile[0] != '~' && BookmarkFile[0] != '/') {
		    Str tmp = Strnew_charp(CurrentDir);
		    if (Strlastchar(tmp) != '/')
			Strcat_char(tmp, '/');
		    Strcat_charp(tmp, BookmarkFile);
		    BookmarkFile = cleanupName(tmp->ptr);
		}
	    }
	    else if (!strcmp("-F", argv[i]))
		RenderFrame = TRUE;
	    else if (!strcmp("-W", argv[i])) {
		if (WrapDefault)
		    WrapDefault = FALSE;
		else
		    WrapDefault = TRUE;
	    }
	    else if (!strcmp("-dump", argv[i]))
		w3m_dump = DUMP_BUFFER;
	    else if (!strcmp("-dump_source", argv[i]))
		w3m_dump = DUMP_SOURCE;
	    else if (!strcmp("-dump_head", argv[i]))
		w3m_dump = DUMP_HEAD;
	    else if (!strcmp("-dump_both", argv[i]))
		w3m_dump = (DUMP_HEAD | DUMP_SOURCE);
	    else if (!strcmp("-dump_extra", argv[i]))
		w3m_dump = (DUMP_HEAD | DUMP_SOURCE | DUMP_EXTRA);
	    else if (!strcmp("-halfdump", argv[i]))
		w3m_dump = DUMP_HALFDUMP;
	    else if (!strcmp("-halfload", argv[i])) {
		w3m_dump = 0;
		w3m_halfload = TRUE;
		DefaultType = default_type = "text/html";
	    }
	    else if (!strcmp("-backend", argv[i])) {
		w3m_backend = TRUE;
	    }
	    else if (!strcmp("-backend_batch", argv[i])) {
		w3m_backend = TRUE;
		if (++i >= argc)
		    usage();
		if (!backend_batch_commands)
		    backend_batch_commands = newTextList();
		pushText(backend_batch_commands, argv[i]);
	    }
	    else if (!strcmp("-cols", argv[i])) {
		if (++i >= argc)
		    usage();
		COLS = atoi(argv[i]);
		if (COLS > MAXIMUM_COLS) {
		    COLS = MAXIMUM_COLS;
		}
	    }
	    else if (!strcmp("-ppc", argv[i])) {
		double ppc;
		if (++i >= argc)
		    usage();
		ppc = atof(argv[i]);
		if (ppc >= MINIMUM_PIXEL_PER_CHAR &&
		    ppc <= MAXIMUM_PIXEL_PER_CHAR) {
		    pixel_per_char = ppc;
		    set_pixel_per_char = TRUE;
		}
	    }
#ifdef USE_IMAGE
	    else if (!strcmp("-ppl", argv[i])) {
		double ppc;
		if (++i >= argc)
		    usage();
		ppc = atof(argv[i]);
		if (ppc >= MINIMUM_PIXEL_PER_CHAR &&
		    ppc <= MAXIMUM_PIXEL_PER_CHAR * 2) {
		    pixel_per_line = ppc;
		    set_pixel_per_line = TRUE;
		}
	    }
#endif
	    else if (!strcmp("-num", argv[i]))
		showLineNum = TRUE;
	    else if (!strcmp("-no-proxy", argv[i]))
		use_proxy = FALSE;
#ifdef INET6
	    else if (!strcmp("-4", argv[i]) || !strcmp("-6", argv[i]))
		set_param_option(Sprintf("dns_order=%c", argv[i][1])->ptr);
#endif
	    else if (!strcmp("-post", argv[i])) {
		if (++i >= argc)
		    usage();
		post_file = argv[i];
	    }
	    else if (!strcmp("-header", argv[i])) {
		Str hs;
		if (++i >= argc)
		    usage();
		if ((hs = make_optional_header_string(argv[i])) != NULL) {
		    if (header_string == NULL)
			header_string = hs;
		    else
			Strcat(header_string, hs);
		}
		while (argv[i][0]) {
		    argv[i][0] = '\0';
		    argv[i]++;
		}
	    }
#ifdef USE_MOUSE
	    else if (!strcmp("-no-mouse", argv[i])) {
		use_mouse = FALSE;
	    }
#endif				/* USE_MOUSE */
#ifdef USE_COOKIE
	    else if (!strcmp("-no-cookie", argv[i])) {
		use_cookie = FALSE;
		accept_cookie = FALSE;
	    }
	    else if (!strcmp("-cookie", argv[i])) {
		use_cookie = TRUE;
		accept_cookie = TRUE;
	    }
#endif				/* USE_COOKIE */
	    else if (!strcmp("-S", argv[i]))
		squeezeBlankLine = TRUE;
	    else if (!strcmp("-X", argv[i]))
		Do_not_use_ti_te = TRUE;
	    else if (!strcmp("-title", argv[i]))
		displayTitleTerm = getenv("TERM");
	    else if (!strncmp("-title=", argv[i], 7))
		displayTitleTerm = argv[i] + 7;
	    else if (!strcmp("-o", argv[i]) ||
		     !strcmp("-show-option", argv[i])) {
		if (!strcmp("-show-option", argv[i]) || ++i >= argc ||
		    !strcmp(argv[i], "?")) {
		    show_params(stdout);
		    exit(0);
		}
		if (!set_param_option(argv[i])) {
		    /* option set failed */
		    /* FIXME: gettextize? */
		    fprintf(stderr, "%s: bad option\n", argv[i]);
		    show_params_p = 1;
		    usage();
		}
	    }
	    else if (!strcmp("-dummy", argv[i])) {
		/* do nothing */
	    }
	    else if (!strcmp("-debug", argv[i])) {
		w3m_debug = TRUE;
	    }
	    else if (!strcmp("-reqlog",argv[i])) {
		w3m_reqlog=rcFile("request.log");
	    }
	    else {
		usage();
	    }
	}
	else if (*argv[i] == '+') {
	    line_str = argv[i] + 1;
	}
	else {
	    load_argv[load_argc++] = argv[i];
	}
	i++;
    }

#ifdef	__WATT32__
    if (w3m_debug)
	dbug_init();
    sock_init();
#endif

#ifdef __MINGW32_VERSION
    {
      int err;
      WORD wVerReq;

      wVerReq = MAKEWORD(1, 1);

      err = WSAStartup(wVerReq, &WSAData);
      if (err != 0)
        {
	  fprintf(stderr, "Can't find winsock\n");
	  return 1;
        }
      _fmode = _O_BINARY;
    }
#endif

    FirstTab = NULL;
    LastTab = NULL;
    nTab = 0;
    CurrentTab = NULL;
    CurrentKey = -1;
    if (BookmarkFile == NULL)
	BookmarkFile = rcFile(BOOKMARK);

    if (!isatty(1) && !w3m_dump) {
	/* redirected output */
	w3m_dump = DUMP_BUFFER;
    }
    if (w3m_dump) {
	if (COLS == 0)
	    COLS = DEFAULT_COLS;
    }

#ifdef USE_BINMODE_STREAM
    setmode(fileno(stdout), O_BINARY);
#endif
    if (!w3m_dump && !w3m_backend) {
	fmInit();
#ifdef SIGWINCH
	mySignal(SIGWINCH, resize_hook);
#else				/* not SIGWINCH */
	setlinescols();
	setupscreen();
#endif				/* not SIGWINCH */
    }
#ifdef USE_IMAGE
    else if (w3m_halfdump && displayImage)
	activeImage = TRUE;
#endif

    sync_with_option();
#ifdef USE_COOKIE
    initCookie();
#endif				/* USE_COOKIE */
#ifdef USE_HISTORY
    if (UseHistory)
	loadHistory(URLHist);
#endif				/* not USE_HISTORY */

#ifdef USE_M17N
    wtf_init(DocumentCharset, DisplayCharset);
    /*  if (w3m_dump)
     *    WcOption.pre_conv = WC_TRUE; 
     */
#endif

    if (w3m_backend)
	backend();

    if (w3m_dump)
	mySignal(SIGINT, SIG_IGN);
#ifdef SIGCHLD
    mySignal(SIGCHLD, sig_chld);
#endif
#ifdef SIGPIPE
    mySignal(SIGPIPE, SigPipe);
#endif

    /*orig_GC_warn_proc =*/ GC_set_warn_proc(wrap_GC_warn_proc);
    err_msg = Strnew();
    if (load_argc == 0) {
	/* no URL specified */
	if (!isatty(0)) {
	    redin = newFileStream(fdopen(dup(0), "rb"), (void (*)())pclose);
	    newbuf = openGeneralPagerBuffer(redin);
	    dup2(1, 0);
	}
	else if (load_bookmark) {
	    newbuf = loadGeneralFile(BookmarkFile, NULL, NO_REFERER, 0, NULL);
	    if (newbuf == NULL)
		Strcat_charp(err_msg, "w3m: Can't load bookmark.\n");
	}
	else if (visual_start) {
	    /* FIXME: gettextize? */
	    Str s_page;
	    s_page =
		Strnew_charp
		("<title>W3M startup page</title><center><b>Welcome to ");
	    Strcat_charp(s_page, "<a href='http://w3m.sourceforge.net/'>");
	    Strcat_m_charp(s_page,
			   "w3m</a>!<p><p>This is w3m version ",
			   w3m_version,
			   "<br>Written by <a href='mailto:aito@fw.ipsj.or.jp'>Akinori Ito</a>",
			   NULL);
	    newbuf = loadHTMLString(s_page);
	    if (newbuf == NULL)
		Strcat_charp(err_msg, "w3m: Can't load string.\n");
	    else if (newbuf != NO_BUFFER)
		newbuf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
	}
	else if ((p = getenv("HTTP_HOME")) != NULL ||
		 (p = getenv("WWW_HOME")) != NULL) {
	    newbuf = loadGeneralFile(p, NULL, NO_REFERER, 0, NULL);
	    if (newbuf == NULL)
		Strcat(err_msg, Sprintf("w3m: Can't load %s.\n", p));
	    else if (newbuf != NO_BUFFER)
		pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
	}
	else {
	    if (fmInitialized)
		fmTerm();
	    usage();
	}
	if (newbuf == NULL) {
	    if (fmInitialized)
		fmTerm();
	    if (err_msg->length)
		fprintf(stderr, "%s", err_msg->ptr);
	    w3m_exit(2);
	}
	i = -1;
    }
    else {
	i = 0;
    }
    for (; i < load_argc; i++) {
	if (i >= 0) {
	    SearchHeader = search_header;
	    DefaultType = default_type;
	    if (w3m_dump == DUMP_HEAD) {
		request = New(FormList);
		request->method = FORM_METHOD_HEAD;
		newbuf =
		    loadGeneralFile(load_argv[i], NULL, NO_REFERER, 0,
				    request);
	    }
	    else {
		if (post_file && i == 0) {
		    FILE *fp;
		    Str body;
		    if (!strcmp(post_file, "-"))
			fp = stdin;
		    else
			fp = fopen(post_file, "r");
		    if (fp == NULL) {
			/* FIXME: gettextize? */
			Strcat(err_msg,
			       Sprintf("w3m: Can't open %s.\n", post_file));
			continue;
		    }
		    body = Strfgetall(fp);
		    if (fp != stdin)
			fclose(fp);
		    request =
			newFormList(NULL, "post", NULL, NULL, NULL, NULL,
				    NULL);
		    request->body = body->ptr;
		    request->boundary = NULL;
		    request->length = body->length;
		}
		else {
		    request = NULL;
		}
		newbuf =
		    loadGeneralFile(load_argv[i], NULL, NO_REFERER, 0,
				    request);
	    }
	    if (newbuf == NULL) {
		/* FIXME: gettextize? */
		Strcat(err_msg,
		       Sprintf("w3m: Can't load %s.\n", load_argv[i]));
		continue;
	    }
	    else if (newbuf == NO_BUFFER)
		continue;
	    switch (newbuf->real_scheme) {
	    case SCM_MAILTO:
		break;
	    case SCM_LOCAL:
	    case SCM_LOCAL_CGI:
		unshiftHist(LoadHist, conv_from_system(load_argv[i]));
	    default:
		pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
		break;
	    }
	}
	else if (newbuf == NO_BUFFER)
	    continue;
	if (newbuf->pagerSource ||
	    (newbuf->real_scheme == SCM_LOCAL && newbuf->header_source &&
	     newbuf->currentURL.file && strcmp(newbuf->currentURL.file, "-")))
	    newbuf->search_header = search_header;
	if (CurrentTab == NULL) {
	    FirstTab = LastTab = CurrentTab = newTab();
	    nTab = 1;
	    Firstbuf = Currentbuf = newbuf;
	}
	else if (open_new_tab) {
	    _newT();
	    Currentbuf->nextBuffer = newbuf;
	    delBuffer(Currentbuf);
	}
	else {
	    Currentbuf->nextBuffer = newbuf;
	    Currentbuf = newbuf;
	}
	if (!w3m_dump || w3m_dump == DUMP_BUFFER) {
	    if (Currentbuf->frameset != NULL && RenderFrame)
		rFrame();
	}
	if (w3m_dump)
	    do_dump(Currentbuf);
	else {
	    Currentbuf = newbuf;
#ifdef USE_BUFINFO
	    saveBufferInfo();
#endif
	}
    }
    if (w3m_dump) {
	if (err_msg->length)
	    fprintf(stderr, "%s", err_msg->ptr);
#ifdef USE_COOKIE
	save_cookies();
#endif				/* USE_COOKIE */
	w3m_exit(0);
    }

    if (add_download_list) {
	add_download_list = FALSE;
	CurrentTab = LastTab;
	if (!FirstTab) {
	    FirstTab = LastTab = CurrentTab = newTab();
	    nTab = 1;
	}
	if (!Firstbuf || Firstbuf == NO_BUFFER) {
	    Firstbuf = Currentbuf = newBuffer(INIT_BUFFER_WIDTH);
	    Currentbuf->bufferprop = BP_INTERNAL | BP_NO_URL;
	    Currentbuf->buffername = DOWNLOAD_LIST_TITLE;
	}
	else
	    Currentbuf = Firstbuf;
	ldDL();
    }
    else
	CurrentTab = FirstTab;
    if (!FirstTab || !Firstbuf || Firstbuf == NO_BUFFER) {
	if (newbuf == NO_BUFFER) {
	    if (fmInitialized)
		/* FIXME: gettextize? */
		inputChar("Hit any key to quit w3m:");
	}
	if (fmInitialized)
	    fmTerm();
	if (err_msg->length)
	    fprintf(stderr, "%s", err_msg->ptr);
	if (newbuf == NO_BUFFER) {
#ifdef USE_COOKIE
	    save_cookies();
#endif				/* USE_COOKIE */
	    if (!err_msg->length)
		w3m_exit(0);
	}
	w3m_exit(2);
    }
    if (err_msg->length)
	disp_message_nsec(err_msg->ptr, FALSE, 1, TRUE, FALSE);

    SearchHeader = FALSE;
    DefaultType = NULL;
#ifdef USE_M17N
    UseContentCharset = TRUE;
    WcOption.auto_detect = auto_detect;
#endif

    Currentbuf = Firstbuf;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    if (line_str) {
	_goLine(line_str);
    }
    for (;;) {
	if (add_download_list) {
	    add_download_list = FALSE;
	    ldDL();
	}
	if (Currentbuf->submit) {
	    Anchor *a = Currentbuf->submit;
	    Currentbuf->submit = NULL;
	    gotoLine(Currentbuf, a->start.line);
	    Currentbuf->pos = a->start.pos;
	    _followForm(TRUE);
	    continue;
	}
	/* event processing */
	if (CurrentEvent) {
	    CurrentKey = -1;
	    CurrentKeyData = NULL;
	    CurrentCmdData = (char *)CurrentEvent->data;
	    w3mFuncList[CurrentEvent->cmd].func();
	    CurrentCmdData = NULL;
	    CurrentEvent = CurrentEvent->next;
	    continue;
	}
	/* get keypress event */
#ifdef USE_ALARM
	if (Currentbuf->event) {
	    if (Currentbuf->event->status != AL_UNSET) {
		SetCurrentAlarm(Currentbuf->event);
		if (CurrentAlarm()->sec == 0) {	/* refresh (0sec) */
		    Currentbuf->event = NULL;
		    CurrentKey = -1;
		    CurrentKeyData = NULL;
		    CurrentCmdData = (char *)CurrentAlarm()->data;
		    w3mFuncList[CurrentAlarm()->cmd].func();
		    CurrentCmdData = NULL;
		    continue;
		}
	    }
	    else
		Currentbuf->event = NULL;
	}
	if (!Currentbuf->event)
	    SetCurrentAlarm(DefaultAlarm());
#endif
#ifdef USE_MOUSE
	mouse_action.in_action = FALSE;
	if (use_mouse)
	    mouse_active();
#endif				/* USE_MOUSE */
#ifdef USE_ALARM
	if (CurrentAlarm()->sec > 0) {
	    mySignal(SIGALRM, SigAlarm);
	    alarm(CurrentAlarm()->sec);
	}
#endif
#ifdef SIGWINCH
	mySignal(SIGWINCH, resize_hook);
#endif
#ifdef USE_IMAGE
	if (activeImage && displayImage && Currentbuf->img &&
	    !Currentbuf->image_loaded) {
	    do {
#ifdef SIGWINCH
		if (need_resize_screen)
		    resize_screen();
#endif
		loadImage(Currentbuf, IMG_FLAG_NEXT);
	    } while (sleep_till_anykey(1, 0) <= 0);
	}
#ifdef SIGWINCH
	else
#endif
#endif
#ifdef SIGWINCH
	{
	    do {
		if (need_resize_screen)
		    resize_screen();
	    } while (sleep_till_anykey(1, 0) <= 0);
	}
#endif
	c = getch();
#ifdef USE_ALARM
	if (CurrentAlarm()->sec > 0) {
	    alarm(0);
	}
#endif
#ifdef USE_MOUSE
	if (use_mouse)
	    mouse_inactive();
#endif				/* USE_MOUSE */
	if (IS_ASCII(c)) {	/* Ascii */
	    if (('0' <= c) && (c <= '9') &&
		(prec_num() || (GlobalKeymap[c] == FUNCNAME_nulcmd))) {
		set_prec_num(prec_num() * 10 + (int)(c - '0'));
		if (prec_num() > PREC_LIMIT())
		   set_prec_num( PREC_LIMIT());
	    }
	    else {
		set_buffer_environ(Currentbuf);
		save_buffer_position(Currentbuf);
		keyPressEventProc((int)c);
		set_prec_num(0);
	    }
	}
	set_prev_key(CurrentKey);
	CurrentKey = -1;
	CurrentKeyData = NULL;
    }
}

static void
keyPressEventProc(int c)
{
    CurrentKey = c;
    w3mFuncList[(int)GlobalKeymap[c]].func();
}

void
pushEvent(int cmd, void *data)
{
    Event *event;

    event = New(Event);
    event->cmd = cmd;
    event->data = data;
    event->next = NULL;
    if (CurrentEvent)
	LastEvent->next = event;
    else
	CurrentEvent = event;
    LastEvent = event;
}

void
escdmap(char c)
{
    int d;
    d = (int)c - (int)'0';
    c = getch();
    if (IS_DIGIT(c)) {
	d = d * 10 + (int)c - (int)'0';
	c = getch();
    }
    if (c == '~')
	escKeyProc((int)d, K_ESCD, EscDKeymap);
}

void
tmpClearBuffer(Buffer *buf)
{
    if (buf->pagerSource == NULL && writeBufferCache(buf) == 0) {
	buf->firstLine = NULL;
	buf->topLine = NULL;
	buf->currentLine = NULL;
	buf->lastLine = NULL;
    }
}



#ifdef USE_BUFINFO
void
saveBufferInfo()
{
    FILE *fp;

    if (w3m_dump)
	return;
    if ((fp = fopen(rcFile("bufinfo"), "w")) == NULL) {
	return;
    }
    fprintf(fp, "%s\n", currentURL()->ptr);
    fclose(fp);
}
#endif




#ifdef SIGWINCH
static MySignalHandler
resize_hook(SIGNAL_ARG)
{
    need_resize_screen = TRUE;
    mySignal(SIGWINCH, resize_hook);
    SIGNAL_RETURN;
}

static void
resize_screen(void)
{
    need_resize_screen = FALSE;
    setlinescols();
    setupscreen();
    if (CurrentTab)
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
}
#endif				/* SIGWINCH */

#ifdef SIGPIPE
static MySignalHandler
SigPipe(SIGNAL_ARG)
{
#ifdef USE_MIGEMO
    init_migemo();
#endif
    mySignal(SIGPIPE, SigPipe);
    SIGNAL_RETURN;
}
#endif





/* process form */
void
followForm(void)
{
    _followForm(FALSE);
}

void
follow_map(struct parsed_tagarg *arg)
{
    char *name = tag_get_value(arg, "link");
#if defined(MENU_MAP) || defined(USE_IMAGE)
    Anchor *an;
    MapArea *a;
    int x, y;
    ParsedURL p_url;

    an = retrieveCurrentImg(Currentbuf);
    x = Currentbuf->cursorX + Currentbuf->rootX;
    y = Currentbuf->cursorY + Currentbuf->rootY;
    a = follow_map_menu(Currentbuf, name, an, x, y);
    if (a == NULL || a->url == NULL || *(a->url) == '\0') {
#endif
#ifndef MENU_MAP
	Buffer *buf = follow_map_panel(Currentbuf, name);

	if (buf != NULL)
	    cmd_loadBuffer(buf, BP_NORMAL, LB_NOLINK);
#endif
#if defined(MENU_MAP) || defined(USE_IMAGE)
	return;
    }
    if (*(a->url) == '#') {
	gotoLabel(a->url + 1);
	return;
    }
    parseURL2(a->url, &p_url, baseURL(Currentbuf));
    pushHashHist(URLHist, parsedURL2Str(&p_url)->ptr);
    if (check_target && open_tab_blank && a->target &&
	(!strcasecmp(a->target, "_new") || !strcasecmp(a->target, "_blank"))) {
	Buffer *buf;

	_newT();
	buf = Currentbuf;
	cmd_loadURL(a->url, baseURL(Currentbuf),
		    parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
	if (buf != Currentbuf)
	    delBuffer(buf);
	else
	    deleteTab(CurrentTab);
	displayBuffer(Currentbuf, B_FORCE_REDRAW);
	return;
    }
    cmd_loadURL(a->url, baseURL(Currentbuf),
		parsedURL2Str(&Currentbuf->currentURL)->ptr, NULL);
#endif
}






#ifdef USE_M17N

void
change_charset(struct parsed_tagarg *arg)
{
    Buffer *buf = Currentbuf->linkBuffer[LB_N_INFO];
    wc_ces charset;

    if (buf == NULL)
	return;
    delBuffer(Currentbuf);
    Currentbuf = buf;
    if (Currentbuf->bufferprop & BP_INTERNAL)
	return;
    charset = Currentbuf->document_charset;
    for (; arg; arg = arg->next) {
	if (!strcmp(arg->arg, "charset"))
	    charset = atoi(arg->value);
    }
    _docCSet(charset);
}


#endif

/* mark URL-like patterns as anchors */
void
chkURLBuffer(Buffer *buf)
{
    static char *url_like_pat[] = {
	"https?://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*[a-zA-Z0-9_/=\\-]",
	"file:/[a-zA-Z0-9:%\\-\\./=_\\+@#,\\$;]*",
#ifdef USE_GOPHER
	"gopher://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
#endif				/* USE_GOPHER */
	"ftp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*[a-zA-Z0-9_/]",
#ifdef USE_NNTP
	"news:[^<> 	][^<> 	]*",
	"nntp://[a-zA-Z0-9][a-zA-Z0-9:%\\-\\./_]*",
#endif				/* USE_NNTP */
#ifndef USE_W3MMAILER		/* see also chkExternalURIBuffer() */
	"mailto:[^<> 	][^<> 	]*@[a-zA-Z0-9][a-zA-Z0-9\\-\\._]*[a-zA-Z0-9]",
#endif
#ifdef INET6
	"https?://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./?=~_\\&+@#,\\$;]*",
	"ftp://[a-zA-Z0-9:%\\-\\./_@]*\\[[a-fA-F0-9:][a-fA-F0-9:\\.]*\\][a-zA-Z0-9:%\\-\\./=_+@#,\\$]*",
#endif				/* INET6 */
	NULL
    };
    int i;
    for (i = 0; url_like_pat[i]; i++) {
	reAnchor(buf, url_like_pat[i]);
    }
#ifdef USE_EXTERNAL_URI_LOADER
    chkExternalURIBuffer(buf);
#endif
    buf->check_url |= CHK_URL;
}



#ifdef USE_NNTP
/* mark Message-ID-like patterns as NEWS anchors */
void
chkNMIDBuffer(Buffer *buf)
{
    static char *url_like_pat[] = {
	"<[!-;=?-~]+@[a-zA-Z0-9\\.\\-_]+>",
	NULL,
    };
    int i;
    for (i = 0; url_like_pat[i]; i++) {
	reAnchorNews(buf, url_like_pat[i]);
    }
    buf->check_url |= CHK_NMID;
}

#endif				/* USE_NNTP */

#ifdef USE_MOUSE







#ifdef USE_GPM
int
gpm_process_mouse(Gpm_Event * event, void *data)
{
    int btn = MOUSE_BTN_RESET, x, y;
    if (event->type & GPM_UP)
	btn = MOUSE_BTN_UP;
    else if (event->type & GPM_DOWN) {
	switch (event->buttons) {
	case GPM_B_LEFT:
	    btn = MOUSE_BTN1_DOWN;
	    break;
	case GPM_B_MIDDLE:
	    btn = MOUSE_BTN2_DOWN;
	    break;
	case GPM_B_RIGHT:
	    btn = MOUSE_BTN3_DOWN;
	    break;
	}
    }
    else {
	GPM_DRAWPOINTER(event);
	return 0;
    }
    x = event->x;
    y = event->y;
    process_mouse(btn, x - 1, y - 1);
    return 0;
}
#endif				/* USE_GPM */

#ifdef USE_SYSMOUSE
int
sysm_process_mouse(int x, int y, int nbs, int obs)
{
    int btn;
    int bits;

    if (obs & ~nbs)
	btn = MOUSE_BTN_UP;
    else if (nbs & ~obs) {
	bits = nbs & ~obs;
	btn = bits & 0x1 ? MOUSE_BTN1_DOWN :
	    (bits & 0x2 ? MOUSE_BTN2_DOWN :
	     (bits & 0x4 ? MOUSE_BTN3_DOWN : 0));
    }
    else			/* nbs == obs */
	return 0;
    process_mouse(btn, x, y);
    return 0;
}
#endif				/* USE_SYSMOUSE */

#endif				/* USE_MOUSE */

void
set_buffer_environ(Buffer *buf)
{
    static Buffer *prev_buf = NULL;
    static Line *prev_line = NULL;
    static int prev_pos = -1;
    Line *l;

    if (buf == NULL)
	return;
    if (buf != prev_buf) {
	set_environ("W3M_SOURCEFILE", buf->sourcefile);
	set_environ("W3M_FILENAME", buf->filename);
	set_environ("W3M_TITLE", buf->buffername);
	set_environ("W3M_URL", parsedURL2Str(&buf->currentURL)->ptr);
	set_environ("W3M_TYPE", (char*)(buf->real_type ? buf->real_type : "unknown"));
#ifdef USE_M17N
	set_environ("W3M_CHARSET", wc_ces_to_charset(buf->document_charset));
#endif
    }
    l = buf->currentLine;
    if (l && (buf != prev_buf || l != prev_line || buf->pos != prev_pos)) {
	Anchor *a;
	ParsedURL pu;
	char *s = GetWord(buf);
	set_environ("W3M_CURRENT_WORD", (char*)(s ? s : ""));
	a = retrieveCurrentAnchor(buf);
	if (a) {
	    parseURL2(a->url, &pu, baseURL(buf));
	    set_environ("W3M_CURRENT_LINK", parsedURL2Str(&pu)->ptr);
	}
	else
	    set_environ("W3M_CURRENT_LINK", "");
	a = retrieveCurrentImg(buf);
	if (a) {
	    parseURL2(a->url, &pu, baseURL(buf));
	    set_environ("W3M_CURRENT_IMG", parsedURL2Str(&pu)->ptr);
	}
	else
	    set_environ("W3M_CURRENT_IMG", "");
	a = retrieveCurrentForm(buf);
	if (a)
	    set_environ("W3M_CURRENT_FORM", form2str((FormItemList *)a->url));
	else
	    set_environ("W3M_CURRENT_FORM", "");
	set_environ("W3M_CURRENT_LINE", Sprintf("%d",
						l->real_linenumber)->ptr);
	set_environ("W3M_CURRENT_COLUMN", Sprintf("%d",
						  buf->currentColumn +
						  buf->cursorX + 1)->ptr);
    }
    else if (!l) {
	set_environ("W3M_CURRENT_WORD", "");
	set_environ("W3M_CURRENT_LINK", "");
	set_environ("W3M_CURRENT_IMG", "");
	set_environ("W3M_CURRENT_FORM", "");
	set_environ("W3M_CURRENT_LINE", "0");
	set_environ("W3M_CURRENT_COLUMN", "0");
    }
    prev_buf = buf;
    prev_line = l;
    prev_pos = buf->pos;
}

char *
searchKeyData(void)
{
    char *data = NULL;

    if (CurrentKeyData != NULL && *CurrentKeyData != '\0')
	data = CurrentKeyData;
    else if (CurrentCmdData != NULL && *CurrentCmdData != '\0')
	data = CurrentCmdData;
    else if (CurrentKey >= 0)
	data = getKeyData(CurrentKey);
    CurrentKeyData = NULL;
    CurrentCmdData = NULL;
    if (data == NULL || *data == '\0')
	return NULL;
    return allocStr(data, -1);
}

#ifdef __EMX__
#ifdef USE_M17N
static char *
getCodePage(void)
{
    unsigned long CpList[8], CpSize;

    if (!getenv("WINDOWID") && !DosQueryCp(sizeof(CpList), CpList, &CpSize))
	return Sprintf("CP%d", *CpList)->ptr;
    return NULL;
}
#endif
#endif

void
deleteFiles()
{
    Buffer *buf;
    char *f;

    for (CurrentTab = FirstTab; CurrentTab; CurrentTab = CurrentTab->nextTab) {
	while (Firstbuf && Firstbuf != NO_BUFFER) {
	    buf = Firstbuf->nextBuffer;
	    discardBuffer(Firstbuf);
	    Firstbuf = buf;
	}
    }
    while ((f = popText(fileToDelete)) != NULL)
	unlink(f);
}

void
w3m_exit(int i)
{
#ifdef USE_MIGEMO
    init_migemo();		/* close pipe to migemo */
#endif
    stopDownload();
    deleteFiles();
#ifdef USE_SSL
    free_ssl_ctx();
#endif
    disconnectFTP();
#ifdef USE_NNTP
    disconnectNews();
#endif
#ifdef __MINGW32_VERSION
    WSACleanup();
#endif
    exit(i);
}


#ifdef USE_ALARM



AlarmEvent *
setAlarmEvent(AlarmEvent * event, int sec, short status, int cmd, void *data)
{
    if (event == NULL)
	event = New(AlarmEvent);
    event->sec = sec;
    event->status = status;
    event->cmd = cmd;
    event->data = data;
    return event;
}
#endif



TabBuffer *
newTab(void)
{
    TabBuffer *n;

    n = New(TabBuffer);
    if (n == NULL)
	return NULL;
    n->nextTab = NULL;
    n->currentBuffer = NULL;
    n->firstBuffer = NULL;
    return n;
}




void
calcTabPos(void)
{
    TabBuffer *tab;
#if 0
    int lcol = 0, rcol = 2, col;
#else
    int lcol = 0, rcol = 0, col;
#endif
    int n1, n2, na, nx, ny, ix, iy;

#ifdef USE_MOUSE
    lcol = mouse_action.menu_str ? mouse_action.menu_width : 0;
#endif

    if (nTab <= 0)
	return;
    n1 = (COLS - rcol - lcol) / TabCols;
    if (n1 >= nTab) {
	n2 = 1;
	ny = 1;
    }
    else {
	if (n1 < 0)
	    n1 = 0;
	n2 = COLS / TabCols;
	if (n2 == 0)
	    n2 = 1;
	ny = (nTab - n1 - 1) / n2 + 2;
    }
    na = n1 + n2 * (ny - 1);
    n1 -= (na - nTab) / ny;
    if (n1 < 0)
	n1 = 0;
    na = n1 + n2 * (ny - 1);
    tab = FirstTab;
    for (iy = 0; iy < ny && tab; iy++) {
	if (iy == 0) {
	    nx = n1;
	    col = COLS - rcol - lcol;
	}
	else {
	    nx = n2 - (na - nTab + (iy - 1)) / (ny - 1);
	    col = COLS;
	}
	for (ix = 0; ix < nx && tab; ix++, tab = tab->nextTab) {
	    tab->x1 = col * ix / nx;
	    tab->x2 = col * (ix + 1) / nx - 1;
	    tab->y = iy;
	    if (iy == 0) {
		tab->x1 += lcol;
		tab->x2 += lcol;
	    }
	}
    }
}

TabBuffer *
deleteTab(TabBuffer * tab)
{
    Buffer *buf, *next;

    if (nTab <= 1)
	return FirstTab;
    if (tab->prevTab) {
	if (tab->nextTab)
	    tab->nextTab->prevTab = tab->prevTab;
	else
	    LastTab = tab->prevTab;
	tab->prevTab->nextTab = tab->nextTab;
	if (tab == CurrentTab)
	    CurrentTab = tab->prevTab;
    }
    else {			/* tab == FirstTab */
	tab->nextTab->prevTab = NULL;
	FirstTab = tab->nextTab;
	if (tab == CurrentTab)
	    CurrentTab = tab->nextTab;
    }
    nTab--;
    buf = tab->firstBuffer;
    while (buf && buf != NO_BUFFER) {
	next = buf->nextBuffer;
	discardBuffer(buf);
	buf = next;
    }
    return FirstTab;
}

void
addDownloadList(pid_t pid, char *url, char *save, char *lock, clen_t size)
{
    DownloadList *d;

    d = New(DownloadList);
    d->pid = pid;
    d->url = url;
    if (save[0] != '/' && save[0] != '~')
	save = Strnew_m_charp(CurrentDir, "/", save, NULL)->ptr;
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
    add_download_list = TRUE;
}

int
checkDownloadList(void)
{
    DownloadList *d;
    struct stat st;

    if (!FirstDL)
	return FALSE;
    for (d = FirstDL; d != NULL; d = d->next) {
	if (d->running && !lstat(d->lock, &st))
	    return TRUE;
    }
    return FALSE;
}

static char *
convert_size3(clen_t size)
{
    Str tmp = Strnew();
    int n;

    do {
	n = size % 1000;
	size /= 1000;
	tmp = Sprintf((char*)(size ? ",%.3d%s" : "%d%s"), n, tmp->ptr);
    } while (size);
    return tmp->ptr;
}

static Buffer *
DownloadListBuffer(void)
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
    src = Strnew_charp("<html><head><title>" DOWNLOAD_LIST_TITLE
		       "</title></head>\n<body><h1 align=center>"
		       DOWNLOAD_LIST_TITLE "</h1>\n"
		       "<form method=internal action=download><hr>\n");
    for (d = LastDL; d != NULL; d = d->prev) {
	if (lstat(d->lock, &st))
	    d->running = FALSE;
	Strcat_charp(src, "<pre>\n");
	Strcat(src, Sprintf("%s\n  --&gt; %s\n  ", html_quote(d->url),
			    html_quote(conv_from_system(d->save))));
	duration = cur_time - d->time;
	if (!stat(d->save, &st)) {
	    size = st.st_size;
	    if (!d->running) {
		if (!d->err)
		    d->size = size;
		duration = st.st_mtime - d->time;
	    }
	}
	else
	    size = 0;
	if (d->size) {
	    int i, l = COLS - 6;
	    if (size < d->size)
		i = 1.0 * l * size / d->size;
	    else
		i = l;
	    l -= i;
	    while (i-- > 0)
		Strcat_char(src, '#');
	    while (l-- > 0)
		Strcat_char(src, '_');
	    Strcat_char(src, '\n');
	}
	if ((d->running || d->err) && size < d->size)
	    Strcat(src, Sprintf("  %s / %s bytes (%d%%)",
				convert_size3(size), convert_size3(d->size),
				(int)(100.0 * size / d->size)));
	else
	    Strcat(src, Sprintf("  %s bytes loaded", convert_size3(size)));
	if (duration > 0) {
	    rate = size / duration;
	    Strcat(src, Sprintf("  %02d:%02d:%02d  rate %s/sec",
				duration / (60 * 60), (duration / 60) % 60,
				duration % 60, convert_size(rate, 1)));
	    if (d->running && size < d->size && rate) {
		eta = (d->size - size) / rate;
		Strcat(src, Sprintf("  eta %02d:%02d:%02d", eta / (60 * 60),
				    (eta / 60) % 60, eta % 60));
	    }
	}
	Strcat_char(src, '\n');
	if (!d->running) {
	    Strcat(src, Sprintf("<input type=submit name=ok%d value=OK>",
				d->pid));
	    switch (d->err) {
	    case 0: if (size < d->size)
			Strcat_charp(src, " Download ended but probably not complete");
		    else
			Strcat_charp(src, " Download complete");
		    break;
	    case 1: Strcat_charp(src, " Error: could not open destination file");
		    break;
	    case 2: Strcat_charp(src, " Error: could not write to file (disk full)");
		    break;
	    default: Strcat_charp(src, " Error: unknown reason");
	    }
	}
	else
	    Strcat(src, Sprintf("<input type=submit name=stop%d value=STOP>",
				d->pid));
	Strcat_charp(src, "\n</pre><hr>\n");
    }
    Strcat_charp(src, "</form></body></html>");
    return loadHTMLString(src);
}

void
download_action(struct parsed_tagarg *arg)
{
    DownloadList *d;
    pid_t pid;

    for (; arg; arg = arg->next) {
	if (!strncmp(arg->arg, "stop", 4)) {
	    pid = (pid_t) atoi(&arg->arg[4]);
#ifndef __MINGW32_VERSION
	    kill(pid, SIGKILL);
#endif
	}
	else if (!strncmp(arg->arg, "ok", 2))
	    pid = (pid_t) atoi(&arg->arg[2]);
	else
	    continue;
	for (d = FirstDL; d; d = d->next) {
	    if (d->pid == pid) {
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
    ldDL();
}

void
stopDownload(void)
{
    DownloadList *d;

    if (!FirstDL)
	return;
    for (d = FirstDL; d != NULL; d = d->next) {
	if (!d->running)
	    continue;
#ifndef __MINGW32_VERSION
	kill(d->pid, SIGKILL);
#endif
	unlink(d->lock);
    }
}

/* download panel */
DEFUN(ldDL, DOWNLOAD_LIST, "Display download list panel")
{
    Buffer *buf;
    int replace = FALSE, new_tab = FALSE;
#ifdef USE_ALARM
    int reload;
#endif

    if (Currentbuf->bufferprop & BP_INTERNAL &&
	!strcmp(Currentbuf->buffername, DOWNLOAD_LIST_TITLE))
	replace = TRUE;
    if (!FirstDL) {
	if (replace) {
	    if (Currentbuf == Firstbuf && Currentbuf->nextBuffer == NULL) {
		if (nTab > 1)
		    deleteTab(CurrentTab);
	    }
	    else
		delBuffer(Currentbuf);
	    displayBuffer(Currentbuf, B_FORCE_REDRAW);
	}
	return;
    }
#ifdef USE_ALARM
    reload = checkDownloadList();
#endif
    buf = DownloadListBuffer();
    if (!buf) {
	displayBuffer(Currentbuf, B_NORMAL);
	return;
    }
    buf->bufferprop |= (BP_INTERNAL | BP_NO_URL);
    if (replace) {
	COPY_BUFROOT(buf, Currentbuf);
	restorePosition(buf, Currentbuf);
    }
    if (!replace && open_tab_dl_list) {
	_newT();
	new_tab = TRUE;
    }
    pushBuffer(buf);
    if (replace || new_tab)
	deletePrevBuf();
#ifdef USE_ALARM
    if (reload)
	Currentbuf->event = setAlarmEvent(Currentbuf->event, 1, AL_IMPLICIT,
					  FUNCNAME_reload, NULL);
#endif
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

static void
save_buffer_position(Buffer *buf)
{
    BufferPos *b = buf->undo;

    if (!buf->firstLine)
	return;
    if (b && b->top_linenumber == TOP_LINENUMBER(buf) &&
	b->cur_linenumber == CUR_LINENUMBER(buf) &&
	b->currentColumn == buf->currentColumn && b->pos == buf->pos)
	return;
    b = New(BufferPos);
    b->top_linenumber = TOP_LINENUMBER(buf);
    b->cur_linenumber = CUR_LINENUMBER(buf);
    b->currentColumn = buf->currentColumn;
    b->pos = buf->pos;
    b->bpos = buf->currentLine ? buf->currentLine->bpos : 0;
    b->next = NULL;
    b->prev = buf->undo;
    if (buf->undo)
	buf->undo->next = b;
    buf->undo = b;
}

static void
resetPos(BufferPos * b)
{
    Buffer buf;
    Line top, cur;

    top.linenumber = b->top_linenumber;
    cur.linenumber = b->cur_linenumber;
    cur.bpos = b->bpos;
    buf.topLine = &top;
    buf.currentLine = &cur;
    buf.pos = b->pos;
    buf.currentColumn = b->currentColumn;
    restorePosition(Currentbuf, &buf);
    Currentbuf->undo = b;
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
}

DEFUN(undoPos, UNDO, "Cancel the last cursor movement")
{
    BufferPos *b = Currentbuf->undo;
    int i;

    if (!Currentbuf->firstLine)
	return;
    if (!b || !b->prev)
	return;
    for (i = 0; i < PREC_NUM() && b->prev; i++, b = b->prev) ;
    resetPos(b);
}

DEFUN(redoPos, REDO, "Cancel the last undo")
{
    BufferPos *b = Currentbuf->undo;
    int i;

    if (!Currentbuf->firstLine)
	return;
    if (!b || !b->next)
	return;
    for (i = 0; i < PREC_NUM() && b->next; i++, b = b->next) ;
    resetPos(b);
}

}