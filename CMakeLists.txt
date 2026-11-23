cmake_minimum_required(VERSION 3.21)

project(sqlitexx
    VERSION 0.1.0
    DESCRIPTION "C++ wrapper for SQLite"
    LANGUAGES C CXX
)

option(SQLITEXX_BUILD_TESTS "Build the sqlitexx tests" ${PROJECT_IS_TOP_LEVEL})
option(SQLITEXX_WARNINGS_AS_ERRORS "Treat warnings in sqlitexx and its tests as errors (CI turns this on)" OFF)

# Compile options for sqlitexx's own code (not for SQLite)
if(MSVC)
    set(SQLITEXX_COMPILE_OPTIONS
        /W4
        /permissive-
        /utf-8
        /w14062 # switch on an enum misses a value and has no default
    )
    if(SQLITEXX_WARNINGS_AS_ERRORS)
        list(APPEND SQLITEXX_COMPILE_OPTIONS /WX)
    endif()
else()
    set(SQLITEXX_COMPILE_OPTIONS
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow # members have no prefix, so a local must not hide one
        -Wold-style-cast
        # Fallthrough is left to tests, not to [[fallthrough]] annotations. GCC turns the warning on in -Wextra and
        # Clang does not, so switch it off for both to keep them in step.
        -Wno-implicit-fallthrough
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        list(APPEND SQLITEXX_COMPILE_OPTIONS
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
        )
    endif()
    if(SQLITEXX_WARNINGS_AS_ERRORS)
        list(APPEND SQLITEXX_COMPILE_OPTIONS -Werror)
    endif()
endif()

# --- SQLite -----------------------------------------------------------------------------------------------------------
#
# By default the pinned SQLite amalgamation is downloaded (checked against its SHA3-256 hash) and built as a static
# library. For an offline build, unpack the zip somewhere and pass -DFETCHCONTENT_SOURCE_DIR_SQLITE3=<that directory>.
#
# Applications that already link a SQLite of their own, and package managers, set SQLITEXX_USE_SYSTEM_SQLITE=ON:
# two copies of SQLite in one process can corrupt databases on POSIX systems, because each copy keeps its own locks.

option(SQLITEXX_USE_SYSTEM_SQLITE "Use an installed SQLite (find_package) instead of building the pinned one" OFF)

# The oldest SQLite the native layer compiles against (sqlite3_changes64, SQLITE_OPEN_EXRESCODE)
set(SQLITEXX_SQLITE_MIN_VERSION 3.37)

if(SQLITEXX_USE_SYSTEM_SQLITE)
    find_package(SQLite3 ${SQLITEXX_SQLITE_MIN_VERSION} REQUIRED)

    # CMake 4.1 renamed the imported target; older versions only have SQLite::SQLite3
    if(TARGET SQLite3::SQLite3)
        set(SQLITEXX_SQLITE_TARGET SQLite3::SQLite3)
    else()
        set(SQLITEXX_SQLITE_TARGET SQLite::SQLite3)
    endif()
    message(STATUS "sqlitexx: using the installed SQLite ${SQLite3_VERSION}")
else()
    include(FetchContent)

    if(POLICY CMP0135)
        # Extracted files get the extraction time, so a new download rebuilds what depends on it
        cmake_policy(SET CMP0135 NEW)
    endif()

    FetchContent_Declare(sqlite3
        URL https://sqlite.org/2026/sqlite-amalgamation-3540100.zip
        URL_HASH SHA3_256=92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30
    )
    FetchContent_MakeAvailable(sqlite3)

    add_library(sqlitexx_sqlite3 STATIC "${sqlite3_SOURCE_DIR}/sqlite3.c")
    target_include_directories(sqlitexx_sqlite3 SYSTEM PUBLIC "$<BUILD_INTERFACE:${sqlite3_SOURCE_DIR}>")

    if(UNIX)
        find_package(Threads REQUIRED)
        target_link_libraries(sqlitexx_sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS} m)
    endif()

    set(SQLITEXX_SQLITE_TARGET sqlitexx_sqlite3)
    message(STATUS "sqlitexx: building SQLite 3.54.1 from the amalgamation")
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

# The public headers do not include sqlite3.h; users who want the raw C API link SQLite themselves.
target_link_libraries(sqlitexx PRIVATE ${SQLITEXX_SQLITE_TARGET})

target_compile_options(sqlitexx PRIVATE ${SQLITEXX_COMPILE_OPTIONS})
if(MSVC)
    target_compile_options(sqlitexx PUBLIC /Zc:__cplusplus)
endif()

# --- Tests ------------------------------------------------------------------------------------------------------------

if(SQLITEXX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
