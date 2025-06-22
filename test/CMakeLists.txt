add_executable("richc_tests")

if(MSVC)
	target_compile_options("richc_tests" PRIVATE /W4 /WX /wd4100 /wd4820 /wd4189 /wd4206 /wd4101)
else()
	target_compile_options("richc_tests" PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function)
endif()

target_link_libraries("richc_tests" PRIVATE "richc")

target_sources("richc_tests" PRIVATE
	"arena_tests.c"
    "file_tests.c"
    "main.c"
    "str_tests.c"
)

add_custom_command(
	TARGET "richc_tests"
	COMMENT "Run richc tests"
	POST_BUILD
	COMMAND "richc_tests"
)
