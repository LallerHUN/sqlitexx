add_executable(sqlitexx_native_test native_test.cpp)
target_link_libraries(sqlitexx_native_test PRIVATE sqlitexx::sqlitexx)

if(MSVC)
    target_compile_options(sqlitexx_native_test PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(sqlitexx_native_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME sqlitexx_native_test COMMAND sqlitexx_native_test)
