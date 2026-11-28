foreach(test_name
    aggregate_test
    backup_test
    blob_stream_test
    busy_timeout_test
    collation_test
    cstring_view_test
    function_test
    hooks_test
    native_test
    newer_types_test
    out_of_memory_test
    records_test
    registrations_test
    rows_test
    statement_cache_test
    typed_test
    transaction_test
)
    add_executable(sqlnx_${test_name} ${test_name}.cpp check.hpp)
    target_link_libraries(sqlnx_${test_name} PRIVATE sqlnx::sqlite)
    target_compile_options(sqlnx_${test_name} PRIVATE ${SQLNX_COMPILE_OPTIONS})

    add_test(NAME sqlnx_${test_name} COMMAND sqlnx_${test_name})
endforeach()

# Counts the connection's statements through the C API
target_link_libraries(sqlnx_statement_cache_test PRIVATE ${SQLNX_SQLITE_TARGET})
