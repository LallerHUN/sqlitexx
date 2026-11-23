cmake_minimum_required(VERSION 3.21)

project(sqlitexx
    VERSION 0.1.0
    DESCRIPTION "C++ wrapper for SQLite"
    LANGUAGES C CXX
)

option(SQLITEXX_BUILD_TESTS "Build the sqlitexx tests" ${PROJECT_IS_TOP_LEVEL})
option(SQLITEXX_BUILD_EXAMPLES "Build the sqlitexx examples (and run them as tests)" ${PROJECT_IS_TOP_LEVEL})
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

# --- Dependencies -----------------------------------------------------------------------------------------------------

include(FetchContent)

if(POLICY CMP0135)
    # Extracted files get the extraction time, so a new download rebuilds what depends on it
    cmake_policy(SET CMP0135 NEW)
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

# --- expected ---------------------------------------------------------------------------------------------------------
#
# The typed API returns expected<T, error>: std::expected from C++23, expected-lite before it. The choice is made here,
# once, and handed to everything that links sqlitexx as a compile definition. Letting each translation unit pick for
# itself would give one class two different definitions in one program (an ODR violation) whenever the library and
# its users are compiled with different C++ standards.
#
#   AUTO  std::expected when CMAKE_CXX_STANDARD is 23 or newer and the standard library has it, expected-lite otherwise
#   STD   std::expected; sqlitexx and everything linking it then need C++23
#   LITE  expected-lite, whatever the standard

set(SQLITEXX_EXPECTED "AUTO" CACHE STRING "expected<T, E> for the typed API: AUTO, STD or LITE")
set_property(CACHE SQLITEXX_EXPECTED PROPERTY STRINGS AUTO STD LITE)

option(SQLITEXX_USE_SYSTEM_EXPECTED_LITE "Use an installed expected-lite (find_package) instead of the pinned one" OFF)

set(sqlitexx_expected "${SQLITEXX_EXPECTED}")
if(sqlitexx_expected STREQUAL "AUTO")
    set(sqlitexx_expected LITE)
    if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD GREATER_EQUAL 23 AND NOT CMAKE_CXX_STANDARD EQUAL 98)
        include(CheckCXXSourceCompiles)
        check_cxx_source_compiles([[
            #include <expected>
            #if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202211L
            #error "no std::expected with monadic operations"
            #endif
            int main() { return 0; }
        ]] SQLITEXX_HAVE_STD_EXPECTED)
        if(SQLITEXX_HAVE_STD_EXPECTED)
            set(sqlitexx_expected STD)
        endif()
    endif()
endif()

if(sqlitexx_expected STREQUAL "STD")
    set(sqlitexx_expected_definitions SQLITEXX_USE_STD_EXPECTED=1)
    set(sqlitexx_expected_feature cxx_std_23)
    set(sqlitexx_expected_target "")
    message(STATUS "sqlitexx: expected is std::expected")
elseif(sqlitexx_expected STREQUAL "LITE")
    # nsel_CONFIG_SELECT_EXPECTED keeps nonstd::expected a class of its own: expected-lite would otherwise turn it into
    # std::expected in translation units compiled as C++23.
    set(sqlitexx_expected_definitions
        SQLITEXX_USE_STD_EXPECTED=0
        nsel_CONFIG_SELECT_EXPECTED=nsel_EXPECTED_NONSTD
    )
    set(sqlitexx_expected_feature cxx_std_17)

    if(SQLITEXX_USE_SYSTEM_EXPECTED_LITE)
        find_package(expected-lite CONFIG REQUIRED)
        set(sqlitexx_expected_target nonstd::expected-lite)
        message(STATUS "sqlitexx: expected is the installed expected-lite ${expected-lite_VERSION}")
    else()
        # SOURCE_SUBDIR names a directory that does not exist, so only the header is used and expected-lite's own
        # CMake project (its tests and install rules) stays out of this build.
        FetchContent_Declare(expected_lite
            URL https://github.com/nonstd-lite/expected-lite/archive/refs/tags/v0.10.0.tar.gz
            URL_HASH SHA256=cfe082e4ffedeeedac47763504102646a39c080599c7c1fe99299d6a1f99af92
            SOURCE_SUBDIR do-not-add
        )
        FetchContent_MakeAvailable(expected_lite)

        add_library(sqlitexx_expected_lite INTERFACE)
        target_include_directories(sqlitexx_expected_lite SYSTEM INTERFACE
            "$<BUILD_INTERFACE:${expected_lite_SOURCE_DIR}/include>"
        )
        set(sqlitexx_expected_target sqlitexx_expected_lite)
        message(STATUS "sqlitexx: expected is expected-lite 0.10.0")
    endif()
else()
    message(FATAL_ERROR "SQLITEXX_EXPECTED must be AUTO, STD or LITE, not '${SQLITEXX_EXPECTED}'")
endif()

# --- sqlitexx ---------------------------------------------------------------------------------------------------------

add_library(sqlitexx STATIC
    src/database.cpp
    src/native/connection.cpp
    src/native/result.cpp
    src/native/statement.cpp
    src/statement.cpp
)
add_library(sqlitexx::sqlitexx ALIAS sqlitexx)

target_compile_features(sqlitexx PUBLIC ${sqlitexx_expected_feature})
target_compile_definitions(sqlitexx PUBLIC ${sqlitexx_expected_definitions})
target_link_libraries(sqlitexx PUBLIC ${sqlitexx_expected_target})
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

# --- Tests and examples -----------------------------------------------------------------------------------------------

if(SQLITEXX_BUILD_TESTS OR SQLITEXX_BUILD_EXAMPLES)
    enable_testing()
endif()

if(SQLITEXX_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(SQLITEXX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
