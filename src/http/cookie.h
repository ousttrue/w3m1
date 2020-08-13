#pragma once
#include <string_view>
#include "html/html.h"
#include "transport/url.h"

void set_cookie_flag(struct parsed_tagarg *arg);
Str find_cookie(const URL *pu);
int add_cookie(const URL &pu, Str name, Str value, time_t expires,
               Str domain, Str path, int flag, Str comment, int version,
               Str port, Str commentURL);
void save_cookies(void);
void initCookie(void);
int check_cookie_accept_domain(std::string_view domain);
void readHeaderCookie(const URL &pu, Str lineBuf2);
