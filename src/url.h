#pragma once

char *filename_extension(char *patch, int is_url);
void initURIMethods();
Str searchURIMethods(ParsedURL *pu);
void chkExternalURIBuffer(Buffer *buf);
ParsedURL *schemeToProxy(int scheme);
