"""PreToolUse hook of the clean-room agents (sh-test-writer, sh-test-reviewer).

Tests are written and reviewed from the requirements and the architecture, never from the code
(docs/process.md §3.1). This hook lets the agent read only the documents and the test script, run only
the checker and read-only git commands on those files, and edit only the test script.
Exit 2 blocks the call; the text on stderr is what the agent sees.
"""
import json
import os
import re
import shlex
import sys

TEST_FILE = "tools/sh_test_system.py"  # the runner; the tests are in tools/sh_tests/ (one module per area)
TEST_DIR = "tools/sh_tests/"
TESTS = f"{TEST_FILE} and {TEST_DIR}"
READ_OK = ("docs/", ".agent/", ".claude/", "AGENTS.md", TEST_FILE, TEST_DIR, "tools/test_system.py")  # old name, for git diff
EDIT_OK = (TEST_FILE, TEST_DIR)
SHELL_META = re.compile(r"[|;&<>`$\n]")
PATCH_FLAGS = re.compile(r"^(-p|-u|-c|-m|--patch|--cc|--word-diff.*|-L.*|-U.*|--unified.*|--ext-diff|--textconv)$")
WHY = "Clean room (docs/process.md §3.1): tests come from the requirements and the architecture, not from the code."


def block(msg):
    print(f"{msg}\n{WHY}", file=sys.stderr)
    sys.exit(2)


def rel(path, cwd):
    """Repo-relative path with forward slashes, or None when it leaves the repo."""
    root = os.path.normcase(os.path.abspath(os.environ.get("CLAUDE_PROJECT_DIR") or cwd))
    full = os.path.normcase(os.path.abspath(os.path.join(cwd, path)))
    if full != root and not full.startswith(root + os.sep):
        return None
    return os.path.relpath(full, root).replace(os.sep, "/")


def allowed(path, cwd, ok):
    r = rel(path, cwd) if path else None
    ok = [os.path.normcase(p).replace(os.sep, "/") for p in ok]  # same case folding as rel() on Windows
    return r is not None and any(r == p.rstrip("/") or (p.endswith("/") and r.startswith(p)) for p in ok)


def check_bash(cmd, cwd):
    if SHELL_META.search(cmd):
        block(f"Blocked: no pipes, redirects or chained commands here: {cmd}")
    try:
        argv = shlex.split(cmd)
    except ValueError:
        block(f"Blocked: cannot parse: {cmd}")
    if not argv:
        block("Blocked: empty command.")
    if argv[0] in ("python", "python3", "py") and argv[1:3] in (["tools/sh_process_check.py", "trace"],
                                                                 ["tools/sh_process_check.py", "reqs"]):
        return
    if argv[:2] == ["git", "status"]:
        return
    if argv[:2] == ["git", "log"]:
        if any(PATCH_FLAGS.match(a) for a in argv[2:]):
            block("Blocked: git log may list commits, not show their changes; use git diff with paths.")
        return
    if argv[:2] == ["git", "diff"]:
        paths = argv[argv.index("--") + 1:] if "--" in argv else []
        if not paths:
            block("Blocked: git diff needs '-- <paths>', e.g. git diff origin/main -- tools/sh_test_system.py")
        bad = [p for p in paths if not allowed(p, cwd, READ_OK)]
        if bad:
            block(f"Blocked: {', '.join(bad)} is outside docs/, .agent/, AGENTS.md and {TESTS}.")
        return
    block("Blocked: allowed commands are 'python tools/sh_process_check.py trace|reqs', 'git status', "
          "'git log ...' and 'git diff <refs> -- <docs or test paths>'.")


def main():
    data = json.load(sys.stdin)
    tool, inp, cwd = data.get("tool_name", ""), data.get("tool_input") or {}, data.get("cwd") or os.getcwd()
    if tool == "Read":
        if not allowed(inp.get("file_path"), cwd, READ_OK):
            block(f"Blocked: {inp.get('file_path')} - read only docs/, .agent/, AGENTS.md and {TESTS}.")
    elif tool in ("Grep", "Glob"):
        if not allowed(inp.get("path"), cwd, READ_OK):
            block(f"Blocked: give {tool} a path inside docs/, .agent/, AGENTS.md or {TESTS} "
                  f"(got {inp.get('path') or 'the repo root'}).")
    elif tool in ("Edit", "Write", "NotebookEdit"):
        target = inp.get("file_path") or inp.get("notebook_path")
        if not allowed(target, cwd, EDIT_OK):
            block(f"Blocked: {target} - this agent edits only {TESTS}.")
    elif tool == "Bash":
        check_bash(inp.get("command", ""), cwd)
    # Other tools (e.g. the report hand-back) touch no files; the agent's `tools` list limits what exists.
    sys.exit(0)


if __name__ == "__main__":
    main()
