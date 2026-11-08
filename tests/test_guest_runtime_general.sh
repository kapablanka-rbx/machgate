#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
[ -f tests/fixtures/darwin_initializer_working_directory ] ||
    bash tests/fixtures/build_fixtures.sh
[ -f tests/fixtures/darwin_ios_exit42 ] ||
    bash tests/fixtures/build_fixtures.sh

FIXTURE_DIR="$(mktemp -d)"
trap 'rm -rf "$FIXTURE_DIR"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -D_GNU_SOURCE -Isrc \
    src/loader.c src/adapters/startup_cache.c tests/loader_guest_platform_test.c -o "$FIXTURE_DIR/platform_test"

mkdir "$FIXTURE_DIR/guest"
cp tests/fixtures/darwin_initializer_working_directory "$FIXTURE_DIR/guest/program"

"$FIXTURE_DIR/platform_test" \
    tests/fixtures/darwin_exit42 tests/fixtures/darwin_ios_exit42
(
    cd "$FIXTURE_DIR"
    "$BUILD_DIR/machgate" guest/program
)
[ -f "$FIXTURE_DIR/guest/initializer-output.txt" ]
[ ! -e "$FIXTURE_DIR/initializer-output.txt" ]
status=0
"$BUILD_DIR/machgate" tests/fixtures/darwin_ios_exit42 || status=$?
[ "$status" -eq 42 ]
