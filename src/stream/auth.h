#pragma once
#include <Str.h>
#include "stream/url.h"

int find_auth_user_passwd(URL *pu, char *realm, Str *uname, Str *pwd, int is_proxy);
void add_auth_user_passwd(URL *pu, char *realm, Str uname, Str pwd, int is_proxy);
void invalidate_auth_user_passwd(URL *pu, char *realm, Str uname, Str pwd, int is_proxy);
