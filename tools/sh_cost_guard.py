#!/usr/bin/env python3
"""Cost, token, and runaway loop guard hook (owner, 07.11.2026).

Smart differentiation:
- Passive tools (read_file, git status/diff, local test runners like sh_test_system.py or pio run)
  run on local CPU with 0 generative tokens and are NOT heavily penalized.
- Active modification tools (edit_file, write_file, Edit, Write) alter the codebase and carry
  strict limits (max 5 consecutive edits before requiring user checkpoint).
- Total tool actions are capped at 15 per burst or 10 minutes elapsed.

Usage:
  python tools/sh_cost_guard.py claude-prompt         Claude Code UserPromptSubmit hook
  python tools/sh_cost_guard.py claude-tool           Claude Code PreToolUse hook
  python tools/sh_cost_guard.py agy-pre-invocation    Antigravity PreInvocation hook
  python tools/sh_cost_guard.py agy-pre-tool          Antigravity PreToolUse hook
  python tools/sh_cost_guard.py reset                 Reset counter and timer
  python tools/sh_cost_guard.py status                Show current budget usage
"""
import json
import os
import re
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATE_DIR = os.path.join(ROOT, ".claude", "state")
STATE_FILE = os.path.join(STATE_DIR, "cost_guard.json")

DEFAULT_MAX_EDITS = 5
DEFAULT_MAX_TOTAL = 15
DEFAULT_MAX_SECONDS = 600  # 10 minutes

PASSIVE_TOOLS = {
    "read_file", "view_file", "list_resources", "read_resource",
    "grep", "glob", "read", "ask_question", "list_plugin_accounts",
    "manage_task"
}

ACTIVE_EDIT_TOOLS = {
    "edit_file", "write_file", "edit", "write", "notebookedit"
}

LOCAL_TEST_CMDS = re.compile(
    r"^(git\s+(status|diff|log|branch)|"
    r"python\s+tools/sh_(test_system|process_check|cost_guard|chain)\.py|"
    r"pio\s+run|"
    r"dir|ls|type|cat|echo)",
    re.IGNORECASE
)


def get_limits():
    max_edits = int(os.environ.get("SH_MAX_EDITS", DEFAULT_MAX_EDITS))
    max_total = int(os.environ.get("SH_MAX_TOOL_CALLS", DEFAULT_MAX_TOTAL))
    max_seconds = int(os.environ.get("SH_MAX_TIME_SEC", DEFAULT_MAX_SECONDS))
    disabled = os.environ.get("SH_COST_GUARD", "").strip().lower() in ("off", "0", "false", "disabled")
    return max_edits, max_total, max_seconds, disabled


def load_state():
    try:
        with open(STATE_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {
            "prompt_time": time.time(),
            "total_calls": 0,
            "edit_calls": 0,
            "passive_calls": 0,
            "last_tool_time": time.time(),
            "last_tool_name": ""
        }


def save_state(state):
    os.makedirs(STATE_DIR, exist_ok=True)
    with open(STATE_FILE, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)


def reset_state():
    state = {
        "prompt_time": time.time(),
        "total_calls": 0,
        "edit_calls": 0,
        "passive_calls": 0,
        "last_tool_time": time.time(),
        "last_tool_name": ""
    }
    save_state(state)
    return state


def classify_call(tool_name, tool_input):
    name = (tool_name or "").strip().lower()
    if name in ACTIVE_EDIT_TOOLS:
        return "edit"
    if name in PASSIVE_TOOLS:
        return "passive"
    if name in ("run_command", "bash"):
        cmd = (tool_input.get("CommandLine") or tool_input.get("command") or "").strip()
        if LOCAL_TEST_CMDS.search(cmd):
            return "passive"
        return "active"
    return "active"


def update_and_check(tool_name, tool_input):
    max_edits, max_total, max_seconds, disabled = get_limits()
    if disabled:
        return False, "", None

    cat = classify_call(tool_name, tool_input)
    state = load_state()
    now = time.time()
    state["total_calls"] = state.get("total_calls", 0) + 1
    if cat == "edit":
        state["edit_calls"] = state.get("edit_calls", 0) + 1
    elif cat == "passive":
        state["passive_calls"] = state.get("passive_calls", 0) + 1
    state["last_tool_time"] = now
    state["last_tool_name"] = tool_name
    save_state(state)

    prompt_time = state.get("prompt_time", now)
    elapsed = now - prompt_time

    limit_exceeded = False
    reason = ""

    if state["edit_calls"] > max_edits:
        limit_exceeded = True
        reason = f"Atins limita de {max_edits} modificari consecutive de fisiere fara checkpoint."
    elif state["total_calls"] > max_total:
        limit_exceeded = True
        reason = f"Atins limita de {max_total} actiuni consecutive fara checkpoint."
    elif elapsed > max_seconds:
        limit_exceeded = True
        reason = f"Timpul alocat fara checkpoint a depasit {max_seconds / 60:.1f} minute."

    return limit_exceeded, reason, state


def handle_claude_prompt():
    reset_state()
    prompt_guidance = (
        "\n[COST & TOKEN GUARD ACTIVE]\n"
        "1. Plan-First: Pentru orice cerinta de modificare, prezinta intai un micro-plan (3-5 linii) si cere confirmare.\n"
        "2. Fara editari oarbe: Nu modifica fisiere la primul turn inainte ca utilizatorul sa vada directia.\n"
        "3. Testele locale (sh_test_system.py, git status) nu consuma tokens de generare si sunt permise liber.\n"
        "4. Limita executie activa: max 5 editari consecutive sau 15 actiuni totale inainte de raportare.\n"
    )
    print(prompt_guidance)
    sys.exit(0)


def handle_claude_tool():
    try:
        raw = sys.stdin.read()
        data = json.loads(raw) if raw.strip() else {}
    except Exception:
        data = {}

    tool_name = data.get("tool_name", "")
    tool_input = data.get("tool_input", {})

    blocked, reason, state = update_and_check(tool_name, tool_input)
    if blocked:
        elapsed_min = (time.time() - state.get("prompt_time", time.time())) / 60.0
        msg = (
            f"\n[COST & TOKEN GUARD - RUNAWAY CHECKPOINT]\n"
            f"{reason}\n"
            f"Statistici sesiune: {state.get('edit_calls', 0)} editari, {state.get('total_calls', 0)} total "
            f"({elapsed_min:.1f} min elapsed).\n"
            f"OPRESTE-TE ACUM:\n"
            f"1. Prezinta un sumar concis cu ce s-a facut pana acum.\n"
            f"2. Prezinta micro-planul pentru pasii ramasi.\n"
            f"3. Cere aprobarea explicita a proprietarului pentru a continua.\n"
        )
        print(msg, file=sys.stderr)
        sys.exit(2)

    sys.exit(0)


def handle_agy_pre_invocation():
    reset_state()
    response = {
        "injectSteps": [
          {
            "ephemeralMessage": (
              "[COST & TOKEN GUARD]\n"
              "- Regula Plan-First: Prezinta un micro-plan (3-5 linii) inainte de modificari si asteapta aprobarea.\n"
              "- Testele si verificarile locale sunt pasive; bugetul de editari este max 5 fisiere per checkpoint.\n"
              "- Gandire concisa si focusata pentru minimizarea consumului de tokens."
            )
          }
        ]
    }
    print(json.dumps(response))
    sys.exit(0)


def handle_agy_pre_tool():
    try:
        raw = sys.stdin.read()
        data = json.loads(raw) if raw.strip() else {}
    except Exception:
        data = {}

    tool_name = (data.get("toolCall") or {}).get("name", "")
    tool_input = (data.get("toolCall") or {}).get("args", {})

    blocked, reason, state = update_and_check(tool_name, tool_input)
    if blocked:
        elapsed_min = (time.time() - state.get("prompt_time", time.time())) / 60.0
        response = {
            "decision": "ask",
            "reason": (
                f"Cost & Token Guard Checkpoint: {reason} "
                f"({state.get('edit_calls', 0)} editari, {state.get('total_calls', 0)} total, {elapsed_min:.1f} min). "
                f"Te rog revizuieste micro-planul si aproba continuarea."
            )
        }
    else:
        response = {"decision": "allow"}

    print(json.dumps(response))
    sys.exit(0)


def handle_status():
    max_edits, max_total, max_seconds, disabled = get_limits()
    state = load_state()
    now = time.time()
    elapsed = now - state.get("prompt_time", now)
    print("Cost Guard Status:")
    print(f"  Status: {'DISABLED' if disabled else 'ACTIVE'}")
    print(f"  Editari active (fisiere): {state.get('edit_calls', 0)} / {max_edits}")
    print(f"  Total actiuni: {state.get('total_calls', 0)} / {max_total}")
    print(f"  Actiuni pasive (teste/read): {state.get('passive_calls', 0)}")
    print(f"  Timp scurs: {elapsed:.1f}s ({elapsed / 60.0:.1f} min) / {max_seconds}s")
    print(f"  Ultima unealta: {state.get('last_tool_name', 'none')}")
    print(f"  Fisier stare: {STATE_FILE}")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1].lower()
    if cmd == "claude-prompt":
        handle_claude_prompt()
    elif cmd == "claude-tool":
        handle_claude_tool()
    elif cmd == "agy-pre-invocation":
        handle_agy_pre_invocation()
    elif cmd == "agy-pre-tool":
        handle_agy_pre_tool()
    elif cmd == "reset":
        reset_state()
        print("Cost Guard: State reset successfully.")
    elif cmd == "status":
        handle_status()
    else:
        print(f"Unknown command: {cmd}\n")
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
