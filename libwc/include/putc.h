#pragma once
#include "ces.h"
#include <stdio.h>

void wc_putc_init(CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
void wc_putc(char *c, FILE *f);
void wc_putc_end(FILE *f);
void wc_putc_clear_status(void);
