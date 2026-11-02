#!/usr/bin/env python3
"""PreToolUse hook (Edit / Write): order of work inside the owner's chain (owner, 02.11.2026).

While a change runs through `sh-change`, `tools/sh_chain.py` keeps a marker `.claude/state/chain.json`:
  phase "tests"  the tests are written; the code (src/, include/, web/) is closed
  phase "code"   the tests are frozen; the test file is closed
No marker = no limits (other work is not touched). A block exits 2; the message goes to the model.
A test or a requirement that is wrong or unclear is never "fixed" here: the model stops and asks the owner.
"""
import json
import os
import sys

TEST_FILES = ("tools/sh_test_system.py", "tools/sh_tests/")  # the runner and the per-area test modules
CODE_DIRS = ("src/", "include/", "web/")
GENERATED = ("_html_gz.h", "git_version.h")


def block(text):
    print(f"Chain guard: {text}", file=sys.stderr)
    sys.exit(2)


def main():
    try:
        data = json.load(sys.stdin)
    except ValueError:
        sys.exit(0)
    root = os.environ.get("CLAUDE_PROJECT_DIR") or data.get("cwd") or os.getcwd()
    try:
        with open(os.path.join(root, ".claude", "state", "chain.json"), encoding="utf-8") as f:
            state = json.load(f)
    except (OSError, ValueError):
        sys.exit(0)

    path = (data.get("tool_input") or {}).get("file_path") or (data.get("tool_input") or {}).get("notebook_path")
    if not path:
        sys.exit(0)
    full = path if os.path.isabs(path) else os.path.join(data.get("cwd") or root, path)
    rel = os.path.relpath(os.path.normpath(full), os.path.normpath(root)).replace(os.sep, "/")
    if rel.startswith(".."):
        sys.exit(0)

    phase = state.get("phase")
    label = state.get("label", "")
    if rel.lower().startswith(TEST_FILES) and rel.endswith(".py") and phase == "code":
        block(f"the tests of '{label}' are frozen. If a test or the requirement is wrong or unclear, STOP and ask the "
              "owner (decision, plain Romanian, options); never change a test to make it pass. Only after the owner "
              "decided: python tools/sh_chain.py unfreeze --because <PB-n|D-n|C-n>.")
    if rel.startswith(CODE_DIRS) and not rel.endswith(GENERATED) and phase == "tests":
        block(f"tests first for '{label}'. Write the tests in tools/sh_tests/ (the area file) from the requirement, run them on the board "
              "(they fail), then python tools/sh_chain.py freeze. The code opens after the freeze.")
    sys.exit(0)


if __name__ == "__main__":
    main()
