#!/usr/bin/env bash
set -euo pipefail

MACHGATE_TRACE_SIGNALS=1 \
MACHGATE_TRACE_LCMAIN=1 \
MACHGATE_TRACE_ALLOC_MISMATCH=0 \
MACHGATE_TIMEOUT="${MACHGATE_TIMEOUT:-600}" \
MACHGATE_TARBALL="${MACHGATE_TARBALL:-$(dirname "$0")/machgate-linux-arm64.tar.gz}" \
"$(dirname "$0")/run-macho-docker.sh" "$@"
