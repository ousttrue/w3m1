#pragma once
#include "buffer.h"
#include "wc_types.h"

extern long long current_content_length;
extern wc_ces content_charset;
extern wc_ces meta_charset;
extern int frame_source;

struct ParsedURL;
struct FormList;
struct URLFile;
BufferPtr loadFile(char *path);
char *checkContentType(BufferPtr buf);
void readHeader(URLFile *uf, BufferPtr newBuf, int thru, ParsedURL *pu);
BufferPtr loadGeneralFile(char *path, const ParsedURL *current, char *referer, int flag, FormList *request);
