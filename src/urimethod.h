#pragma once
#include "stream/url.h"
#include "frontend/buffer.h"

/* mark URL, Message-ID */
#define CHK_URL 1
#define CHK_NMID 2

void initURIMethods();
Str searchURIMethods(URL *pu);
void chkExternalURIBuffer(const BufferPtr &buf);
void chkURLBuffer(const BufferPtr &buf);
