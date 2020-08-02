#include "config.h"
#include "cli.h"
#include "w3m.h"
#include "rc.h"
#include <unistd.h>

void fversion(FILE *f)
{
    fprintf(f, "w3m version %s, options %s\n", w3mApp::w3m_version.data(),
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

void fusage(FILE *f, int err, bool show_params_p)
{
    fversion(f);
    /* FIXME: gettextize? */
    fprintf(f, "usage: w3m [options] [URL or filename]\noptions:\n");
    fprintf(f, "    -t tab           set tab width\n");
    fprintf(f, "    -r               ignore backspace effect\n");
    fprintf(f, "    -l line          # of preserved line (default 10000)\n");

    fprintf(f, "    -I charset       document charset\n");
    fprintf(f, "    -O charset       display/output charset\n");
    fprintf(f, "    -e               EUC-JP\n");
    fprintf(f, "    -s               Shift_JIS\n");
    fprintf(f, "    -j               JIS\n");

    fprintf(f, "    -B               load bookmark\n");
    fprintf(f, "    -bookmark file   specify bookmark file\n");
    fprintf(f, "    -T type          specify content-type\n");
    fprintf(f, "    -m               internet message mode\n");
    fprintf(f, "    -v               visual startup mode\n");

    fprintf(f, "    -M               monochrome display\n");

    fprintf(f,
            "    -N               open URL of command line on each new tab\n");
    fprintf(f, "    -F               automatically render frame\n");
    fprintf(f,
            "    -cols width      specify column width (used with -dump)\n");
    fprintf(f,
            "    -ppc count       specify the number of pixels per character (4.0...32.0)\n");

    fprintf(f,
            "    -ppl count       specify the number of pixels per line (4.0...64.0)\n");

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

    fprintf(f, "    -4               IPv4 only (-o dns_order=4)\n");
    fprintf(f, "    -6               IPv6 only (-o dns_order=6)\n");

    fprintf(f, "    -no-mouse        don't use mouse\n");

    fprintf(f, "    -cookie          use cookie (-no-cookie: don't use cookie)\n");

    fprintf(f, "    -graph           use DEC special graphics for border of table and menu\n");
    fprintf(f, "    -no-graph        use ASCII character for border of table and menu\n");
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
    {
        // RC
        show_params(f);
    }
    exit(err);
}
