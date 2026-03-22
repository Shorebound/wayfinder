#!/usr/bin/env python3
"""Post-clang-format fixup for Wayfinder.

clang-format with Allman braces + SplitEmptyRecord: false produces:

    struct MyStruct
    {}

This script collapses empty bodies onto the declaration line:

    struct MyStruct {}

Usage:
    python format-fixup.py [--check] <file>...

    --check   Exit 1 if any file needs changes (CI mode). No files modified.
"""

import argparse
import re
import sys
from pathlib import Path

# Matches a type/function declaration followed by a newline then empty braces.
# Captures: everything up to the newline, then the empty braces line.
EMPTY_BODY = re.compile(
    r'^([^\S\n]*(?:struct|class|enum|union)\b[^\n{]*)\n(\s*\{\})', re.MULTILINE
)


def fixup(text: str) -> str:
    """Collapse empty type bodies onto the declaration line."""
    return EMPTY_BODY.sub(r'\1 {}', text)


def process_file(path: Path, *, check: bool) -> bool:
    """Process a single file. Returns True if changes were made/needed."""
    original = path.read_text(encoding='utf-8')
    fixed = fixup(original)
    if original == fixed:
        return False
    if check:
        print(f'{path}: would fix empty body formatting')
    else:
        path.write_text(fixed, encoding='utf-8')
        print(f'{path}: fixed empty body formatting')
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true',
                        help='Check mode — report but do not modify files.')
    parser.add_argument('files', nargs='+', type=Path,
                        help='Source files to process.')
    args = parser.parse_args()

    changed = False
    for path in args.files:
        if not path.is_file():
            print(f'{path}: not found, skipping', file=sys.stderr)
            continue
        if process_file(path, check=args.check):
            changed = True

    if args.check and changed:
        print('\nSome files need format-fixup. Run without --check to fix.',
              file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
