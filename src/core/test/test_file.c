#include "richc/file.h"
#include "richc/test.h"

// File tests run with the working directory set to the test binary's directory
// (see test/CMakeLists.txt). Scratch files therefore use a plain relative name -
// created in the build tree and cleaned up with rc_file_delete - and bundled
// read-only fixtures live under the staged "data/" directory.

RC_TEST_GROUP_DATA(file) {
    rc_arena a;
};

RC_TEST_GROUP_INIT(file, fix)
{
    fix->a = rc_arena_make_default();
}

RC_TEST_GROUP_DEINIT(file, fix)
{
    rc_arena_deinit(&fix->a);
}

RC_TEST_STEP(file, text_roundtrip, fix)
{
    rc_str path = RC_STR("richc_file_test.txt");
    rc_str content = RC_STR("hello\nworld");
    RC_CHECK_TRUE(rc_file_save_text(path, content) == RC_FILE_OK);

    rc_file_load_text_result r = rc_file_load_text(path, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.text.view, ==, content);
    // The result is null-terminated, so as_cstr returns the buffer directly.
    RC_CHECK_TRUE(rc_str_as_cstr(r.text.view, NULL, 0) == r.text.view.data);

    rc_file_delete(path);
}

RC_TEST_STEP(file, text_capacity, fix)
{
    rc_str path = RC_STR("richc_file_test.txt");
    RC_CHECK_TRUE(rc_file_save_text(path, RC_STR("abc")) == RC_FILE_OK);

    rc_file_load_text_result r = rc_file_load_text(path, 100, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.text.view, ==, RC_STR("abc"));
    RC_CHECK(r.text.len, ==, 3u);
    RC_CHECK(r.text.cap, ==, 100u);              // minimum_capacity is a byte floor, like rc_mstr_make
    rc_mstr_append(&r.text, RC_STR("def"), &fix->a);
    RC_CHECK(r.text.view, ==, RC_STR("abcdef"));

    rc_file_delete(path);
}

RC_TEST_STEP(file, empty_text, fix)
{
    rc_str path = RC_STR("richc_file_empty.txt");
    RC_CHECK_TRUE(rc_file_save_text(path, RC_STR("")) == RC_FILE_OK);

    rc_file_load_text_result r = rc_file_load_text(path, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.text.len, ==, 0u);
    RC_CHECK_TRUE(rc_mstr_is_valid(&r.text));     // valid empty, null-terminated

    rc_file_delete(path);
}

RC_TEST_STEP(file, binary_roundtrip, fix)
{
    rc_str path = RC_STR("richc_file_test.bin");
    uint8_t bytes[] = {0, 1, 2, 253, 254, 255};
    rc_view_bytes content = RC_VIEW(bytes);
    RC_CHECK_TRUE(rc_file_save_binary(path, content) == RC_FILE_OK);

    rc_file_load_binary_result r = rc_file_load_binary(path, 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.contents.num, ==, 6u);
    RC_CHECK(rc_array_bytes_get(&r.contents, 3), ==, (uint8_t)253);
    RC_CHECK(rc_array_bytes_get(&r.contents, 5), ==, (uint8_t)255);

    rc_file_delete(path);
}

RC_TEST_STEP(file, binary_capacity, fix)
{
    rc_str path = RC_STR("richc_file_test.bin");
    uint8_t bytes[] = {10, 20, 30};
    rc_view_bytes content = RC_VIEW(bytes);
    RC_CHECK_TRUE(rc_file_save_binary(path, content) == RC_FILE_OK);

    rc_file_load_binary_result r = rc_file_load_binary(path, 16, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.contents.num, ==, 3u);
    RC_CHECK(r.contents.cap, ==, 16u);               // minimum_capacity honoured
    rc_array_bytes_push(&r.contents, 40, &fix->a);   // growable
    RC_CHECK(r.contents.num, ==, 4u);
    RC_CHECK(rc_array_bytes_get(&r.contents, 3), ==, (uint8_t)40);

    rc_file_delete(path);
}

RC_TEST_STEP(file, load_fixture, fix)
{
    // A file bundled with the tests (read-only), reached via the staged data dir.
    rc_file_load_text_result r = rc_file_load_text(RC_STR("data/hello.txt"), 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.text.view, ==, RC_STR("hello from a bundled fixture\n"));
}

RC_TEST_STEP(file, not_found, fix)
{
    rc_file_load_text_result r = rc_file_load_text(RC_STR("richc_no_such_file_zzz.txt"), 0, &fix->a);
    RC_CHECK_TRUE(r.error == RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK_FALSE(rc_mstr_is_valid(&r.text));
}

RC_TEST(file, size)
{
    rc_str path = RC_STR("richc_file_size.bin");
    uint8_t bytes[] = {1, 2, 3, 4, 5, 6, 7};
    rc_view_bytes content = RC_VIEW(bytes);
    RC_CHECK_TRUE(rc_file_save_binary(path, content) == RC_FILE_OK);

    rc_file_size_result r = rc_file_size(path);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.size, ==, 7u);

    rc_file_delete(path);
}

RC_TEST(file, size_empty)
{
    rc_str path = RC_STR("richc_file_size_empty.bin");
    RC_CHECK_TRUE(rc_file_save_text(path, RC_STR("")) == RC_FILE_OK);

    rc_file_size_result r = rc_file_size(path);
    RC_CHECK_TRUE(r.error == RC_FILE_OK);
    RC_CHECK(r.size, ==, 0u);

    rc_file_delete(path);
}

RC_TEST(file, size_not_found)
{
    rc_file_size_result r = rc_file_size(RC_STR("richc_no_such_file_zzz.bin"));
    RC_CHECK_TRUE(r.error == RC_FILE_ERROR_NOT_FOUND);
    RC_CHECK(r.size, ==, 0u);
}

RC_TEST(file, exists)
{
    rc_str path = RC_STR("richc_file_exists.bin");
    RC_CHECK_FALSE(rc_file_exists(path));            // not present yet

    RC_CHECK_TRUE(rc_file_save_text(path, RC_STR("data")) == RC_FILE_OK);
    RC_CHECK_TRUE(rc_file_exists(path));             // present after save

    rc_file_delete(path);
    RC_CHECK_FALSE(rc_file_exists(path));            // gone again
}

RC_TEST(file, delete)
{
    rc_str path = RC_STR("richc_file_delete.bin");
    RC_CHECK_TRUE(rc_file_save_text(path, RC_STR("data")) == RC_FILE_OK);

    // The file exists, so delete succeeds and the file is gone afterwards.
    RC_CHECK_TRUE(rc_file_delete(path) == RC_FILE_OK);
    RC_CHECK_FALSE(rc_file_exists(path));
}

RC_TEST(file, delete_not_found)
{
    rc_file_error e = rc_file_delete(RC_STR("richc_no_such_file_zzz.bin"));
    RC_CHECK_TRUE(e == RC_FILE_ERROR_NOT_FOUND);
}
