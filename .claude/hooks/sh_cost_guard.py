#!/usr/bin/env python3
"""Claude Code hook wrapper for tools/sh_cost_guard.py (owner, 07.11.2026)."""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TARGET = os.path.join(ROOT, "tools", "sh_cost_guard.py")

if __name__ == "__main__":
    args = [sys.executable, TARGET] + sys.argv[1:]
    proc = subprocess.run(args)
    sys.exit(proc.returncode)
