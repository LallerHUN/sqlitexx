# Contributing to sqlitexx

Thanks for helping. This page covers how to build and test, the code rules that tools can't check for you, and how
changes get in.

For anything bigger than a small fix, open an issue first, so we can agree on the approach before you spend time on
it.

## Building and testing

```shell
cmake -S . -B build -DSQLITEXX_WARNINGS_AS_ERRORS=ON
cmake --build build --config Debug
ctest --test-dir build --build-config Debug --output-on-failure
```

CMake downloads SQLite and the backports (expected-lite, string-view-lite, optional-lite) on the first configure; see
the README for the options and for offline builds. Build with `SQLITEXX_WARNINGS_AS_ERRORS=ON` locally too: CI does,
and a warning fails the pull request.

CI also builds:

- with C++11, C++14, C++17, C++20 and C++23 (MSVC from C++14), so with both the backports and the standard types;
- with exceptions disabled (`-fno-exceptions`, or `/EHs-c- -D_HAS_EXCEPTIONS=0` on MSVC);
- with Clang's AddressSanitizer and UndefinedBehaviorSanitizer;
- against the system SQLite (`SQLITEXX_USE_SYSTEM_SQLITE=ON`);
- the installed package, used from [tests/install](tests/install) through `find_package(sqlitexx)`.

If you change something that only one of those touches, try it locally the same way; the workflow in
[.github/workflows/ci.yml](.github/workflows/ci.yml) shows the exact flags.

On Linux, or in WSL on Windows, [tools/check.sh](tools/check.sh) runs all of CI's Linux jobs on your working tree,
uncommitted changes included, and reports which ones fail:

```shell
tools/check.sh                   # every job
tools/check.sh clang-asan format # only these; --list shows the names
```

It needs gcc, clang, cmake, ninja, the SQLite development files (`libsqlite3-dev`) and clang-format 22.1.3. Build
directories are kept in `~/.cache/sqlitexx-check`, so a second run only rebuilds what changed. In WSL, run it on the
Windows checkout (`wsl --cd /mnt/c/<path to sqlitexx> -e bash tools/check.sh`); the builds still go to the Linux
file system, where they are fast.

### The reference

The [API reference](https://lallerhun.github.io/sqlitexx/) is generated from the `///` comments in the headers by
Doxygen, with the doxygen-awesome-css theme. [tools/docs.sh](tools/docs.sh) builds it into `build/docs/html`, the
same way the [docs workflow](.github/workflows/docs.yml) does:

```shell
tools/docs.sh                          # doxygen from PATH
DOXYGEN=/path/to/doxygen tools/docs.sh # or a given one; CI uses the official 1.15.0 release
```

Every pull request builds it, and a Doxygen warning fails the build. Pushes to `main`, which means releases, publish
it to GitHub Pages.

Doc comments use `///` and document the declaration right below them; the first sentence is the summary. A header's
opening comment starts with `/// \file` and becomes its file page. Section labels in a class are member groups:
`/// \name Statements`, the group's description, then `///@{` and `///@}` around its members. Plain `//` comments
are for the implementation and stay out of the reference.

## Where things go

| Path | What |
|---|---|
| `include/sqlitexx/` | Public headers. `sqlitexx.hpp` includes all of them. |
| `include/sqlitexx/native/` | The native layer: the one-to-one mirror of the C API. |
| `include/sqlitexx/detail/` | Public-header internals that users must not rely on. |
| `src/` | Implementation files. |
| `src/detail/` | Private headers. |
| `src/impl_sqlitexx.hpp` | The implementation files' single include. It only includes other headers. |
| `tests/` | One test program per area (`native_test`, `typed_test`, ...). |
| `examples/` | Programs shown in the README. CI builds and runs them, so keep them in step with the README. |
| `docs/` | The Doxygen configuration of the API reference (see [The reference](#the-reference)). |
| `tools/` | Scripts for contributors: `check.sh` runs the CI jobs, `docs.sh` builds the reference. |

A new declaration gets its own header, named after it, rather than being added to an unrelated one.

## Formatting

[.clang-format](.clang-format) decides the layout. CI checks it with **clang-format 22.1.3**; other versions can
format differently. Visual Studio 2026 ships that version, and `pip install clang-format==22.1.3` installs it
anywhere. Format your changes before committing:

```shell
clang-format -i <changed files>
```

The main choices, so they don't surprise you: Allman braces, braces around every body (clang-format adds missing
ones), 120 columns, and a declaration with two or more parameters puts each on its own line, even when it would fit
on one. Template parameter lists break the same way.

## Code rules clang-format can't check

### Order inside a class

In each `public`, `protected` and `private` section: types, constants and data members first, then the functions,
grouped by what they do (lifetime, binding, reading, ...), with a short comment heading each group.

### Naming

- `snake_case` for everything except template parameters, which are `CamelCase` and say what they stand for
  (`Predicate`, `Handler`, `Callback`), not `F` or `T` when a better name exists.
- No prefixes or suffixes on data members (`m_`, `_`). A member is named for **what it holds**: `db`, `stmt`,
  `callback`.
- A constructor parameter is named for **where the value comes from** or what role it plays: `prepared`, `opened`,
  `other`, `target`. That keeps it from clashing with the member it initializes; `-Wshadow` catches the cases that
  slip through.
- When a member would clash with a member function, the member gives way: `error` has `code()` and `message()`, and
  stores `result_code` and `text`.
- Internal helper types that only bundle values (in `detail`) are aggregates, filled with braces, rather than classes
  with a constructor.

### C++ standard

sqlitexx supports C++11 and newer, and CI builds every standard from C++11 to C++23. Library code, tests and examples
are all C++11:

- no `if constexpr` (use tag dispatch or overloads), fold expressions, structured bindings, `auto` return type
  deduction, generic lambdas, inline variables, class template argument deduction or `namespace a::b`;
- nothing from C++14 either: no `std::make_unique`, `std::enable_if_t` and the other `_t` aliases, `constexpr`
  functions with more than a return statement, variable templates, or digit separators;
- take vocabulary types from sqlitexx's aliases (`sqlitexx::string_view`, `sqlitexx::optional`,
  `sqlitexx::expected`), never from `std::` directly: before C++17 (or C++23 for `expected`) they are backports;
- helpers that standard C++ only has from C++14 or C++17 on live in `detail/utility.hpp` (`index_sequence`,
  `apply`).

Tests and examples follow the same rules, since CI builds them as C++11 too.

Support for types from newer standards (`std::byte`, `std::span`, `char8_t`) lives in headers of its own
([value_bytes.hpp](include/sqlitexx/value_bytes.hpp), [value_char8.hpp](include/sqlitexx/value_char8.hpp)), behind
feature-test macros such as `__cpp_lib_span`. Code behind such a macro may use the standard it tests for. It only adds
new `value_traits` specializations and never changes an existing declaration, so translation units compiled with
different standards still agree on every type they share.

### Errors, never exceptions

- sqlitexx never throws. Functions that can fail return `expected<T, error>` (typed layer) or `native::result`
  (native layer). No `throw`, no `try`: CI builds with exceptions disabled.
- Errors sqlitexx detects itself use SQLite result codes too (`done`, `mismatch`, `misuse`), so callers check one
  kind of code. The README's Errors section lists their meanings; add a line there when you introduce a new case.
- Destructors and moves are `noexcept`.

### Headers

- Public headers never include `sqlite3.h`. The native layer forward-declares `sqlite3` and `sqlite3_stmt`, and
  `src/native/result.cpp` checks with `static_assert` that sqlitexx's copies of SQLite's constants still match.
- Write `(std::numeric_limits<T>::max)()`, with the parentheses: `<windows.h>` without `NOMINMAX` defines `min` and
  `max` macros. `typed_test.cpp` includes `<windows.h>` first to keep this honest.

### Warnings

GCC and Clang build with `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast`, MSVC
with `/W4` and C4062; [CMakeLists.txt](CMakeLists.txt) has the full list. Fix warnings rather than silencing them.
The one deliberate gap is implicit fallthrough: a `switch` case that runs on into the next is left to the tests
rather than marked with `[[fallthrough]]`.

## Tests

Every change in behavior comes with a test. The tests are plain programs using `CHECK` from
[tests/check.hpp](tests/check.hpp); add a function to the matching `*_test.cpp` and call it from `main`. Test the
failure paths too: the error code, and the message when it matters.

## Commits and pull requests

- Branch from `develop` and name the branch `<kind>/<topic>`, where kind is `feature`, `fix`, `docs`, `ci` or
  `refactor`: `feature/backup`, `fix/blob-size`. No `#` in branch names; it breaks URLs and some tools.
- Open the pull request against `develop`. Keep it to one concern; a rename or a reformat goes in its own pull
  request.
- Commit subjects say what changed, starting with the area: `Transactions: RAII transaction and in_transaction`. The
  body says why, and anything a reviewer could not tell from the diff.
- A change users can notice (a new function, different behavior, a fix) adds a line to `## [Unreleased]` in
  [CHANGELOG.md](CHANGELOG.md), under Added, Changed, Deprecated, Removed, Fixed or Security. Mark a change that
  breaks the API with **Breaking:**. Tests, CI and internal refactoring need no entry.
- Pull requests are merged with a merge commit once CI passes, so every commit in your branch stays in the history:
  make each one meaningful.

## Branches and releases

- `develop` is where work lands. It always builds and passes CI, but its API may change at any time.
- `main` holds released versions only: every commit on it is a release, tagged `vX.Y.Z`.
- Versions follow [Semantic Versioning](https://semver.org). Before 1.0, a minor version (`0.2`) may break the API,
  which is why the CMake package only accepts the same minor version.

### Making a release

A release goes through a short-lived release branch between `develop` and `main`, so that `develop` can keep moving
and fixes found during the release land in one place.

1. Cut `release/vX.Y.Z` from `develop`. Set the version in `CMakeLists.txt` and wherever `find_package(sqlitexx X.Y)`
   appears (README, `tests/install`, the install comment in `CMakeLists.txt`). In CHANGELOG.md, turn
   `## [Unreleased]` into `## [X.Y.Z] - YYYY-MM-DD` with a new empty `[Unreleased]` above it, and update the
   compare links at the bottom. Push.
2. Open a pull request from `release/vX.Y.Z` into `main`, and write the release notes while CI runs: the version's
   CHANGELOG.md section, plus upgrading steps, requirements and known limits. Anything that has to change for the
   release, such as a fix CI or the review turns up, is committed to the release branch; features stay on `develop`
   for the next release.
3. Review the pull request and the notes together, before merging. `main` holds only tagged releases, so nothing is
   merged into it that can't be tagged right away.
4. When CI passes and the review is done, merge it into `main`, put an annotated tag `vX.Y.Z` on that merge commit,
   and publish the GitHub release with the reviewed notes, one step after the other.
5. Open a pull request from `release/vX.Y.Z` into `develop` and merge it, so the version and any release fixes reach
   `develop` too. The release is finished only after this step: skipping it leaves `develop` behind `main`.
6. Delete the release branch.

### Patch releases

A fix for the latest release is a release of its own: cut `release/vX.Y.Z+1` from the latest tag (the tip of
`main`), commit the fix, the version bump and its CHANGELOG.md section to it, and continue from step 2 above. If the
fix is already on `develop`, cherry-pick it.

`main` follows only the newest minor version. To fix an older one after a newer minor is out, create
`support/X.Y` from its latest tag when the first such fix is needed, not in advance:

1. Pull requests with the fix, the version bump and the CHANGELOG.md section go into `support/X.Y`.
2. Tag the merge commit on `support/X.Y` with `vX.Y.Z` and publish a GitHub release. Nothing is merged into `main`.
3. Open a pull request into `develop` with the release's CHANGELOG.md section, so that `develop` lists every release,
   and the fix itself (a cherry-pick) if it applies there.

## License

sqlitexx is under the [Boost Software License 1.0](LICENSE). By contributing, you agree that your contribution is
distributed under the same license.
