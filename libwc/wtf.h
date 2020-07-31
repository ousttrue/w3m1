#pragma once
#include "Str.h"
#include "ccs.h"
#include "ces.h"

#define WTF_TYPE_ASCII 0x0
#define WTF_TYPE_CTRL 0x1
#define WTF_TYPE_WCHAR1 0x2
#define WTF_TYPE_WCHAR2 0x4
#define WTF_TYPE_WIDE 0x8
#define WTF_TYPE_UNKNOWN 0x10
#define WTF_TYPE_UNDEF 0x20
#define WTF_TYPE_WCHAR1W (WTF_TYPE_WCHAR1 | WTF_TYPE_WIDE)
#define WTF_TYPE_WCHAR2W (WTF_TYPE_WCHAR2 | WTF_TYPE_WIDE)

extern uint8_t WTF_WIDTH_MAP[];
extern uint8_t WTF_LEN_MAP[];
extern uint8_t WTF_TYPE_MAP[];
extern CodedCharacterSet wtf_gr_ccs;

extern void wtf_init(CharacterEncodingScheme ces1, CharacterEncodingScheme ces2);

/* extern int     wtf_width(uint8_t *p); */
#define wtf_width(p) (WcOption.use_wide ? (int)WTF_WIDTH_MAP[(uint8_t) * (p)] \
                                        : ((int)WTF_WIDTH_MAP[(uint8_t) * (p)] ? 1 : 0))
extern int wtf_strwidth(uint8_t *p);
/* extern size_t  wtf_len1(uint8_t *p); */
#define wtf_len1(p) ((int)WTF_LEN_MAP[(uint8_t) * (p)])
extern size_t wtf_len(uint8_t *p);
/* extern int     wtf_type(uint8_t *p); */
#define wtf_type(p) WTF_TYPE_MAP[(uint8_t) * (p)]

extern void wtf_push(Str os, CodedCharacterSet ccs, uint32_t code);
void wtf_push_unknown(Str os, const uint8_t *p, size_t len);
extern wc_wchar_t wtf_parse(uint8_t **p);
extern wc_wchar_t wtf_parse1(uint8_t **p);

extern CodedCharacterSet wtf_get_ccs(uint8_t *p);
extern uint32_t wtf_get_code(uint8_t *p);

extern bool wtf_is_hangul(uint8_t *p);

extern char *wtf_conv_fit(char *s, CharacterEncodingScheme ces);

#define get_mctype(c) ((Lineprop)wtf_type((uint8_t *)(c)) << 8)
#define get_mclen(c) wtf_len1((uint8_t *)(c))
#define get_mcwidth(c) wtf_width((uint8_t *)(c))
#define get_strwidth(c) wtf_strwidth((uint8_t *)(c))
#define get_Str_strwidth(c) wtf_strwidth((uint8_t *)((c)->ptr))
