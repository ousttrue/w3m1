/*
 * HTML forms 
 */
#pragma once
#include "html/html.h"
#include "frontend/buffer.h"

#define FORM_UNKNOWN -1
#define FORM_INPUT_TEXT 0
#define FORM_INPUT_PASSWORD 1
#define FORM_INPUT_CHECKBOX 2
#define FORM_INPUT_RADIO 3
#define FORM_INPUT_SUBMIT 4
#define FORM_INPUT_RESET 5
#define FORM_INPUT_HIDDEN 6
#define FORM_INPUT_IMAGE 7
#define FORM_SELECT 8
#define FORM_TEXTAREA 9
#define FORM_INPUT_BUTTON 10
#define FORM_INPUT_FILE 11

#define FORM_I_TEXT_DEFAULT_SIZE 40
#define FORM_I_SELECT_DEFAULT_SIZE 40
#define FORM_I_TEXTAREA_DEFAULT_WIDTH 40

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

struct FormSelectOptionItem : gc_cleanup
{
    std::string value;
    std::string label;
    bool checked = false;
    FormSelectOptionItem *next = nullptr;
};

struct FormItemList
{
    int type;
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
    FormSelectOptionItem *select_option;
    std::string label;
    std::string init_label;
    int selected;
    int init_selected;
    struct FormList *parent;
};

//
// <form action="some.cgi" method="pos"></form>
// representation
//
struct FormList : gc_cleanup
{
    std::vector<FormItemList> items;
    FormItemList *item() { return items.empty() ? nullptr : &items.front(); }
    FormItemList *lastitem() { return items.empty() ? nullptr : &items.back(); }
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

    FormList(std::string_view a, FormMethodTypes m)
        : action(a), method(m)
    {
    }

    static FormList *Create(
        std::string_view action,
        std::string_view method,
        std::string_view charset = "",
        std::string_view enctype = "",
        std::string_view target = "",
        std::string_view name = "");
};

void chooseSelectOption(FormItemList *fi, FormSelectOptionItem *item);
void updateSelectOption(FormItemList *fi, FormSelectOptionItem *item);
int formChooseOptionByMenu(FormItemList *fi, int x, int y);

FormItemList *formList_addInput(FormList *fl, struct parsed_tag *tag, class HtmlContext *context);
void formUpdateBuffer(const Anchor *a, BufferPtr buf, FormItemList *form);
void formRecheckRadio(const Anchor *a, BufferPtr buf, FormItemList *form);

int formtype(const char *typestr);
void formResetBuffer(BufferPtr buf, AnchorList &formitem);
Str textfieldrep(Str s, int width);
void do_internal(std::string_view action, std::string_view data);
