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
    int add_cookie(const URL &pu, Str name, Str value, time_t expires,
                   Str domain, Str path, int flag, Str comment, bool version2,
                   Str port, Str commentURL);;

public:
    bool use_cookie = false;
    bool show_cookie = true;
    bool accept_cookie = false;
    AcceptBadCookieTypes accept_bad_cookie = ACCEPT_BAD_COOKIE_DISCARD;
    std::string cookie_reject_domains;
    std::string cookie_accept_domains;
    std::string cookie_avoid_wrong_number_of_dots;

    static CookieManager &Instance();
    void Initialize();
    void ProcessHttpHeader(const URL &pu, bool version2, std::string_view value);
    bool check_cookie_accept_domain(const std::string &domain);
    void save_cookies();
    std::shared_ptr<struct Buffer> cookie_list_panel();
};

void set_cookie_flag(struct parsed_tagarg *arg);
Str find_cookie(const URL &pu);
void initCookie(void);
