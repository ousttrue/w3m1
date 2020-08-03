#pragma once
#include <stdint.h>

uint32_t ucs4_from_entity(const char **s);
inline uint32_t ucs4_from_entity(char **s)
{
    return ucs4_from_entity(const_cast<const char **>(s));
}
