#!/usr/bin/env bash
# Builds the API reference into build/docs/html: Doxygen (docs/Doxyfile) with the doxygen-awesome-css theme, on Linux
# or in WSL. The docs workflow runs it the same way.
#
#   tools/docs.sh                    doxygen from PATH
#   DOXYGEN=/path/to/doxygen tools/docs.sh
#
# The theme is downloaded once into build/docs/theme and checked against the hashes below. Any Doxygen warning fails
# the build (WARN_AS_ERROR in the Doxyfile).

set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
doxygen=${DOXYGEN:-doxygen}

theme_version=v2.5.0
theme_dir="$root/build/docs/theme"
theme_files=(doxygen-awesome.css doxygen-awesome-sidebar-only.css)
theme_hashes=(
    ea37516ec7a7a7780719b9d9a285fe3268a8d5411523d8b93af98d77e67cd1a1
    909d17055552149555fc87c5e0cf171a41d09a9e358c389a51b25ddfafd462af
)

mkdir -p "$theme_dir"
for i in "${!theme_files[@]}"; do
    file="$theme_dir/${theme_files[$i]}"
    if [ ! -f "$file" ] || ! echo "${theme_hashes[$i]}  $file" | sha256sum --check --status; then
        curl -sSLf -o "$file" \
            "https://raw.githubusercontent.com/jothepro/doxygen-awesome-css/$theme_version/${theme_files[$i]}"
        echo "${theme_hashes[$i]}  $file" | sha256sum --check --quiet
    fi
done

# The version shown in the reference is the one in CMakeLists.txt
version=$(sed -n 's/^ *VERSION \([0-9][0-9.]*\)\r*$/\1/p' "$root/CMakeLists.txt" | head -n 1)

rm -rf "$root/build/docs/html"
cd "$root/docs"
(
    cat Doxyfile
    echo "PROJECT_NUMBER = $version"
) | "$doxygen" -
echo "Reference: build/docs/html/index.html"
