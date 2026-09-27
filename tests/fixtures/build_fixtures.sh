#!/bin/bash
# Build test Mach-O fixtures using the LLVM cross-compilation toolchain.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

LLVM_MC="${LLVM_MC:-$(command -v llvm-mc || command -v llvm-mc-18 || true)}"
LD64_LLD="${LD64_LLD:-$(command -v ld64.lld || command -v ld64.lld-18 || true)}"
LLVM_LIPO="${LLVM_LIPO:-$(command -v llvm-lipo || command -v llvm-lipo-18 || true)}"
CLANGXX="${CLANGXX:-$(command -v clang++ || command -v clang++-18 || true)}"

[ -n "$LLVM_MC" ] || { echo "llvm-mc not found" >&2; exit 1; }
[ -n "$LD64_LLD" ] || { echo "ld64.lld not found" >&2; exit 1; }
[ -n "$LLVM_LIPO" ] || { echo "llvm-lipo not found" >&2; exit 1; }
[ -n "$CLANGXX" ] || { echo "clang++ not found" >&2; exit 1; }

echo "Building test fixtures..."

download_cpp_public_source()
{
    local name="$1"
    local url="$2"
    local expected_sha="$3"
    local output="$4"

    if [ ! -f "$output" ]; then
        if command -v curl >/dev/null 2>&1; then
            curl -L --fail --retry 3 --connect-timeout 20 -o "$output" "$url"
        else
            wget -O "$output" "$url"
        fi
    fi

    local actual_sha
    actual_sha="$(sha256sum "$output" | awk '{print $1}')"
    if [ "$actual_sha" != "$expected_sha" ]; then
        echo "$name checksum mismatch" >&2
        echo "  expected: $expected_sha" >&2
        echo "  actual:   $actual_sha" >&2
        exit 1
    fi
}

build_cpp_public_test_registrars()
{
    if [ "${MACHGATE_BUILD_CPP_PUBLIC_REGISTRARS:-0}" != "1" ]; then
        echo "  Skipped cpp_public_test_registrars (set MACHGATE_BUILD_CPP_PUBLIC_REGISTRARS=1)"
        return
    fi

    local cache_dir="${MACHGATE_CPP_PUBLIC_SOURCE_CACHE:-$SCRIPT_DIR/../external/cache/public-cpp-static-init}"
    local work_dir="${MACHGATE_CPP_PUBLIC_SOURCE_WORK:-$SCRIPT_DIR/.cpp_public_test_registrars}"
    local limit="${MACHGATE_CPP_PUBLIC_REGISTRAR_LIMIT:-707}"

    mkdir -p "$cache_dir"
    rm -rf "$work_dir"
    mkdir -p "$work_dir/sources"

    download_cpp_public_source "CLI11 v2.6.2" \
        "https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.6.2.tar.gz" \
        "c6ea6b2e5608b3ea8617999bd5f47420c71b2ebdb8dc4767c1034d1da5785711" \
        "$cache_dir/CLI11-v2.6.2.tar.gz"
    download_cpp_public_source "Catch2 v3.9.1" \
        "https://github.com/catchorg/Catch2/archive/refs/tags/v3.9.1.tar.gz" \
        "a215c2a723bd7483efd236dc86066842a389cb4e344c61119c978acdf24d39be" \
        "$cache_dir/Catch2-v3.9.1.tar.gz"
    download_cpp_public_source "spdlog v1.17.0" \
        "https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz" \
        "d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744" \
        "$cache_dir/spdlog-v1.17.0.tar.gz"
    download_cpp_public_source "fmt 12.2.0" \
        "https://github.com/fmtlib/fmt/archive/refs/tags/12.2.0.tar.gz" \
        "8b852bb5aa6e7d8564f9e81394055395dd1d1936d38dfd3a17792a02bebd7af0" \
        "$cache_dir/fmt-12.2.0.tar.gz"
    download_cpp_public_source "Microsoft GSL v4.2.2" \
        "https://github.com/microsoft/GSL/archive/refs/tags/v4.2.2.tar.gz" \
        "59e2a0a0ea22e8bcf9db2dc4d4bd21212ac6595748295fc27a7e02cf75eac4b5" \
        "$cache_dir/GSL-v4.2.2.tar.gz"

    tar -xzf "$cache_dir/CLI11-v2.6.2.tar.gz" -C "$work_dir/sources"
    tar -xzf "$cache_dir/Catch2-v3.9.1.tar.gz" -C "$work_dir/sources"
    tar -xzf "$cache_dir/spdlog-v1.17.0.tar.gz" -C "$work_dir/sources"
    tar -xzf "$cache_dir/fmt-12.2.0.tar.gz" -C "$work_dir/sources"
    tar -xzf "$cache_dir/GSL-v4.2.2.tar.gz" -C "$work_dir/sources"

    python3 - "$work_dir/sources" "$work_dir/cpp_public_test_registrars.cpp" "$limit" <<'PY'
import json
import pathlib
import re
import sys

source_root = pathlib.Path(sys.argv[1])
output_path = pathlib.Path(sys.argv[2])
limit = int(sys.argv[3])
mask = (1 << 64) - 1
project_roots = [
    ("cli11", "CLI11-2.6.2"),
    ("catch2", "Catch2-3.9.1"),
    ("spdlog", "spdlog-1.17.0"),
    ("fmt", "fmt-12.2.0"),
    ("gsl", "GSL-4.2.2"),
]
patterns = [
    re.compile(r'\b(?:TEST_CASE|TEMPLATE_TEST_CASE|TEMPLATE_LIST_TEST_CASE|SCENARIO|METHOD_AS_TEST_CASE|TEST_CASE_METHOD)\s*\(\s*"([^"]+)"'),
    re.compile(r'\b(?:TEST|TEST_F|TEST_P)\s*\(\s*([^,\)]+)\s*,\s*([^\)]+)\)'),
]
suffixes = {".cpp", ".cc", ".cxx", ".hpp", ".h", ".hh"}

def c_string(value):
    return json.dumps(value, ensure_ascii=True)

def rotate(value):
    return ((value << 7) | (value >> 57)) & mask

def mix_text(value, text):
    result = value & mask
    for byte in text.encode("utf-8"):
        result ^= byte
        result = (result * 1099511628211) & mask
        result ^= result >> 32
        result &= mask
    return result

def case_fingerprint(source_index, line_number, path, name):
    result = (1469598103934665603 ^ ((source_index + 1) << 40) ^ line_number) & mask
    result = mix_text(result, path)
    result = mix_text(result, name)
    result ^= (len(path) << 24) ^ len(name)
    return result & mask

def add_registration(state, count, fingerprint, source_index, line_number, name):
    result = (state + (fingerprint ^ ((count + 1) << 32))) & mask
    result = rotate(result)
    result ^= ((source_index + 1) * 0x9e3779b97f4a7c15) & mask
    result ^= (line_number << 17) & mask
    result ^= len(name)
    return result & mask

def aggregate_entries(entries):
    result = 0x84222325cbf29ce4
    for entry_index, entry in enumerate(entries):
        result ^= entry["fingerprint"]
        result = (result * 0x100000001b3) & mask
        result ^= (entry["source_index"] << 48) ^ (entry["line"] << 16) ^ entry_index
        result &= mask
    return result

def collect_cases(source_index, project_name, project_dir):
    result = []
    root = source_root / project_dir
    for path in sorted(root.rglob("*")):
        if path.suffix.lower() not in suffixes:
            continue
        try:
            text = path.read_text(errors="ignore")
        except OSError:
            continue
        relative_path = str(path.relative_to(root))
        for line_number, line in enumerate(text.splitlines(), 1):
            for pattern in patterns:
                match = pattern.search(line)
                if not match:
                    continue
                name = "::".join(part.strip() for part in match.groups())
                result.append({
                    "source_index": source_index,
                    "project": project_name,
                    "path": relative_path,
                    "line": line_number,
                    "name": name,
                })
                break
    return result

per_project = [
    collect_cases(source_index, project_name, project_dir)
    for source_index, (project_name, project_dir) in enumerate(project_roots)
]
entries = []
while len(entries) < limit:
    made_progress = False
    for cases in per_project:
        if cases and len(entries) < limit:
            entries.append(cases.pop(0))
            made_progress = True
    if not made_progress:
        break

if len(entries) != limit:
    raise SystemExit(f"only found {len(entries)} public test registrations, need {limit}")

for entry in entries:
    entry["fingerprint"] = case_fingerprint(
        entry["source_index"], entry["line"], entry["path"], entry["name"]
    )
expected_sink = 0
for entry_index, entry in enumerate(entries):
    expected_sink = add_registration(
        expected_sink, entry_index, entry["fingerprint"],
        entry["source_index"], entry["line"], entry["name"]
    )
expected_aggregate = aggregate_entries(entries)

with output_path.open("w") as output:
    output.write('extern "C" int main(int argc, char **argv, char **envp);\n')
    output.write("struct RegistryEntry { unsigned long long fingerprint; unsigned int line; unsigned int source; unsigned int name_length; };\n")
    output.write(f"static RegistryEntry registry[{len(entries)}];\n")
    output.write("static volatile unsigned long long sink;\n")
    output.write("static unsigned int registry_count;\n")
    output.write("static unsigned long long rotate_left(unsigned long long value) { return (value << 7) | (value >> 57); }\n")
    output.write("static unsigned long long mix_text(unsigned long long value, const char *text, unsigned int length) {\n")
    output.write("    unsigned long long result = value;\n")
    output.write("    for (unsigned int index = 0; index < length; index++) {\n")
    output.write("        result ^= (unsigned char)text[index];\n")
    output.write("        result *= 1099511628211ULL;\n")
    output.write("        result ^= result >> 32;\n")
    output.write("    }\n")
    output.write("    return result;\n")
    output.write("}\n")
    output.write("static void register_case(unsigned int source, unsigned int line, const char *path, unsigned int path_length, const char *name, unsigned int name_length) {\n")
    output.write("    unsigned long long fingerprint = 1469598103934665603ULL ^ ((unsigned long long)(source + 1) << 40) ^ line;\n")
    output.write("    fingerprint = mix_text(fingerprint, path, path_length);\n")
    output.write("    fingerprint = mix_text(fingerprint, name, name_length);\n")
    output.write("    fingerprint ^= ((unsigned long long)path_length << 24) ^ name_length;\n")
    output.write(f"    if (registry_count < {len(entries)}U) {{\n")
    output.write("        registry[registry_count].fingerprint = fingerprint;\n")
    output.write("        registry[registry_count].line = line;\n")
    output.write("        registry[registry_count].source = source;\n")
    output.write("        registry[registry_count].name_length = name_length;\n")
    output.write("    }\n")
    output.write("    sink += fingerprint ^ ((unsigned long long)(registry_count + 1) << 32);\n")
    output.write("    sink = rotate_left(sink);\n")
    output.write("    sink ^= (unsigned long long)(source + 1) * 0x9e3779b97f4a7c15ULL;\n")
    output.write("    sink ^= (unsigned long long)line << 17;\n")
    output.write("    sink ^= name_length;\n")
    output.write("    registry_count++;\n")
    output.write("}\n")
    for entry_index, entry in enumerate(entries):
        path_literal = c_string(entry["path"])
        name_literal = c_string(entry["name"])
        output.write(f"__attribute__((constructor)) static void registrar_{entry_index:04d}(void) {{ register_case({entry['source_index']}U, {entry['line']}U, {path_literal}, {len(entry['path'].encode('utf-8'))}U, {name_literal}, {len(entry['name'].encode('utf-8'))}U); }}\n")
    output.write("static unsigned long long aggregate_registry(void) {\n")
    output.write("    unsigned long long result = 0x84222325cbf29ce4ULL;\n")
    output.write("    for (unsigned int index = 0; index < registry_count; index++) {\n")
    output.write("        result ^= registry[index].fingerprint;\n")
    output.write("        result *= 0x100000001b3ULL;\n")
    output.write("        result ^= ((unsigned long long)registry[index].source << 48) ^ ((unsigned long long)registry[index].line << 16) ^ index;\n")
    output.write("    }\n")
    output.write("    return result;\n")
    output.write("}\n")
    output.write('extern "C" int main(int argc, char **argv, char **envp) {\n')
    output.write("    (void)argc;\n")
    output.write("    (void)argv;\n")
    output.write("    (void)envp;\n")
    output.write(f"    unsigned int count_ok = registry_count == {len(entries)}U;\n")
    output.write(f"    unsigned int sink_ok = sink == {expected_sink}ULL;\n")
    output.write(f"    unsigned int aggregate_ok = aggregate_registry() == {expected_aggregate}ULL;\n")
    output.write("    register unsigned long x0 __asm__(\"x0\") = (count_ok && sink_ok && aggregate_ok) ? 0 : 45;\n")
    output.write("    register unsigned long x16 __asm__(\"x16\") = 1;\n")
    output.write('    __asm__ volatile("svc #0x80" : : "r"(x0), "r"(x16) : "memory");\n')
    output.write("    __builtin_unreachable();\n")
    output.write("}\n")
PY

    "$CLANGXX" -target arm64-apple-macos11 -fno-exceptions -fno-rtti -nostdlib \
        -c "$work_dir/cpp_public_test_registrars.cpp" -o "$work_dir/cpp_public_test_registrars.o"
    "$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
        -o cpp_public_test_registrars -e _main "$work_dir/cpp_public_test_registrars.o"
    rm -rf "$work_dir"
    echo "  Built cpp_public_test_registrars"
}

# --- exit42: static arm64 Mach-O, exit(42) ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o exit42.o exit42.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o exit42 -e _main exit42.o
rm -f exit42.o
echo "  Built exit42"

# --- exit0: static arm64 Mach-O, exit(0) ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o exit0.o exit0.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o exit0 -e _main exit0.o
rm -f exit0.o
echo "  Built exit0"

# --- hello_write: static arm64 Mach-O, write + exit ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o hello_write.o hello_write.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o hello_write -e _main hello_write.o
rm -f hello_write.o
echo "  Built hello_write"

# --- darwin_exit42: static arm64 Mach-O, Darwin SYS_exit(42) ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_exit42.o darwin_exit42.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_exit42 -e _main darwin_exit42.o
rm -f darwin_exit42.o
echo "  Built darwin_exit42"

# --- darwin_write_stdout: static arm64 Mach-O, Darwin write + exit ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_write_stdout.o darwin_write_stdout.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_write_stdout -e _main darwin_write_stdout.o
rm -f darwin_write_stdout.o
echo "  Built darwin_write_stdout"

# --- darwin_execve_exit42: static arm64 Mach-O, Darwin execve to darwin_exit42 ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_execve_exit42.o darwin_execve_exit42.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_execve_exit42 -e _main darwin_execve_exit42.o
rm -f darwin_execve_exit42.o
echo "  Built darwin_execve_exit42"

# --- darwin_panicwrap_self_reexec: static arm64 Mach-O, panicwrap-shaped fork/reexec ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_panicwrap_self_reexec.o darwin_panicwrap_self_reexec.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_panicwrap_self_reexec -e _main darwin_panicwrap_self_reexec.o
rm -f darwin_panicwrap_self_reexec.o
echo "  Built darwin_panicwrap_self_reexec"

# --- darwin_read_close: static arm64 Mach-O, Darwin read + close ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_read_close.o darwin_read_close.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_read_close -e _main darwin_read_close.o
rm -f darwin_read_close.o
echo "  Built darwin_read_close"

# --- darwin_open_lseek: static arm64 Mach-O, Darwin open + lseek ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_open_lseek.o darwin_open_lseek.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_open_lseek -e _main darwin_open_lseek.o
rm -f darwin_open_lseek.o
echo "  Built darwin_open_lseek"

# --- darwin_fstat: static arm64 Mach-O, Darwin fstat ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_fstat.o darwin_fstat.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_fstat -e _main darwin_fstat.o
rm -f darwin_fstat.o
echo "  Built darwin_fstat"

# --- darwin_mmap_protect: static arm64 Mach-O, Darwin mmap/mprotect/munmap ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_mmap_protect.o darwin_mmap_protect.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_mmap_protect -e _main darwin_mmap_protect.o
rm -f darwin_mmap_protect.o
echo "  Built darwin_mmap_protect"

# --- cpp_many_ctors: 707 C++ static constructors, Darwin exit(0) on success ---
{
    cat <<'EOF'
extern "C" int main(int argc, char **argv, char **envp);
static volatile unsigned long long sink;
#define CTOR(N) __attribute__((constructor)) static void ctor_##N(void) { sink += (unsigned long long)(N + 1); }
EOF
    for constructor_index in $(seq 0 706); do
        printf 'CTOR(%s)\n' "$constructor_index"
    done
    cat <<'EOF'
extern "C" int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    register unsigned long x0 __asm__("x0") = (sink == 250278ULL) ? 0 : 43;
    register unsigned long x16 __asm__("x16") = 1;
    __asm__ volatile("svc #0x80" : : "r"(x0), "r"(x16) : "memory");
    __builtin_unreachable();
}
EOF
} > cpp_many_ctors.cpp
"$CLANGXX" -target arm64-apple-macos11 -fno-exceptions -fno-rtti -nostdlib \
    -c cpp_many_ctors.cpp -o cpp_many_ctors.o
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o cpp_many_ctors -e _main cpp_many_ctors.o
rm -f cpp_many_ctors.o cpp_many_ctors.cpp
echo "  Built cpp_many_ctors"

build_cpp_public_test_registrars

# --- darwin_range_000_099: range dispatcher probe ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_range_000_099.o darwin_range_000_099.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_range_000_099 -e _main darwin_range_000_099.o
rm -f darwin_range_000_099.o
echo "  Built darwin_range_000_099"

# --- darwin_range_100_199: range dispatcher probe ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_range_100_199.o darwin_range_100_199.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_range_100_199 -e _main darwin_range_100_199.o
rm -f darwin_range_100_199.o
echo "  Built darwin_range_100_199"

# --- darwin_range_200_399: range dispatcher probe ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_range_200_399.o darwin_range_200_399.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_range_200_399 -e _main darwin_range_200_399.o
rm -f darwin_range_200_399.o
echo "  Built darwin_range_200_399"

# --- darwin_range_400_plus: range dispatcher probe ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_range_400_plus.o darwin_range_400_plus.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o darwin_range_400_plus -e _main darwin_range_400_plus.o
rm -f darwin_range_400_plus.o
echo "  Built darwin_range_400_plus"

# --- fat_binary: universal binary with x86_64 + arm64 ---
# Build x86_64 stub
"$LLVM_MC" -triple x86_64-apple-macos11 -filetype=obj -o fat_stub_x86.o fat_stub_x86.s
"$LD64_LLD" -arch x86_64 -platform_version macos 11.0.0 11.0.0 -o fat_stub_x86 -e _main fat_stub_x86.o
rm -f fat_stub_x86.o

# Create fat binary from x86_64 + arm64 exit42
"$LLVM_LIPO" -create -output fat_binary fat_stub_x86 exit42
rm -f fat_stub_x86
echo "  Built fat_binary"

# --- with_lc_main: arm64 Mach-O using LC_MAIN entry point ---
# ld64.lld produces LC_MAIN when -no_pie and -e _main are not used together.
# We use -lc to force dyld-style linking which generates LC_MAIN.
# If that doesn't work, we just use the same approach as above (LC_UNIXTHREAD).
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o with_lc_main.o with_lc_main.s
# Try to create with LC_MAIN by using -lSystem and no -e flag
# ld64.lld will use LC_MAIN when _main is the entry and we use dyld-style linking
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -o with_lc_main with_lc_main.o -e _main 2>/dev/null || \
    "$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
        -o with_lc_main -e _main with_lc_main.o
rm -f with_lc_main.o
echo "  Built with_lc_main"

# --- trampoline_target: Mach-O with a static function to trampoline ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o trampoline_target.o trampoline_target.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 -o trampoline_target -e _main trampoline_target.o
rm -f trampoline_target.o
echo "  Built trampoline_target"

# --- darwin_popen_exit42: popen() of a Mach-O guest re-execs via the loader ---
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o darwin_popen_exit42.o darwin_popen_exit42.s
"$LLVM_MC" -triple arm64-apple-macos11 -filetype=obj -o libsystem_stub_empty.o empty_dylib.s
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -dylib -o libSystem.dylib libsystem_stub_empty.o
"$LD64_LLD" -arch arm64 -platform_version macos 11.0.0 11.0.0 \
    -fixup_chains \
    -o darwin_popen_exit42 -e _main darwin_popen_exit42.o libSystem.dylib
rm -f darwin_popen_exit42.o libsystem_stub_empty.o
echo "  Built darwin_popen_exit42"

# --- libtest_native.so: native .so with replacement function ---
gcc -shared -fPIC -o libtest_native.so libtest_native.c
echo "  Built libtest_native.so"

echo "All fixtures built."
