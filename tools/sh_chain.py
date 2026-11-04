#!/usr/bin/env python3
"""State of the owner's chain for the guard hook `.claude/hooks/sh_chain_guard.py` (stdlib only).

  python tools/sh_chain.py start "<label>"              tests phase: code closed, tests open
  python tools/sh_chain.py freeze                       after the red run: tests frozen (hash recorded), code open
  python tools/sh_chain.py unfreeze --because PB-12     only after the owner decided: tests open again
  python tools/sh_chain.py done                         the change is committed: no limits
  python tools/sh_chain.py status
  python tools/sh_chain.py hash                         hash of all test files (for the step record)
  python tools/sh_chain.py commit-check <msg-file>      git commit-msg hook (.githooks/commit-msg), any IDE

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


SAFETY_FILES = ("src/ThermalZoneManager", "src/BoilerRadioTrigger", "src/OutputBank", "src/main.cpp", "src/ParamStore",
                "src/DiagnosticEngine", "include/ThermalZoneManager", "include/BoilerRadioTrigger", "include/OutputBank",
                "include/ParamStore", "include/DiagnosticEngine", "include/Config.h")  # AGENTS.md section 3 list
CODE_DIRS = ("src/", "include/", "web/")
GENERATED = ("_html_gz.h", "git_version.h")
ROW = re.compile(r"\b(PB|D|C)-\d+\b")
BANNED = re.compile(r"co-authored-by|generated with", re.I)


def git(*a):
    return os.popen("git -C \"%s\" %s" % (ROOT, " ".join(a))).read()


def commit_check(msg_file):
    """Git commit-msg hook: the chain rules of AGENTS.md for any IDE (same rules as .claude/hooks/sh_chain_guard.py)."""
    if os.environ.get("SH_HOOK_SKIP") == "owner":
        print("!" * 60 + "\n!! SH_HOOK_SKIP=owner: chain checks of this commit are SKIPPED\n" + "!" * 60, file=sys.stderr)
        return
    with open(msg_file, encoding="utf-8", errors="replace") as f:
        msg = "".join(l for l in f if not l.startswith("#"))
    bad = []
    if BANNED.search(msg):
        bad.append("message carries a Co-Authored-By / 'Generated with' line (AGENTS.md section 5)")
    staged = [l.split("\t") for l in git("diff --cached --name-status --no-renames").splitlines() if l]
    names = [n[1] for n in staged]
    is_test = lambda n: n == TEST_FILE or n.startswith(TEST_DIR + "/")
    code = [n for n in names if n.startswith(CODE_DIRS) and not n.endswith(GENERATED)]
    state = load()
    if state and state.get("phase") == "tests" and code:
        bad.append("phase 'tests': code is closed, write and freeze the tests first (sh_chain.py freeze): " + ", ".join(code))
    if state and state.get("phase") == "code" and state.get("hash"):
        if any(is_test(n) for n in names) or test_hash() != state["hash"]:
            bad.append("phase 'code': tests are frozen, but they differ from the frozen hash (unfreeze needs an owner decision)")
    if not ROW.search(msg):
        for n in names:
            if is_test(n) and n.endswith(".py"):
                removed = [l for l in git("diff --cached -U0 -- \"%s\"" % n).splitlines()
                           if l.startswith("-") and not l.startswith("---")]
                if removed:
                    bad.append(f"{n}: an existing test line is edited or removed; additions only, unless the message "
                               "cites the PB-n / D-n / C-n row of the owner's decision")
    if any(n.startswith(SAFETY_FILES) for n in names) and "Safety-Reviewed:" not in msg:
        bad.append("a safety-list file changed (AGENTS.md section 3): add a 'Safety-Reviewed: <who/how>' line to the message")
    if bad:
        print("Chain check blocked this commit:\n - " + "\n - ".join(bad) +
              "\n(owner only: SH_HOOK_SKIP=owner git commit ...)", file=sys.stderr)
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["start", "freeze", "unfreeze", "done", "status", "hash", "commit-check"])
    ap.add_argument("label", nargs="?", default="")
    ap.add_argument("--because", default="")
    args = ap.parse_args()
    if args.cmd == "commit-check":
        return commit_check(args.label)
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
