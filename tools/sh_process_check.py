#!/usr/bin/env python3
"""
Traceability and process check (stdlib only). Rules: docs/process.md.

  trace    requirements (docs/requirements_and_features.md) <-> modules (docs/architecture.md §3.2)
           <-> tests (tools/sh_test_system.py docstrings) <-> code tags (// FR-x in src/, include/, web/)
  reqs     requirement writing rules: one value per quantity, measurable wording,
           one meaning per DTC code (doc and firmware)
  process  rules from AGENTS.md checked against the branch diff:
           safety defaults, changelog + activeContext updated, commit message format,
           debug leftovers, secrets, new libraries, firmware size, build identity,
           problems log (docs/problems.md) and PB-n citations in commits
  board   live config of the board (GET /api/config/system) against the safety limits,
           and which commit the board runs. Needs the board; not part of 'all'.
  release  gate before push / PR to main: an OK acceptance row for a build equal to HEAD,
           no open safety problem, APP_VERSION not already released. Not part of 'all'.

Usage:
  python tools/sh_process_check.py                 # trace + reqs + process, diff against origin/main or main
  python tools/sh_process_check.py trace --matrix docs/traceability.md
  python tools/sh_process_check.py process --base HEAD~3 --strict
  python tools/sh_process_check.py board --ip 192.168.1.20
  python tools/sh_process_check.py release

Exit code 1 on any ERROR (or WARN with --strict).
"""

import argparse
import ast
import functools
import glob
import json
import os
import re
import subprocess
import sys
import urllib.request

from sh_git_version import FIRMWARE_INPUTS, firmware_commit

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REQ_DOC = "docs/requirements_and_features.md"
ARCH_DOC = "docs/architecture.md"
TEST_FILE = "tools/sh_test_system.py"
CODE_DIRS = ("src", "include", "web")
CODE_SKIP = re.compile(r"_html_gz\.h$|git_version\.h$")
CHANGELOG = "docs/changelog.md"
ACTIVE_CONTEXT = ".agent/memory-bank/activeContext.md"
PROBLEMS = "docs/problems.md"
ACCEPTANCE = "docs/acceptance.md"
VERSION_H = "include/Version.h"
PB_ID = re.compile(r"\bPB-\d+\b")
PB_STATUS = ("open", "analysing", "fixed", "closed", "rejected")
PB_OPEN = ("open", "analysing")
CODE_PATHS = ("src/", "include/", "web/", "tools/", "platformio.ini")
FIRMWARE_MAX_BYTES = int(1.25 * 1024 * 1024)

REQ_ID = r"N?FR-\d+(?:\.\d+)?"
COMMIT_TYPES = "feat|fix|refactor|perf|test|docs|chore|ci|build|style|revert"
COMMIT_RE = re.compile(rf"^(?:{COMMIT_TYPES})(?:,(?:{COMMIT_TYPES}))*(?:\([\w ,./-]+\))?!?: \S")
DEBUG_RE = re.compile(r"\b(?:TODO|FIXME|XXX|HACK)\b|console\.log\(|\bdebugger;|^\s*#if\s+0\b|\[DEBUG\]")

# (file, regex capturing a number, unit, requirements §6 parameter, what it is). The allowed range is read
# from the §6 table, the only source of defaults and ranges (process R2).
# Compile-time defaults, NVS fallbacks and the API clamps that can change them at runtime.
API_CLAMP = r'server\.arg\("{}"\)\.toInt\(\);\s*if \(s >= (\d+) && s <= (\d+)\)'
SAFETY_LIMITS = [
    ("include/Config.h", r"^\s*#define\s+DEFAULT_ACTUATOR_DELAY_MS\s+(\d+)", "ms", "interlockDelay",
     "actuator opening interlock (FR-2)"),
    ("include/Config.h", r"^\s*#define\s+DEFAULT_PUMP_OVERRUN_MS\s+(\d+)", "ms", "overrun", "overrun (FR-3)"),
    ("include/Config.h", r"^\s*#define\s+DEFAULT_MIN_BOILER_OFF_MS\s+(\d+)", "ms", "burnerMinPause",
     "burner minimum pause (FR-4)"),
    ("src/BoilerRadioTrigger.cpp", r'getUInt\("min_pause",\s*(\d+)', "ms", "burnerMinPause",
     "burner minimum pause (FR-4)"),
    ("src/BoilerRadioTrigger.cpp", r'getUInt\("min_run",\s*(\d+)', "ms", "burnerMinRun", "burner minimum run (FR-4)"),
    ("src/ThermalZoneManager.cpp", r"_freezeProtectionTemp\(([\d.]+)f?\)", "C", "frostTemp",
     "frost protection threshold (FR-16.1)"),
    ("src/ThermalZoneManager.cpp", r'getFloat\("frz_temp",\s*([\d.]+)', "C", "frostTemp",
     "frost protection NVS default (FR-16.1)"),
    ("src/ThermalZoneManager.cpp", r'getUInt\("s_tout",\s*(\d+)', "s", "sensorTimeout",
     "sensor timeout isolation (FR-16.2, DTC-101)"),
    ("src/WebApi.cpp", API_CLAMP.format("actuatorDelaySec"), "s", "interlockDelay", "API actuatorDelaySec range (FR-2)"),
    ("src/WebApi.cpp", API_CLAMP.format("pumpOverrunSec"), "s", "overrun", "API pumpOverrunSec range (FR-3)"),
    ("src/WebApi.cpp", API_CLAMP.format("shortCyclingSec"), "s", "burnerMinRun", "API shortCyclingSec range (FR-4)"),
    ("src/WebApi.cpp", r"if \(f >= ([\d.]+)f? && f <= ([\d.]+)f?\) thermalZones\.setFreezeProtectionTemp", "C",
     "frostTemp", "API frost threshold range (FR-16.1)"),
]

# Live values from GET /api/config/system: (JSON key, unit, requirements §6 parameter, what it is).
# "zones[]." checks every zone.
BOARD_IP = "192.168.1.20"
BOARD_LIMITS = [
    ("actuatorDelaySec", "s", "interlockDelay", "actuator opening interlock (FR-2)"),
    ("pumpOverrunSec", "s", "overrun", "overrun (FR-3)"),
    ("shortCyclingSec", "s", "burnerMinRun", "burner minimum run (FR-4)"),
    ("freezeProtectionTemp", "C", "frostTemp", "frost protection threshold (FR-16.1)"),
    ("sensorTimeoutSec", "s", "sensorTimeout", "sensor timeout isolation (FR-16.2, DTC-101)"),
    ("zones[].windowDropThreshold", "C", "windowThreshold", "window drop threshold (FR-40)"),
]
# Safety values the firmware enforces but the API does not report, so they cannot be checked live.
BOARD_NOT_EXPOSED = ["burner minimum pause (FR-4, BoilerRadioTrigger min_pause)"]

# Requirement writing rules (docs/process.md §2). A quantity is found by keyword; the value nearest to the
# keyword on the same line is its value. Kinds: time (s), temp (C), size (KB).
QUANTITIES = [
    ("sensor timeout", "time", r"sensor.?timeout|timeout senzor|lips\w* (?:de )?telemetri|no valid reading|DTC-101"),
    ("actuator opening interlock", "time",
     r"interlock(?!\w* anti)|interblocare(?! anti| cazan)|cursa\w* (?:completă )?de deschidere|actuator opening|"
     r"interlockDelay"),
    ("pump overrun", "time", r"overrun|post-circula"),
    ("burner anti-short-cycling", "time", r"short.?cycl|anti-ciclare|min(?:imum)? pause|pauz\w* minim|ciclu termic"),
    ("frost protection", "temp", r"frost|îngheț|inghet|freeze"),
    ("free heap", "size", r"heap"),
]
NUM = r"(\d+(?:[.,]\d+)?)"
TIME_UNIT = r"ms|secunde|seconds?|sec|s|minute|minutes?|min|ore|hours?|h"
VALUE_RE = {
    # "60s – 7200s", "120 și 180 de secunde", "3–5 minute", "120s"
    "time": re.compile(NUM + rf"(?:\s*(?:{TIME_UNIT})\b)?(?:\s*(?:–|-|și|si|and|to)\s*" + NUM + r")?\s*(?:de\s+)?"
                       rf"({TIME_UNIT})\b", re.I),
    "temp": re.compile(r"(?<![\d.])" + NUM + r"(?:\s*(?:–|-|și|and|to)\s*" + NUM + r")?\s*(?:°\s*C|\^\\circ)", re.I),
    "size": re.compile(NUM + r"(?:\s*(?:–|-|și|and|to)\s*" + NUM + r")?\s*(KB|MB)\b"),
}
TO_BASE = {"ms": 0.001, "s": 1, "sec": 1, "second": 1, "seconds": 1, "secunde": 1, "min": 60, "minute": 60,
           "minutes": 60, "h": 3600, "hour": 3600, "hours": 3600, "ore": 3600, "kb": 1, "mb": 1024}
BASE_UNIT = {"time": "s", "temp": "C", "size": "KB"}
# Words that make a requirement untestable unless a number is attached (EN + RO, the doc is mixed).
VAGUE_RE = re.compile(r"\b(fast|quick(?:ly)?|instant(?:ly)?|immediate(?:ly)?|approximately|optimal|robust|seamless|"
                      r"user-friendly|as needed|etc|rapid[ăa]?|instantane?u?|imediat[ăa]?|aproximativ|optim[ăa]?|"
                      r"inteligent[ăa]?|robust[ăa]?)\b", re.I)
DTC_RE = re.compile(r"DTC-\d+")
DTC_NAME_RE = re.compile(r"`((?:ERR|WARN|INFO)_[A-Z0-9_]+)`")
DTC_RAISE_RE = re.compile(r'raiseDtc\("(DTC-\d+)"')


class Report:
    def __init__(self):
        self.errors = 0
        self.warnings = 0

    def section(self, title):
        print(f"\n== {title} ==")

    def error(self, msg):
        self.errors += 1
        print(f"  ERROR  {msg}")

    def warn(self, msg):
        self.warnings += 1
        print(f"  WARN   {msg}")

    def info(self, msg):
        print(f"  INFO   {msg}")


def read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
        return f.read()


class GitError(Exception):
    pass


def git(*args, probe=False):
    """Run git with unquoted UTF-8 paths. Failures raise GitError, or return None when probe=True."""
    try:
        r = subprocess.run(["git", "-c", "core.quotepath=off", *args], cwd=ROOT,
                           capture_output=True, text=True, encoding="utf-8")
    except FileNotFoundError:
        raise GitError("git not found on PATH")
    if r.returncode != 0:
        if probe:
            return None
        raise GitError(f"git {' '.join(args)}: {r.stderr.strip() or f'exit {r.returncode}'}")
    return r.stdout


def git_paths(*args):
    return {p for p in git(*args).splitlines() if p}


# ---------------------------------------------------------------- traceability

def parse_requirements(text):
    """Return ({id: title}, {id: [line numbers]} of the bold labels).

    A bold label at the start of a list item (`- **FR-2 (Title):**`) defines an ID; the parent of a
    defined ID (FR-10 for FR-10.2) is known too, titled by its section heading. Struck-through (~~)
    lines are retired."""
    reqs, defined, heading = {}, {}, ""
    for n, line in enumerate(text.splitlines(), 1):
        if line.startswith("#"):
            heading = re.sub(r"^#+\s*[\d.]*\s*", "", line).strip()
        if "~~" in line:
            continue
        m = re.match(rf"\s*[-*]\s+\*\*({REQ_ID})\s*(?:\(([^)]*)\))?", line)
        if m:
            defined.setdefault(m.group(1), []).append(n)
            reqs[m.group(1)] = (m.group(2) or "").strip()
            if parent_of(m.group(1)):
                reqs.setdefault(parent_of(m.group(1)), heading)
    return reqs, defined


def parse_modules(text):
    """Return {req id: [modules]} from the architecture table with a Module and a Requirements column
    (§3.2): the requirement -> module allocation, kept by hand in that one direction."""
    alloc, cols = {}, None
    for line in text.splitlines():
        if not line.startswith("|"):
            cols = None
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if cols is None:  # header row of a new table
            cols = (cells.index("Module"), cells.index("Requirements")) if {"Module", "Requirements"} <= set(cells) else ()
        elif cols and not re.fullmatch(r"[-: ]+", cells[0]):
            for rid in re.findall(REQ_ID, cells[cols[1]]):
                alloc.setdefault(rid, []).append(cells[cols[0]].strip("`"))
    return alloc


def parse_tests(source):
    """Return (registry dict, [test dicts]) from tools/sh_test_system.py without importing it."""
    tree = ast.parse(source)
    registry, tests = {}, []
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and any(getattr(t, "id", None) == "SWE1_REQUIREMENTS" for t in node.targets):
            registry = ast.literal_eval(node.value)
        if isinstance(node, ast.FunctionDef) and node.name.startswith("test_"):
            doc = ast.get_docstring(node) or ""
            consts = {}
            for sub in ast.walk(node):
                if isinstance(sub, ast.Assign) and isinstance(sub.value, ast.Constant):
                    for t in sub.targets:
                        if isinstance(t, ast.Name) and t.id in ("test_id", "req"):
                            consts[t.id] = sub.value.value
            m = re.match(r"(SWE\.\d-TC[\d.]+):", doc)
            groups = [g for g in re.findall(r"\(([^()]*)\)", doc) if re.search(REQ_ID, g)]
            refs = re.findall(REQ_ID, groups[-1]) if groups else []
            tests.append({"func": node.name, "line": node.lineno, "tc": m.group(1) if m else None,
                          "refs": refs, "test_id": consts.get("test_id"),
                          "req": re.findall(REQ_ID, consts.get("req", "")),
                          "promised": promised_values(doc.split(":", 1)[-1] + " " + str(consts.get("name", ""))),
                          **collect_checks(node)})
    return registry, tests


# A number with a unit in a test title is a promise: "(120s)", "(>100 KB)", "(<12°C)", "(TWDT 90s)".
PROMISE_RE = re.compile(r"(?<![\w.-])(\d+(?:\.\d+)?)\s*(ms|s|sec|min|h|°C|C|%|KB|MB|dBm)(?![\w])")
UNIT_SCALES = {"s": (1, 1000), "sec": (1, 1000), "min": (1, 60, 60000), "h": (1, 60, 3600),
               "KB": (1, 1000, 1024), "MB": (1, 1000 * 1000, 1024 * 1024)}


def promised_values(text):
    """Return [(token, {accepted numeric forms})] for unit-bearing numbers in a test title/docstring."""
    out = {}
    for m in PROMISE_RE.finditer(text):
        val = float(m.group(1))
        out[m.group(0)] = {round(val * k, 6) for k in UNIT_SCALES.get(m.group(2), (1,))}
    return list(out.items())


def is_success_status_check(expr):
    """True if expr only checks that a request succeeded: every `and` operand compares .status_code and
    accepts a 2xx code. Expecting 4xx (a rejection) is a real behavior check, so it returns False."""
    operands = expr.values if isinstance(expr, ast.BoolOp) and isinstance(expr.op, ast.And) else [expr]
    for op in operands:
        if not (isinstance(op, ast.Compare) and "status_code" in ast.unparse(op)):
            return False
        codes = [c.value for c in ast.walk(op) if isinstance(c, ast.Constant) and isinstance(c.value, int)]
        if codes and not any(200 <= c < 300 for c in codes):
            return False
    return True


def collect_checks(func):
    """Asserts of a test, and the numeric / string constants compared anywhere in its body."""
    asserts = [n for n in ast.walk(func) if isinstance(n, ast.Assert)]
    raises = [n for n in ast.walk(func) if isinstance(n, ast.Raise)]
    nums, strs = set(), []
    for cmp in (n for n in ast.walk(func) if isinstance(n, ast.Compare)):
        for c in ast.walk(cmp):
            if isinstance(c, ast.Constant) and isinstance(c.value, (int, float)) and not isinstance(c.value, bool):
                nums.add(round(float(c.value), 6))
            elif isinstance(c, ast.Constant) and isinstance(c.value, str):
                strs.append(c.value)
    status_only = bool(asserts) and not raises and all(is_success_status_check(a.test) for a in asserts)
    return {"asserts": len(asserts) + len(raises), "compared_nums": nums, "compared_strs": strs,
            "status_only": status_only}


def check_hollow_tests(rep, tests):
    """SWE.5 tests that cannot fail on what they promise (heuristic, WARN only)."""
    for t in tests:
        where = f"{TEST_FILE}:{t['line']} {t['tc'] or t['func']}"
        if not t["asserts"]:
            rep.warn(f"{where}: hollow test - no assert, it can only fail on a request error")
            continue
        if t["status_only"]:
            rep.warn(f"{where}: hollow test - only asserts status_code, never the returned values")
        for token, forms in t["promised"]:
            if not (forms & t["compared_nums"]) and not any(token in s for s in t["compared_strs"]):
                rep.warn(f"{where}: hollow test - title promises '{token}' but no assert/compare checks that value")


def scan_code_tags():
    """Return {req id: sorted [file:line]} for FR/NFR tags in firmware and UI sources."""
    tags = {}
    for d in CODE_DIRS:
        for path in sorted(glob.glob(os.path.join(ROOT, d, "**", "*"), recursive=True)):
            if not os.path.isfile(path) or CODE_SKIP.search(path):
                continue
            rel = os.path.relpath(path, ROOT).replace(os.sep, "/")
            with open(path, encoding="utf-8", errors="replace") as f:
                for n, line in enumerate(f, 1):
                    for rid in re.findall(rf"\b({REQ_ID})\b", line):
                        tags.setdefault(rid, []).append(f"{rel}:{n}")
    return tags


def parent_of(rid):
    return rid.rsplit(".", 1)[0] if "." in rid else None


def check_trace(rep, matrix_path=None):
    rep.section("Traceability (SWE.1 requirements <-> SWE.2 modules <-> SWE.5 tests <-> code)")
    reqs, defined = parse_requirements(read(REQ_DOC))
    modules = parse_modules(read(ARCH_DOC))
    registry, tests = parse_tests(read(TEST_FILE))
    tags = scan_code_tags()
    known = lambda rid: rid in reqs

    for rid, lines in sorted(defined.items(), key=lambda kv: sort_key(kv[0])):
        if len(lines) > 1:
            rep.error(f"{REQ_DOC}: {rid} defined {len(lines)} times (lines {', '.join(map(str, lines))})")

    # A module may cite only a requirement of its own (not retired, not a parent group); every one needs a module.
    for rid, mods in sorted(modules.items(), key=lambda kv: sort_key(kv[0])):
        if rid not in defined:
            rep.error(f"{ARCH_DOC} §3.2: {rid} ({mods[0]}) is not an active requirement in {REQ_DOC}")
    for rid in sorted(set(defined) - set(modules), key=sort_key):
        rep.warn(f"{rid} '{reqs[rid]}' has no module in {ARCH_DOC} §3.2")

    seen_tc = {}
    for t in tests:
        where = f"{TEST_FILE}:{t['line']} {t['func']}"
        if not t["tc"]:
            rep.error(f"{where}: docstring does not start with 'SWE.5-TCxx.y:'")
        elif t["tc"] in seen_tc:
            rep.error(f"{where}: duplicate {t['tc']} (also {seen_tc[t['tc']]})")
        else:
            seen_tc[t["tc"]] = t["func"]
        if t["tc"] and t["test_id"] and t["tc"] != t["test_id"]:
            rep.error(f"{where}: docstring {t['tc']} != test_id {t['test_id']}")
        if not t["refs"]:
            rep.error(f"{where}: no requirement reference '(FR-x, ...)' at end of docstring")
        if t["req"] and sorted(t["req"]) != sorted(t["refs"]):
            rep.error(f"{where}: docstring refs {t['refs']} != req {t['req']}")
        for rid in t["refs"]:
            if not known(rid):
                rep.error(f"{where}: {rid} not defined in {REQ_DOC}")
    check_hollow_tests(rep, tests)

    for rid in sorted(set(registry) - set(reqs), key=sort_key):
        if not known(rid):
            rep.error(f"SWE1_REQUIREMENTS has {rid} '{registry[rid]}' but {REQ_DOC} does not")
    for rid, places in sorted(tags.items(), key=lambda kv: sort_key(kv[0])):
        if not known(rid):
            rep.error(f"code tag {rid} at {places[0]} not defined in {REQ_DOC}")

    # Coverage is counted on leaves cited by their own ID; a test citing only the parent (FR-10 for
    # FR-10.1) is reported separately, it does not prove the leaf.
    leaves = [r for r in reqs if not any(o.startswith(r + ".") for o in reqs)]
    test_of = {r: [t["tc"] for t in tests if r in t["refs"]] for r in reqs}
    via_parent = {r: test_of.get(parent_of(r), []) for r in leaves}
    untested = [r for r in leaves if not test_of[r]]
    for rid in sorted(untested, key=sort_key):
        if via_parent[rid]:
            rep.warn(f"{rid} '{reqs[rid]}' only covered via parent {parent_of(rid)} ({len(via_parent[rid])} tests)")
        else:
            rep.warn(f"{rid} '{reqs[rid]}' has no test")
    tagged = sum(1 for r in leaves if r in tags)
    rep.info(f"{len(reqs)} requirement IDs ({len(leaves)} leaves), {len(tests)} tests, {len(tags)} tagged IDs in code")
    rep.info(f"coverage (own ID): tests {len(leaves) - len(untested)}/{len(leaves)}, code tags {tagged}/{len(leaves)}")
    rep.info(f"allocated to a module: {len(set(defined) & set(modules))}/{len(defined)} requirements")

    if matrix_path:
        write_matrix(matrix_path, reqs, modules, test_of, tags)
        rep.info(f"matrix written to {matrix_path}")


def sort_key(rid):
    kind, nums = rid.split("-", 1)
    return (kind, [int(x) for x in nums.split(".")])


def write_matrix(path, reqs, modules, test_of, tags):
    lines = ["# Traceability matrix", "",
             f"Generated by `tools/sh_process_check.py trace --matrix`. "
             f"Source: `{REQ_DOC}`, `{ARCH_DOC}` §3.2, `{TEST_FILE}`.", "",
             "| Requirement | Title | Modules | Tests | Code |", "|---|---|---|---|---|"]
    for rid in sorted(reqs, key=sort_key):
        tests = ", ".join(test_of[rid])
        if not tests and test_of.get(parent_of(rid)):
            tests = f"via {parent_of(rid)}: " + ", ".join(test_of[parent_of(rid)])
        code = sorted({p.split(":")[0] for p in tags.get(rid, [])})
        title = reqs[rid].replace("|", "\\|")
        mods = ", ".join(modules.get(rid, [])) or "—"
        lines.append(f"| {rid} | {title} | {mods} | {tests or '—'} | {', '.join(code) or '—'} |")
    with open(os.path.join(ROOT, path), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


# ---------------------------------------------------------------- requirement rules

def requirement_lines(text):
    """Yield (req id, line number, line, is_definition) for each line that belongs to a requirement: its
    bold-label list item with the indented lines below it, or a table row starting with its ID."""
    current = None
    for n, line in enumerate(text.splitlines(), 1):
        label = re.match(rf"\s*[-*]\s+\*\*({REQ_ID})", line)
        row = re.match(rf"\|\s*\*{{0,2}}({REQ_ID})\b", line)
        if "~~" in line:
            current = None if label else current
            continue
        if row:
            yield row.group(1), n, line, False
            continue
        if label:
            current = label.group(1)
        elif line.strip() and not line.startswith((" ", "\t")):
            current = None
        if current:
            yield current, n, line, True


def nearest_value(kind, line, pos, max_dist=80):
    """The value of `kind` closest to position `pos` on the line, normalized to the base unit, or None."""
    best = None
    for m in VALUE_RE[kind].finditer(line):
        dist = min(abs(m.start() - pos), abs(m.end() - pos))
        if dist <= max_dist and (best is None or dist < best[0]):
            best = (dist, m)
    return format_value(kind, best[1]) if best else None


def format_value(kind, m):
    groups = m.groups()
    scale = TO_BASE.get(groups[2].lower(), 1) if len(groups) > 2 else 1
    nums = [float(g.replace(",", ".")) * scale for g in groups[:2] if g]
    return "-".join(f"{v:g}" for v in nums) + f" {BASE_UNIT[kind]}"


def check_dtcs(rep, text):
    """One meaning per DTC: defined once in the doc, one symbolic name, raised by the firmware."""
    names, defined, mentioned, unnamed = {}, {}, {}, {}
    for n, line in enumerate(text.splitlines(), 1):
        if "~~" in line:
            continue
        codes = DTC_RE.findall(line)
        line_names = DTC_NAME_RE.findall(line)
        for code in codes:
            mentioned.setdefault(code, n)
            if not line_names:
                unnamed.setdefault(code, []).append(n)
        for m in re.finditer(r"`?(DTC-\d+)`?\s*:\s*`((?:ERR|WARN|INFO)_\w+)`", line):
            defined.setdefault(m.group(1), []).append(n)
        if len(set(codes)) == 1:
            for name in line_names:
                names.setdefault(codes[0], {}).setdefault(name, n)
    raised = {}
    for d in ("src", "include"):
        for path in sorted(glob.glob(os.path.join(ROOT, d, "**", "*"), recursive=True)):
            if os.path.isfile(path) and not CODE_SKIP.search(path):
                rel = os.path.relpath(path, ROOT).replace(os.sep, "/")
                with open(path, encoding="utf-8", errors="replace") as f:
                    for n, line in enumerate(f, 1):
                        for code in DTC_RAISE_RE.findall(line):
                            raised.setdefault(code, f"{rel}:{n}")

    for code in sorted(set(mentioned) | set(raised)):
        if len(defined.get(code, [])) > 1:
            rep.warn(f"{code} defined {len(defined[code])} times in {REQ_DOC} (lines {', '.join(map(str, defined[code]))})")
        if len(names.get(code, {})) > 1:
            shown = ", ".join(f"{name} (l.{n})" for name, n in names[code].items())
            rep.warn(f"{code} has {len(names[code])} names in {REQ_DOC}: {shown}")
        if unnamed.get(code):
            rep.warn(f"{code} cited without its name (`ERR_...`/`WARN_...`) at {REQ_DOC} lines "
                     f"{', '.join(map(str, unnamed[code]))} - meaning not checkable")
        if code in mentioned and code not in defined:
            rep.warn(f"{code} used in {REQ_DOC}:{mentioned[code]} but never defined ('`{code}`: `ERR_...`')")
        if code in raised and code not in mentioned:
            rep.warn(f"{code} raised at {raised[code]} but not in {REQ_DOC}")
        elif code in defined and code not in raised:
            rep.warn(f"{code} defined in {REQ_DOC}:{defined[code][0]} but the firmware never raises it")


def check_reqs(rep):
    rep.section("Requirement rules (one value per quantity, measurable wording, one meaning per DTC)")
    text = read(REQ_DOC)
    lines = list(requirement_lines(text))

    # A body line without any quantity keyword takes the quantity named in its requirement's title
    # ("**FR-4 (Anti-Short-Cycling Guard):**" -> "... mai scurt de 3–5 minute").
    any_keyword = re.compile("|".join(f"(?:{k})" for _, _, k in QUANTITIES), re.I)
    title_quantity = {}
    for rid, n, line, is_definition in lines:
        if is_definition and re.match(rf"\s*[-*]\s+\*\*{re.escape(rid)}\b", line):
            hits = [name for name, _, keyword in QUANTITIES if re.search(keyword, line, re.I)]
            if len(hits) == 1:
                title_quantity[rid] = hits[0]
    seen = {name: {} for name, _, _ in QUANTITIES}
    for rid, n, line, is_definition in lines:
        for name, kind, keyword in QUANTITIES:
            values = [nearest_value(kind, line, k.start()) for k in re.finditer(keyword, line, re.I)]
            if not values and is_definition and title_quantity.get(rid) == name and not any_keyword.search(line):
                values = [format_value(kind, m) for m in VALUE_RE[kind].finditer(line)]
            for value in filter(None, values):
                seen[name].setdefault(value, {})[f"{rid}:{n}"] = None
    for name, values in seen.items():
        if len(values) > 1:
            shown = " | ".join(f"{v} ({', '.join(where)})" for v, where in values.items())
            rep.warn(f"{name}: {len(values)} different values - {shown}")

    vague = {}
    for rid, n, line, is_definition in lines:
        if is_definition:
            for word in VAGUE_RE.findall(re.sub(r"`[^`]*`", "", line)):
                vague.setdefault(rid, {}).setdefault(word.lower(), n)
    for rid, words in sorted(vague.items(), key=lambda kv: sort_key(kv[0])):
        rep.warn(f"{REQ_DOC}:{min(words.values())} {rid}: not measurable? vague wording: {', '.join(words)}")

    check_dtcs(rep, text)


# ---------------------------------------------------------------- process

def resolve_base(base):
    for cand in ([base] if base else ["origin/main", "main"]):
        if git("rev-parse", "--verify", "--quiet", cand + "^{commit}", probe=True):
            return cand, git("merge-base", "HEAD", cand).strip()
    return None, None


@functools.lru_cache(maxsize=None)
def param_ranges():
    """{parameter name: Range cell} of the requirements §6 table."""
    text = read(REQ_DOC)
    start = text.index("\n## 6.")
    end = text.find("\n## ", start + 1)
    ranges = {}
    for line in text[start:end if end > 0 else None].splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) == 4:
            for name in re.findall(r"`(\w+)`", cells[0]):
                ranges.setdefault(name, cells[2])
    return ranges


def param_limits(param, unit):
    """Allowed (min, max) of a §6 parameter in `unit` (ms, s or C), or None if §6 gives no such range."""
    kind = "temp" if unit == "C" else "time"
    m = VALUE_RE[kind].search(param_ranges().get(param, ""))
    if not m or not m.group(2):
        return None
    scale = (TO_BASE[m.group(3).lower()] if kind == "time" else 1) * (1000 if unit == "ms" else 1)
    return float(m.group(1).replace(",", ".")) * scale, float(m.group(2).replace(",", ".")) * scale


def limit_rule(param, unit, what):
    """(min, max, rule text) for a safety entry; min/max None when §6 has no range for it."""
    lim = param_limits(param, unit)
    if not lim:
        return None, None, f"{what}: `{param}` has no {unit} range in {REQ_DOC} §6"
    return lim[0], lim[1], f"{what} `{param}` {lim[0]:g}-{lim[1]:g} {unit} ({REQ_DOC} §6)"


def check_safety_limits(rep):
    """One captured group is a single value; two groups are a [min, max] clamp range."""
    for rel, pattern, unit, param, what in SAFETY_LIMITS:
        lo, hi, rule = limit_rule(param, unit, what)
        m = re.search(pattern, read(rel), re.MULTILINE)
        if lo is None:
            rep.error(rule)
        elif not m:
            rep.error(f"{rel}: safety limit not found ({rule}) - pattern needs update?")
        elif float(m.group(1)) < lo or float(m.groups()[-1]) > hi:
            shown = "-".join(f"{float(g):g}" for g in m.groups())
            rep.error(f"{rel}: {' '.join(m.group(0).split())[:80]} -> {shown} {unit} violates {rule}")


def check_build_identity(rep):
    """The board must report the commit it was built from (docs/process.md §6)."""
    if not re.search(r"^extra_scripts\s*=.*\bpre:tools/sh_git_version\.py", read("platformio.ini"), re.MULTILINE):
        rep.error("platformio.ini: extra_scripts misses pre:tools/sh_git_version.py - board cannot report its commit")
    for path in sorted(glob.glob(os.path.join(ROOT, "include", "*.h")) + glob.glob(os.path.join(ROOT, "src", "*"))):
        if CODE_SKIP.search(path):
            continue
        with open(path, encoding="utf-8", errors="replace") as f:
            for n, line in enumerate(f, 1):
                if re.match(r'\s*#define\s+APP_GIT_COMMIT\s+"[0-9a-f]{7,}', line):
                    rel = os.path.relpath(path, ROOT).replace(os.sep, "/")
                    rep.error(f"{rel}:{n}: hard-coded APP_GIT_COMMIT - it must come from git_version.h")


def read_table(rel):
    """Rows of the first Markdown table in a file as [(line number, {lower-case header: cell})]."""
    if not os.path.exists(os.path.join(ROOT, rel)):
        return None
    header, rows = None, []
    for n, line in enumerate(read(rel).splitlines(), 1):
        if not line.startswith("|"):
            if header:
                break
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if header is None:
            header = [c.lower() for c in cells]
        elif not all(re.fullmatch(r":?-+:?", c) for c in cells):
            rows.append((n, dict(zip(header, cells))))
    return rows


def check_problems(rep, release=False):
    """docs/problems.md rows: unique IDs, known status, fix commit for fixed/closed. Returns {id: row}.
    An open safety problem is a WARN, and an ERROR at the release gate."""
    rows = read_table(PROBLEMS)
    if rows is None:
        rep.error(f"{PROBLEMS} missing (docs/process.md §8)")
        return {}
    problems = {}
    for n, row in rows:
        where, pid, status = f"{PROBLEMS}:{n}", row.get("id", ""), row.get("status", "").lower()
        if not re.fullmatch(PB_ID.pattern, pid):
            rep.error(f"{where}: ID '{pid}' is not PB-n")
            continue
        if pid in problems:
            rep.error(f"{where}: {pid} used twice - the first row counts")
            continue
        if status not in PB_STATUS:
            rep.error(f"{where}: {pid} status '{status}' not in {'/'.join(PB_STATUS)}")
        if row.get("safety", "").lower() not in ("yes", "no"):
            rep.error(f"{where}: {pid} Safety must be yes or no")
        if status in ("fixed", "closed") and not re.search(r"\b[0-9a-f]{7,}\b", row.get("fix", "")):
            rep.error(f"{where}: {pid} is {status} but Fix has no commit")
        problems[pid] = row
    open_safety = [p for p, r in problems.items() if r.get("status", "").lower() in PB_OPEN
                   and r.get("safety", "").lower() == "yes"]
    if open_safety:
        (rep.error if release else rep.warn)(f"open safety problems: {', '.join(open_safety)} - they block a release")
    return problems


def check_process(rep, base, strict_size):
    rep.section("Process (AGENTS.md rules)")
    check_safety_limits(rep)
    check_build_identity(rep)
    problems = check_problems(rep)

    if git("ls-files", "include/secrets.h"):
        rep.error("include/secrets.h is tracked by git")

    bins = glob.glob(os.path.join(ROOT, ".pio", "build", "*", "firmware.bin"))
    for b in bins:
        size = os.path.getsize(b)
        msg = f"{os.path.relpath(b, ROOT)} {size / 1048576:.2f} MB (limit {FIRMWARE_MAX_BYTES / 1048576:.2f} MB)"
        if size >= FIRMWARE_MAX_BYTES:
            rep.error(msg)
        else:
            rep.info(msg)
    if not bins:
        (rep.error if strict_size else rep.info)("no firmware.bin - run 'pio run' to check the OTA size limit")

    ref, mb = resolve_base(base)
    if not mb:
        (rep.error if base else rep.warn)(f"base '{base or 'origin/main|main'}' not found - diff checks skipped")
        return
    untracked = git_paths("ls-files", "--others", "--exclude-standard")
    changed = git_paths("diff", "--name-only", mb) | untracked
    commits = [l.split("\x00", 1) for l in git("log", "--no-merges", "--format=%h%x00%s", f"{mb}..HEAD").splitlines()]
    rep.info(f"diff vs {ref} ({mb[:7]}): {len(changed)} files, {len(commits)} commits")
    if not changed and not commits:
        return

    code_changed = sorted(p for p in changed if p.startswith(CODE_PATHS))
    if code_changed:
        for doc in (CHANGELOG, ACTIVE_CONTEXT):
            if doc not in changed:
                rep.error(f"code changed ({code_changed[0]}{' ...' if len(code_changed) > 1 else ''}) but {doc} not updated")

    for sha, subject in commits:
        if not COMMIT_RE.match(subject):
            rep.error(f"commit {sha} '{subject}' - expected 'type(scope): subject', type in {COMMIT_TYPES}")
    for entry in git("log", "--no-merges", "--format=%h%x00%B%x01", f"{mb}..HEAD").split("\x01"):
        sha, _, body = entry.strip().partition("\x00")
        for pid in sorted(set(PB_ID.findall(body)) - set(problems)):
            rep.error(f"commit {sha} cites {pid}, not in {PROBLEMS}")

    ini_diff = git("diff", "-U0", mb, "--", "platformio.ini").splitlines()
    if any(p.startswith("lib/") for p in changed) or any(
            l[:1] in "+-" and not l.startswith(("+++", "---")) and "lib_deps" in l for l in ini_diff):
        rep.warn("library change (lib/ or lib_deps) - needs explicit user confirmation")

    self_rel = os.path.relpath(os.path.abspath(__file__), ROOT).replace(os.sep, "/")
    diff = git("diff", "-U0", mb, "--", *CODE_DIRS, "tools")
    current = None
    for line in diff.splitlines():
        if line.startswith("+++ "):
            current = line[6:] if line.startswith("+++ b/") else None
        elif line.startswith("+") and current and current != self_rel and DEBUG_RE.search(line[1:]):
            rep.warn(f"{current}: debug leftover? {line[1:].strip()[:100]}")
    for path in sorted(untracked):
        if path.startswith(tuple(d + "/" for d in CODE_DIRS) + ("tools/",)) and path != self_rel:
            full = os.path.join(ROOT, path)
            if os.path.isfile(full):
                with open(full, encoding="utf-8", errors="replace") as f:
                    for n, line in enumerate(f, 1):
                        if DEBUG_RE.search(line):
                            rep.warn(f"{path}:{n}: debug leftover? {line.strip()[:100]}")


# ---------------------------------------------------------------- release

def same_firmware(commit, ref="HEAD"):
    """True if the firmware inputs of `commit` and `ref` are identical (docs-only commits in between are fine)."""
    return git("diff", "--quiet", commit, ref, "--", *FIRMWARE_INPUTS, probe=True) is not None


def check_release(rep):
    """Gate before push / PR to main (docs/process.md §5): what goes out is what the owner tested."""
    rep.section("Release gate (acceptance, problems, version)")
    head = firmware_commit(ROOT)
    if head.endswith("-dirty"):
        rep.error(f"uncommitted firmware changes ({head}) - commit, build, flash and test that build")

    rows = read_table(ACCEPTANCE)
    if rows is None:
        rep.error(f"{ACCEPTANCE} missing (docs/process.md §5)")
        rows = []
    accepted = []
    for n, row in rows:
        commit, result = row.get("board commit", ""), row.get("result", "").upper()
        if result not in ("OK", "NOK") or not re.fullmatch(r"[0-9a-f]{7,}(?:-dirty)?", commit):
            rep.error(f"{ACCEPTANCE}:{n}: Board commit must be a hash as the board reports it, Result OK or NOK")
            continue
        if result != "OK" or commit.endswith("-dirty"):
            continue
        if not git("rev-parse", "--verify", "--quiet", commit + "^{commit}", probe=True):
            rep.warn(f"{ACCEPTANCE}:{n}: commit {commit} not in this repository (rebased away?)")
        elif same_firmware(commit):
            accepted.append(f"{commit} ({row.get('date', '?')}, line {n})")
    if accepted:
        rep.info(f"HEAD firmware accepted by the owner: {', '.join(accepted)}")
    else:
        rep.error(f"no OK row in {ACCEPTANCE} for a clean build with the same firmware as HEAD ({head}) "
                  f"- flash HEAD, test on the board, add the row")

    check_problems(rep, release=True)

    m = re.search(r'^\s*#define\s+APP_VERSION\s+"([^"]+)"', read(VERSION_H), re.MULTILINE)
    if not m:
        rep.error(f"{VERSION_H}: APP_VERSION not found")
        return
    tag = f"v{m.group(1)}"
    tagged = git("rev-parse", "--verify", "--quiet", tag + "^{commit}", probe=True)
    if tagged and not same_firmware(tagged.strip()):
        rep.error(f"{tag} already released with different firmware - bump APP_VERSION in {VERSION_H}")
    elif tagged:
        rep.info(f"HEAD firmware is the released {tag}")
    else:
        rep.info(f"after the merge to main: git tag {tag}")


# ---------------------------------------------------------------- board

def http_json(ip, path):
    with urllib.request.urlopen(f"http://{ip}{path}", timeout=5) as r:
        return json.load(r)


def check_board(rep, ip):
    """What runs on the boiler: live config within the safety limits, and the commit it was built from."""
    rep.section(f"Board {ip} (live config vs safety limits)")
    try:
        cfg = http_json(ip, "/api/config/system")
    except (OSError, ValueError) as e:
        rep.error(f"GET /api/config/system failed: {e}")
        return
    for key, unit, param, what in BOARD_LIMITS:
        lo, hi, rule = limit_rule(param, unit, what)
        if lo is None:
            rep.error(rule)
            continue
        if key.startswith("zones[]."):
            field = key.split(".", 1)[1]
            items = [(f"zone {z.get('id', i)} {field}", z.get(field)) for i, z in enumerate(cfg.get("zones", []))]
        else:
            items = [(key, cfg.get(key))]
        for label, val in items:
            if not isinstance(val, (int, float)) or isinstance(val, bool):
                rep.error(f"{label} missing in /api/config/system ({rule}) - API changed?")
            elif val < lo or val > hi:
                rep.error(f"{label} = {val:g} violates {rule}")
            else:
                rep.info(f"{label} = {val:g}")
    for what in BOARD_NOT_EXPOSED:
        rep.warn(f"{what} not reported by the API - cannot be verified on the board")

    try:
        running = str(http_json(ip, "/api/boiler/status").get("gitCommit", ""))
    except (OSError, ValueError) as e:
        rep.warn(f"GET /api/boiler/status failed ({e}) - board build not checked against HEAD")
        return
    local = firmware_commit(ROOT)
    if running.split("-")[0] != local.split("-")[0]:
        rep.warn(f"board runs '{running}', workspace is {local} - board tests verify that build, not this one")
    elif "-dirty" in running or "-dirty" in local:
        rep.warn(f"board runs '{running}', workspace is {local} - uncommitted firmware changes, build not traceable")
    else:
        rep.info(f"board runs {running} = HEAD")


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("check", nargs="?", choices=["all", "trace", "reqs", "process", "board", "release"], default="all")
    ap.add_argument("--ip", default=BOARD_IP, help=f"board address for 'board' (default {BOARD_IP})")
    ap.add_argument("--base", help="git ref to diff against (default: origin/main, then main)")
    ap.add_argument("--matrix", metavar="PATH", nargs="?", const="docs/traceability.md",
                    help="write the traceability matrix as Markdown (default PATH: docs/traceability.md)")
    ap.add_argument("--require-firmware", action="store_true", help="missing firmware.bin is an error (CI)")
    ap.add_argument("--strict", action="store_true", help="warnings fail the run")
    args = ap.parse_args()

    rep = Report()
    if args.check in ("all", "trace"):
        check_trace(rep, args.matrix)
    if args.check in ("all", "reqs"):
        check_reqs(rep)
    if args.check in ("all", "process"):
        try:
            check_process(rep, args.base, args.require_firmware)
        except GitError as e:
            rep.error(f"diff checks aborted: {e}")
    if args.check == "board":
        try:
            check_board(rep, args.ip)
        except GitError as e:
            rep.error(f"board check aborted: {e}")
    if args.check == "release":
        try:
            check_release(rep)
        except GitError as e:
            rep.error(f"release check aborted: {e}")

    failed = rep.errors or (args.strict and rep.warnings)
    print(f"\nResult: {rep.errors} errors, {rep.warnings} warnings -> {'FAIL' if failed else 'PASS'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
