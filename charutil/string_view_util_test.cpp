#include <catch.hpp>
#include "string_view_util.h"
#include "myctype.h"

TEST_CASE("splitter", "[svu]")
{
    auto splitter = svu::splitter("a b c", IS_SPACE);
    auto it = splitter.begin();

    REQUIRE(*it == "a");
    ++it;
    auto b = *it;
    REQUIRE(*it == "b");
    ++it;
    REQUIRE(*it == "c");
    ++it;
    REQUIRE(it == splitter.end());
}

TEST_CASE("splitter2", "[svu]")
{
    auto splitter = svu::splitter(" abc  def   ghi    ", IS_SPACE);
    auto it = splitter.begin();

    REQUIRE(*it == "abc");
    ++it;
    auto b = *it;
    REQUIRE(*it == "def");
    ++it;
    REQUIRE(*it == "ghi");
    ++it;
    REQUIRE(it == splitter.end());
}
