# A project that uses an installed sqlnx the way a user would. Not part of the main build: CI installs sqlnx,
# then configures this with CMAKE_PREFIX_PATH pointing at the install prefix.

cmake_minimum_required(VERSION 3.21)

project(sqlnx_install_test LANGUAGES CXX)

find_package(sqlnx 0.7 REQUIRED)
message(STATUS "sqlnx ${sqlnx_VERSION} from ${sqlnx_DIR}, expected: ${sqlnx_EXPECTED}, string_view and optional: ${sqlnx_CXX17_TYPES}")

add_executable(consumer consumer.cpp)
target_link_libraries(consumer PRIVATE sqlnx::sqlite)

enable_testing()
add_test(NAME consumer COMMAND consumer)
