"""Shared version computation for Smart Heating firmware and web UI.

Format: v1.<month>.<day>.<daily_revision> (e.g. v1.10.27.1)
"""
import os
import subprocess
import datetime


def get_version_info(project_dir):
    def git(*args):
        try:
            return subprocess.check_output(
                ["git", *args],
                cwd=project_dir,
                text=True,
                stderr=subprocess.DEVNULL,
            ).strip()
        except Exception:
            return ""

    now = datetime.datetime.now()
    month = now.month
    day = now.day
    today_str = f"{now.year}-{month:02d}-{day:02d} 00:00"

    commits_today_str = git("rev-list", "--count", f"--since={today_str}", "HEAD")
    commits_today = int(commits_today_str) if commits_today_str.isdigit() else 0
    total_commits_str = git("rev-list", "--count", "HEAD")
    total_commits = int(total_commits_str) if total_commits_str.isdigit() else 0

    is_dirty = bool(git("status", "--porcelain", "--untracked-files=no"))
    commit_hash = git("rev-parse", "--short", "HEAD") or "unknown"
    if is_dirty:
        commit_hash += "-dirty"

    daily_rev = max(1, commits_today + (1 if is_dirty else 0))
    version_str = f"v1.{month}.{day}.{daily_rev}"

    return {
        "version": version_str,
        "daily_rev": daily_rev,
        "total_commits": total_commits,
        "commit": commit_hash,
        "build_date": now.strftime("%b %d %Y"),
        "build_time": now.strftime("%H:%M:%S"),
    }

