#include <stdio.h>   // remove()

#include "richc/file.h"
#include "richc/test.h"

RC_TEST_GROUP_DATA(file) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(file, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(file, fix)
{
    rc_arena_destroy(&fix->a);
}

RC_TEST_STEP(file, text_roundtrip, fix)
{
    rc_str path = RC_STR("/tmp/richc_file_test.txt");
    rc_str content = RC_STR("hello\nworld");
    RC_CHECK_TRUE(rc_file_save_text(path, content) == RC_FILE_OK);

    rc_file_load_text_result r = rc_file_load_text(path, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.text, ==, content);
    // The result is null-terminated, so as_cstr returns the buffer directly.
    RC_CHECK_TRUE(rc_str_as_cstr(r.text, NULL, 0) == r.text.data);

    remove("/tmp/richc_file_test.txt");
}

RC_TEST_STEP(file, text_mut, fix)
{
    rc_str path = RC_STR("/tmp/richc_file_test.txt");
    RC_CHECK_TRUE(rc_file_save_text(path, RC_STR("abc")) == RC_FILE_OK);

    rc_file_load_text_mut_result r = rc_file_load_text_mut(path, 100, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.text.view, ==, RC_STR("abc"));
    RC_CHECK(r.text.len, ==, 3u);
    RC_CHECK(r.text.cap, ==, 100u);              // minimum_capacity honoured
    rc_mstr_append(&r.text, RC_STR("def"), &fix->a);
    RC_CHECK(r.text.view, ==, RC_STR("abcdef"));

    remove("/tmp/richc_file_test.txt");
}

RC_TEST_STEP(file, empty_text, fix)
{
    rc_str path = RC_STR("/tmp/richc_file_empty.txt");
    RC_CHECK_TRUE(rc_file_save_text(path, RC_STR("")) == RC_FILE_OK);

    rc_file_load_text_result r = rc_file_load_text(path, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.text.len, ==, 0u);
    RC_CHECK_TRUE(rc_str_is_valid(r.text));      // valid empty, null-terminated

    remove("/tmp/richc_file_empty.txt");
}

RC_TEST_STEP(file, binary_roundtrip, fix)
{
    rc_str path = RC_STR("/tmp/richc_file_test.bin");
    uint8_t bytes[] = {0, 1, 2, 253, 254, 255};
    rc_view_bytes content = RC_VIEW(bytes);
    RC_CHECK_TRUE(rc_file_save_binary(path, content) == RC_FILE_OK);

    rc_file_load_binary_result r = rc_file_load_binary(path, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.data.num, ==, 6u);
    RC_CHECK(rc_view_bytes_get(r.data, 3), ==, (uint8_t)253);
    RC_CHECK(rc_view_bytes_get(r.data, 5), ==, (uint8_t)255);

    remove("/tmp/richc_file_test.bin");
}

RC_TEST_STEP(file, binary_mut, fix)
{
    rc_str path = RC_STR("/tmp/richc_file_test.bin");
    uint8_t bytes[] = {10, 20, 30};
    rc_view_bytes content = RC_VIEW(bytes);
    RC_CHECK_TRUE(rc_file_save_binary(path, content) == RC_FILE_OK);

    rc_file_load_binary_mut_result r = rc_file_load_binary_mut(path, 16, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.data.num, ==, 3u);
    RC_CHECK(r.data.cap, ==, 16u);               // minimum_capacity honoured
    rc_array_bytes_push(&r.data, 40, &fix->a);   // mutable
    RC_CHECK(r.data.num, ==, 4u);
    RC_CHECK(rc_array_bytes_get(&r.data, 3), ==, (uint8_t)40);

    remove("/tmp/richc_file_test.bin");
}

RC_TEST_STEP(file, not_found, fix)
{
    rc_file_load_text_result r = rc_file_load_text(RC_STR("/tmp/richc_no_such_file_zzz.txt"), &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK_FALSE(rc_str_is_valid(r.text));
}

RC_TEST(file, size)
{
    rc_str path = RC_STR("/tmp/richc_file_size.bin");
    uint8_t bytes[] = {1, 2, 3, 4, 5, 6, 7};
    rc_view_bytes content = RC_VIEW(bytes);
    RC_CHECK_TRUE(rc_file_save_binary(path, content) == RC_FILE_OK);

    rc_file_size_result r = rc_file_size(path);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.size, ==, 7u);

    remove("/tmp/richc_file_size.bin");
}

RC_TEST(file, size_empty)
{
    rc_str path = RC_STR("/tmp/richc_file_size_empty.bin");
    RC_CHECK_TRUE(rc_file_save_text(path, RC_STR("")) == RC_FILE_OK);

    rc_file_size_result r = rc_file_size(path);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.size, ==, 0u);

    remove("/tmp/richc_file_size_empty.bin");
}

RC_TEST(file, size_not_found)
{
    rc_file_size_result r = rc_file_size(RC_STR("/tmp/richc_no_such_file_zzz.bin"));
    RC_CHECK_TRUE(r.error == RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK(r.size, ==, 0u);
}
