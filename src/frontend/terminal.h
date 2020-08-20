#pragma once
#include <string>

class Terminal
{
    int m_tty = -1;
    std::string m_term;
    int m_is_xterm = 0;

    Terminal(const Terminal &) = delete;
    Terminal &operator=(const Terminal &) = delete;
    Terminal();
    ~Terminal();

public:
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
};
