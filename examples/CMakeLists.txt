add_executable(sqlitexx_quick_start quick_start.cpp)
target_link_libraries(sqlitexx_quick_start PRIVATE sqlitexx::sqlitexx)
target_compile_options(sqlitexx_quick_start PRIVATE ${SQLITEXX_COMPILE_OPTIONS})

# Running it is a test too: the README shows this program, so it has to work
add_test(NAME sqlitexx_quick_start COMMAND sqlitexx_quick_start)
