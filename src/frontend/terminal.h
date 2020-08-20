#pragma once
#include <string>

class Terminal
{
    int m_tty = -1;
    std::string m_term;
    int is_xterm = 0;

    Terminal(const Terminal &) = delete;
    Terminal &operator=(const Terminal &) = delete;
    Terminal();
    ~Terminal();

public:
    static Terminal &Instance();
    static int write1(int c);
    static void writestr(const char *s);
    int tty()const{ return m_tty; }
};
