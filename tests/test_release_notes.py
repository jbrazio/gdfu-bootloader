#!/usr/bin/env python3
"""Run with: python3 tests/test_release_notes.py"""

from pathlib import Path
import runpy
import unittest

release = runpy.run_path(
    str(Path(__file__).resolve().parents[1] / "scripts/release-notes.py")
)
release_notes = release["release_notes"]

CHANGELOG = """# Changelog

## [UNRELEASED]

### Added

- Preview change.

## [2026.11.1] - 2026-11-12

### Fixed

- Stable change.

## [2026.10.1]

- Old change.
"""


class ReleaseNotesTest(unittest.TestCase):
    def test_development_version(self):
        for version in ("2026.11.1-rc2-abcdef1", "2026.11.1-abcdef1"):
            self.assertEqual(release_notes(CHANGELOG, version, True), ("### Added\n\n- Preview change.\n", True))
        for version in ("gdfu-abcdef1", "2026.11.1-rc2-abcdef1-dirty", "2026.11.1"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                release_notes(CHANGELOG, version, True)

    def test_stable_accepts_release_candidate_section(self):
        self.assertEqual(release_notes("## [2026.11.1-rc2]\n\nRC.\n", "2026.11.1-rc2"), ("RC.\n", False))

    def test_stable_accepts_full_version_heading(self):
        self.assertEqual(
            release_notes("## [2026.11.1]\n\n- Initial release.\n", "2026.11.1"),
            ("- Initial release.\n", False),
        )

    def test_stable_uses_matching_version(self):
        self.assertEqual(
            release_notes(CHANGELOG, "2026.11.1"),
            ("### Fixed\n\n- Stable change.\n", False),
        )

    def test_prerelease_uses_unreleased_case_insensitively(self):
        self.assertEqual(
            release_notes(CHANGELOG, "2026.11.1-rc1-abcdef1", True),
            ("### Added\n\n- Preview change.\n", True),
        )
        self.assertEqual(
            release_notes(CHANGELOG.replace("UNRELEASED", "Unreleased"), "2026.11.2-rc1-abcdef1", True)[1],
            True,
        )

    def test_unbracketed_heading(self):
        self.assertEqual(release_notes("## 2026.11.1\n\nNotes.\n", "2026.11.1"), ("Notes.\n", False))

    def test_invalid_tags_fail(self):
        for tag in ("v2026.11.1", "2026.11", "2026.11.1+42", "2026.11.1-abcdef1", "2026.11.1+", "2026.11.1-extra", "$(id)"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                release_notes(CHANGELOG, tag)

    def test_missing_empty_and_duplicate_sections_fail(self):
        for changelog, tag, *pre in (
            (CHANGELOG, "2027.1.1"),
            ("## [2026.11.1]\n\n", "2026.11.1"),
            ("## [Unreleased]\n\n## [2026.11.1]\nNotes.\n", "2026.11.1-abcdef1", True),
            ("## [2026.11.1]\nFirst.\n## [2026.11.1]\nSecond.\n", "2026.11.1"),
        ):
            with self.subTest(changelog=changelog, tag=tag), self.assertRaises(ValueError):
                release_notes(changelog, tag, *pre)


if __name__ == "__main__":
    unittest.main()
