#!/usr/bin/env python3
"""Reproduce CI checks locally.

Usage:
    python tools/ci-local.py              # lint + build + test
    python tools/ci-local.py --fix        # auto-fix format issues first
    python tools/ci-local.py --skip-build # lint only (fast)
    python tools/ci-local.py --tidy       # include clang-tidy analysis

Exit codes:
    0  All checks passed
    1  One or more checks failed
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LINT_SCRIPT = ROOT / 'tools' / 'lint.py'

# The dev-clang preset mirrors CI's Clang + Ninja setup on Windows.
# On Linux/macOS, "dev" would also work — but dev-clang catches more warnings.
CONFIGURE_PRESET = 'dev-clang'
BUILD_PRESET = 'clang-debug'
TEST_PRESET = 'clang-test'


def _banner(msg: str) -> None:
    width = max(len(msg) + 4, 60)
    print(f'\n{"=" * width}')
    print(f'  {msg}')
    print(f'{"=" * width}\n')


def _run(cmd: list[str], *, cwd: Path = ROOT) -> int:
    """Run a command and return its exit code."""
    print(f'> {" ".join(cmd)}')
    result = subprocess.run(cmd, cwd=cwd)
    return result.returncode


def run_lint(*, fix: bool, tidy: bool) -> int:
    """Run tools/lint.py with the requested flags."""
    _banner('Lint & Format Check')
    cmd = [sys.executable, str(LINT_SCRIPT)]
    if fix:
        cmd.append('--fix')
    if tidy:
        cmd.append('--tidy')
    return _run(cmd)


def run_configure() -> int:
    _banner(f'Configure ({CONFIGURE_PRESET})')
    return _run(['cmake', '--preset', CONFIGURE_PRESET])


def run_build() -> int:
    _banner(f'Build ({BUILD_PRESET})')
    return _run(['cmake', '--build', '--preset', BUILD_PRESET])


def run_test() -> int:
    _banner(f'Test ({TEST_PRESET})')
    return _run(['ctest', '--preset', TEST_PRESET, '--output-on-failure'])


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument('--fix', action='store_true',
                        help='Auto-fix format issues before checking')
    parser.add_argument('--skip-build', action='store_true',
                        help='Run lint checks only (skip configure/build/test)')
    parser.add_argument('--tidy', action='store_true',
                        help='Include clang-tidy analysis')
    args = parser.parse_args()

    failed: list[str] = []

    rc = run_lint(fix=args.fix, tidy=args.tidy)
    if rc != 0:
        failed.append('lint')

    if not args.skip_build:
        rc = run_configure()
        if rc != 0:
            failed.append('configure')
        else:
            rc = run_build()
            if rc != 0:
                failed.append('build')
            else:
                rc = run_test()
                if rc != 0:
                    failed.append('test')

    if failed:
        _banner(f'FAILED: {", ".join(failed)}')
        return 1

    _banner('All checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
