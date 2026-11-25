foreach(test_name
    aggregate_test
    backup_test
    blob_stream_test
    busy_timeout_test
    collation_test
    cstring_view_test
    function_test
    native_test
    newer_types_test
    out_of_memory_test
    registrations_test
    rows_test
    typed_test
    transaction_test
)
    add_executable(sqlitexx_${test_name} ${test_name}.cpp check.hpp)
    target_link_libraries(sqlitexx_${test_name} PRIVATE sqlitexx::sqlitexx)
    target_compile_options(sqlitexx_${test_name} PRIVATE ${SQLITEXX_COMPILE_OPTIONS})

    add_test(NAME sqlitexx_${test_name} COMMAND sqlitexx_${test_name})
endforeach()
