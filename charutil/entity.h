#pragma once
#include <string_view>
#include <stdint.h>

std::tuple<std::string_view, uint32_t> ucs4_from_entity(std::string_view s);
