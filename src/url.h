#pragma once
#include "tab.h"
#include "parsetagx.h"
#include "textlist.h"

const char *filename_extension(const char *patch, int is_url);
void initURIMethods();
Str searchURIMethods(ParsedURL *pu);
void chkExternalURIBuffer(BufferPtr buf);
ParsedURL *schemeToProxy(int scheme);
TextList *make_domain_list(char *domain_list);
