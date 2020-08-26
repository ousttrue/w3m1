#pragma once
#include <wc.h>

int next_status(char c, int *status);
int read_token(Str buf, char **instr, int *status, int pre, int append);
