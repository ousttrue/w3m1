#pragma once
#include "Str.h"
#include "transport/url.h"
#include "frontend/buffer.h"

struct HRequest;
struct FormList;
struct TextList;
struct auth_param
{
    char *name;
    Str val;
};
struct http_auth
{
    int pri;
    char *scheme;
    auth_param *param;
    Str (*cred)(http_auth *ha, Str uname, Str pw, URL *pu,
                HRequest *hr, FormList *request);
};
http_auth *findAuthentication(http_auth *hauth, BufferPtr buf, char *auth_field);
Str get_auth_param(auth_param *auth, char *name);
Str qstr_unquote(Str s);
int find_auth_user_passwd(URL *pu, char *realm, Str *uname, Str *pwd, int is_proxy);
void add_auth_user_passwd(URL *pu, char *realm, Str uname, Str pwd, int is_proxy);
void invalidate_auth_user_passwd(URL *pu, char *realm, Str uname, Str pwd, int is_proxy);
void getAuthCookie(struct http_auth *hauth, char *auth_header,
              TextList *extra_header, URL *pu, HRequest *hr,
              FormList *request,
              Str *uname, Str *pwd);
