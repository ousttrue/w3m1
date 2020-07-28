#pragma once
#include <string_view>
#include "html/parsetagx.h"
#include "url.h"

void set_cookie_flag(struct parsed_tagarg *arg);
Str find_cookie(const ParsedURL *pu);
int add_cookie(ParsedURL *pu, Str name, Str value, time_t expires,
               Str domain, Str path, int flag, Str comment, int version,
               Str port, Str commentURL);
void save_cookies(void);
void initCookie(void);
int check_cookie_accept_domain(std::string_view domain);
void readHeaderCookie(ParsedURL *pu, Str lineBuf2);
