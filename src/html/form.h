/* $Id: form.h,v 1.6 2003/09/22 21:02:18 ukai Exp $ */
/*
 * HTML forms 
 */
#ifndef FORM_H
#define FORM_H


#include "parsetagx.h"
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

struct FormSelectOptionItem
{
    Str value;
    Str label;
    int checked;
    FormSelectOptionItem *next;
};

struct FormItemList
{
    int type;
    Str name;
    Str value, init_value;
    int checked, init_checked;
    int accept;
    int size;
    int rows;
    int maxlength;
    int readonly;
    FormSelectOptionItem *select_option;
    Str label, init_label;
    int selected, init_selected;
    struct FormList *parent;
    FormItemList *next;
};

struct FormList
{
    FormItemList *item;
    FormItemList *lastitem;
    FormMethodTypes method;
    Str action;
    char *target;
    char *name;
    CharacterEncodingScheme charset;
    int enctype;
    FormList *next;
    int nitems;
    char *body;
    char *boundary;
    unsigned long length;
};

#define MAX_TEXTAREA 10 /* max number of <textarea>..</textarea> \
                         * within one document */
#ifdef MENU_SELECT
#define MAX_SELECT 10 /* max number of <select>..</select> \
                       * within one document */
#endif                /* MENU_SELECT */

#ifdef MENU_SELECT

struct FormSelectOption
{
    FormSelectOptionItem *first;
    FormSelectOptionItem *last;
};

void addSelectOption(FormSelectOption *fso, Str value, Str label, int chk);
void chooseSelectOption(FormItemList *fi, FormSelectOptionItem *item);
void updateSelectOption(FormItemList *fi, FormSelectOptionItem *item);
int formChooseOptionByMenu(FormItemList *fi, int x, int y);
#endif /* MENU_SELECT */

FormItemList *formList_addInput(FormList *fl,
                                struct parsed_tag *tag);
void formUpdateBuffer(const Anchor *a, BufferPtr buf, FormItemList *form);
void formRecheckRadio(const Anchor *a, BufferPtr buf, FormItemList *form);

FormList *newFormList(char *action, const char *method, char *charset,
                      char *enctype, char *target, char *name,
                      FormList *_next);
int formtype(char *typestr);

#endif /* not FORM_H */
