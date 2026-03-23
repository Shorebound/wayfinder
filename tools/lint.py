#!/usr/bin/env python3
"""Unified lint runner for Wayfinder.

Runs clang-format, format-fixup.py, and optionally clang-tidy on engine
source files. Designed for local development and pre-commit hook use.

Usage:
    python tools/lint.py                    # check all source files
    python tools/lint.py --fix              # format in-place
    python tools/lint.py --changed          # only files changed vs main
    python tools/lint.py --staged           # only staged files (pre-commit)
    python tools/lint.py --tidy             # also run clang-tidy
    python tools/lint.py --build-dir PATH   # override compile_commands.json location

Exit codes:
    0  All checks passed
    1  Issues found (or fixed in --fix mode)
    2  Configuration error (missing tool, missing compile_commands.json)
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

# Directories to scan for source files.
SOURCE_DIRS = ['engine', 'sandbox', 'tests', 'tools']
SOURCE_EXTENSIONS = {'.cpp', '.h'}
EXCLUDE_DIRS = {'thirdparty', '_deps', 'build', 'shadercompiler'}

# Direct includes of these headers are banned — use the wrapper instead.
# Each entry: (regex matching the banned include, allowed wrapper file, replacement include).
BANNED_INCLUDES: list[tuple[re.Pattern[str], Path, str]] = [
    (
        re.compile(r'^\s*#\s*include\s*<flecs\.h>'),
        Path('engine/wayfinder/src/ecs/Flecs.h'),
        '"ecs/Flecs.h"',
    ),
]

REPO_ROOT = Path(__file__).resolve().parent.parent
FIXUP_SCRIPT = REPO_ROOT / 'tools' / 'format-fixup.py'
DEFAULT_BUILD_DIR = REPO_ROOT / 'build' / 'clang'


def _supports_colour() -> bool:
    """Check if the terminal supports colour output."""
    if os.environ.get('NO_COLOR'):
        return False
    if os.environ.get('FORCE_COLOR'):
        return True
    return hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()


_USE_COLOUR = _supports_colour()


def _colour(text: str, code: str) -> str:
    if _USE_COLOUR:
        return f'\033[{code}m{text}\033[0m'
    return text


def _green(text: str) -> str:
    return _colour(text, '32')


def _red(text: str) -> str:
    return _colour(text, '31')


def _yellow(text: str) -> str:
    return _colour(text, '33')


def _bold(text: str) -> str:
    return _colour(text, '1')


def _find_tool(name: str) -> str | None:
    """Find a tool, preferring the versioned -20 variant."""
    versioned = f'{name}-20'
    if shutil.which(versioned):
        return versioned
    if shutil.which(name):
        return name
    return None


def _discover_all_files() -> list[Path]:
    """Find all C++ source files in the project."""
    files: list[Path] = []
    for source_dir in SOURCE_DIRS:
        root = REPO_ROOT / source_dir
        if not root.exists():
            continue
        for path in root.rglob('*'):
            if path.suffix not in SOURCE_EXTENSIONS:
                continue
            if any(part in EXCLUDE_DIRS for part in path.parts):
                continue
            files.append(path)
    files.sort()
    return files


def _discover_changed_files(staged_only: bool = False) -> list[Path]:
    """Find C++ files changed vs main (or staged only)."""
    cmd = ['git', 'diff', '--name-only', '--diff-filter=ACMR']
    if staged_only:
        cmd.append('--cached')
    else:
        cmd.extend(['origin/main...HEAD'])

    result = subprocess.run(
        cmd, capture_output=True, text=True, cwd=REPO_ROOT
    )
    if result.returncode != 0:
        print(_red(f'git diff failed: {result.stderr.strip()}'), file=sys.stderr)
        return []

    files: list[Path] = []
    for line in result.stdout.strip().splitlines():
        path = REPO_ROOT / line
        if path.suffix not in SOURCE_EXTENSIONS:
            continue
        if any(part in EXCLUDE_DIRS for part in Path(line).parts):
            continue
        if any(line.startswith(d + '/') or line.startswith(d + '\\') for d in SOURCE_DIRS):
            if path.exists():
                files.append(path)
    files.sort()
    return files


def _run_format_fix(files: list[Path], *, tool: str) -> bool:
    """Fix mode: run clang-format -i then format-fixup in-place.

    Returns True if any file was changed.
    """
    if not files:
        return False

    # Snapshot file contents to detect changes.
    originals: dict[Path, str] = {}
    for path in files:
        try:
            originals[path] = path.read_text(encoding='utf-8')
        except (UnicodeDecodeError, OSError):
            pass

    print(_bold(f'\n-- formatting ({len(files)} files) --'))

    cmd = [tool, '-i']
    cmd.extend(str(f) for f in files)
    subprocess.run(cmd, cwd=REPO_ROOT)

    cmd = [sys.executable, str(FIXUP_SCRIPT)]
    cmd.extend(str(f) for f in files)
    subprocess.run(cmd, cwd=REPO_ROOT)

    any_changed = False
    for path, before in originals.items():
        try:
            if path.read_text(encoding='utf-8') != before:
                any_changed = True
                break
        except (UnicodeDecodeError, OSError):
            pass

    if any_changed:
        print(_yellow('  Applied fixes.'))
    else:
        print(_green('  Clean.'))

    return any_changed


def _run_format_check(files: list[Path], *, tool: str) -> bool:
    """Check mode: verify clang-format + format-fixup produce no changes.

    Returns True if issues were found.
    """
    if not files:
        return False

    print(_bold(f'\n-- formatting ({len(files)} files) --'))

    dirty: list[Path] = []
    for path in files:
        try:
            original_text = path.read_text(encoding='utf-8')
        except (UnicodeDecodeError, OSError):
            continue

        result = subprocess.run(
            [tool, f'--assume-filename={path}'],
            input=path.read_bytes(), capture_output=True, cwd=REPO_ROOT,
        )
        if result.returncode != 0:
            dirty.append(path)
            continue

        cf_text = result.stdout.decode('utf-8', errors='replace')
        if '\r\n' not in original_text and '\r\n' in cf_text:
            cf_text = cf_text.replace('\r\n', '\n')

        formatted_text = _apply_fixup(cf_text)

        if formatted_text != original_text:
            dirty.append(path)

    if dirty:
        for path in dirty:
            print(_red(f'  {path.relative_to(REPO_ROOT)}: needs formatting'))
        print(_red(f'\n  {len(dirty)} file(s) need formatting.'))
        return True

    print(_green('  Clean.'))
    return False


# Cache the fixup function so we only import the module once.
_fixup_fn = None


def _apply_fixup(text: str) -> str:
    """Apply format-fixup.py transformations to *text* in-memory."""
    global _fixup_fn
    if _fixup_fn is None:
        from importlib.util import spec_from_file_location, module_from_spec
        spec = spec_from_file_location('format_fixup', FIXUP_SCRIPT)
        mod = module_from_spec(spec)
        spec.loader.exec_module(mod)
        _fixup_fn = mod.fixup
    return _fixup_fn(text)


def _run_clang_tidy(files: list[Path], *, build_dir: Path, tool: str) -> bool:
    """Run clang-tidy. Returns True if issues were found."""
    if not files:
        return False

    compile_commands = build_dir / 'compile_commands.json'
    if not compile_commands.exists():
        print(_red(f'\n  compile_commands.json not found at {compile_commands}'), file=sys.stderr)
        print(_yellow('  Generate it with: cmake --preset dev-clang'), file=sys.stderr)
        sys.exit(2)

    # Filter to .cpp files only — headers are analysed via includes.
    cpp_files = [f for f in files if f.suffix == '.cpp']
    if not cpp_files:
        print(_bold('\n-- clang-tidy --'))
        print(_yellow('  No .cpp files to analyse.'))
        return False

    print(_bold(f'\n-- clang-tidy ({len(cpp_files)} files) --'))

    cmd = [tool, '-p', str(build_dir)]
    cmd.extend(str(f) for f in cpp_files)

    result = subprocess.run(cmd, cwd=REPO_ROOT)
    if result.returncode != 0:
        print(_red('  clang-tidy reported issues.'))
        return True

    print(_green('  Clean.'))
    return False


def _check_banned_includes(files: list[Path]) -> bool:
    """Check for banned direct includes. Returns True if violations found."""
    if not files or not BANNED_INCLUDES:
        return False

    print(_bold(f'\n-- banned includes ({len(files)} files) --'))

    violations: list[str] = []
    for path in files:
        rel = path.relative_to(REPO_ROOT)
        for pattern, allowed_file, replacement in BANNED_INCLUDES:
            if rel == allowed_file:
                continue
            try:
                for line_no, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
                    if pattern.search(line):
                        violations.append(
                            f'  {rel}:{line_no}: use #include {replacement} instead'
                        )
            except OSError:
                pass

    if violations:
        for v in violations:
            print(_red(v))
        return True

    print(_green('  Clean.'))
    return False


def _restage_files(files: list[Path]) -> None:
    """Re-stage files that were modified by --fix."""
    cmd = ['git', 'add']
    cmd.extend(str(f) for f in files)
    subprocess.run(cmd, cwd=REPO_ROOT, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Unified lint runner for Wayfinder.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument('--fix', action='store_true',
                        help='Fix issues in-place instead of just checking.')
    parser.add_argument('--tidy', action='store_true',
                        help='Also run clang-tidy (requires compile_commands.json).')
    parser.add_argument('--changed', action='store_true',
                        help='Only check files changed vs origin/main.')
    parser.add_argument('--staged', action='store_true',
                        help='Only check staged files (for pre-commit hooks).')
    parser.add_argument('--build-dir', type=Path, default=DEFAULT_BUILD_DIR,
                        help='Path to build directory with compile_commands.json.')
    args = parser.parse_args()

    if args.changed and args.staged:
        print(_red('Cannot use both --changed and --staged.'), file=sys.stderr)
        return 2

    # Find tools.
    clang_format = _find_tool('clang-format')
    if not clang_format:
        print(_red('clang-format not found. Install clang-format-20.'), file=sys.stderr)
        return 2

    clang_tidy = None
    if args.tidy:
        clang_tidy = _find_tool('clang-tidy')
        if not clang_tidy:
            print(_red('clang-tidy not found. Install clang-tidy-20.'), file=sys.stderr)
            return 2

    # Discover files.
    if args.staged:
        files = _discover_changed_files(staged_only=True)
    elif args.changed:
        files = _discover_changed_files(staged_only=False)
    else:
        files = _discover_all_files()

    if not files:
        print(_yellow('No files to check.'))
        return 0

    print(_bold(f'Checking {len(files)} file(s)...'))

    issues = False
    issues |= _check_banned_includes(files)
    if args.fix:
        issues |= _run_format_fix(files, tool=clang_format)
    else:
        issues |= _run_format_check(files, tool=clang_format)

    if args.tidy and clang_tidy:
        issues |= _run_clang_tidy(files, build_dir=args.build_dir, tool=clang_tidy)

    # Re-stage files if we fixed staged files.
    if args.fix and args.staged and issues:
        _restage_files(files)

    print()
    if issues:
        if args.fix:
            print(_yellow('Issues were fixed. Review and re-commit.'))
        else:
            print(_red('Issues found. Run with --fix to auto-format.'))
        return 1

    print(_green('All checks passed.'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
