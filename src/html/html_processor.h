#pragma once
#include "Str.h"
#include "parsetagx.h"

int GetCurHSeq();
void SetCurHSeq(int seq);
Str getLinkNumberStr(int correction);

Str process_img(struct parsed_tag *tag, int width);
Str process_anchor(struct parsed_tag *tag, char *tagbuf);
Str process_input(struct parsed_tag *tag);
Str process_select(struct parsed_tag *tag);
Str process_textarea(struct parsed_tag *tag, int width);
Str process_form_int(struct parsed_tag *tag, int fid);
inline Str process_form(struct parsed_tag *tag)
{
    return process_form_int(tag, -1);
}