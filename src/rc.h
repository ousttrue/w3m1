#pragma once
#include "parsetag.h"
void panel_set_option(struct parsed_tagarg *);
char *rcFile(char *base);
char *etcFile(char *base);
char *confFile(char *base);
int str_to_bool(char *value, int old);
void show_params(FILE *fp);
