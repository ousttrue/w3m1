#pragma once
#include "parsetag.h"
void panel_set_option(struct parsed_tagarg *);
char *rcFile(const char *base);
char *etcFile(char *base);
char *confFile(char *base);
int str_to_bool(const char *value, int old);
void show_params(FILE *fp);
char *auxbinFile(const char *base);