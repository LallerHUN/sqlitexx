foreach(test_name
    busy_timeout_test
    cstring_view_test
    function_test
    native_test
    newer_types_test
    rows_test
    typed_test
    transaction_test
)
    add_executable(sqlitexx_${test_name} ${test_name}.cpp check.hpp)
    target_link_libraries(sqlitexx_${test_name} PRIVATE sqlitexx::sqlitexx)
    target_compile_options(sqlitexx_${test_name} PRIVATE ${SQLITEXX_COMPILE_OPTIONS})

    add_test(NAME sqlitexx_${test_name} COMMAND sqlitexx_${test_name})
endforeach()
