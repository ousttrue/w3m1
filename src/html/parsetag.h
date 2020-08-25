#pragma once
#include <vector>
#include <tcb/span.hpp>

struct parsed_tagarg
{
    char *arg;
    char *value;
};

using parsed_tagarg_func = void (*)(tcb::span<parsed_tagarg>);

const char *tag_get_value(tcb::span<parsed_tagarg> t, const char *arg);

struct std::vector<parsed_tagarg> cgistr2tagarg(const char *cgistr);
