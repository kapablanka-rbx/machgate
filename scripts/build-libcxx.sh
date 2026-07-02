#!/bin/bash
# Build Apple-ABI-compatible libc++ for aarch64 Linux.
#
# This produces a libc++.so.1 with Apple's std::string layout
# (_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT) so Mach-O binaries compiled
# against Apple's libc++ get the correct memory layout.
#
# Uses bmdhacks/llvm-project fork (darwin-abi-compat branch) which
# includes Darwin ABI compatibility patches:
#   - pthread_mutex_t padded to 64 bytes (matching macOS)
#   - Darwin mutex/condvar signature detection and reinitialization
#   - fpos<mbstate_t> padded to 136 bytes (macOS mbstate_t = 128 bytes)
#
# Compiler auto-detection: uses clang if available (>= 15), falls back to GCC.
#
# Usage: ./scripts/build-libcxx.sh
# Output: build-libcxx/lib/libc++.so.1

set -e

# Always work from the project root
cd "$(dirname "$0")/.."

BUILD_DIR=build-libcxx
SRC_DIR=extern/llvm-project
OVERLAY_SRC=scripts/libcxx_allocator_overlay.c
SHIM_LIB="${MACHGATE_SHIM_LIB:-}"

if [ ! -d "$SRC_DIR/libcxx" ]; then
    echo "Error: $SRC_DIR/libcxx not found. Run: git submodule update --init extern/llvm-project"
    exit 1
fi

# Detect compiler: prefer clang >= 15, fall back to GCC
if command -v clang++ >/dev/null 2>&1; then
    CLANG_VER=$(clang++ --version | head -1 | grep -oP '\d+' | head -1)
    if [ "$CLANG_VER" -ge 15 ] 2>/dev/null; then
        CC_TO_USE=clang
        CXX_TO_USE=clang++
        echo "Using clang $CLANG_VER"
    else
        CC_TO_USE=gcc
        CXX_TO_USE=g++
        echo "Using GCC (clang too old: $CLANG_VER)"
    fi
else
    CC_TO_USE=gcc
    CXX_TO_USE=g++
    echo "Using GCC (clang not found)"
fi

# Verify this checkout has the Darwin ABI compatibility patches.
if ! grep -q "_LIBCPP_ABI_DARWIN_MBSTATE_COMPAT" "$SRC_DIR/libcxx/include/__ios/fpos.h" ||
   ! grep -q "_LIBCPP_DARWIN_MUTEX_SIZE" "$SRC_DIR/libcxx/include/__threading_support"; then
    echo "Error: $SRC_DIR does not have the Darwin ABI compatibility libc++ patches"
    echo "Run: git submodule update --init extern/llvm-project"
    exit 1
fi

echo "Configuring Apple-ABI libc++ build..."

if [ -f "$BUILD_DIR/CMakeCache.txt" ] &&
   ! grep -q "CMAKE_HOME_DIRECTORY:INTERNAL=$PWD/$SRC_DIR/runtimes" "$BUILD_DIR/CMakeCache.txt"; then
    echo "Removing stale $BUILD_DIR CMake cache from a different checkout path"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
OVERLAY_OBJ="$PWD/$BUILD_DIR/machgate_libcxx_allocator_overlay.o"
"$CC_TO_USE" -fPIC -O2 -c "$OVERLAY_SRC" -o "$OVERLAY_OBJ"

if [ -z "$SHIM_LIB" ]; then
    for candidate in \
        "$PWD/build/libsystem_shim.so" \
        "$PWD/build-arm64-release/libsystem_shim.so" \
        "$PWD/build-arm64/libsystem_shim.so"; do
        if [ -f "$candidate" ]; then
            SHIM_LIB="$candidate"
            break
        fi
    done
fi
if [ ! -f "$SHIM_LIB" ]; then
    echo "Error: libsystem_shim.so not found. Build MachGate first or set MACHGATE_SHIM_LIB=/path/to/libsystem_shim.so" >&2
    exit 1
fi

WRAP_SYMBOLS=(
    malloc free calloc realloc posix_memalign memalign aligned_alloc valloc
    _Znwm _Znam _ZnwmRKSt9nothrow_t _ZnamRKSt9nothrow_t
    _ZnwmSt11align_val_t _ZnamSt11align_val_t
    _ZnwmSt11align_val_tRKSt9nothrow_t _ZnamSt11align_val_tRKSt9nothrow_t
    _ZdlPv _ZdaPv _ZdlPvm _ZdaPvm
    _ZdlPvRKSt9nothrow_t _ZdaPvRKSt9nothrow_t
    _ZdlPvSt11align_val_t _ZdaPvSt11align_val_t
    _ZdlPvmSt11align_val_t _ZdaPvmSt11align_val_t
    _ZdlPvSt11align_val_tRKSt9nothrow_t _ZdaPvSt11align_val_tRKSt9nothrow_t
)
WRAP_LINK_FLAGS="$OVERLAY_OBJ -L$(dirname "$SHIM_LIB") -lsystem_shim"
for symbol in "${WRAP_SYMBOLS[@]}"; do
    WRAP_LINK_FLAGS+=" -Wl,--wrap=${symbol}"
done

cmake -G Ninja -S "$SRC_DIR/runtimes" -B "$BUILD_DIR" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
    -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="$CC_TO_USE" \
    -DCMAKE_CXX_COMPILER="$CXX_TO_USE" \
    -DLIBCXX_ABI_VERSION=1 \
    -DLIBCXX_ABI_DEFINES="_LIBCPP_ABI_ALTERNATE_STRING_LAYOUT;_LIBCPP_ABI_DARWIN_MBSTATE_COMPAT" \
    -DLIBCXX_ENABLE_SHARED=ON \
    -DLIBCXX_ENABLE_STATIC=OFF \
    -DLIBCXX_INCLUDE_TESTS=OFF \
    -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
    -DLIBCXXABI_ENABLE_SHARED=ON \
    -DLIBCXXABI_ENABLE_STATIC=OFF \
    -DLIBCXXABI_INCLUDE_TESTS=OFF \
    -DCMAKE_SHARED_LINKER_FLAGS="$WRAP_LINK_FLAGS"

echo ""
echo "Building Apple-ABI libc++..."
cmake --build "$BUILD_DIR" --target cxx cxxabi --parallel

echo ""
echo "Output:"
echo "  $BUILD_DIR/lib/libc++.so.1"
echo "  $BUILD_DIR/lib/libc++abi.so.1"
