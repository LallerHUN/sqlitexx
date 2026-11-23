#!/usr/bin/env bash
# Runs the Linux jobs of .github/workflows/ci.yml locally, on Linux or in WSL, against the working tree as it is
# (uncommitted changes included). Build directories are kept between runs, so later runs only rebuild what changed.
#
#   tools/check.sh              all jobs
#   tools/check.sh gcc17 format the named jobs only
#   tools/check.sh --list       the job names
#
# Needs gcc, clang, cmake, ninja, libsqlite3-dev and clang-format 22.1.3 (pipx install clang-format==22.1.3).
# SQLITEXX_CHECK_DIR sets where the build directories go (default: ~/.cache/sqlitexx-check). In WSL keep it in the
# Linux file system: building under /mnt/c is several times slower.

set -u

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
work="${SQLITEXX_CHECK_DIR:-$HOME/.cache/sqlitexx-check}"
clang_format="${CLANG_FORMAT:-$(command -v clang-format || true)}"
if [ -x "$HOME/.local/bin/clang-format" ] && [ -z "${CLANG_FORMAT:-}" ]
then
    clang_format="$HOME/.local/bin/clang-format"
fi

gcc_args=(-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++)
clang_args=(-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++)
sanitizers="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"

# The same names as in ci.yml, shortened
all_jobs=(gcc17 gcc20 gcc23 clang-asan gcc-noexc17 gcc-noexc23 gcc-system-sqlite install-gcc install-system-sqlite format)

# build <job> <build type> <C++ standard> <extra CMake arguments...>: a build job from ci.yml
build()
{
    local dir="$work/$1" type="$2" std="$3"
    shift 3
    cmake -S "$root" -B "$dir" -G Ninja \
        "-DCMAKE_BUILD_TYPE=$type" \
        "-DCMAKE_CXX_STANDARD=$std" \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DSQLITEXX_WARNINGS_AS_ERRORS=ON \
        "$@" &&
        cmake --build "$dir" --parallel &&
        ctest --test-dir "$dir" --output-on-failure
}

# install <job> <extra CMake arguments...>: an install job from ci.yml
install()
{
    local dir="$work/$1"
    shift
    rm -rf "$dir/prefix" &&
        cmake -S "$root" -B "$dir/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
            -DSQLITEXX_BUILD_TESTS=OFF -DSQLITEXX_BUILD_EXAMPLES=OFF "$@" &&
        cmake --build "$dir/build" --parallel &&
        cmake --install "$dir/build" --prefix "$dir/prefix" &&
        cmake -S "$root/tests/install" -B "$dir/consumer" -G Ninja -DCMAKE_BUILD_TYPE=Release \
            "-DCMAKE_PREFIX_PATH=$dir/prefix" -DCMAKE_CXX_COMPILER=g++ &&
        cmake --build "$dir/consumer" &&
        ctest --test-dir "$dir/consumer" --output-on-failure
}

format()
{
    if [ -z "$clang_format" ]
    then
        echo "clang-format not found: pipx install clang-format==22.1.3"
        return 1
    fi
    "$clang_format" --version
    case "$("$clang_format" --version)" in
    *" 22.1.3"*) ;;
    *) echo "warning: CI uses clang-format 22.1.3, other versions can format differently" ;;
    esac
    # Tracked and new files, so a file not yet added is checked too
    git -c "safe.directory=$root" -C "$root" ls-files -z --cached --others --exclude-standard \
        '*.hpp' '*.cpp' '*.h' |
        (cd "$root" && xargs -0 "$clang_format" --dry-run --Werror)
}

run()
{
    case "$1" in
    gcc17) build "$1" Release 17 "${gcc_args[@]}" ;;
    gcc20) build "$1" Release 20 "${gcc_args[@]}" ;;
    gcc23) build "$1" Release 23 "${gcc_args[@]}" ;;
    clang-asan)
        build "$1" Debug 17 "${clang_args[@]}" "-DCMAKE_CXX_FLAGS=$sanitizers" \
            "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined"
        ;;
    gcc-noexc17) build "$1" Release 17 "${gcc_args[@]}" -DCMAKE_CXX_FLAGS=-fno-exceptions ;;
    gcc-noexc23) build "$1" Release 23 "${gcc_args[@]}" -DCMAKE_CXX_FLAGS=-fno-exceptions ;;
    gcc-system-sqlite) build "$1" Release 17 "${gcc_args[@]}" -DSQLITEXX_USE_SYSTEM_SQLITE=ON ;;
    install-gcc) install "$1" "${gcc_args[@]}" ;;
    install-system-sqlite) install "$1" "${gcc_args[@]}" -DCMAKE_CXX_STANDARD=23 -DSQLITEXX_USE_SYSTEM_SQLITE=ON ;;
    format) format ;;
    *)
        echo "unknown job: $1 (tools/check.sh --list shows the names)"
        return 2
        ;;
    esac
}

if [ "${1:-}" = "--list" ]
then
    printf '%s\n' "${all_jobs[@]}"
    exit 0
fi

jobs=("$@")
if [ ${#jobs[@]} -eq 0 ]
then
    jobs=("${all_jobs[@]}")
fi

mkdir -p "$work"
failed=()
for job in "${jobs[@]}"
do
    echo
    echo "=== $job"
    log="$work/$job.log"
    if run "$job" >"$log" 2>&1
    then
        echo "passed"
    else
        failed+=("$job")
        tail -n 40 "$log"
        echo "FAILED (full log: $log)"
    fi
done

echo
if [ ${#failed[@]} -eq 0 ]
then
    echo "All ${#jobs[@]} jobs passed."
    exit 0
fi
echo "${#failed[@]} of ${#jobs[@]} jobs failed: ${failed[*]}"
exit 1
