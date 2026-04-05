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

3. Split lambda arguments — rejoins a lambda that clang-format split onto the
   next line from its enclosing call:
       .each(                .each([](int x)
           [](int x)     →   {
       {                         body
           body               });
       });

       Dispatch<Foo>(         Dispatch<Foo>([](Foo& value)
           [](Foo& value) →   {
       {                         body
           body               });
       });

Usage:
    python format-fixup.py [--check] <file>...

    --check   Exit 1 if any file needs changes (CI mode). No files modified.
"""

import argparse
import re
import sys
from pathlib import Path

# Matches a type declaration (struct/class/enum/union) followed by a newline
# then an empty '{}' body.  Handles both \n and \r\n line endings.
EMPTY_BODY = re.compile(
    r'^([^\S\r\n]*(?:struct|class|enum|union)\b[^\r\n{]*)\r?\n(\s*\{\})', re.MULTILINE
)

# Matches a lambda that has been split onto the next line from its enclosing
# call, such as:
#     .each(
#         [capture](params)
# or:
#     Dispatch<Foo>(
#         [capture](params)
# which should become:
#     .each([capture](params)
#     Dispatch<Foo>([capture](params)
LAMBDA_ARG_SPLIT = re.compile(
    r'^([^\S\r\n]*)(.*\()[ \t]*\r?\n[^\S\r\n]+(\[)',
    re.MULTILINE,
)

# Matches "= {" on the same line where the body continues on subsequent lines
# (i.e. NOT "= {}" or "= { single_line }"). Captures the leading whitespace,
# everything before "= {", and the rest after the opening brace.
INIT_BRACE_SAME_LINE = re.compile(
    r'^([^\S\r\n]*)(.*?)[^\S\r\n]*=[^\S\r\n]*\{([^\r\n}]*)\r?$', re.MULTILINE
)


def _fix_init_brace(match: re.Match) -> str:
    """Move '= {' to '=\\n<indent>{' when the body is multi-line."""
    indent = match.group(1)
    decl = match.group(2)
    after_brace = match.group(3)

    # If the content after '{' on this line is empty or only whitespace,
    # the list continues on the next line — move the brace down.
    if after_brace.strip() == '':
        # The regex consumed a trailing \r (via \r?$) but not the \n that
        # follows.  Mirror the \r so both the "=" line and the "{" line
        # keep consistent \r\n endings once the leftover \n is appended.
        cr = '\r' if match.group(0).endswith('\r') else ''
        return f'{indent}{decl} ={cr}\n{indent}{{{cr}'
    # Otherwise it's a single-line init like "= {1, 2}" — leave it.
    return match.group(0)


def fixup(text: str) -> str:
    """Apply all formatting fixups."""
    text = EMPTY_BODY.sub(r'\1 {}', text)
    text = LAMBDA_ARG_SPLIT.sub(r'\1\2\3', text)
    text = INIT_BRACE_SAME_LINE.sub(_fix_init_brace, text)
    return text


def process_file(path: Path, *, check: bool) -> bool:
    """Process a single file. Returns True if changes were made/needed."""
    try:
        original = path.read_text(encoding='utf-8')
    except (UnicodeDecodeError, OSError) as exc:
        print(f'{path}: skipped ({exc})', file=sys.stderr)
        return False
    fixed = fixup(original)
    if original == fixed:
        return False
    if check:
        print(f'{path}: would fix formatting (empty bodies / initialiser braces / lambda call wrapping)')
    else:
        try:
            path.write_text(fixed, encoding='utf-8')
        except OSError as exc:
            print(f'{path}: write failed ({exc})', file=sys.stderr)
            return False
        print(f'{path}: fixed formatting (empty bodies / initialiser braces / lambda call wrapping)')
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

    if changed and args.check:
        print('\nSome files need format-fixup. Run without --check to fix.',
              file=sys.stderr)
    return 1 if changed else 0


if __name__ == '__main__':
    sys.exit(main())
