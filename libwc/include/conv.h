#pragma once
#include "ces.h"
#include "Str.h"
struct wc_status;
void wc_push_end(Str os, wc_status *st);

/// convert a character
SingleCharacter GetCharacter(CharacterEncodingScheme ces, const uint8_t **src);
template <typename T>
SingleCharacter GetSingleCharacter(CharacterEncodingScheme ces, const T **src)
{
    static_assert(sizeof(T) == 1); // char variant
    return GetCharacter(ces, (const uint8_t **)(src));
}

SingleCharacter ToWtf(CharacterEncodingScheme ces, SingleCharacter src);
SingleCharacter FromWtf(CharacterEncodingScheme ces, SingleCharacter src);

inline SingleCharacter wc_char_conv(SingleCharacter src, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    if (f_ces == WC_CES_WTF && t_ces == WC_CES_WTF)
    {
        // no conversion
        return src;
    }

    if (f_ces == WC_CES_WTF)
    {
        // src => wtf
        return ToWtf(t_ces, src);
    }

    // wtf <= src
    auto wtf = FromWtf(f_ces, src);
    if (t_ces == WC_CES_WTF)
    {
        return wtf;
    }

    // wtf => dst
    return ToWtf(t_ces, src);
}

Str wc_Str_conv(Str is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
inline Str wc_conv(const char *is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv(Strnew(is), f_ces, t_ces);
}
inline Str wc_conv_n(const char *is, int n, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv(Strnew_charp_n(is, n), f_ces, t_ces);
}

Str wc_Str_conv_strict(Str is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
inline Str wc_conv_strict(const char *is, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv_strict(Strnew(is), f_ces, t_ces);
}
inline Str wc_conv_n_strict(const char *is, int n, CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv_strict(Strnew_charp_n(is, n), f_ces, t_ces);
}

Str wc_Str_conv_with_detect(Str is, CharacterEncodingScheme *f_ces, CharacterEncodingScheme hint, CharacterEncodingScheme t_ces);
inline Str wc_conv_with_detect(const char *is, CharacterEncodingScheme *f_ces, CharacterEncodingScheme hint, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv_with_detect(Strnew(is), f_ces, hint, t_ces);
}
inline Str wc_conv_n_with_detect(const char *is, int n, CharacterEncodingScheme *f_ces, CharacterEncodingScheme hint, CharacterEncodingScheme t_ces)
{
    return wc_Str_conv_with_detect(Strnew_charp_n(is, n), f_ces, hint, t_ces);
}

const char *from_unicode(uint32_t codepoint, CharacterEncodingScheme ces);

void wc_char_conv_init(CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces);
Str wc_char_conv(char c);

class WCWriter
{
    FILE *m_f;
    CharacterEncodingScheme m_from;
    CharacterEncodingScheme m_to;
    struct wc_status *m_status;
    Str m_buffer;

    WCWriter(const WCWriter &) = delete;
    WCWriter &operator=(const WCWriter &) = delete;

public:
    WCWriter(CharacterEncodingScheme f_ces, CharacterEncodingScheme t_ces, FILE *f);
    ~WCWriter();
    void putc(const char *c, int len);
    void putc(const char *c)
    {
        putc(c, strlen(c));
    }
    void putc(const SingleCharacter &c)
    {
        putc((const char*)c.bytes.data(), c.size());
    }
    void end();
    void clear_status();
};
