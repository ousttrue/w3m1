#pragma once
#include "frontend/buffer.h"
#include "stream/istream.h"
#include "stream/http.h"
#include <memory>

using LoaderFunc = BufferPtr (*)(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset);
BufferPtr loadBuffer(const URL &url, const InputStreamPtr &stream, CharacterEncodingScheme content_charset);
int doFileCopy(const char *tmpf, const char *defstr);
BufferPtr loadcmdout(const char *cmd, LoaderFunc loadproc, CharacterEncodingScheme content_charset = WC_CES_UTF_8);

BufferPtr loadGeneralFile(const URL &url,
                          const URL *current = nullptr, HttpReferrerPolicy referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin,
                          struct Form *form = nullptr);

ContentStream GetStream(const URL &url,
                        const URL *current = nullptr, HttpReferrerPolicy referer = HttpReferrerPolicy::StrictOriginWhenCrossOrigin,
                        struct Form *form = nullptr);
BufferPtr LoadStream(const ContentStream &stream);
