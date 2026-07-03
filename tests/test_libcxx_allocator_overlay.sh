#!/bin/bash
set -euo pipefail

ROOT="${MACHGATE_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
LIBCXX_DIR="${LIBCXX_DIR:-$ROOT/build-libcxx/lib}"

check_library()
{
    local library="$1"
    local path="$LIBCXX_DIR/$library"

    if [ ! -f "$path" ]; then
        if [ "${MACHGATE_REQUIRE_LIBCXX_OVERLAY:-0}" = "1" ]; then
            echo "missing $path" >&2
            exit 1
        fi
        echo "SKIP: $path not built"
        exit 0
    fi

    local bad_refs
    bad_refs="$(
        nm -D --undefined-only "$path" |
            awk '
                /@GLIBC/ && $NF ~ /(^|@)(malloc|free|calloc|realloc|posix_memalign|memalign|aligned_alloc|valloc)(@|$)/ { print }
                /@GLIBC/ && $NF ~ /(^|@)(_Znwm|_Znam|_ZdlPv|_ZdaPv|_ZdlPvm|_ZdaPvm)(@|St|RK|$)/ { print }
            '
    )"
    if [ -n "$bad_refs" ]; then
        echo "$path still has direct allocator imports:" >&2
        echo "$bad_refs" >&2
        exit 1
    fi

    for symbol in machgate_shim_malloc machgate_shim_free machgate_shim_calloc machgate_shim_realloc; do
        if ! nm -D --undefined-only "$path" | awk -v symbol="$symbol" '
            {
                name = $NF
                sub(/@.*/, "", name)
                if (name == symbol)
                    found = 1
            }
            END { exit !found }
        '; then
            echo "$path is not linked through $symbol" >&2
            exit 1
        fi
    done

    for symbol in \
        machgate_shim_guest_operator_new \
        machgate_shim_guest_operator_new_nothrow \
        machgate_shim_guest_operator_delete \
        machgate_shim_guest_operator_delete_nothrow; do
        if ! nm -D --undefined-only "$path" | awk -v symbol="$symbol" '
            {
                name = $NF
                sub(/@.*/, "", name)
                if (name == symbol)
                    found = 1
            }
            END { exit !found }
        '; then
            echo "$path is not linked through $symbol" >&2
            exit 1
        fi
    done
}

check_overlay_object()
{
    local object="$ROOT/build-libcxx/machgate_libcxx_allocator_overlay.o"

    if [ ! -f "$object" ]; then
        if [ "${MACHGATE_REQUIRE_LIBCXX_OVERLAY:-0}" = "1" ]; then
            echo "missing $object" >&2
            exit 1
        fi
        return
    fi

    for symbol in __wrap__Znwm __wrap__ZdlPv; do
        if ! nm "$object" | awk -v symbol="$symbol" '
            {
                name = $NF
                sub(/@.*/, "", name)
                if (name == symbol)
                    found = 1
            }
            END { exit !found }
        '; then
            echo "$object does not define $symbol" >&2
            exit 1
        fi
    done

    for symbol in \
        machgate_shim_guest_operator_new \
        machgate_shim_guest_operator_new_nothrow \
        machgate_shim_guest_operator_delete \
        machgate_shim_guest_operator_delete_nothrow; do
        if ! nm "$object" | awk -v symbol="$symbol" '
            {
                name = $NF
                sub(/@.*/, "", name)
                if (name == symbol)
                    found = 1
            }
            END { exit !found }
        '; then
            echo "$object does not reference $symbol" >&2
            exit 1
        fi
    done
}

check_overlay_object
check_library libc++.so.1.0
check_library libc++abi.so.1.0
