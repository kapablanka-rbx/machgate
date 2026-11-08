#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"

cat > "$WORK_DIR/library.c" <<'C'
static _Thread_local unsigned long initialized = 19;
static _Thread_local unsigned char zero_fill[4096] __attribute__((aligned(4096)));
static int constructor_valid;

__attribute__((constructor)) static void initialize(void)
{
	constructor_valid = initialized == 19 && zero_fill[4095] == 0;
	initialized = 23;
	zero_fill[4095] = 47;
}

int library_valid(void)
{
	return constructor_valid && initialized == 23 && zero_fill[4095] == 47 &&
	       ((unsigned long)zero_fill & 4095) == 0;
}
C
cat > "$WORK_DIR/main.c" <<'C'
static _Thread_local unsigned long initialized = 73;
static _Thread_local unsigned char zero_fill[8192] __attribute__((aligned(8192)));
extern int library_valid(void);

int main(void)
{
	int valid = library_valid() && initialized == 73 && zero_fill[8191] == 0 &&
	            ((unsigned long)zero_fill & 8191) == 0;
	initialized = 83;
	zero_fill[8191] = 59;
	valid = valid && library_valid() && initialized == 83 && zero_fill[8191] == 59;
	register unsigned long status __asm__("x0") = valid ? 0 : 61;
	register unsigned long syscall_number __asm__("x16") = 1;
	__asm__ volatile("svc #0x80" : : "r"(status), "r"(syscall_number) : "memory");
	__builtin_unreachable();
}
C
for SOURCE in main library; do
    "$CLANG" -target arm64-apple-macos13 -O0 -fno-builtin -fno-stack-protector \
        -nostdlib -c "$WORK_DIR/$SOURCE.c" -o "$WORK_DIR/$SOURCE.o"
done
"$CLANG" -target arm64-apple-macos13 -c tests/fixtures/tlv_context_provider_stub.s \
    -o "$WORK_DIR/system.o"
"$LINKER" -arch arm64 -platform_version macos 13.0 13.0 -dylib \
    -install_name /usr/lib/libSystem.B.dylib -o "$WORK_DIR/system.dylib" "$WORK_DIR/system.o"
printf 'libSystem.B = %s/libsystem_shim.so\n' "$BUILD_DIR" > "$WORK_DIR/map"

for FIXUPS in -no_fixup_chains -fixup_chains; do
    "$LINKER" -arch arm64 -platform_version macos 13.0 13.0 "$FIXUPS" -dylib \
        -install_name @executable_path/libStorage.dylib -o "$WORK_DIR/libStorage.dylib" \
        "$WORK_DIR/library.o" "$WORK_DIR/system.dylib"
    "$LINKER" -arch arm64 -platform_version macos 13.0 13.0 "$FIXUPS" -e _main \
        -o "$WORK_DIR/guest" "$WORK_DIR/main.o" "$WORK_DIR/libStorage.dylib" "$WORK_DIR/system.dylib"
    for CACHE in 0 1 1; do
        MACHGATE_DYLIB_MAP="$WORK_DIR/map" MACHGATE_UNRESOLVED_IMPORTS=strict \
        MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
        timeout -k 2 10 "$BUILD_DIR/machgate" "$WORK_DIR/guest"
    done
done
printf '%s\n' 'Guest TLS: separate images, initializer access, alignment and cached execution passed'
