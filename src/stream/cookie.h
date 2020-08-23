#pragma once
#include "stream/url.h"
#include <string_view>
#include <memory>
#include <vector>

enum AcceptBadCookieTypes
{
    ACCEPT_BAD_COOKIE_DISCARD = 0,
    ACCEPT_BAD_COOKIE_ACCEPT = 1,
    ACCEPT_BAD_COOKIE_ASK = 2,
};

///
/// cookie singleton
///
class CookieManager
{
    bool default_use_cookie = true;
    std::vector<std::string> Cookie_reject_domains;
    std::vector<std::string> Cookie_accept_domains;
    std::vector<std::string> Cookie_avoid_wrong_number_of_dots_domains;

    int check_avoid_wrong_number_of_dots_domain(Str domain);

    CookieManager();
    ~CookieManager();
    CookieManager(const CookieManager &) = delete;
    CookieManager &operator=(const CookieManager &) = delete;

public:
    bool use_cookie = false;
    bool show_cookie = true;
    bool accept_cookie = false;
    AcceptBadCookieTypes accept_bad_cookie = ACCEPT_BAD_COOKIE_DISCARD;
    std::string cookie_reject_domains;
    std::string cookie_accept_domains;
    std::string cookie_avoid_wrong_number_of_dots;

    static CookieManager &Instance();
    void Parse();
    void readHeaderCookie(const URL &pu, Str lineBuf2);
    int add_cookie(const URL &pu, Str name, Str value, time_t expires,
                   Str domain, Str path, int flag, Str comment, int version,
                   Str port, Str commentURL);;
    bool check_cookie_accept_domain(std::string_view domain);
    void save_cookies();
    std::shared_ptr<struct Buffer> cookie_list_panel();
};

void set_cookie_flag(struct parsed_tagarg *arg);
Str find_cookie(const URL *pu);
void initCookie(void);
