#pragma once
#include "stream/url.h"
#include "frontend/buffer.h"

void initURIMethods();
Str searchURIMethods(URL *pu);
void chkExternalURIBuffer(const BufferPtr &buf);
void chkURLBuffer(const BufferPtr &buf);