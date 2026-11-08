#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT
CLANG="$(command -v clang || command -v clang-18)"
LINKER="$(command -v ld64.lld || command -v ld64.lld-18)"
mkdir -p "$WORK_DIR/sub"

cat > "$WORK_DIR/leaf.s" <<'ASM'
.data
.p2align 3
leaf_storage: .quad 0
leaf_pointer: .quad leaf_storage
.section __DATA,__mod_init_func,mod_init_funcs
.p2align 3
.quad leaf_initialize
.text
leaf_initialize:
adrp x9, leaf_storage@PAGE
add x9, x9, leaf_storage@PAGEOFF
ldr x10, [x9]
add x10, x10, #33
str x10, [x9]
ret
.globl _leaf_value
_leaf_value:
mov x16, #20
svc #0x80
b.cs 1f
cbz x0, 1f
adrp x9, leaf_pointer@PAGE
ldr x9, [x9, leaf_pointer@PAGEOFF]
ldr x0, [x9]
ret
1:
mov x0, #0
ret
ASM
cat > "$WORK_DIR/outer.s" <<'ASM'
.data
.p2align 3
outer_storage: .quad 0
outer_pointer: .quad outer_storage
.section __DATA,__mod_init_func,mod_init_funcs
.p2align 3
.quad outer_initialize
.text
outer_initialize:
stp x29, x30, [sp, #-16]!
mov x29, sp
bl _leaf_value
add x0, x0, #9
adrp x9, outer_storage@PAGE
str x0, [x9, outer_storage@PAGEOFF]
ldp x29, x30, [sp], #16
ret
.globl _outer_value
_outer_value:
adrp x9, outer_pointer@PAGE
ldr x9, [x9, outer_pointer@PAGEOFF]
ldr x0, [x9]
ret
ASM
cat > "$WORK_DIR/main.s" <<'ASM'
.text
.globl _main
_main:
bl _outer_value
cmp x0, #42
mov x0, #0
mov x9, #77
csel x0, x0, x9, eq
mov x16, #1
svc #0x80
ASM
for SOURCE in leaf outer main; do
    "$CLANG" -target arm64-apple-macos13 -c "$WORK_DIR/$SOURCE.s" -o "$WORK_DIR/$SOURCE.o"
done

for FIXUPS in -no_fixup_chains -fixup_chains; do
    "$LINKER" -arch arm64 -platform_version macos 13.0 13.0 "$FIXUPS" -dylib \
        -install_name @loader_path/sub/libLeaf.dylib -o "$WORK_DIR/sub/libLeaf.dylib" "$WORK_DIR/leaf.o"
    "$LINKER" -arch arm64 -platform_version macos 13.0 13.0 "$FIXUPS" -dylib \
        -undefined dynamic_lookup -install_name @executable_path/libOuter.dylib \
        -o "$WORK_DIR/libOuter.dylib" "$WORK_DIR/outer.o" "$WORK_DIR/sub/libLeaf.dylib"
    "$LINKER" -arch arm64 -platform_version macos 13.0 13.0 "$FIXUPS" \
        -undefined dynamic_lookup -e _main -o "$WORK_DIR/guest" \
        "$WORK_DIR/main.o" "$WORK_DIR/libOuter.dylib"
    for CACHE in 0 1 1; do
        MACHGATE_DYLIB_MAP=/dev/null MACHGATE_UNRESOLVED_IMPORTS=strict \
        MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
        MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
        timeout -k 2 10 "$BUILD_DIR/machgate" "$WORK_DIR/guest"
    done
    if [ "$FIXUPS" = -no_fixup_chains ]; then
        cp "$WORK_DIR/sub/libLeaf.dylib" "$WORK_DIR/valid-leaf.dylib"
        python3 - "$WORK_DIR/sub/libLeaf.dylib" <<'PY'
import pathlib
import struct
import sys
path = pathlib.Path(sys.argv[1])
data = bytearray(path.read_bytes())
cursor = 32
for _ in range(struct.unpack_from('<I', data, 16)[0]):
    command, size = struct.unpack_from('<II', data, cursor)
    if command == 0x80000022:
        offset, rebase_size = struct.unpack_from('<II', data, cursor + 8)
        position = offset
        while position < offset + rebase_size:
            opcode = data[position] & 0xf0
            if opcode == 0:
                struct.pack_into('<I', data, cursor + 12, position - offset)
                break
            position += 1
            operands = 2 if opcode == 0x80 else int(opcode in (0x20, 0x30, 0x60, 0x70))
            for _ in range(operands):
                while data[position] & 0x80:
                    position += 1
                position += 1
        else:
            raise AssertionError('fixture has no terminating DONE opcode')
        break
    cursor += size
else:
    raise AssertionError('fixture has no classic rebase stream')
path.write_bytes(data)
PY
        for CACHE in 0 1 1; do
            MACHGATE_DYLIB_MAP=/dev/null MACHGATE_UNRESOLVED_IMPORTS=strict \
            MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" \
            MACHGATE_STARTUP_CACHE="$CACHE" MACHGATE_STARTUP_CACHE_DIR="$WORK_DIR/cache" \
            timeout -k 2 10 "$BUILD_DIR/machgate" "$WORK_DIR/guest"
        done
        cp "$WORK_DIR/valid-leaf.dylib" "$WORK_DIR/sub/libLeaf.dylib"
        python3 - "$WORK_DIR/sub/libLeaf.dylib" <<'PY'
import pathlib
import struct
import sys
path = pathlib.Path(sys.argv[1])
data = bytearray(path.read_bytes())
cursor = 32
for _ in range(struct.unpack_from('<I', data, 16)[0]):
    command, size = struct.unpack_from('<II', data, cursor)
    if command == 0x80000022:
        offset, rebase_size = struct.unpack_from('<II', data, cursor + 8)
        assert rebase_size and data[offset] == 0x11
        data[offset] = 0x40
        break
    cursor += size
else:
    raise AssertionError('fixture has no classic rebase stream')
path.write_bytes(data)
PY
        status=0
        MACHGATE_DYLIB_MAP=/dev/null MACHGATE_UNRESOLVED_IMPORTS=strict \
        MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" MACHGATE_STARTUP_CACHE=0 \
        timeout -k 2 10 "$BUILD_DIR/machgate" "$WORK_DIR/guest" > "$WORK_DIR/invalid.log" 2>&1 || status=$?
        if [ "$status" -ne 1 ] || ! rg -q 'invalid or unsupported pointer rebase stream' "$WORK_DIR/invalid.log"; then
            printf 'Invalid rebase without SET_TYPE was accepted (status %s)\n' "$status" >&2
            cat "$WORK_DIR/invalid.log" >&2
            exit 1
        fi
        cp "$WORK_DIR/valid-leaf.dylib" "$WORK_DIR/sub/libLeaf.dylib"
        for INVALID in short-segment overlapping-segments truncated-rebase; do
            python3 - "$WORK_DIR/valid-leaf.dylib" "$WORK_DIR/sub/libLeaf.dylib" "$INVALID" <<'PY'
import pathlib
import struct
import sys
source, target, variant = sys.argv[1:]
data = bytearray(pathlib.Path(source).read_bytes())
if variant == 'short-segment':
    data = struct.pack('<8I2I', 0xfeedfacf, 0x100000c, 0, 6, 1, 8, 0, 0, 0x19, 8)
elif variant == 'truncated-rebase':
    cursor = 32
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        command, size = struct.unpack_from('<II', data, cursor)
        if command == 0x80000022:
            offset = struct.unpack_from('<I', data, cursor + 8)[0]
            data[offset:offset + 3] = bytes((0x11, 0x22, 0x80))
            struct.pack_into('<I', data, cursor + 12, 3)
            break
        cursor += size
    else:
        raise AssertionError('fixture has no classic rebase stream')
else:
    cursor = 32
    segments = []
    for _ in range(struct.unpack_from('<I', data, 16)[0]):
        command, size = struct.unpack_from('<II', data, cursor)
        if command == 0x19:
            segments.append(cursor)
        cursor += size
    assert len(segments) >= 2
    address = struct.unpack_from('<Q', data, segments[0] + 24)[0]
    struct.pack_into('<Q', data, segments[1] + 24, address)
pathlib.Path(target).write_bytes(data)
PY
            status=0
            MACHGATE_DYLIB_MAP=/dev/null MACHGATE_UNRESOLVED_IMPORTS=strict \
            MACHGATE_SYSTEM_SHIM_LIB="$BUILD_DIR/libsystem_shim.so" MACHGATE_STARTUP_CACHE=0 \
            timeout -k 2 10 "$BUILD_DIR/machgate" "$WORK_DIR/guest" > "$WORK_DIR/invalid.log" 2>&1 || status=$?
            if [ "$status" -ne 1 ] || ! rg -q 'invalid Mach-O segment command|Mach-O dylib segments overlap|invalid or unsupported pointer rebase stream' "$WORK_DIR/invalid.log"; then
                printf 'Invalid library %s was not rejected (status %s)\n' "$INVALID" "$status" >&2
                cat "$WORK_DIR/invalid.log" >&2
                exit 1
            fi
        done
        cp "$WORK_DIR/valid-leaf.dylib" "$WORK_DIR/sub/libLeaf.dylib"
    fi
done
printf '%s\n' 'Guest dylibs: nested paths, rebasing, binding, initializer order, syscalls and cached execution passed'
