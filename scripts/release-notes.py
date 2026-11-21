#!/usr/bin/env python3
"""Validate a release tag and extract its changelog section."""

import argparse
from pathlib import Path
import re


def release_notes(changelog, version, prerelease=False):
    release = r"[0-9]{4}\.[0-9]+\.[0-9]+(?:-rc[0-9]+)?"
    pattern = release + r"-[0-9a-f]{7,40}" if prerelease else release
    if not re.fullmatch(pattern, version):
        raise ValueError(f"Invalid version tag: {version}")

    section = "unreleased" if prerelease else version
    matches = []
    for entry in re.split(r"(?m)^##[ \t]+", changelog)[1:]:
        heading, _, body = entry.partition("\n")
        name = re.match(r"\[([^\]]+)\]|(\S+)", heading)
        if name and (name[1] or name[2]).casefold() == section:
            matches.append(body.strip())

    if len(matches) != 1 or not matches[0]:
        raise ValueError(f"CHANGELOG.md must contain one non-empty [{section}] section")
    return matches[0] + "\n", prerelease


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", nargs="?", help="Defaults to VERSION")
    parser.add_argument("--changelog", type=Path, default=Path("CHANGELOG.md"))
    parser.add_argument("--notes-file", type=Path, default=Path("release-notes.md"))
    parser.add_argument("--github-output", type=Path)
    parser.add_argument("--prerelease", action="store_true", help="Use the [Unreleased] section")
    args = parser.parse_args()
    try:
        version = args.version or (Path(__file__).resolve().parents[1] / "VERSION").read_text(encoding="utf-8").strip()
        notes, prerelease = release_notes(args.changelog.read_text(encoding="utf-8"), version, args.prerelease)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    args.notes_file.write_text(notes, encoding="utf-8")
    if args.github_output:
        with args.github_output.open("a", encoding="utf-8") as output:
            output.write(f"version={version}\nprerelease={str(prerelease).lower()}\n")


if __name__ == "__main__":
    main()
