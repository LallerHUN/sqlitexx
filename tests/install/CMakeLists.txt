# A project that uses an installed sqlitexx the way a user would. Not part of the main build: CI installs sqlitexx,
# then configures this with CMAKE_PREFIX_PATH pointing at the install prefix.

cmake_minimum_required(VERSION 3.21)

project(sqlitexx_install_test LANGUAGES CXX)

find_package(sqlitexx 0.1 REQUIRED)
message(STATUS "sqlitexx ${sqlitexx_VERSION} from ${sqlitexx_DIR}, expected: ${sqlitexx_EXPECTED}, string_view and optional: ${sqlitexx_CXX17_TYPES}")

add_executable(consumer consumer.cpp)
target_link_libraries(consumer PRIVATE sqlitexx::sqlitexx)

enable_testing()
add_test(NAME consumer COMMAND consumer)
