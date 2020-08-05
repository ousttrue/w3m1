#include <catch.hpp>
#include "conv.h"

TEST_CASE("ascii", "[SingleCharacter]")
{
    {
        auto src = "abc";
        auto p = src;
        auto c = GetSingleCharacter(CharacterEncodingScheme::WC_CES_US_ASCII, &p);
        auto ex = SingleCharacter("a");
        REQUIRE(c.size() == 1);
        REQUIRE(src + c.size() == p);
        REQUIRE(c == ex);
    }
}

TEST_CASE("sjis", "[SingleCharacter]")
{
}

TEST_CASE("euc-jp", "[SingleCharacter]")
{
}

TEST_CASE("utf-8", "[SingleCharacter]")
{
    {
        auto src = u8"Дいう";
        auto p = src;
        auto c = GetSingleCharacter(CharacterEncodingScheme::WC_CES_UTF_8, &p);
        auto ex = SingleCharacter((const char *)u8"Д");
        REQUIRE(c.size() == 2);
        REQUIRE(src + c.size() == p);
        REQUIRE(c == ex);
    }

    {
        auto src = u8"あいう";
        auto p = src;
        auto c = GetSingleCharacter(CharacterEncodingScheme::WC_CES_UTF_8, &p);
        auto ex = SingleCharacter((const char *)u8"あ");
        REQUIRE(c.size() == 3);
        REQUIRE(src + c.size() == p);
        REQUIRE(c == ex);
    }

    {
        auto src = u8"𠮟いう";
        auto p = src;
        auto c = GetSingleCharacter(CharacterEncodingScheme::WC_CES_UTF_8, &p);
        auto ex = SingleCharacter((const char *)u8"𠮟");
        REQUIRE(src + c.size() == p);
        REQUIRE(c.size() == 4);
        REQUIRE(c == ex);
    }

    {
        auto src = u8"😀いう";
        auto p = src;
        auto c = GetSingleCharacter(CharacterEncodingScheme::WC_CES_UTF_8, &p);
        auto ex = SingleCharacter((const char *)u8"😀");
        REQUIRE(src + c.size() == p);
        REQUIRE(c.size() == 4);
        REQUIRE(c == ex);
    }
}
