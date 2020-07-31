#pragma once
#include "Str.h"
#include "ccs.h"
#include "ces.h"

enum WobblyTransformationFormatTypes : uint8_t
{
    WTF_TYPE_ASCII = 0x0,
    WTF_TYPE_CTRL = 0x1,
    WTF_TYPE_WCHAR1 = 0x2,
    WTF_TYPE_WCHAR2 = 0x4,
    WTF_TYPE_WIDE = 0x8,
    WTF_TYPE_UNKNOWN = 0x10,
    WTF_TYPE_UNDEF = 0x20,
    WTF_TYPE_WCHAR1W = (WTF_TYPE_WCHAR1 | WTF_TYPE_WIDE),
    WTF_TYPE_WCHAR2W = (WTF_TYPE_WCHAR2 | WTF_TYPE_WIDE),
};

extern uint8_t WTF_WIDTH_MAP[];
extern uint8_t WTF_LEN_MAP[];
extern uint8_t WTF_TYPE_MAP[];
extern CodedCharacterSet wtf_gr_ccs;

void wtf_init(CharacterEncodingScheme ces1, CharacterEncodingScheme ces2);
int wtf_width(uint8_t *p);
int wtf_strwidth(uint8_t *p);
uint8_t wtf_len1(uint8_t *p);
size_t wtf_len(uint8_t *p);
uint8_t wtf_type(uint8_t *p);

using Writer = void (*)(void *userdata, const void *buf, int size);
// push code as ccs
void wtf_push(Writer writer, void *data, CodedCharacterSet ccs, uint32_t code);
inline void wtf_push(Str os, CodedCharacterSet ccs, uint32_t code)
{
    Writer writer = [](void *data, const void *buf, int size) {
        ((Str)data)->Push((const char *)buf, size);
    };
    wtf_push(writer, os, ccs, code);
}
void wtf_push_unknown(Writer writer, void *data, const uint8_t *p, size_t len);
inline void wtf_push_unknown(Str os, const uint8_t *p, size_t len)
{
    Writer writer = [](void *data, const void *buf, int size) {
        ((Str)data)->Push((const char *)buf, size);
    };
    wtf_push_unknown(writer, os, p, len);
}

wc_wchar_t wtf_parse(uint8_t **p);
wc_wchar_t wtf_parse1(uint8_t **p);
CodedCharacterSet wtf_get_ccs(uint8_t *p);
uint32_t wtf_get_code(uint8_t *p);
bool wtf_is_hangul(uint8_t *p);
char *wtf_conv_fit(char *s, CharacterEncodingScheme ces);

inline int get_mclen(const char *c)
{
    return wtf_len1((uint8_t *)(c));
}
inline int get_mcwidth(const char *c)
{
    return wtf_width((uint8_t *)(c));
}
inline int get_strwidth(const char *c)
{
    return wtf_strwidth((uint8_t *)(c));
}
inline int get_Str_strwidth(Str c)
{
    return wtf_strwidth((uint8_t *)((c)->ptr));
}
