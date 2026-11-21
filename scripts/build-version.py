#!/usr/bin/env python3
"""Derive a build version from the manually maintained VERSION file."""

import argparse
import os
from pathlib import Path
import re
import subprocess


def build_version(root: Path, version: str, branch: str | None = None) -> str:
    if not re.fullmatch(r"[0-9]{4}\.[0-9]+\.[0-9]+(?:-rc[0-9]+)?", version):
        raise ValueError(f"Invalid version: {version}")

    def git(*args):
        return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()

    branch = branch if branch is not None else git('rev-parse', '--abbrev-ref', 'HEAD')
    if branch == 'dev':
        version += f"-{git('rev-parse', '--short', 'HEAD')}"
    if git('status', '--porcelain', '--untracked-files=normal'):
        version += '-dirty'
    return version


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('version', nargs='?')
    parser.add_argument('--branch', default=os.environ.get('GDFU_BUILD_BRANCH'))
    parser.add_argument('--header', type=Path)
    parser.add_argument('--softdevice', default='')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    try:
        version = args.version if args.version is not None else (root / 'VERSION').read_text().strip()
        version = build_version(root, version, args.branch)
        if args.header:
            content = f'#define UF2_VERSION_BASE "{version}"\n#define UF2_VERSION UF2_VERSION_BASE\n'
            content += f'#define BLEDIS_FW_VERSION UF2_VERSION_BASE \" {args.softdevice}\"\n'
            if not args.header.exists() or args.header.read_text() != content:
                args.header.write_text(content)
        else:
            print(version)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'build-version: {error}\n')


if __name__ == '__main__':
    main()
