#pragma once
#include <wc.h>

#define MAILCAP_NEEDSTERMINAL 0x01
#define MAILCAP_COPIOUSOUTPUT 0x02
#define MAILCAP_HTMLOUTPUT 0x04

struct Mailcap
{
    const char *type;
    char *viewer;
    int flags;
    char *test;
    char *nametemplate;
    char *edit;
};

int mailcapMatch(Mailcap *mcap, const char *type);
Mailcap *searchMailcap(Mailcap *table, char *type);
void initMailcap();
Mailcap *searchExtViewer(const char *type);
Str unquote_mailcap(const char *qstr, const char *type, char *name, char *attr, int *mc_stat);
