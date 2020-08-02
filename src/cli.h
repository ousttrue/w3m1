#pragma once
#include <stdio.h>

void fversion(FILE *f);

void fusage(FILE *f, int exit_code, bool show_params_p);
inline void help(bool show_params_p)
{
    fusage(stdout, 0, show_params_p);
}
inline void usage(bool show_params_p)
{
    fusage(stderr, 1, show_params_p);
}
