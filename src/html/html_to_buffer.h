#pragma once
#include "frontend/propstring.h"
#include "frontend/buffer.h"
#include "frontend/anchor.h"
#include <memory>
#include <vector>

struct FormData;
using FormDataPtr = std::shared_ptr<FormData>;
class HtmlToBuffer
{
    Lineprop effect = P_UNKNOWN;
    Lineprop ex_effect = P_UNKNOWN;
    char symbol = '\0';
    // HtmlTags m_internal = HTML_UNKNOWN;

    AnchorPtr a_href = nullptr;
    AnchorPtr a_img = nullptr;
    AnchorPtr a_form = nullptr;
    std::vector<AnchorPtr> a_textarea;
    std::vector<AnchorPtr> a_select;

    FormDataPtr m_form;

public:
    HtmlToBuffer(const FormDataPtr &form)
        : m_form(form)
    {
    }

    BufferPtr CreateBuffer(const URL &url, std::string_view title, CharacterEncodingScheme charset, struct TextLineList *list);

private:
    void BufferFromLines(const BufferPtr &buf, struct TextLineList *list);
    void ProcessLine(const BufferPtr &buffer, Str line, int lines);
    void Process(const BufferPtr &buf, HtmlTagPtr tag, int pos, const char *str);
    FormItemPtr formList_addInput(FormPtr fl, HtmlTagPtr tag);
};
