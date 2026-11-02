#!/usr/bin/env python3
"""State of the owner's chain for the guard hook `.claude/hooks/sh_chain_guard.py` (stdlib only).

  python tools/sh_chain.py start "<label>"              tests phase: code closed, tests open
  python tools/sh_chain.py freeze                       after the red run: tests frozen (hash recorded), code open
  python tools/sh_chain.py unfreeze --because PB-12     only after the owner decided: tests open again
  python tools/sh_chain.py done                         the change is committed: no limits
  python tools/sh_chain.py status
  python tools/sh_chain.py hash                         hash of all test files (for the step record)

The marker `.claude/state/chain.json` is local (not committed). `sh_process_check.py process` reports an ERROR
when the test file differs from the frozen hash, which also catches edits made through the shell.
"""
import argparse
import hashlib
import json
import os
import re
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE = os.path.join(ROOT, ".claude", "state", "chain.json")
TEST_FILE = "tools/sh_test_system.py"
TEST_DIR = "tools/sh_tests"
REASON = re.compile(r"^(PB|D|C|O)-\d+$")


def test_hash():
    """One hash over the runner and every tools/sh_tests/*.py (same rule as sh_process_check.check_chain)."""
    names = [TEST_FILE] + sorted(f"{TEST_DIR}/{n}" for n in os.listdir(os.path.join(ROOT, TEST_DIR)) if n.endswith(".py"))
    h = hashlib.sha256()
    for name in names:
        with open(os.path.join(ROOT, name), "rb") as f:
            h.update(name.encode() + b"\0" + f.read())
    return h.hexdigest()


def load():
    try:
        with open(STATE, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def save(state):
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    with open(STATE, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["start", "freeze", "unfreeze", "done", "status", "hash"])
    ap.add_argument("label", nargs="?", default="")
    ap.add_argument("--because", default="")
    args = ap.parse_args()
    state = load()

    if args.cmd == "hash":
        print(test_hash())
        return
    if args.cmd == "start":
        if state:
            sys.exit(f"a change is already running ('{state.get('label')}', phase {state.get('phase')}): "
                     "finish it with 'done' first")
        state = {"label": args.label or "change", "phase": "tests", "started": time.strftime("%Y-%m-%d %H:%M"),
                 "history": []}
        save(state)
    elif args.cmd == "status":
        print(json.dumps(state, indent=1) if state else "no change running (no limits)")
        return
    elif not state:
        sys.exit("no change running: start one first")
    elif args.cmd == "freeze":
        if state["phase"] != "tests":
            sys.exit("already frozen")
        state.update(phase="code", hash=test_hash())
        save(state)
    elif args.cmd == "unfreeze":
        if state["phase"] != "code":
            sys.exit("not frozen")
        if not REASON.match(args.because):
            sys.exit("--because needs the PB-n / D-n / C-n / O-n row of the owner's decision")
        state["history"].append({"because": args.because, "at": time.strftime("%Y-%m-%d %H:%M")})
        state.update(phase="tests")
        state.pop("hash", None)
        save(state)
    elif args.cmd == "done":
        os.remove(STATE)
        print("done: no limits")
        return
    print(f"{state['label']}: phase {state['phase']}")


if __name__ == "__main__":
    main()
