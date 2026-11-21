#!/usr/bin/env python3
"""Run with: python3 tests/test_ci_gdfu.py (requires bash, jq and PyYAML: pip3 install pyyaml)."""

import json
import os
import re
from pathlib import Path
import subprocess
import tempfile
import textwrap

import yaml

root = Path(__file__).resolve().parents[1]
workflow = (root / ".github/workflows/githubci.yml").read_text()
steps = yaml.safe_load(workflow)["jobs"]["set-matrix"]["steps"]
script = next((step["run"] for step in steps if step.get("id") == "set-matrix"), None)
assert script is not None, "no step with id 'set-matrix' in the set-matrix job"
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / "output"
    subprocess.run(
        ["bash", "-eo", "pipefail", "-c", textwrap.dedent(script)],
        cwd=root / "src/boards",
        env={**os.environ, "GITHUB_OUTPUT": str(output)},
        check=True,
    )
    boards = json.loads(output.read_text().removeprefix("matrix="))

# Re-derived in Python with the same rule as the CI grep and tools/build_all.py.
expected = sorted(
    mk.parent.name
    for mk in (root / "src/boards").glob("*/board.mk")
    if re.search(r"^\s*CFLAGS\s*\+=\s*-DGFSK_DFU_ENABLED=1\s*$", mk.read_text(), re.M)
)
assert boards == expected, (boards, expected)
assert boards, "no GDFU boards selected"
for board in boards:
    assert "#define SX1262_CS_PIN" in (root / "src/boards" / board / "board.h").read_text(), board
assert 'args=("BOARD=$BOARD" "GFSK_DFU_ENABLED=1")' in workflow
assert 'for profile in 1 2; do' in workflow
assert 'make "${args[@]}" "GDFU_PROFILE=$profile" all' in workflow
assert 'make "${args[@]}" "GDFU_PROFILE=$profile" copy-artifact' in workflow
jobs = yaml.safe_load(workflow)["jobs"]
gate = next(step["run"] for step in jobs["set-matrix"]["steps"] if step.get("id") == "version")


def decide(branch, version, tags, event="push"):
    """Run the CI gate with fake gh/git/build-version; return (publish, version)."""
    with tempfile.TemporaryDirectory() as directory:
        directory = Path(directory)
        (directory / "VERSION").write_text(version + "\n")
        for name, body in (("gh", "printf '%s\\n' $TAGS"), ("git", "for t in $TAGS; do printf 'x\\trefs/tags/%s\\n' \"$t\"; done"),
                           ("python3", "[[ \"$1\" == *build-version.py ]] && echo \"$(cat VERSION)$([[ $GDFU_BUILD_BRANCH == dev ]] && echo -abc1234)\"; true")):
            (directory / name).write_text(f"#!/bin/bash\n{body}\n")
            (directory / name).chmod(0o755)
        output = directory / "output"
        subprocess.run(["bash", "-c", gate], cwd=directory, check=True, capture_output=True, env={
            **os.environ, "PATH": f"{directory}:{os.environ['PATH']}", "GITHUB_OUTPUT": str(output),
            "TAGS": " ".join(tags), "GDFU_BUILD_BRANCH": branch, "EVENT_NAME": event})
        values = dict(line.split("=", 1) for line in output.read_text().splitlines())
        return values["publish"] == "true", values["version"]


assert decide("dev", "2026.11.1-rc2", []) == (True, "2026.11.1-rc2-abc1234")
assert decide("dev", "2026.11.1-rc2", ["v2026.11.1-rc2-fff0000"]) == (False, "2026.11.1-rc2-abc1234")
assert decide("dev", "2026.11.1-rc2", ["v2026.11.1-rc20-fff0000", "v2026.11.1-rc1-fff0000"])[0]
assert not decide("dev", "2026.11.1", [])[0]
assert decide("master", "2026.11.1", ["v2026.11.1-rc2-fff0000"]) == (True, "2026.11.1")
assert not decide("master", "2026.11.1", ["v2026.11.1"])[0]
assert not decide("master", "2026.11.1-rc2", [])[0]
assert not decide("feature", "2026.11.1", [])[0]
assert decide("master", "2026.11.1", ["v2026.11.1"], "workflow_dispatch")[0]
assert decide("dev", "2026.11.1-rc2", ["v2026.11.1-rc2-abc1234"], "workflow_dispatch")[0]
assert jobs["build"]["if"] == "github.event_name == 'pull_request' || needs.set-matrix.outputs.publish == 'true'"
assert jobs["release"]["if"] == "needs.set-matrix.outputs.publish == 'true'"
assert jobs["release"]["env"]["RELEASE_TAG"] == "v${{ needs.set-matrix.outputs.version }}"
print(f"PASS: CI selects {len(boards)} GDFU boards and enables GDFU builds")
