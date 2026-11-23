cmake_minimum_required(VERSION 3.21)

project(sqlitexx
    VERSION 0.1.0
    DESCRIPTION "C++ wrapper for SQLite"
    LANGUAGES C CXX
)

option(SQLITEXX_BUILD_TESTS "Build the sqlitexx tests" ${PROJECT_IS_TOP_LEVEL})

# --- SQLite (static, from the amalgamation) ---------------------------------------------------------------------------

set(SQLITEXX_SQLITE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/sqlite" CACHE PATH
    "Directory holding the SQLite amalgamation (sqlite3.c and sqlite3.h)")

if(NOT EXISTS "${SQLITEXX_SQLITE_DIR}/sqlite3.c" OR NOT EXISTS "${SQLITEXX_SQLITE_DIR}/sqlite3.h")
    message(FATAL_ERROR
        "No SQLite amalgamation in ${SQLITEXX_SQLITE_DIR}.\n"
        "Generate it in the sqlite submodule (needs tclsh):\n"
        "  Windows:  nmake /f Makefile.msc sqlite3.c\n"
        "  Unix:     ./configure && make sqlite3.c\n"
        "or point SQLITEXX_SQLITE_DIR at an unpacked sqlite-amalgamation download.")
endif()

add_library(sqlitexx_sqlite3 STATIC "${SQLITEXX_SQLITE_DIR}/sqlite3.c")
target_include_directories(sqlitexx_sqlite3 SYSTEM PUBLIC "$<BUILD_INTERFACE:${SQLITEXX_SQLITE_DIR}>")

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(sqlitexx_sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

# --- sqlitexx ---------------------------------------------------------------------------------------------------------

add_library(sqlitexx STATIC
    src/native/connection.cpp
    src/native/result.cpp
    src/native/statement.cpp
)
add_library(sqlitexx::sqlitexx ALIAS sqlitexx)

target_compile_features(sqlitexx PUBLIC cxx_std_17)
set_target_properties(sqlitexx PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(sqlitexx
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# The public headers do not include sqlite3.h; users who want the raw C API link sqlitexx_sqlite3 themselves.
target_link_libraries(sqlitexx PRIVATE sqlitexx_sqlite3)

if(MSVC)
    target_compile_options(sqlitexx PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(sqlitexx PUBLIC /Zc:__cplusplus)
else()
    target_compile_options(sqlitexx PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()

# --- Tests ------------------------------------------------------------------------------------------------------------

if(SQLITEXX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
