#!/usr/bin/env bash
set -euo pipefail

usage()
{
    cat >&2 <<'EOF'
Usage: bin/run-macho-docker.sh /path/to/macos-arm64-binary [args...]

Runs an ARM64 macOS Mach-O CLI binary through the published MachGate release
inside an ARM64 Ubuntu Docker container. Works on ARM64 hosts and on Intel
hosts with Docker QEMU/binfmt support.

Optional environment:
  MACHGATE_VERSION    Release version to download, default: latest
                      Use a number like 0.3.0 to pin a release.
  MACHGATE_LOCAL_DIR  Local Linux ARM64 MachGate directory to mount instead
                      of downloading a release. Supports either release layout
                      with bin/machgate and lib/libsystem_shim.so, or build
                      layout with machgate and libsystem_shim.so.
  MACHGATE_TARBALL    Local MachGate linux-arm64 release tarball to mount and
                      unpack instead of downloading a release.
  MACHGATE_IMAGE      Docker image to use, default: ubuntu:24.04
  MACHGATE_LIBCXX     Local Apple-ABI libc++.so.1 to mount and map as
                      libc++.1.dylib for C++ Mach-O binaries.
  MACHGATE_VERBOSE    Set to 1 to run MachGate with -v.
  MACHGATE_TIMEOUT    Kill MachGate after this many seconds, default: disabled.
  MACHGATE_TRACE_*    Debug trace flags are passed through to the container.
  QEMU_STRACE         Pass through to qemu-user when Docker uses binfmt.
EOF
}

if [ "$#" -lt 1 ]; then
    usage
    exit 2
fi

guest_binary="$1"
shift

if [ ! -f "$guest_binary" ]; then
    echo "Mach-O binary not found: $guest_binary" >&2
    exit 1
fi

guest_dir="$(cd "$(dirname "$guest_binary")" && pwd)"
guest_name="$(basename "$guest_binary")"
version="${MACHGATE_VERSION:-latest}"
image="${MACHGATE_IMAGE:-ubuntu:24.04}"
local_dir="${MACHGATE_LOCAL_DIR:-}"
tarball="${MACHGATE_TARBALL:-}"
libcxx="${MACHGATE_LIBCXX:-}"
verbose="${MACHGATE_VERBOSE:-}"
guest_timeout="${MACHGATE_TIMEOUT:-}"

if [ -n "$local_dir" ] && [ -n "$tarball" ]; then
    echo "Use only one of MACHGATE_LOCAL_DIR or MACHGATE_TARBALL" >&2
    exit 1
fi

echo "machgate-docker: checking Docker linux/arm64 support with ${image}" >&2

docker_args=(
    docker run --rm -i --platform linux/arm64
    -v "$guest_dir:/input:ro"
)

while IFS='=' read -r env_name _; do
    case "$env_name" in
        MACHGATE_TRACE_*)
            docker_args+=(-e "$env_name")
            ;;
    esac
done < <(env)

for env_name in MACHGATE_EXTERNAL_MAP_LIBCXX; do
    if [ -n "${!env_name:-}" ]; then
        docker_args+=(-e "$env_name=${!env_name}")
    fi
done

for env_name in QEMU_STRACE; do
    if [ -n "${!env_name:-}" ]; then
        docker_args+=(-e "$env_name=${!env_name}")
    fi
done

if [ -n "$local_dir" ]; then
    if [ ! -d "$local_dir" ]; then
        echo "MACHGATE_LOCAL_DIR is not a directory: $local_dir" >&2
        exit 1
    fi
    local_dir="$(cd "$local_dir" && pwd)"
    docker_args+=(-v "$local_dir:/opt/machgate-local:ro")
fi

tarball_name=""
if [ -n "$tarball" ]; then
    if [ ! -f "$tarball" ]; then
        echo "MACHGATE_TARBALL is not a file: $tarball" >&2
        exit 1
    fi
    tarball_dir="$(cd "$(dirname "$tarball")" && pwd)"
    tarball_name="$(basename "$tarball")"
    docker_args+=(-v "$tarball_dir:/machgate-dist:ro")
fi

libcxx_name=""
if [ -n "$libcxx" ]; then
    if [ ! -f "$libcxx" ]; then
        echo "MACHGATE_LIBCXX is not a file: $libcxx" >&2
        exit 1
    fi
    libcxx_dir="$(cd "$(dirname "$libcxx")" && pwd)"
    libcxx_name="$(basename "$libcxx")"
    docker_args+=(-v "$libcxx_dir:/machgate-libcxx:ro")
fi

if ! docker run --rm --platform linux/arm64 --entrypoint /bin/sh "$image" -c 'test "$(uname -m)" = aarch64' >/dev/null 2>&1; then
    cat >&2 <<EOF
Docker cannot execute linux/arm64 containers on this host.

On an Intel Linux host, install/register ARM64 binfmt support once:

  docker run --privileged --rm tonistiigi/binfmt --install arm64

Then rerun:

  $0 "$guest_binary" $*
EOF
    exit 1
fi

echo "machgate-docker: running ${guest_binary}" >&2

"${docker_args[@]}" \
    "$image" \
    bash -s -- "$version" "$guest_name" "$local_dir" "$tarball_name" "$libcxx_name" "$verbose" "$guest_timeout" "$@" <<'EOF'
set -euo pipefail

requested_version="$1"
guest_name="$2"
local_dir="$3"
tarball_name="$4"
libcxx_name="$5"
verbose="$6"
guest_timeout="$7"
shift 7

export DEBIAN_FRONTEND=noninteractive

if [ -n "$local_dir" ]; then
    echo "machgate-docker: using local MachGate directory /opt/machgate-local" >&2
    if [ -x /opt/machgate-local/bin/machgate ]; then
        machgate_root=/opt/machgate-local
        machgate_bin=/opt/machgate-local/bin/machgate
        shim_path=/opt/machgate-local/lib/libsystem_shim.so
    elif [ -x /opt/machgate-local/machgate ]; then
        machgate_root=/opt/machgate-local
        machgate_bin=/opt/machgate-local/machgate
        shim_path=/opt/machgate-local/libsystem_shim.so
    else
        echo "MACHGATE_LOCAL_DIR must contain bin/machgate or machgate" >&2
        exit 1
    fi
    if [ ! -f "$shim_path" ]; then
        echo "MACHGATE_LOCAL_DIR is missing libsystem_shim.so next to the selected layout" >&2
        exit 1
    fi
elif [ -n "$tarball_name" ]; then
    echo "machgate-docker: unpacking local MachGate tarball /machgate-dist/${tarball_name}" >&2
    tar -xzf "/machgate-dist/${tarball_name}" -C /opt
    machgate_root=/opt/machgate
    machgate_bin=/opt/machgate/bin/machgate
    shim_path=/opt/machgate/lib/libsystem_shim.so
else
    echo "machgate-docker: installing container dependencies" >&2
    apt-get update
    apt-get install -y --no-install-recommends ca-certificates curl tar zlib1g

    if [ "$requested_version" = "latest" ]; then
        echo "machgate-docker: resolving latest MachGate release" >&2
        latest_tag="$(curl -fsSL https://api.github.com/repos/kapabl/machgate/releases/latest |
            sed -n 's/.*"tag_name"[[:space:]]*:[[:space:]]*"v\([^"]*\)".*/\1/p' |
            head -1)"
        if [ -z "$latest_tag" ]; then
            echo "Could not resolve latest MachGate release" >&2
            exit 1
        fi
        release_url="https://github.com/kapabl/machgate/releases/download/v${latest_tag}/machgate-${latest_tag}-linux-arm64.tar.gz"
    else
        version="${requested_version#v}"
        release_url="https://github.com/kapabl/machgate/releases/download/v${version}/machgate-${version}-linux-arm64.tar.gz"
    fi

    echo "machgate-docker: downloading ${release_url}" >&2
    curl -fL --retry 3 --retry-delay 2 -o /tmp/machgate.tar.gz \
        "$release_url"
    echo "machgate-docker: unpacking MachGate release" >&2
    tar -xzf /tmp/machgate.tar.gz -C /opt
    machgate_root=/opt/machgate
    machgate_bin=/opt/machgate/bin/machgate
    shim_path=/opt/machgate/lib/libsystem_shim.so
fi

libcxx_path=""
if [ -n "$libcxx_name" ]; then
    libcxx_path="/machgate-libcxx/${libcxx_name}"
    if [ ! -f "$libcxx_path" ]; then
        echo "Mounted MACHGATE_LIBCXX is missing inside the container: $libcxx_path" >&2
        exit 1
    fi
elif [ -f "${machgate_root}/lib/libc++.so.1" ]; then
    libcxx_path="${machgate_root}/lib/libc++.so.1"
elif [ "${MACHGATE_EXTERNAL_MAP_LIBCXX:-0}" = "1" ]; then
    echo "machgate-docker: MACHGATE_EXTERNAL_MAP_LIBCXX=1 requested, but ${machgate_root}/lib/libc++.so.1 is not present" >&2
fi

cat > /tmp/machgate.conf <<'CONFIG_EOF'
[general]
dylib_map = /tmp/dylib_map.conf
CONFIG_EOF

cat > /tmp/dylib_map.conf <<'MAP_EOF'
libSystem.B = __SHIM_PATH__
CoreFoundation = __SHIM_PATH__
CoreServices = __SHIM_PATH__
Security = __SHIM_PATH__
IOKit = __SHIM_PATH__
libresolv = __SHIM_PATH__
libicucore = __SHIM_PATH__
libz.1 = libz.so
libiconv = libc.so.6
libobjc = STUB
Foundation = SKIP
SystemConfiguration = SKIP
AppKit = SKIP
MAP_EOF
sed -i "s#__SHIM_PATH__#${shim_path}#g" /tmp/dylib_map.conf
if [ -n "$libcxx_path" ]; then
    echo "libc++.1 = ${libcxx_path}" >> /tmp/dylib_map.conf
    echo "machgate-docker: mapped libc++.1.dylib to ${libcxx_path}" >&2
fi

if [ -n "$libcxx_path" ]; then
    export LD_LIBRARY_PATH="$(dirname "$libcxx_path"):${machgate_root}/lib:${machgate_root}"
else
    export LD_LIBRARY_PATH="${machgate_root}/lib:${machgate_root}"
fi
export MACHGATE_CONFIG=/tmp/machgate.conf
machgate_args=()
if [ "$verbose" = "1" ]; then
    machgate_args=(-v)
fi

echo "machgate-docker: exec ${machgate_bin} ${machgate_args[*]} /input/${guest_name} $*" >&2
set +e
if [ -n "$guest_timeout" ]; then
    timeout --preserve-status --kill-after=5s "${guest_timeout}s" \
        "$machgate_bin" "${machgate_args[@]}" "/input/${guest_name}" "$@"
else
    "$machgate_bin" "${machgate_args[@]}" "/input/${guest_name}" "$@"
fi
guest_status="$?"
set -e

if [ -n "$guest_timeout" ] && [ "$guest_status" -eq 143 ]; then
    echo "machgate-docker: guest timed out after ${guest_timeout}s" >&2
fi
echo "machgate-docker: guest exit ${guest_status}" >&2
exit "$guest_status"
EOF
