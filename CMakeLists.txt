cmake_minimum_required(VERSION 3.21)

project(sqlitexx
    VERSION 0.6.0
    DESCRIPTION "C++ wrapper for SQLite"
    LANGUAGES C CXX
)

option(SQLITEXX_BUILD_TESTS "Build the sqlitexx tests" ${PROJECT_IS_TOP_LEVEL})
option(SQLITEXX_BUILD_EXAMPLES "Build the sqlitexx examples (and run them as tests)" ${PROJECT_IS_TOP_LEVEL})
option(SQLITEXX_WARNINGS_AS_ERRORS "Treat warnings in sqlitexx and its tests as errors (CI turns this on)" OFF)
option(SQLITEXX_INSTALL "Generate the install rules (cmake --install) and the CMake package" ${PROJECT_IS_TOP_LEVEL})

include(GNUInstallDirs)

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
    set_target_properties(sqlitexx_sqlite3 PROPERTIES
        EXPORT_NAME sqlite3
        DEBUG_POSTFIX d
    )

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
    set(sqlitexx_expected_feature cxx_std_11)

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

        # Installed under sqlitexx's own include directory, so it cannot replace a different expected-lite version that
        # the user has installed as <nonstd/expected.hpp>
        add_library(sqlitexx_expected_lite INTERFACE)
        target_include_directories(sqlitexx_expected_lite SYSTEM INTERFACE
            "$<BUILD_INTERFACE:${expected_lite_SOURCE_DIR}/include>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/sqlitexx/third_party>"
        )
        set_target_properties(sqlitexx_expected_lite PROPERTIES EXPORT_NAME expected_lite)
        set(sqlitexx_expected_target sqlitexx_expected_lite)
        message(STATUS "sqlitexx: expected is expected-lite 0.10.0")
    endif()
else()
    message(FATAL_ERROR "SQLITEXX_EXPECTED must be AUTO, STD or LITE, not '${SQLITEXX_EXPECTED}'")
endif()

# --- string_view and optional -----------------------------------------------------------------------------------------
#
# The public API takes string_view and returns optional: std:: ones from C++17, string-view-lite and optional-lite
# before it. As with expected, the choice is made here once, for the library and everything linking it.
#
#   AUTO  the std:: types unless CMAKE_CXX_STANDARD is set below 17, the backports then
#   STD   the std:: types; sqlitexx and everything linking it then need C++17
#   LITE  string-view-lite and optional-lite, whatever the standard

set(SQLITEXX_CXX17_TYPES "AUTO" CACHE STRING "string_view and optional for the API: AUTO, STD or LITE")
set_property(CACHE SQLITEXX_CXX17_TYPES PROPERTY STRINGS AUTO STD LITE)

option(SQLITEXX_USE_SYSTEM_LITE_TYPES
    "Use installed string-view-lite and optional-lite (find_package) instead of the pinned ones" OFF
)

set(sqlitexx_cxx17_types "${SQLITEXX_CXX17_TYPES}")
if(sqlitexx_cxx17_types STREQUAL "AUTO")
    set(sqlitexx_cxx17_types STD)
    if(DEFINED CMAKE_CXX_STANDARD AND (CMAKE_CXX_STANDARD LESS 17 OR CMAKE_CXX_STANDARD EQUAL 98))
        set(sqlitexx_cxx17_types LITE)
    endif()
endif()

if(sqlitexx_cxx17_types STREQUAL "STD")
    set(sqlitexx_cxx17_definitions SQLITEXX_USE_STD_CXX17_TYPES=1)
    set(sqlitexx_cxx17_feature cxx_std_17)
    set(sqlitexx_cxx17_targets "")
    message(STATUS "sqlitexx: string_view and optional are std::string_view and std::optional")
elseif(sqlitexx_cxx17_types STREQUAL "LITE")
    # As for expected-lite: keep the nonstd types classes of their own, even when compiled as C++17
    set(sqlitexx_cxx17_definitions
        SQLITEXX_USE_STD_CXX17_TYPES=0
        nssv_CONFIG_SELECT_STRING_VIEW=nssv_STRING_VIEW_NONSTD
        optional_CONFIG_SELECT_OPTIONAL=optional_OPTIONAL_NONSTD
    )
    set(sqlitexx_cxx17_feature cxx_std_11)

    if(SQLITEXX_USE_SYSTEM_LITE_TYPES)
        find_package(string-view-lite CONFIG REQUIRED)
        find_package(optional-lite CONFIG REQUIRED)
        set(sqlitexx_cxx17_targets nonstd::string-view-lite nonstd::optional-lite)
        message(STATUS "sqlitexx: string_view and optional are the installed string-view-lite and optional-lite")
    else()
        FetchContent_Declare(string_view_lite
            URL https://github.com/nonstd-lite/string-view-lite/archive/refs/tags/v1.8.2.tar.gz
            URL_HASH SHA256=62852addccb3b73b72030c6afd130e8b63f9b2a00b032b0af4048b0cc6b49f82
            SOURCE_SUBDIR do-not-add
        )
        FetchContent_Declare(optional_lite
            URL https://github.com/nonstd-lite/optional-lite/archive/refs/tags/v3.6.0.tar.gz
            URL_HASH SHA256=2be17fcfc764809612282c3e728cabc42afe703b9dc333cc87c48d882fcfc2c2
            SOURCE_SUBDIR do-not-add
        )
        FetchContent_MakeAvailable(string_view_lite optional_lite)

        add_library(sqlitexx_lite_types INTERFACE)
        target_include_directories(sqlitexx_lite_types SYSTEM INTERFACE
            "$<BUILD_INTERFACE:${string_view_lite_SOURCE_DIR}/include>"
            "$<BUILD_INTERFACE:${optional_lite_SOURCE_DIR}/include>"
            "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/sqlitexx/third_party>"
        )
        set_target_properties(sqlitexx_lite_types PROPERTIES EXPORT_NAME lite_types)
        set(sqlitexx_cxx17_targets sqlitexx_lite_types)
        message(STATUS "sqlitexx: string_view and optional are string-view-lite 1.8.2 and optional-lite 3.6.0")
    endif()
else()
    message(FATAL_ERROR "SQLITEXX_CXX17_TYPES must be AUTO, STD or LITE, not '${SQLITEXX_CXX17_TYPES}'")
endif()

# --- sqlitexx ---------------------------------------------------------------------------------------------------------

add_library(sqlitexx STATIC
    src/backup.cpp
    src/blob_stream.cpp
    src/database.cpp
    src/hook_slots.cpp
    src/native/backup.cpp
    src/native/blob_handle.cpp
    src/native/connection.cpp
    src/native/context.cpp
    src/native/result.cpp
    src/native/statement.cpp
    src/native/value.cpp
    src/registrations.cpp
    src/statement.cpp
    src/statement_cache.cpp
    src/transaction.cpp
)
add_library(sqlitexx::sqlitexx ALIAS sqlitexx)

target_compile_features(sqlitexx PUBLIC ${sqlitexx_expected_feature} ${sqlitexx_cxx17_feature})
target_compile_definitions(sqlitexx PUBLIC ${sqlitexx_expected_definitions} ${sqlitexx_cxx17_definitions})
target_link_libraries(sqlitexx PUBLIC ${sqlitexx_expected_target} ${sqlitexx_cxx17_targets})
set_target_properties(sqlitexx PROPERTIES
    CXX_EXTENSIONS OFF
    DEBUG_POSTFIX d
)

target_include_directories(sqlitexx
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# The public headers do not include sqlite3.h; users who want the raw C API link SQLite themselves.
target_link_libraries(sqlitexx PRIVATE ${SQLITEXX_SQLITE_TARGET})

target_compile_options(sqlitexx PRIVATE ${SQLITEXX_COMPILE_OPTIONS})
if(MSVC)
    target_compile_options(sqlitexx PUBLIC /Zc:__cplusplus)
endif()

# --- Install ----------------------------------------------------------------------------------------------------------
#
# cmake --install puts the headers, the libraries and a CMake package into the install prefix, for
#
#     find_package(sqlitexx 0.6 REQUIRED)
#     target_link_libraries(my_app PRIVATE sqlitexx::sqlitexx)
#
# The bundled SQLite (sqlitexx::sqlite3) and the bundled backports (expected-lite, string-view-lite, optional-lite) are
# installed along; installed ones (SQLITEXX_USE_SYSTEM_*) are looked up again by the package. Debug libraries get a 'd'
# suffix, so Debug and Release can share one prefix.

if(SQLITEXX_INSTALL)
    include(CMakePackageConfigHelpers)

    set(sqlitexx_cmake_dir "${CMAKE_INSTALL_LIBDIR}/cmake/sqlitexx")
    set(sqlitexx_doc_dir "${CMAKE_INSTALL_DATADIR}/doc/sqlitexx")

    set(sqlitexx_install_targets sqlitexx)
    if(TARGET sqlitexx_sqlite3)
        list(APPEND sqlitexx_install_targets sqlitexx_sqlite3)
    endif()
    if(TARGET sqlitexx_expected_lite)
        list(APPEND sqlitexx_install_targets sqlitexx_expected_lite)
        install(FILES "${expected_lite_SOURCE_DIR}/include/nonstd/expected.hpp"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sqlitexx/third_party/nonstd"
        )
        install(FILES "${expected_lite_SOURCE_DIR}/LICENSE.txt"
            DESTINATION "${sqlitexx_doc_dir}"
            RENAME expected-lite-LICENSE.txt
        )
    endif()
    if(TARGET sqlitexx_lite_types)
        list(APPEND sqlitexx_install_targets sqlitexx_lite_types)
        install(FILES
            "${string_view_lite_SOURCE_DIR}/include/nonstd/string_view.hpp"
            "${optional_lite_SOURCE_DIR}/include/nonstd/optional.hpp"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sqlitexx/third_party/nonstd"
        )
        install(FILES "${string_view_lite_SOURCE_DIR}/LICENSE.txt"
            DESTINATION "${sqlitexx_doc_dir}"
            RENAME string-view-lite-LICENSE.txt
        )
        install(FILES "${optional_lite_SOURCE_DIR}/LICENSE.txt"
            DESTINATION "${sqlitexx_doc_dir}"
            RENAME optional-lite-LICENSE.txt
        )
    endif()

    install(TARGETS ${sqlitexx_install_targets}
        EXPORT sqlitexxTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(DIRECTORY include/sqlitexx DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(FILES LICENSE README.md DESTINATION "${sqlitexx_doc_dir}")

    install(EXPORT sqlitexxTargets
        NAMESPACE sqlitexx::
        DESTINATION "${sqlitexx_cmake_dir}"
    )

    # What the package has to find again in the consumer's build
    set(sqlitexx_needs_threads OFF)
    if(TARGET sqlitexx_sqlite3 AND UNIX)
        set(sqlitexx_needs_threads ON)
    endif()
    set(sqlitexx_needs_expected_lite OFF)
    if(sqlitexx_expected STREQUAL "LITE" AND SQLITEXX_USE_SYSTEM_EXPECTED_LITE)
        set(sqlitexx_needs_expected_lite ON)
    endif()
    set(sqlitexx_needs_lite_types OFF)
    if(sqlitexx_cxx17_types STREQUAL "LITE" AND SQLITEXX_USE_SYSTEM_LITE_TYPES)
        set(sqlitexx_needs_lite_types ON)
    endif()

    if(SQLITEXX_USE_SYSTEM_SQLITE)
        set(sqlitexx_sqlite_summary "installed, found with find_package(SQLite3)")
    else()
        set(sqlitexx_sqlite_summary "bundled 3.54.1, installed as sqlitexx::sqlite3")
    endif()
    if(sqlitexx_expected STREQUAL "STD")
        set(sqlitexx_expected_summary "std::expected (C++23)")
    elseif(SQLITEXX_USE_SYSTEM_EXPECTED_LITE)
        set(sqlitexx_expected_summary "installed expected-lite, found with find_package(expected-lite)")
    else()
        set(sqlitexx_expected_summary "bundled expected-lite 0.10.0, installed in include/sqlitexx/third_party")
    endif()
    if(sqlitexx_cxx17_types STREQUAL "STD")
        set(sqlitexx_cxx17_summary "std::string_view and std::optional (C++17)")
    elseif(SQLITEXX_USE_SYSTEM_LITE_TYPES)
        set(sqlitexx_cxx17_summary "installed string-view-lite and optional-lite, found with find_package")
    else()
        set(sqlitexx_cxx17_summary
            "bundled string-view-lite 1.8.2 and optional-lite 3.6.0, installed in include/sqlitexx/third_party"
        )
    endif()

    configure_package_config_file(cmake/sqlitexxConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/sqlitexxConfig.cmake"
        INSTALL_DESTINATION "${sqlitexx_cmake_dir}"
    )
    # Before 1.0, minor versions may break the API, so only the same minor version counts as compatible
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/sqlitexxConfigVersion.cmake"
        COMPATIBILITY SameMinorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/sqlitexxConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/sqlitexxConfigVersion.cmake"
        DESTINATION "${sqlitexx_cmake_dir}"
    )
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
