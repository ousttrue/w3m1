#pragma once
#include <string>

extern char *T_cd, *T_ce, *T_kr, *T_kl, *T_cr, *T_bt, *T_ta, *T_sc, *T_rc,
    *T_so, *T_se, *T_us, *T_ue, *T_cl, *T_cm, *T_al, *T_sr, *T_md, *T_me,
    *T_ti, *T_te, *T_nd, *T_as, *T_ae, *T_eA, *T_ac, *T_op;

class Terminal
{
    int m_tty = -1;
    std::string m_term;
    int m_is_xterm = 0;

    Terminal(const Terminal &) = delete;
    Terminal &operator=(const Terminal &) = delete;
    Terminal();

public:
    ~Terminal();
    static Terminal &Instance();
    static int tty();
    static FILE *file();
    static void flush();
    static int is_xterm();
    static int write1(int c);
    static void writestr(const char *s);
    static int tcgetattr(struct termios *__termios_p);
    static int tcsetattr(const struct termios *__termios_p);
    static const char *ttyname_tty();
    static void move(int line, int column);
    static void xterm_on();
    static void xterm_off();
    static int lines();
    static int columns();
    static void mouse_on();
    static void mouse_off();
    static void term_echo();
    static void term_noecho();
    static void term_raw();
    static void term_cooked();
    static void term_cbreak();
    static char getch();
    static void skip_escseq();
    static int sleep_till_anykey(int sec, int purge);
    static void title(const char *s);
    static bool graph_ok();
    static void write_graphchar(int c);
    static int SymbolWidth(); // const { return symbol_width; }
    static int SymbolWidth0(); // const { return symbol_width0; }
};
