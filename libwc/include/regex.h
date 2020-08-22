#pragma once
#include "ccs.h"
#include <string_view>

#define REGEX_MAX 64
#define STORAGE_MAX 256

typedef struct
{
    char type;
    wc_wchar_t wch;
    unsigned char ch;
} longchar;

typedef struct regexchar
{
    union
    {
        longchar *pattern;
        struct regex *sub;
    } p;
    unsigned char mode;
} regexchar;

typedef struct regex
{
    regexchar re[REGEX_MAX];
    longchar storage[STORAGE_MAX];
    char *position;
    char *lposition;
    struct regex *alt_regex;
} Regex;

Regex *newRegex(const char *ex, bool igncase, Regex *regex, const char **error_msg);

int RegexMatch(Regex *re, char *str, int len, int firstp);

void MatchedPosition(Regex *re, char **first, char **last);

/* backward compatibility */
const char *regexCompile(const char *ex, bool igncase);

int regexMatch(const char *str, int len, bool firstp = true);

inline int regexMatch(std::string_view str, bool firstp = true)
{
    return regexMatch(str.data(), str.size(), firstp);
}

void matchedPosition(char **first, char **last);
