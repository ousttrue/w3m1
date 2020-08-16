#pragma once
#include <wc.h>
#include <string_view>
#include <memory>
#include "stream/istream.h"
#include "stream/url.h"

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
Mailcap *searchMailcap(Mailcap *table, std::string_view type);
void initMailcap();
Mailcap *searchExtViewer(std::string_view type);
Str unquote_mailcap(const char *qstr, const char *type, char *name, char *attr, int *mc_stat);
std::shared_ptr<struct Buffer> doExternal(const URL &url, const InputStreamPtr &stream, std::string_view type);
