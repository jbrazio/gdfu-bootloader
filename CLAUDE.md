# CLAUDE.md

## Changelog

Always update `CHANGELOG.md` with the latest changes **before every commit** (including amends). Add them under `## [Unreleased]` in the matching `### Added` / `### Changed` / `### Fixed` section. The release workflow publishes the changelog section as the GitHub release body, so an empty entry fails the build (`scripts/release-notes.py`). Keep entries short and user-facing.

The changelog is for people flashing and running the bootloader, not a git log: only record changes a user would notice (boards, radio/DFU behaviour, protocol, fixes). Never add KTLO or CI entries (pipelines, build/release scripts, tooling, vendored binaries, refactors, dependency bumps, repo housekeeping). Merge related changes into one entry rather than listing each tweak. If a commit has no user-facing change, don't add an entry.

## Git

- `master` is protected: never commit or push to it directly.
- Do all new work in a git worktree on a new branch created from `dev`.
- Commits reach `dev` only through a PR from that branch. Never commit or push directly to `dev`.
- Workflow: worktree branch → PR → `dev` → PR → `master`.
- Open a PR to `master` only when the user asks for one.
