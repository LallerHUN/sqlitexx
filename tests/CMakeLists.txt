foreach(test_name native_test typed_test)
    add_executable(sqlitexx_${test_name} ${test_name}.cpp check.hpp)
    target_link_libraries(sqlitexx_${test_name} PRIVATE sqlitexx::sqlitexx)
    target_compile_options(sqlitexx_${test_name} PRIVATE ${SQLITEXX_COMPILE_OPTIONS})

    add_test(NAME sqlitexx_${test_name} COMMAND sqlitexx_${test_name})
endforeach()
