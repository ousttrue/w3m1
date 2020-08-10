#pragma once
#include <memory>
#include "html/html.h"
#define MAX_ENV_LEVEL 20

Str process_img(struct parsed_tag *tag, int width, class HSequence *seq);
Str process_anchor(struct parsed_tag *tag, char *tagbuf, class HSequence *seq);
Str process_input(struct parsed_tag *tag, class HSequence *seq);

Str process_form_int(struct parsed_tag *tag, int fid);
inline Str process_form(struct parsed_tag *tag)
{
    return process_form_int(tag, -1);
}

int cur_form_id();

// entry point
struct Buffer;
using BufferPtr = std::shared_ptr<Buffer>;
void loadHTMLstream(struct URLFile *f, BufferPtr newBuf, FILE *src, int internal);
BufferPtr loadHTMLString(Str page);
