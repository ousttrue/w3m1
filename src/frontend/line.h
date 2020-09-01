#pragma once
#include "propstring.h"
#include <memory>

struct Line
{
    PropertiedString buffer;

    // on buffer
    // 1 origin ?
    long linenumber = 0;
    // on file
    long real_linenumber = 0;
};
using LinePtr = std::shared_ptr<Line>;
