#!/bin/bash
# Test: send() with Darwin MSG_NOSIGNAL (0x80000) does not raise SIGPIPE.
# Darwin sockets do not deliver SIGPIPE for MSG_NOSIGNAL sends; Linux needs
# the flag translated from 0x80000 to MSG_NOSIGNAL (0x4000) or the process
# dies when writing to a closed socket.
set -e
cd "$(dirname "$0")/.."
MACHGATE_ROOT="${MACHGATE_ROOT:-$(pwd)}"
BUILD_DIR="${BUILD_DIR:-$MACHGATE_ROOT/build}"

[ -f "$BUILD_DIR/libsystem_shim.so" ] || { echo "libsystem_shim.so not built"; exit 1; }

BUILD_DIR="$BUILD_DIR" LD_LIBRARY_PATH="$BUILD_DIR" python3 - <<'PYEOF'
import ctypes, os, socket, sys

lib = ctypes.CDLL(os.path.join(os.environ['BUILD_DIR'], 'libsystem_shim.so'))
lib.send.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int]
lib.send.restype = ctypes.c_ssize_t
lib.__error.restype = ctypes.POINTER(ctypes.c_int)

DARWIN_MSG_NOSIGNAL = 0x80000

left, right = socket.socketpair()
right.close()
left_fd = left.fileno()
buf = ctypes.create_string_buffer(b'x' * 16)

pid = os.fork()
if pid == 0:
    import signal as signal_module
    signal_module.signal(signal_module.SIGPIPE, signal_module.SIG_DFL)
    result = lib.send(left_fd, buf, 16, DARWIN_MSG_NOSIGNAL)
    errno_val = lib.__error().contents.value
    os.write(2, f'child send result={result} errno={errno_val}\n'.encode())
    if result == -1:
        os._exit(0 if errno_val == 32 else 1)
    os._exit(0)

_, status = os.waitpid(pid, 0)
exit_code = os.waitstatus_to_exitcode(status)
left.close()
if exit_code != 0:
    print(f'FAIL: child exit={exit_code} (expected EPIPE=32 or success, no SIGPIPE)')
    sys.exit(1)
print('PASS: send with Darwin MSG_NOSIGNAL returned EPIPE instead of SIGPIPE')
PYEOF
