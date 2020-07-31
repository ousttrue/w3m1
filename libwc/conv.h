#pragma once
#include "status.h"
void wc_push_end(Str os, wc_status *st);

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
