#!/usr/bin/env python3
"""Convenience wrapper for running clang-tidy locally.

Requires a build tree with compile_commands.json. Generate one with:
    cmake --preset dev

Usage:
    python tools/tidy.py                        # analyse all engine .cpp files
    python tools/tidy.py --changed              # only files changed vs origin/main
    python tools/tidy.py --fix                  # apply clang-tidy auto-fixes
    python tools/tidy.py engine/wayfinder/src/scene/entity/Entity.cpp  # specific files
    python tools/tidy.py --build-dir build  # use CI/root build tree

Exit codes:
    0  No issues found
    1  clang-tidy reported warnings/errors
    2  Configuration error (missing tool or compile_commands.json)
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

SOURCE_DIRS = ['engine', 'sandbox', 'tests', 'tools']
EXCLUDE_DIRS = {'thirdparty', '_deps', 'build', 'shadercompiler'}

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BUILD_DIRS = (
    REPO_ROOT / 'build' / 'dev',
    REPO_ROOT / 'build',
)


def _supports_colour() -> bool:
    if os.environ.get('NO_COLOR'):
        return False
    if os.environ.get('FORCE_COLOR'):
        return True
    return hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()


_USE_COLOUR = _supports_colour()


def _colour(text: str, code: str) -> str:
    return f'\033[{code}m{text}\033[0m' if _USE_COLOUR else text


def _green(text: str) -> str:
    return _colour(text, '32')


def _red(text: str) -> str:
    return _colour(text, '31')


def _yellow(text: str) -> str:
    return _colour(text, '33')


def _bold(text: str) -> str:
    return _colour(text, '1')


def _find_tidy() -> str | None:
    for name in ('clang-tidy-22', 'clang-tidy'):
        if shutil.which(name):
            return name
    return None


def _compile_commands_candidates(build_dir: Path | None) -> list[Path]:
    if build_dir is None:
        return [candidate / 'compile_commands.json' for candidate in DEFAULT_BUILD_DIRS]
    if build_dir.name == 'compile_commands.json':
        return [build_dir]
    return [build_dir / 'compile_commands.json']


def _resolve_compile_commands(build_dir: Path | None) -> Path | None:
    for candidate in _compile_commands_candidates(build_dir):
        if candidate.exists():
            return candidate
    return None


def _sanitise_compile_arguments(arguments: list[str]) -> list[str]:
    sanitised: list[str] = []
    index = 0
    while index < len(arguments):
        current = arguments[index]

        if (
            current == '-Xclang'
            and index + 2 < len(arguments)
            and arguments[index + 1] in {'-include-pch', '-include'}
            and 'cmake_pch' in arguments[index + 2]
        ):
            index += 3
            continue

        if current in {'-include', '-include-pch'} and index + 1 < len(arguments) and 'cmake_pch' in arguments[index + 1]:
            index += 2
            continue

        if current == '-Winvalid-pch':
            index += 1
            continue

        if current.startswith(('/Yc', '/Yu', '/FI', '/Fp')) and 'cmake_pch' in current:
            index += 1
            continue

        if current.startswith('@') and current.endswith('.modmap'):
            index += 1
            continue

        if (current.startswith('-fmodule-file=') or current.startswith('-fmodule-mapper=')) and current.endswith('.modmap'):
            index += 1
            continue

        sanitised.append(current)
        index += 1

    return sanitised


def _sanitise_compile_command(command: str) -> str:
    pch_arg = r'(?:"[^"]*cmake_pch[^"]*"|\S*cmake_pch\S*)'
    patterns = (
        re.compile(rf'\s-Xclang\s+-include-pch\s+-Xclang\s+{pch_arg}'),
        re.compile(rf'\s-Xclang\s+-include\s+-Xclang\s+{pch_arg}'),
        re.compile(rf'\s-include-pch\s+{pch_arg}'),
        re.compile(rf'\s-include\s+{pch_arg}'),
        re.compile(rf'\s/(?:Yc|Yu|FI|Fp){pch_arg}'),
        re.compile(r'\s-Winvalid-pch'),
        re.compile(r'\s@\S*\.modmap'),
        re.compile(r'\s-fmodule-file=\S*\.modmap'),
        re.compile(r'\s-fmodule-mapper=\S*\.modmap'),
    )

    sanitised = command
    for pattern in patterns:
        sanitised = pattern.sub('', sanitised)

    return sanitised


def _path_regex(path: Path) -> str:
    escaped = re.escape(str(path.resolve()))
    return escaped.replace(r'\\', r'[/\\]')


def _header_filters() -> tuple[str, str]:
    root = _path_regex(REPO_ROOT)
    source_dirs = '|'.join(SOURCE_DIRS)
    exclude_dirs = '|'.join(sorted(EXCLUDE_DIRS))
    include_filter = rf'^{root}[/\\](?:{source_dirs})[/\\]'
    exclude_filter = rf'^{root}[/\\](?:{exclude_dirs})[/\\]'
    return include_filter, exclude_filter


def _is_repo_owned_path(path_text: str) -> bool:
    normalised = Path(path_text.replace('\\', '/'))
    if not normalised.is_absolute():
        normalised = (REPO_ROOT / normalised).resolve()
    else:
        normalised = normalised.resolve()

    try:
        relative = normalised.relative_to(REPO_ROOT.resolve())
    except ValueError:
        return False

    if not relative.parts:
        return False
    return relative.parts[0] in SOURCE_DIRS and not any(part in EXCLUDE_DIRS for part in relative.parts)


def _filter_tidy_output(output: str) -> tuple[bool, str]:
    primary_diag = re.compile(r'^(?P<path>(?:[A-Za-z]:)?[^:]+):\d+:\d+: (?P<kind>warning|error|remark): ')
    note_diag = re.compile(r'^(?P<path>(?:[A-Za-z]:)?[^:]+):\d+:\d+: note: ')

    kept_lines: list[str] = []
    block_lines: list[str] = []
    keep_block = False
    saw_issue = False

    def flush_block() -> None:
        nonlocal block_lines, keep_block
        if keep_block and block_lines:
            kept_lines.extend(block_lines)
        block_lines = []
        keep_block = False

    for line in output.splitlines(keepends=True):
        primary_match = primary_diag.match(line)
        if primary_match:
            flush_block()
            keep_block = _is_repo_owned_path(primary_match.group('path'))
            saw_issue = saw_issue or keep_block
            block_lines = [line]
            continue

        if block_lines:
            if note_diag.match(line) or line.startswith(' ') or line.startswith('\t') or not line.strip() or line.startswith('Error while processing '):
                block_lines.append(line)
                continue

            flush_block()

        kept_lines.append(line)

    flush_block()
    return saw_issue, ''.join(kept_lines)


def _sanitise_compile_entry(entry: dict[str, object]) -> None:
    command = entry.get('command')
    if isinstance(command, str):
        entry['command'] = _sanitise_compile_command(command)

    arguments = entry.get('arguments')
    if isinstance(arguments, list) and all(isinstance(arg, str) for arg in arguments):
        entry['arguments'] = _sanitise_compile_arguments(arguments)


def _discover_all_cpp() -> list[Path]:
    files: list[Path] = []
    for source_dir in SOURCE_DIRS:
        root = REPO_ROOT / source_dir
        if not root.exists():
            continue
        for path in root.rglob('*.cpp'):
            if any(part in EXCLUDE_DIRS for part in path.parts):
                continue
            files.append(path)
    files.sort()
    return files


def _discover_changed_cpp() -> list[Path]:
    result = subprocess.run(
        ['git', 'diff', '--name-only', '--diff-filter=ACMR', 'origin/main...HEAD'],
        capture_output=True, text=True, cwd=REPO_ROOT,
    )
    if result.returncode != 0:
        print(_red(f'git diff failed: {result.stderr.strip()}'), file=sys.stderr)
        return []

    files: list[Path] = []
    for line in result.stdout.strip().splitlines():
        if not line.endswith('.cpp'):
            continue
        if any(excl in line for excl in EXCLUDE_DIRS):
            continue
        if any(line.startswith(d + '/') or line.startswith(d + '\\') for d in SOURCE_DIRS):
            path = REPO_ROOT / line
            if path.exists():
                files.append(path)
    files.sort()
    return files


def _filter_compile_db(compile_commands: Path, *, config: str) -> Path:
    """Filter a Ninja Multi-Config compile_commands.json to a single config.

    Ninja Multi-Config emits entries for every configuration.  Each entry's
    command contains -DCMAKE_INTDIR=\\"<Config>\\" which we use to filter.
    Returns a temp directory containing the filtered compile_commands.json.

    Also strips PCH includes and module-map references that won't exist when
    running without a prior compile of that translation unit.
    """
    with open(compile_commands, 'r', encoding='utf-8') as f:
        entries = json.load(f)

    marker = f'CMAKE_INTDIR=\\\\\\"{config}\\\\\\"'
    filtered = [e for e in entries if marker in e.get('command', '')]

    if not filtered:
        # Fallback: deduplicate by source file (keep first occurrence).
        seen: set[str] = set()
        for entry in entries:
            src = entry.get('file', '')
            if src not in seen:
                seen.add(src)
                filtered.append(entry)

    for entry in filtered:
        _sanitise_compile_entry(entry)

    tmp_dir = Path(tempfile.mkdtemp(prefix='wayfinder-tidy-'))
    out_path = tmp_dir / 'compile_commands.json'
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(filtered, f, indent=2)

    return tmp_dir


def _run_one(cmd_base: list[str], filepath: str, cwd: str) -> tuple[str, int, str]:
    """Run clang-tidy on a single file.  Returns (filepath, returncode, stderr)."""
    cmd = cmd_base + [filepath]
    result = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    # clang-tidy prints diagnostics to stdout.
    output = result.stdout + result.stderr
    had_issue, filtered_output = _filter_tidy_output(output)
    # Treat tool failures (crashes, missing compile DB) as errors even if no
    # repo-owned diagnostics were detected.
    if result.returncode != 0 and not had_issue:
        return filepath, result.returncode, filtered_output or output
    return filepath, 1 if had_issue else 0, filtered_output


def run_tidy(
    files: list[Path],
    *,
    build_dir: Path | None = None,
    tool: str | None = None,
    fix: bool = False,
    jobs: int = 0,
) -> bool:
    """Run clang-tidy on *files*.  Returns True if issues were found.

    This is the shared core used by both ``tidy.py`` (CLI) and ``lint.py``
    (``--tidy`` flag).  Handles compile-DB filtering, PCH workarounds, and
    ``--quiet`` automatically.

    *jobs* controls parallelism: 0 = auto (cpu_count), 1 = sequential.
    When *fix* is True, jobs is forced to 1 to avoid concurrent writes.
    """
    # Resolve tool.
    if tool is None:
        tool = _find_tidy()
    if not tool:
        print(_red('clang-tidy not found. Install clang-tidy-22 or add it to PATH.'), file=sys.stderr)
        return False  # caller decides severity

    # Validate compile_commands.json.
    compile_commands = _resolve_compile_commands(build_dir)
    if compile_commands is None:
        searched = ', '.join(str(candidate) for candidate in _compile_commands_candidates(build_dir))
        print(_red(f'compile_commands.json not found. Looked in: {searched}'), file=sys.stderr)
        print(_yellow('Generate it with: cmake --preset dev'), file=sys.stderr)
        return False

    # Filter to .cpp — headers are analysed via includes.
    # Also drop files that no longer exist (e.g. deleted in the current branch).
    cpp_files = [f for f in files if f.suffix == '.cpp' and f.exists()]
    if not cpp_files:
        print(_yellow('No .cpp files to analyse.'))
        return False

    # Ninja Multi-Config generates entries for every configuration (Debug,
    # Development, Shipping).  Without filtering, clang-tidy processes each
    # file 3x.  Write a filtered DB with only Debug entries to a temp dir.
    filtered_dir = _filter_compile_db(compile_commands, config='Debug')

    # When applying fixes, run sequentially to avoid concurrent file writes.
    if fix:
        jobs = 1
    elif jobs <= 0:
        jobs = os.cpu_count() or 4

    print(_bold(f'Running clang-tidy on {len(cpp_files)} file(s) ({jobs} parallel)...'))

    cmd_base = [tool, '-p', str(filtered_dir), '--quiet']
    header_filter, exclude_filter = _header_filters()
    cmd_base.extend(['--header-filter', header_filter, '--exclude-header-filter', exclude_filter])
    # Suppress stale-PCH errors when running standalone (PCH might be from
    # a previous build and the source has since been modified).
    cmd_base.extend(['--extra-arg=-Xclang', '--extra-arg=-fno-validate-pch'])
    if fix:
        cmd_base.append('--fix')

    found_issues = False
    cwd = str(REPO_ROOT)

    if jobs == 1:
        # Sequential — simpler output, needed for --fix.
        for i, f in enumerate(cpp_files, 1):
            filepath, rc, output = _run_one(cmd_base, str(f), cwd)
            if output.strip():
                print(output, end='' if output.endswith('\n') else '\n')
            if rc != 0:
                found_issues = True
    else:
        # Parallel — fan out across cores.
        with ThreadPoolExecutor(max_workers=jobs) as pool:
            futures = {
                pool.submit(_run_one, cmd_base, str(f), cwd): f
                for f in cpp_files
            }
            done = 0
            for future in as_completed(futures):
                done += 1
                filepath, rc, output = future.result()
                if output.strip():
                    print(output, end='' if output.endswith('\n') else '\n')
                if rc != 0:
                    found_issues = True

    print()
    if found_issues:
        print(_red(f'clang-tidy reported issues ({len(cpp_files)} files analysed).'))
    else:
        print(_green(f'Clean ({len(cpp_files)} files analysed).'))

    return found_issues


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Run clang-tidy on Wayfinder source files.',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument('files', nargs='*', type=Path,
                        help='Specific .cpp files to analyse. If omitted, scans all engine sources.')
    parser.add_argument('--changed', action='store_true',
                        help='Only analyse .cpp files changed vs origin/main.')
    parser.add_argument('--fix', action='store_true',
                        help='Apply clang-tidy auto-fixes in-place.')
    parser.add_argument('-j', '--jobs', type=int, default=0,
                        help='Number of parallel jobs (0 = auto, 1 = sequential). Forced to 1 with --fix.')
    parser.add_argument('--build-dir', type=Path, default=None,
                        help='Path to build directory with compile_commands.json.')
    args = parser.parse_args()

    # Discover files.
    if args.files:
        # Expand globs (PowerShell doesn't expand wildcards for us).
        expanded: list[Path] = []
        for pattern in args.files:
            if '*' in str(pattern) or '?' in str(pattern):
                expanded.extend(REPO_ROOT.glob(str(pattern)))
            else:
                expanded.append(pattern)
        files = [f.resolve() for f in expanded if f.suffix == '.cpp']
    elif args.changed:
        files = _discover_changed_cpp()
    else:
        files = _discover_all_cpp()

    if not files:
        print(_yellow('No .cpp files to analyse.'))
        return 0

    found_issues = run_tidy(files, build_dir=args.build_dir, fix=args.fix, jobs=args.jobs)
    return 1 if found_issues else 0


if __name__ == '__main__':
    sys.exit(main())
