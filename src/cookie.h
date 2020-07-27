#pragma once
#include <string_view>
#include "parsetagx.h"
#include "buffer.h"

void set_cookie_flag(struct parsed_tagarg *arg);
Str find_cookie(ParsedURL *pu);
int add_cookie(ParsedURL *pu, Str name, Str value, time_t expires,
               Str domain, Str path, int flag, Str comment, int version,
               Str port, Str commentURL);
void save_cookies(void);
void load_cookies(void);
void initCookie(void);
BufferPtr cookie_list_panel(void);
int check_cookie_accept_domain(std::string_view domain);
