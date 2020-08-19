#pragma once
#include <stdio.h>

#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0

void init_rc(void);
void panel_set_option(struct parsed_tagarg *);
char *rcFile(const char *base);
char *etcFile(char *base);
char *confFile(char *base);
int str_to_bool(const char *value, int old);
void show_params(FILE *fp);
char *auxbinFile(const char *base);
int set_param_option(const char *option);
const char *get_param_option(const char *name);
