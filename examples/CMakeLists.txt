add_executable(sqlnx_quick_start quick_start.cpp)
target_link_libraries(sqlnx_quick_start PRIVATE sqlnx::sqlite)
target_compile_options(sqlnx_quick_start PRIVATE ${SQLNX_COMPILE_OPTIONS})

# Running it is a test too: the README shows this program, so it has to work
add_test(NAME sqlnx_quick_start COMMAND sqlnx_quick_start)
