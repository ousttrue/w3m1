#include <catch.hpp>
#include "charutil.h"
#include "entity.h"

TEST_CASE("ucs4_from_entity", "[entity]")
{
    {
        auto src = "&amp;";
        auto p = src;
        auto entity = ucs4_from_entity(&p);
        REQUIRE(entity == 38);
        REQUIRE(p == src + 5);
    }
    {
        auto src = "&amp ";
        auto p = src;
        auto entity = ucs4_from_entity(&p);
        REQUIRE(entity == 38);
        REQUIRE(p == src + 4);
    }
    {
        auto src = "&#38;";
        auto p = src;
        auto entity = ucs4_from_entity(&p);
        REQUIRE(entity == 38);
        REQUIRE(p == src + 5);
    }
    {
        auto src = "&#x26;";
        auto p = src;
        auto entity = ucs4_from_entity(&p);
        REQUIRE(entity == 38);
        REQUIRE(p == src + 6);
    }
    {
        auto src = "&XXX;";
        auto p = src;
        auto entity = ucs4_from_entity(&p);
        REQUIRE(entity == -1);
        REQUIRE(p == src + 5);
    }
}
