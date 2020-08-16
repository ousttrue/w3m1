#pragma once
#include "frontend/buffer.h"
#include "urlfile.h"
#include "stream/http.h"
#include <memory>

extern CharacterEncodingScheme content_charset;
extern CharacterEncodingScheme meta_charset;

struct URL;
struct FormList;

// struct BufferLoader
// {
//     int length = 0;
//     BufferPtr Load(const URLFilePtr &uf, CharacterEncodingScheme charset);
// };
// using BufferLoaderPtr = std::shared_ptr<BufferLoader>;

using LoaderFunc = BufferPtr (*)(const URL &url, const InputStreamPtr &stream);
BufferPtr loadBuffer(const URL &url, const InputStreamPtr &stream);
// BufferPtr loadSomething(const URL &url, const InputStreamPtr &stream, LoaderFunc loadproc);

BufferPtr loadFile(char *path);
BufferPtr loadGeneralFile(const URL &url, const URL *current, HttpReferrerPolicy referer, LoadFlags flag, FormList *request);
int doFileCopy(const char *tmpf, const char *defstr);
BufferPtr loadcmdout(const char *cmd, LoaderFunc loadproc);
BufferPtr LoadStream(const URL &url, const InputStreamPtr &stream, LoadFlags flag);
