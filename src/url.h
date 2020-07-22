#pragma once
#include "tab.h"
#include "parsetagx.h"
#include "textlist.h"

char *filename_extension(char *patch, int is_url);
void initURIMethods();
Str searchURIMethods(ParsedURL *pu);
void chkExternalURIBuffer(Buffer *buf);
ParsedURL *schemeToProxy(int scheme);
TextList *make_domain_list(char *domain_list);
