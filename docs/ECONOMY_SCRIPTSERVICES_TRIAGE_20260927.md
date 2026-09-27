# App.Economy + ScriptServices daemon-churn triage — 2026-09-27

Investigation of the two heaviest-failing suites from the 2026-09-27 01:13
recording (`/tmp/machgate-results.json`):

- ScriptServices.UnitTest: 94 CRASHED + 71 TIMEOUT of 199
- App.Economy.UnitTest: 110-165 CRASHED + 34-45 TIMEOUT + 130 FAILED

## Results after current fixes

All previously-failing cases from both suites pass, both standalone and
through daemon churn (`--daemon` + RUN_GROUP reuse, mirroring the rotest
scheduler):

- ScriptServices: 165/165 previously-failing cases pass (single daemon,
  sequential groups)
- App.Economy: 235/235 previously-failing cases pass (single daemon)
- Economy churn re-run 3x clean once the environment was sanitized (below)

## Root causes

### 1. Darwin MSG_NOSIGNAL not translated (fixed — commit a58c476)

`HttpServerResponse::socketWrite` sends with Darwin `MSG_NOSIGNAL`
(0x80000). Linux's `MSG_NOSIGNAL` is 0x4000. The shim's send/sendto/
sendmsg interposers and the raw gateway handlers passed guest flags
through untranslated, so Linux ignored the bit and a write to a closed
mock-HTTP connection raised SIGPIPE, killing the whole daemon — the guest
harness prints `!! Romem: signal received !!` and every test left in the
group cascades to CRASHED.

Repro: `tests/test_darwin_send_nosigpipe.sh` (child dies with SIGPIPE
before the fix, gets EPIPE after).

### 2. Devspace proxy environment leaking into the guest (environment)

`HTTP_PROXY`/`HTTPS_PROXY` point at the devspace egress DPI proxy
(`devspaces-egress-dpi-stage.rbx.com:3128`). Guest curl honors them, so
mocked `apis.roblox.com` requests tried to connect to port 3128 (confirmed
via `MACHGATE_FD_TRACE_FILE`: `connect ... port=3128`). Intermittent
proxy failures surface as `HttpError: ConnectFail`, after which the
engine's failure path can spin an `RBX Worker` thread at ~99% CPU inside
`STM::Kernel::Impl::task` → `TaskScheduler::Arbiter::step` while every
other thread blocks in `__ulock_wait2` — the daemon wedges until the
scheduler kills it (the TIMEOUT bucket), and the group's remaining cases
cascade to CRASHED.

Not a machgate bug: macOS CI hosts have no proxy env. CI agents and
local devspaces running the corpus must strip
`http_proxy/https_proxy/HTTP_PROXY/HTTPS_PROXY/ALL_PROXY` (keep
`no_proxy`) for the machgate-wrapped daemons. The repo tool
`bin/rotest-daemon-driver.py` strips them by default.

### 3. Healed by earlier fixes

The Lua service timeouts (require_from_subfolder, vector3_is_addable,
Instance_QueryDescendants_MalformedQuery, all FixturelessTests) pass
with the current loader; they were recorded before the kevent
wait-through + MACHGATE_GUEST_NCPU + flock + @executable_path dylib
fixes landed.

## Tooling

`bin/rotest-daemon-driver.py` drives a machgate-wrapped rotest daemon
exactly like the scheduler: spawn with `--daemon
--daemon-control-dir=...`, ping, RUN_GROUP per case, collect
case_result/group_done messages. Use it to reproduce daemon-churn
flakes:

    python3 bin/rotest-daemon-driver.py \
        env LD_LIBRARY_PATH=/opt/machgate/lib \
        MACHGATE_CONFIG=/tmp/machgate/machgate.conf \
        /opt/machgate/bin/machgate <binary> -- <Case/Name> <Case/Name>...
