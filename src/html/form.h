/*
 * HTML forms 
 */
#pragma once
#include <tcb/span.hpp>
#include <memory>

struct Anchor;

enum FormItemTypes
{
    FORM_UNKNOWN = -1,
    FORM_INPUT_TEXT = 0,
    FORM_INPUT_PASSWORD = 1,
    FORM_INPUT_CHECKBOX = 2,
    FORM_INPUT_RADIO = 3,
    FORM_INPUT_SUBMIT = 4,
    FORM_INPUT_RESET = 5,
    FORM_INPUT_HIDDEN = 6,
    FORM_INPUT_IMAGE = 7,
    FORM_SELECT = 8,
    FORM_TEXTAREA = 9,
    FORM_INPUT_BUTTON = 10,
    FORM_INPUT_FILE = 11,
};
FormItemTypes formtype(std::string_view typestr);

enum FormMethodTypes
{
    FORM_METHOD_GET = 0,
    FORM_METHOD_POST = 1,
    FORM_METHOD_INTERNAL = 2,
    FORM_METHOD_HEAD = 3,
};

enum FormEncodingTypes
{
    FORM_ENCTYPE_URLENCODED = 0,
    FORM_ENCTYPE_MULTIPART = 1,
};

struct FormSelectOptionItem
{
    std::string value;
    std::string label;
    bool checked = false;

    FormSelectOptionItem(std::string_view v, std::string_view l, bool c)
        : value(v), label(l), checked(c)
    {
        if (value.empty())
            value = label;
    }
};

struct FormItem
{
    FormItemTypes type;
    std::string name;
    std::string value;
    std::string init_value;
    int checked;
    int init_checked;
    int accept;
    int size;
    int rows;
    int maxlength;
    int readonly;

    std::vector<FormSelectOptionItem> select_option;

    std::string label;
    std::string init_label;
    int selected;
    int init_selected;
    std::weak_ptr<struct Form> parent;

    void input_textarea();
    Str ToStr() const;

    void chooseSelectOption();
    void updateSelectOption();
    bool formChooseOptionByMenu(int x, int y);
};
using FormItemPtr = std::shared_ptr<FormItem>;

//
// <form action="some.cgi" method="pos"></form>
// representation
//
struct Form
{
    std::vector<FormItemPtr> items;
    FormItemPtr item() { return items.empty() ? nullptr : items.front(); }
    FormItemPtr lastitem() { return items.empty() ? nullptr : items.back(); }
    int nitems() const { return items.size(); }

    std::string action;
    FormMethodTypes method;
    std::string target;
    std::string name;
    CharacterEncodingScheme charset;
    int enctype;
    char *boundary;
    char *body = nullptr;
    unsigned long length = 0;

    Form(std::string_view a, FormMethodTypes m)
        : action(a), method(m)
    {
    }

    static std::shared_ptr<Form> Create(
        std::string_view action,
        std::string_view method,
        std::string_view charset = "",
        std::string_view enctype = "",
        std::string_view target = "",
        std::string_view name = "");
};
using FormPtr = std::shared_ptr<Form>;

using BufferPtr = std::shared_ptr<struct Buffer>;
using AnchorPtr = std::shared_ptr<struct Anchor>;
FormItemPtr formList_addInput(FormPtr fl, struct parsed_tag *tag, class HtmlContext *context);
void formUpdateBuffer(const AnchorPtr &a, BufferPtr buf, FormItemPtr form);
void formRecheckRadio(const AnchorPtr &a, BufferPtr buf, FormItemPtr form);

void formResetBuffer(BufferPtr buf, struct AnchorList &formitem);
Str textfieldrep(Str s, int width);
void do_internal(std::string_view action, std::string_view data);
