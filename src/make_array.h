#pragma once
#include <array>

template <class T>
inline std::array<T, 0> make_array()
{
    return std::array<T, 0>{{}};
}
template <
    class... Args,
    class Result = std::array<
        typename std::decay<typename std::common_type<Args...>::type>::type,
        sizeof...(Args)>>
inline Result make_array(Args &&... args)
{
    return Result{{std::forward<Args>(args)...}};
}
