#pragma once
#include "frontend/buffer.h"
#include "stream/istream.h"
#include "stream/http.h"
#include <memory>

extern CharacterEncodingScheme content_charset;
extern CharacterEncodingScheme meta_charset;

using LoaderFunc = BufferPtr (*)(const URL &url, const InputStreamPtr &stream);
BufferPtr loadBuffer(const URL &url, const InputStreamPtr &stream);
BufferPtr loadFile(char *path);
BufferPtr loadGeneralFile(const URL &url,
                          const URL *current = nullptr, HttpReferrerPolicy referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin,
                          struct FormList *form = nullptr);
int doFileCopy(const char *tmpf, const char *defstr);
BufferPtr loadcmdout(const char *cmd, LoaderFunc loadproc);
BufferPtr LoadStream(const URL &url, const InputStreamPtr &stream);
