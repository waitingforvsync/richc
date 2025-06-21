#include "richc/test/test.h"


DEF_TEST(str, str_split) {
	str_t s = STR("file/path/to");
	str_pair_t p = str_last_split(s, str_make("/"));
	REQUIRE(p.first, ==, str_make("file/path"));
	REQUIRE(p.second, ==, str_make("to"));
}

