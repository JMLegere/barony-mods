"""Black-box contracts for pre-signing target-readiness admission.

A signing/notarization boundary must consume a typed, digest-bound capability
receipt rather than infer readiness from a local build or arbitrary verifier
log.  A native target probe may contribute only its canonical target identity,
adapter status, and artifact digest; exact-build evidence remains a separate
release gate.  These tests execute only fake tool shims and never sign,
notarize, upload, or publish a real artifact.
"""
from __future__ import annotations

from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from typing import Any


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
RELEASE_EVIDENCE = REPOSITORY_ROOT / "packaging" / "release_evidence.py"
MACOS_BUILD = REPOSITORY_ROOT / "packaging" / "macos" / "build.sh"
WINDOWS_BUILD = REPOSITORY_ROOT / "packaging" / "windows" / "build.ps1"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.write_text(json.dumps(value, sort_keys=True) + "\n", encoding="utf-8")


def receipt_for(
    *, operating_system: str, architecture: str, subject: Path, exact_evidence: Path
) -> dict[str, Any]:
    """Return the smallest authenticatable receipt accepted at the signing boundary.

    ``adapterStatus`` is intentionally constrained to status 0 (ready) or 3
    (not yet configured) by the intended validator.  The Windows/macOS MVP
    stubs return status 22 (target facts unavailable), so their genuine probe
    output cannot be upgraded into verified readiness.  The receipt binds an
    already-produced exact-evidence file but does not replace validate-exact.
    """

    target_id = f"{operating_system}-{architecture}"
    return {
        "formatVersion": "1.0.0",
        "kind": "target-readiness",
        "target": {"os": operating_system, "architecture": architecture},
        "availability": "available",
        "status": "verified",
        "source": "licensed-self-hosted",
        "exactEvidence": {"path": exact_evidence.name, "sha256": sha256(exact_evidence)},
        "probe": {
            "targetId": target_id,
            "adapterStatus": 3,
            "artifactSha256": sha256(subject),
        },
        "subjects": [{"path": subject.name, "sha256": sha256(subject)}],
    }


class TargetReadinessReceiptContracts(unittest.TestCase):
    """Contracts for the public release-evidence CLI admission boundary."""

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="bml-target-readiness-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.subject = self.root / "barony_bml.dll"
        self.subject.write_bytes(b"licensed target runtime bytes\n")
        self.macos_subjects = {
            "x86_64": self.root / "libbarony_bml.x86_64.dylib",
            "arm64": self.root / "libbarony_bml.arm64.dylib",
        }
        for architecture, subject in self.macos_subjects.items():
            subject.write_bytes(f"licensed macOS {architecture} runtime bytes\n".encode("ascii"))
        self.exact_evidence = self.root / "exact-build.json"
        self.exact_evidence.write_bytes(b"previously validated exact evidence\n")
        self.receipt = self.root / "target-readiness.json"

    def validate(self, receipt: dict[str, Any], *, operating_system: str = "windows",
                 architecture: str = "x86_64", subject: Path | None = None) -> subprocess.CompletedProcess[str]:
        bound_subject = subject or self.subject
        write_json(self.receipt, receipt)
        return subprocess.run(
            [
                os.fspath(RELEASE_EVIDENCE),
                "validate-target-readiness",
                "--input", os.fspath(self.receipt),
                "--release-root", os.fspath(self.root),
                "--os", operating_system,
                "--architecture", architecture,
                "--subject", os.fspath(bound_subject),
            ],
            cwd=REPOSITORY_ROOT,
            text=True,
            capture_output=True,
            check=False,
        )

    def test_only_a_target_matched_available_and_digest_bound_receipt_admits_signing(self) -> None:
        """Signing admission rejects unavailable, cross-target, stale, and local-placeholder claims."""

        valid = receipt_for(
            operating_system="windows",
            architecture="x86_64",
            subject=self.subject,
            exact_evidence=self.exact_evidence,
        )
        unavailable = deepcopy(valid)
        unavailable["availability"] = "unavailable"

        stub_probe = deepcopy(valid)
        stub_probe["probe"]["adapterStatus"] = 22

        cross_target = deepcopy(valid)
        cross_target["target"] = {"os": "macos", "architecture": "x86_64"}

        stale_digest = deepcopy(valid)
        stale_digest["subjects"][0]["sha256"] = "0" * 64

        stale_exact_evidence = deepcopy(valid)
        stale_exact_evidence["exactEvidence"]["sha256"] = "f" * 64

        local_placeholder = {
            "formatVersion": "1.0.0",
            "kind": "target-readiness",
            "target": {"os": "windows", "architecture": "x86_64"},
            "availability": "available",
            "status": "verified",
        }

        cases = (
            ("verified licensed target", valid, True),
            ("unavailable target facts", unavailable, False),
            ("Windows MVP adapter status target_facts_unavailable", stub_probe, False),
            ("receipt for another target", cross_target, False),
            ("receipt with stale subject digest", stale_digest, False),
            ("receipt with stale exact-evidence digest", stale_exact_evidence, False),
            ("local placeholder without exact evidence or probe", local_placeholder, False),
        )
        rejected: list[str] = []
        admitted: list[str] = []
        for name, document, should_admit in cases:
            with self.subTest(case=name):
                result = self.validate(document)
                if should_admit and result.returncode != 0:
                    rejected.append(f"{name}: {result.stderr}")
                if not should_admit and result.returncode == 0:
                    admitted.append(name)

        self.assertEqual([], rejected, "a complete verified readiness receipt must admit signing")
        self.assertEqual([], admitted, "unavailable, mismatched, stale, or placeholder receipts admitted signing")

    def test_macos_thin_targets_each_require_their_own_verified_receipt(self) -> None:
        """macOS x86_64 and arm64 identities cannot share one target capability claim."""

        rejected: list[str] = []
        for architecture, subject in self.macos_subjects.items():
            with self.subTest(architecture=architecture):
                receipt = receipt_for(
                    operating_system="macos",
                    architecture=architecture,
                    subject=subject,
                    exact_evidence=self.exact_evidence,
                )
                result = self.validate(
                    receipt,
                    operating_system="macos",
                    architecture=architecture,
                    subject=subject,
                )
                if result.returncode != 0:
                    rejected.append(f"{architecture}: {result.stderr}")
        self.assertEqual([], rejected, "each macOS thin target needs a distinct verified readiness receipt")


class PreSigningPackagingBoundaryTests(unittest.TestCase):
    """Packaging runs through fake tools and must stop before any signing action."""

    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="bml-pre-signing-boundary-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.command_log = self.root / "commands.log"

    def fake_tool(self, name: str, body: str = "exit 0") -> None:
        path = self.bin / name
        path.write_text("#!/usr/bin/env bash\nset -eu\n" + body + "\n", encoding="utf-8")
        path.chmod(0o755)

    def environment(self, **overrides: str) -> dict[str, str]:
        return {
            **os.environ,
            "PATH": f"{self.bin}{os.pathsep}{os.environ.get('PATH', '')}",
            "BML_COMMAND_LOG": os.fspath(self.command_log),
            **overrides,
        }

    def signing_or_notarization_calls(self) -> tuple[str, ...]:
        calls = self.command_log.read_text(encoding="utf-8") if self.command_log.exists() else ""
        forbidden = frozenset({"signtool", "codesign", "notarytool", "stapler"})
        return tuple(command for command in calls.splitlines() if command in forbidden)

    def test_macos_build_rejects_missing_readiness_receipt_before_codesign_or_notarytool(self) -> None:
        """An unavailable macOS target cannot reach codesign, notarytool, or stapler via local packaging."""

        dylib_x86 = self.root / "runtime-x86_64.dylib"
        dylib_arm = self.root / "runtime-arm64.dylib"
        dylib_x86.write_bytes(b"x86 runtime\n")
        dylib_arm.write_bytes(b"arm runtime\n")
        output = self.root / "output"

        self.fake_tool(
            "pyinstaller",
            """
dist=""
while [ "$#" -gt 0 ]; do
  if [ "$1" = "--distpath" ]; then dist="$2"; shift 2; continue; fi
  shift
done
mkdir -p "$dist/BaronyModLoader.app/Contents/MacOS"
printf 'gui' > "$dist/BaronyModLoader.app/Contents/MacOS/BaronyModLoader"
""",
        )
        self.fake_tool(
            "lipo",
            """
if [ "$1" = "-create" ]; then
  input="$2"
  while [ "$#" -gt 0 ]; do
    if [ "$1" = "-output" ]; then cp "$input" "$2"; exit 0; fi
    shift
  done
fi
exit 0
""",
        )
        for tool in ("codesign", "xcrun", "ditto", "spctl", "xattr", "shasum"):
            self.fake_tool(tool, f'printf "{tool}\\n" >> "$BML_COMMAND_LOG"\nexit 0')

        result = subprocess.run(
            [os.fspath(MACOS_BUILD)],
            cwd=REPOSITORY_ROOT,
            env=self.environment(
                SOURCE_DATE_EPOCH="1700000000",
                BML_MACOS_CODESIGN_IDENTITY="test identity",
                BML_MACOS_X86_64_DYLIB=os.fspath(dylib_x86),
                BML_MACOS_ARM64_DYLIB=os.fspath(dylib_arm),
                BML_APPLE_ID="test@example.invalid",
                BML_APPLE_TEAM_ID="TESTTEAM",
                BML_APPLE_APP_PASSWORD="not-a-secret",
                BML_PACKAGE_OUT=os.fspath(output),
            ),
            text=True,
            capture_output=True,
            check=False,
        )

        failures: list[str] = []
        if result.returncode == 0:
            failures.append("packaging accepted missing target readiness")
        sensitive_calls = self.signing_or_notarization_calls()
        if sensitive_calls:
            failures.append(
                "target readiness was not rejected before signing/notarization: "
                + ", ".join(sensitive_calls)
            )
        self.assertEqual([], failures)

    @unittest.skipUnless(shutil.which("pwsh"), "Windows packaging boundary requires PowerShell")
    def test_windows_build_rejects_missing_readiness_receipt_before_signtool(self) -> None:
        """An unavailable Windows target cannot reach Authenticode through local packaging."""

        native_root = self.root / "native"
        native_root.mkdir()
        (native_root / "barony_bml.dll").write_bytes(b"Windows runtime\n")
        (native_root / "bml-win-launcher.exe").write_bytes(b"Windows launcher\n")
        output = self.root / "output"

        self.fake_tool(
            "pyinstaller",
            """
dist=""
while [ "$#" -gt 0 ]; do
  if [ "$1" = "--distpath" ]; then dist="$2"; shift 2; continue; fi
  shift
done
mkdir -p "$dist/BaronyModLoader"
printf 'gui' > "$dist/BaronyModLoader/BaronyModLoader.exe"
""",
        )
        self.fake_tool("signtool", 'printf "signtool\\n" >> "$BML_COMMAND_LOG"\nexit 0')
        self.fake_tool("tar", "exit 0")
        self.fake_tool("Start-MpScan", "exit 0")
        self.fake_tool("Get-MpThreatDetection", "exit 0")

        result = subprocess.run(
            ["pwsh", "-NoProfile", "-File", os.fspath(WINDOWS_BUILD)],
            cwd=REPOSITORY_ROOT,
            env=self.environment(
                SOURCE_DATE_EPOCH="1700000000",
                BML_WINDOWS_CERTIFICATE_PFX="fake.pfx",
                BML_WINDOWS_CERTIFICATE_PASSWORD="not-a-secret",
                BML_WINDOWS_TIMESTAMP_URL="https://timestamp.invalid",
                BML_WINDOWS_NATIVE_ROOT=os.fspath(native_root),
                BML_PACKAGE_OUT=os.fspath(output),
            ),
            text=True,
            capture_output=True,
            check=False,
        )

        failures: list[str] = []
        if result.returncode == 0:
            failures.append("packaging accepted missing target readiness")
        sensitive_calls = self.signing_or_notarization_calls()
        if sensitive_calls:
            failures.append("target readiness was not rejected before Authenticode")
        self.assertEqual([], failures)


if __name__ == "__main__":
    unittest.main()
