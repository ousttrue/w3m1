#include <catch.hpp>
#include "myctype.h"

TEST_CASE("IS_ALNUM", "[myctype]")
{
    // drop high value
    REQUIRE(IS_ALPHA('A' | 0xFF00));

    REQUIRE(IS_ALPHA('A'));
    REQUIRE_FALSE(IS_DIGIT('A'));
    REQUIRE(IS_ALNUM('A'));

    REQUIRE_FALSE(IS_ALPHA('0'));
    REQUIRE(IS_DIGIT('0'));
    REQUIRE(IS_ALNUM('0'));

    REQUIRE(TOLOWER('A') == 'a');
    REQUIRE(TOLOWER('a') == 'a');
    REQUIRE(TOUPPER('a') == 'A');
    REQUIRE(TOUPPER('A') == 'A');
    REQUIRE(TOUPPER('0') == '0');

    // hex
    REQUIRE(GET_MYCDIGIT('A') == 10);
}

TEST_CASE("skip blanks", "[myctype]")
{
    auto src = "  ABC";
    SKIP_BLANKS(&src);
    REQUIRE(std::string_view(src) == "ABC");
}

TEST_CASE("skip non blanks", "[myctype]")
{
    auto src = "ABC DEF";
    SKIP_NON_BLANKS(&src);
    REQUIRE(std::string_view(src) == " DEF");
}
