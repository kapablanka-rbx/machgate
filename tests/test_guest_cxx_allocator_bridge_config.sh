#!/bin/bash
set -euo pipefail

ROOT="${MACHGATE_ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
MACHGATE_C="$ROOT/src/machgate.c"
OVERLAY="$ROOT/scripts/libcxx_allocator_overlay.c"
RESOLVER="$ROOT/src/resolver.c"

bridge_calls="$(grep -Fc "configure_guest_cxx_allocator_hooks(&machgate_load_results)" "$MACHGATE_C" || true)"

if [ "$bridge_calls" -lt 2 ]; then
    echo "guest C++ allocator bridge must be configured for mapped and standalone runs" >&2
    exit 1
fi

if ! grep -Fq "machgate_shim_guest_operator_" "$OVERLAY"; then
    echo "native libc++ allocator overlay must route C++ operators through guest bridge" >&2
    exit 1
fi

if ! grep -Fq "resolver_lookup_external_definition" "$MACHGATE_C"; then
    echo "guest C++ allocator bridge must bind only public main Mach-O definitions" >&2
    exit 1
fi

if ! grep -Fq '"_%s", name' "$MACHGATE_C"; then
    echo "guest C++ allocator bridge must try Mach-O double-underscore operator names" >&2
    exit 1
fi

if grep -Eq 'chained-cxx-operator|dyld-info-cxx-operator' "$RESOLVER"; then
    echo "C++ allocator operator hooks must not preempt normal Mach-O import resolution" >&2
    exit 1
fi

if awk '
    /static uintptr_t resolve_cxx_operator_fallback/ { in_helper = 1 }
    in_helper && /lookup_macho_symbol/ { found = 1 }
    in_helper && /^}/ { in_helper = 0 }
    END { exit found ? 0 : 1 }
' "$RESOLVER"; then
    echo "C++ allocator fallback must not search the main executable" >&2
    exit 1
fi

if ! grep -Eq 'chained-map-cxx-fallback|dyld-info-map-cxx-fallback' "$RESOLVER"; then
    echo "C++ allocator operator hooks must remain provider-resolution fallbacks" >&2
    exit 1
fi

if ! grep -Eq 'chained-map-cxx-main-override|dyld-info-map-cxx-main-override|deferred-cxx-main-override' "$RESOLVER"; then
    echo "C++ allocator main-executable override guard is missing" >&2
    exit 1
fi

if ! awk '
    /static uintptr_t resolve_main_cxx_operator_override/ { in_helper = 1 }
    in_helper && /macho_symbol_is_stub_definition/ { found = 1 }
    in_helper && /^}/ { in_helper = 0 }
    END { exit found ? 0 : 1 }
' "$RESOLVER"; then
    echo "C++ allocator main executable override must reject symbol stubs" >&2
    exit 1
fi

deferred_cxx_line="$(grep -Fn -m1 'addr = (void*)resolve_cxx_operator_fallback' "$RESOLVER" | cut -d: -f1)"
deferred_rtld_line="$(grep -Fn -m1 'addr = dlsym(RTLD_DEFAULT, lookup);' "$RESOLVER" | cut -d: -f1)"
deferred_main_line="$(grep -Fn -m1 'deferred-cxx-main-override' "$RESOLVER" | cut -d: -f1)"
chained_cxx_line="$(grep -Fn -m1 'chained-map-cxx-fallback' "$RESOLVER" | cut -d: -f1)"
chained_main_line="$(grep -Fn -m1 'chained-map-cxx-main-override' "$RESOLVER" | cut -d: -f1)"
chained_rtld_line="$(grep -Fn -m1 'compiler runtime (libgcc_s' "$RESOLVER" | cut -d: -f1)"
dyld_cxx_line="$(grep -Fn -m1 'dyld-info-map-cxx-fallback' "$RESOLVER" | cut -d: -f1)"
dyld_main_line="$(grep -Fn -m1 'dyld-info-map-cxx-main-override' "$RESOLVER" | cut -d: -f1)"
dyld_rtld_line="$(grep -Fn -m1 'Fallback: try RTLD_DEFAULT for compiler runtime symbols' "$RESOLVER" | cut -d: -f1)"

if [ -z "$deferred_main_line" ] || [ -z "$deferred_cxx_line" ] || [ -z "$deferred_rtld_line" ] ||
   [ -z "$chained_main_line" ] || [ -z "$chained_cxx_line" ] || [ -z "$chained_rtld_line" ] ||
   [ -z "$dyld_main_line" ] || [ -z "$dyld_cxx_line" ] || [ -z "$dyld_rtld_line" ]; then
    echo "C++ allocator fallback ordering guard could not find resolver anchors" >&2
    exit 1
fi

if [ "$deferred_main_line" -gt "$deferred_cxx_line" ] ||
   [ "$chained_main_line" -gt "$chained_cxx_line" ] ||
   [ "$dyld_main_line" -gt "$dyld_cxx_line" ]; then
    echo "C++ allocator main executable override must run before allocator fallback" >&2
    exit 1
fi

if [ "$deferred_cxx_line" -gt "$deferred_rtld_line" ] ||
   [ "$chained_cxx_line" -gt "$chained_rtld_line" ] ||
   [ "$dyld_cxx_line" -gt "$dyld_rtld_line" ]; then
    echo "C++ allocator fallback must run before generic RTLD_DEFAULT fallback" >&2
    exit 1
fi
