#!/usr/bin/env python3
"""Install (or remove) the Wayfinder pre-commit hook.

This is a zero-dependency alternative to the `pre-commit` framework.
It writes a Git hook that runs `python tools/lint.py --fix --staged`
before each commit.

Usage:
    python tools/install-hooks.py              # install
    python tools/install-hooks.py --uninstall  # remove
    python tools/install-hooks.py --force      # overwrite existing hook
"""

import argparse
import stat
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
HOOKS_DIR = REPO_ROOT / '.git' / 'hooks'
HOOK_PATH = HOOKS_DIR / 'pre-commit'

HOOK_CONTENT = """\
#!/usr/bin/env bash
# Wayfinder pre-commit hook — installed by tools/install-hooks.py
# Runs clang-format + format-fixup on staged C++ files.

python tools/lint.py --fix --staged
"""

MARKER = 'installed by tools/install-hooks.py'


def install(*, force: bool) -> int:
    if not HOOKS_DIR.exists():
        print(f'Error: {HOOKS_DIR} does not exist. Is this a git repository?',
              file=sys.stderr)
        return 2

    if HOOK_PATH.exists():
        existing = HOOK_PATH.read_text(encoding='utf-8', errors='replace')
        if MARKER in existing:
            print('Hook already installed. Use --force to reinstall.')
            return 0
        if not force:
            print(f'A pre-commit hook already exists at {HOOK_PATH}.',
                  file=sys.stderr)
            print('Use --force to overwrite.', file=sys.stderr)
            return 1

    HOOK_PATH.write_text(HOOK_CONTENT, encoding='utf-8')
    # Make executable (no-op on Windows, but correct for Git Bash).
    HOOK_PATH.chmod(HOOK_PATH.stat().st_mode | stat.S_IEXEC)
    print(f'Installed pre-commit hook at {HOOK_PATH}')
    return 0


def uninstall() -> int:
    if not HOOK_PATH.exists():
        print('No pre-commit hook found.')
        return 0

    existing = HOOK_PATH.read_text(encoding='utf-8', errors='replace')
    if MARKER not in existing:
        print('Existing hook was not installed by this script. Leaving it alone.',
              file=sys.stderr)
        return 1

    HOOK_PATH.unlink()
    print(f'Removed pre-commit hook at {HOOK_PATH}')
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--uninstall', action='store_true',
                        help='Remove the pre-commit hook.')
    parser.add_argument('--force', action='store_true',
                        help='Overwrite an existing hook.')
    args = parser.parse_args()

    if args.uninstall:
        return uninstall()
    return install(force=args.force)


if __name__ == '__main__':
    sys.exit(main())
