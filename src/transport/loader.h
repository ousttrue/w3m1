#pragma once
#include "frontend/buffer.h"


long long GetCurrentContentLength();
extern CharacterEncodingScheme content_charset;
extern CharacterEncodingScheme meta_charset;
extern int frame_source;
char *check_accept_charset(char *ac);
char *check_charset(char *p);

struct ParsedURL;
struct FormList;
struct URLFile;
BufferPtr loadFile(char *path);
char *checkContentType(BufferPtr buf);
void readHeader(URLFile *uf, BufferPtr newBuf, int thru, ParsedURL *pu);

BufferPtr loadBuffer(URLFile *uf, BufferPtr newBuf);

enum LoadFlags
{
    RG_NONE = 0,
    RG_NOCACHE = 1,
    RG_FRAME = 2,
    RG_FRAME_SRC = 4,
};
struct URLOption
{
    char *referer;
    LoadFlags flag = RG_NONE;
};

BufferPtr loadGeneralFile(std::string_view path, const ParsedURL *current, char *referer, LoadFlags flag, FormList *request);

int save2tmp(URLFile uf, char *tmpf);
int doFileCopy(const char *tmpf, const char *defstr);
