#!/usr/bin/env python3
"""Drive a rotest daemon under machgate the way the scheduler does.

Spawns `machgate <binary> --daemon --daemon-control-dir=...`, waits for
ready.json, sends LIST_TESTS, then RUN_GROUP requests with the given case
names, reusing the process across groups (daemon churn reproduction).

Usage:
  daemon_driver.py <machgate-binary-wrapper-args...> -- <case1> <case2> ...
Prints per-case results from case_result messages, and any fatal/crash output.
"""
import json
import os
import subprocess
import sys
import tempfile
import time

def main():
    args = sys.argv[1:]
    sep = args.index("--")
    cmd = args[:sep]
    cases = args[sep + 1:]

    control = tempfile.mkdtemp(prefix="mg-driver-")
    env = dict(os.environ)
    for proxy_key in ("http_proxy", "https_proxy", "HTTP_PROXY", "HTTPS_PROXY",
                      "ALL_PROXY", "all_proxy"):
        env.pop(proxy_key, None)
    cmd = cmd + ["--daemon", f"--daemon-control-dir={control}"]
    daemon_log = open(os.environ.get("DRIVER_LOG", "/tmp/daemon_stdout.log"), "w")
    proc = subprocess.Popen(cmd, env=env, stdout=daemon_log,
                            stderr=subprocess.STDOUT, text=True)

    def wait_file(name, timeout=900):
        path = os.path.join(control, name)
        deadline = time.time() + timeout
        while time.time() < deadline:
            if os.path.exists(path):
                try:
                    data = json.loads(open(path).read())
                    os.unlink(path)
                    return data
                except Exception:
                    pass
            if proc.poll() is not None:
                print(f"[driver] daemon DIED rc={proc.returncode}", file=sys.stderr)
                daemon_log.flush()
                sys.exit(3)
            time.sleep(0.05)
        print(f"[driver] timeout waiting {name}", file=sys.stderr)
        proc.kill()
        sys.exit(4)

    def send(obj):
        tmp = os.path.join(control, "request.json.tmp")
        with open(tmp, "w") as fh:
            json.dump(obj, fh)
        os.replace(tmp, os.path.join(control, "request.json"))

    ready = wait_file("ready.json")
    seq = 0

    def recv():
        nonlocal seq
        msg = wait_file(f"response-{seq}.json")
        seq += 1
        return msg.get("messages", [])

    send({"kind": "ping"})
    for m in recv():
        print(f"[ping] {m.get('kind')}")

    crashed = 0
    for case in cases:
        send({"kind": "run_group", "group_id": f"g-{case}",
              "cases": [case], "flag_mode": "both",
              "flag_overrides": {}, "tests_path": None})
        done = False
        while not done:
            for m in recv():
                kind = m.get("kind")
                if kind == "case_result":
                    status = m.get("status") or m.get("result")
                    print(f"{case}: {status}")
                elif kind == "group_done":
                    done = True
                elif kind == "fatal":
                    print(f"{case}: FATAL {m}")
                    done = True
                    crashed += 1
                elif kind == "ready":
                    done = True
                elif kind != "case_started":
                    print(f"[msg] {json.dumps(m)[:300]}")
            if not done and proc.poll() is not None:
                print(f"{case}: DAEMON-DIED rc={proc.returncode}", file=sys.stderr)
                crashed += 1
                break

    send({"kind": "shutdown"})
    try:
        proc.wait(timeout=30)
    except subprocess.TimeoutExpired:
        proc.kill()
    print(f"[driver] exit rc={proc.returncode} crashed_groups_lost")

if __name__ == "__main__":
    main()
