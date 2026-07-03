#!/bin/bash
set -euo pipefail

ROOT="${MACHGATE_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
LIBCXX_DIR="${LIBCXX_DIR:-$ROOT/build-libcxx/lib}"
LIBCXX_INCLUDE="${LIBCXX_INCLUDE:-$ROOT/build-libcxx/include/c++/v1}"
REQUIRE="${MACHGATE_REQUIRE_LIBCXX_CTYPE_ABI:-0}"

skip_or_fail()
{
    if [ "$REQUIRE" = "1" ]; then
        echo "$1" >&2
        exit 1
    fi
    echo "SKIP: $1"
    exit 0
}

[ -f "$LIBCXX_DIR/libc++.so.1" ] || skip_or_fail "$LIBCXX_DIR/libc++.so.1 not built"
[ -f "$BUILD_DIR/libsystem_shim.so" ] || skip_or_fail "$BUILD_DIR/libsystem_shim.so not built"
[ -d "$LIBCXX_INCLUDE" ] || skip_or_fail "$LIBCXX_INCLUDE not built"

missing_sizes="$(
    nm -D -S "$LIBCXX_DIR/libc++.so.1" | c++filt |
        awk '
            /std::__1::ctype_base::(space|print|cntrl|upper|lower|alpha|digit|punct|xdigit|blank|alnum|graph)$/ {
                if ($2 != "0000000000000004")
                    print
                seen++
            }
            END {
                if (seen != 12)
                    print "missing ctype_base symbols"
            }
        '
)"
if [ -n "$missing_sizes" ]; then
    echo "$LIBCXX_DIR/libc++.so.1 does not expose 4-byte Darwin ctype masks:" >&2
    echo "$missing_sizes" >&2
    exit 1
fi

if ! nm -D "$LIBCXX_DIR/libc++.so.1" | awk '{ name = $NF; sub(/@.*/, "", name); if (name == "_DefaultRuneLocale") found = 1 } END { exit !found }'; then
    echo "$LIBCXX_DIR/libc++.so.1 does not bind ctype<char>::classic_table to _DefaultRuneLocale" >&2
    exit 1
fi

for symbol in _DefaultRuneLocale __maskrune __tolower __toupper; do
    if ! nm -D "$BUILD_DIR/libsystem_shim.so" | awk -v symbol="$symbol" '{ name = $NF; sub(/@.*/, "", name); if (name == symbol) found = 1 } END { exit !found }'; then
        echo "$BUILD_DIR/libsystem_shim.so does not export $symbol" >&2
        exit 1
    fi
done

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

cat > "$tmpdir/libcxx_ctype_probe.cpp" <<'CPP'
#include <locale>
#include <regex>
#include <string>

int main()
{
    if (sizeof(std::ctype_base::mask) != 4)
        return 10;
    if (std::ctype_base::alpha != 0x00000100)
        return 11;
    if (std::ctype_base::digit != 0x00000400)
        return 12;
    if (std::ctype_base::space != 0x00004000)
        return 13;
    if (std::ctype_base::punct != 0x00002000)
        return 14;
    if (std::ctype_base::blank != 0x00020000)
        return 15;

    const std::ctype<char>& ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
    if (!ctype.is(std::ctype_base::digit, '7'))
        return 20;
    if (!ctype.is(std::ctype_base::alpha, 'A'))
        return 21;
    if (!ctype.is(std::ctype_base::alpha, 'z'))
        return 22;
    if (!ctype.is(std::ctype_base::punct, '_'))
        return 23;
    if (!ctype.is(std::ctype_base::space, ' '))
        return 24;
    if (ctype.is(std::ctype_base::digit, 'A'))
        return 25;

    if (!std::regex_match(std::string("12345"), std::regex("^\\d+$")))
        return 30;
    if (std::regex_match(std::string("12a"), std::regex("^\\d+$")))
        return 31;
    if (!std::regex_match(std::string("Az_09"), std::regex("^\\w+$")))
        return 32;
    if (std::regex_match(std::string("with-dash"), std::regex("^\\w+$")))
        return 33;
    if (!std::regex_match(std::string("file[42].txt"), std::regex("^file\\[42\\]\\.txt$")))
        return 34;

    return 0;
}
CPP

"${CXX:-clang++}" \
    -std=c++17 \
    -nostdinc++ \
    -I "$LIBCXX_INCLUDE" \
    "$tmpdir/libcxx_ctype_probe.cpp" \
    -L "$LIBCXX_DIR" \
    -L "$BUILD_DIR" \
    -Wl,-rpath,"$LIBCXX_DIR" \
    -Wl,-rpath,"$BUILD_DIR" \
    -lc++ \
    -lc++abi \
    -lsystem_shim \
    -o "$tmpdir/libcxx_ctype_probe"

LD_LIBRARY_PATH="$BUILD_DIR:$LIBCXX_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$tmpdir/libcxx_ctype_probe"
