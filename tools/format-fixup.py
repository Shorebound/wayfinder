#!/usr/bin/env python3
"""Post-clang-format fixup for Wayfinder.

Handles formatting patterns that clang-format cannot enforce:

1. Empty type bodies — collapses onto one line:
       struct MyStruct       struct MyStruct {}
       {}                →

2. Initialiser list braces — moves opening brace to new line (Allman):
       auto x = {            auto x =
           1, 2,         →   {
       };                        1, 2,
                              };

Usage:
    python format-fixup.py [--check] <file>...

    --check   Exit 1 if any file needs changes (CI mode). No files modified.
"""

import argparse
import re
import sys
from pathlib import Path

# Matches a type/function declaration followed by a newline then empty braces.
EMPTY_BODY = re.compile(
    r'^([^\S\n]*(?:struct|class|enum|union)\b[^\n{]*)\n(\s*\{\})', re.MULTILINE
)

# Matches "= {" on the same line where the body continues on subsequent lines
# (i.e. NOT "= {}" or "= { single_line }"). Captures the leading whitespace,
# everything before "= {", and the rest after the opening brace.
INIT_BRACE_SAME_LINE = re.compile(
    r'^([^\S\n]*)(.*?)\s*=\s*\{([^\n}]*)$', re.MULTILINE
)


def _fix_init_brace(match: re.Match) -> str:
    """Move '= {' to '=\\n<indent>{' when the body is multi-line."""
    indent = match.group(1)
    decl = match.group(2)
    after_brace = match.group(3)

    # If the content after '{' on this line is empty or only whitespace,
    # the list continues on the next line — move the brace down.
    if after_brace.strip() == '':
        return f'{indent}{decl} =\n{indent}{{'
    # Otherwise it's a single-line init like "= {1, 2}" — leave it.
    return match.group(0)


def fixup(text: str) -> str:
    """Apply all formatting fixups."""
    text = EMPTY_BODY.sub(r'\1 {}', text)
    text = INIT_BRACE_SAME_LINE.sub(_fix_init_brace, text)
    return text


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
