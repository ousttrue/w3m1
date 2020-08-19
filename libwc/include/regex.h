#pragma once
#include "ccs.h"

#define REGEX_MAX	64
#define STORAGE_MAX	256

typedef struct {
    char type;
    wc_wchar_t wch;
    unsigned char ch;
} longchar;

typedef struct regexchar {
    union {
	longchar *pattern;
	struct regex *sub;
    } p;
    unsigned char mode;
} regexchar;


typedef struct regex {
    regexchar re[REGEX_MAX];
    longchar storage[STORAGE_MAX];
    char *position;
    char *lposition;
    struct regex *alt_regex;
} Regex;


Regex *newRegex(const char *ex, int igncase, Regex *regex, const char **error_msg);

int RegexMatch(Regex *re, char *str, int len, int firstp);

void MatchedPosition(Regex *re, char **first, char **last);


/* backward compatibility */
const char *regexCompile(const char *ex, int igncase);

int regexMatch(const char *str, int len, int firstp);

void matchedPosition(char **first, char **last);
