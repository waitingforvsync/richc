#include "richc/test/test.h"
#include "richc/file.h"


DEF_TEST(file, test_paths) {
    str_t s = STR("path/to/blah.6502");
    REQUIRE(file_get_path(s), ==, str_make("path/to/"));
    REQUIRE(file_remove_path(s), ==, str_make("blah.6502"));

    str_t s2 = STR("nopath.6502");
    REQUIRE(file_get_path(s2), ==, str_make(""));
    REQUIRE(file_remove_path(s2), ==, str_make("nopath.6502"));

    str_t s3 = STR("mixed/paths\\test.6502");
    REQUIRE(file_get_path(s3), ==, str_make("mixed/paths\\"));
    REQUIRE(file_remove_path(s3), ==, str_make("test.6502"));

    str_t s4 = STR("mixed\\paths/test.6502");
    REQUIRE(file_get_path(s4), ==, str_make("mixed\\paths/"));
    REQUIRE(file_remove_path(s4), ==, str_make("test.6502"));
}
