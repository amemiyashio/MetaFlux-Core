#!/usr/bin/env python3
"""Verify the signed Ubuntu archive chain for the generic target SDK."""

from __future__ import annotations

import argparse
import hashlib
import json
import lzma
from pathlib import Path
import subprocess
import tempfile
from typing import Any


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--sdk-manifest", required=True, type=Path)
    parser.add_argument("--keyring", required=True, type=Path)
    parser.add_argument("--gpgv", required=True, type=Path)
    parser.add_argument("--dpkg-deb", required=True, type=Path)
    parser.add_argument("--inrelease", action="append", default=[])
    parser.add_argument("--index", action="append", default=[])
    parser.add_argument("--deb", action="append", default=[])
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def fail(message: str) -> None:
    raise ValueError(message)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def assignments(values: list[str], label: str) -> dict[str, Path]:
    result: dict[str, Path] = {}
    for value in values:
        name, separator, raw_path = value.partition("=")
        if separator == "" or not name or not raw_path:
            fail(f"invalid {label} assignment: {value}")
        if name in result:
            fail(f"duplicate {label} assignment: {name}")
        path = Path(raw_path).resolve(strict=True)
        if not path.is_file():
            fail(f"{label} input is not a regular file: {path}")
        result[name] = path
    return result


def verify_file(path: Path, record: dict[str, Any], label: str) -> None:
    observed_size = path.stat().st_size
    if observed_size != record["size"]:
        fail(f"{label} size mismatch: {observed_size} != {record['size']}")
    observed_hash = sha256(path)
    if observed_hash != record["sha256"]:
        fail(f"{label} SHA256 mismatch: {observed_hash} != {record['sha256']}")


def parse_sdk_manifest(path: Path) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line_number, raw_line in enumerate(path.read_text(encoding="ascii").splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue
        key, separator, value = line.partition("=")
        if separator == "" or not key or not value:
            fail(f"invalid target SDK manifest line {line_number}: {raw_line}")
        if key in fields:
            fail(f"duplicate target SDK manifest key: {key}")
        fields[key] = value
    return fields


def verify_sdk_manifest(manifest: dict[str, Any], fields: dict[str, str]) -> None:
    required = {
        "distribution": manifest["distribution"],
        "architecture": "x86_64",
        "provenance-origin": manifest["canonical_archive_origin"] + "/",
    }
    for key, expected in required.items():
        if fields.get(key) != expected:
            fail(f"target SDK manifest {key} mismatch: {fields.get(key)!r} != {expected!r}")
    qualification_keys = sorted(key for key in fields if key.startswith("qualification-"))
    if qualification_keys:
        fail("default target SDK manifest contains qualification-only mirror metadata")

    expected_packages = {package["name"] for package in manifest["packages"]}
    observed_packages = {
        key[len("package.") : -len(".path")]
        for key in fields
        if key.startswith("package.") and key.endswith(".path")
    }
    if observed_packages != expected_packages:
        fail(
            "target SDK package set mismatch: "
            f"{sorted(observed_packages)} != {sorted(expected_packages)}"
        )
    for package in manifest["packages"]:
        prefix = f"package.{package['name']}"
        if fields.get(prefix + ".path") != package["filename"]:
            fail(f"target SDK path mismatch for {package['name']}")
        if fields.get(prefix + ".hash") != package["nix_sri_sha256"]:
            fail(f"target SDK Nix hash mismatch for {package['name']}")


def verify_inrelease(
    gpgv: Path,
    keyring: Path,
    inrelease: Path,
    expected_signers: set[str],
) -> str:
    with tempfile.TemporaryDirectory(prefix="metaflux-ubuntu-release-") as temporary:
        release_path = Path(temporary) / "Release"
        process = subprocess.run(
            [
                str(gpgv),
                "--status-fd=1",
                "--keyring",
                str(keyring),
                "--output",
                str(release_path),
                str(inrelease),
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        if process.returncode != 0:
            fail(f"gpgv rejected {inrelease}: {process.stderr.strip()}")
        signers = {
            fields[2]
            for line in process.stdout.splitlines()
            if line.startswith("[GNUPG:] VALIDSIG ")
            for fields in [line.split()]
        }
        if signers != expected_signers:
            fail(f"unexpected valid signer set for {inrelease}: {sorted(signers)}")
        return release_path.read_text(encoding="utf-8")


def release_sha256_entries(release_text: str) -> dict[str, tuple[str, int]]:
    entries: dict[str, tuple[str, int]] = {}
    in_sha256 = False
    for line in release_text.splitlines():
        if line == "SHA256:":
            in_sha256 = True
            continue
        if not in_sha256:
            continue
        if not line.startswith(" "):
            break
        fields = line.split()
        if len(fields) != 3:
            fail(f"invalid Release SHA256 entry: {line}")
        digest, size_text, relative_path = fields
        if relative_path in entries:
            fail(f"duplicate Release SHA256 path: {relative_path}")
        entries[relative_path] = (digest, int(size_text))
    if not entries:
        fail("signed Release file has no SHA256 entries")
    return entries


def package_records(path: Path) -> list[dict[str, str]]:
    with lzma.open(path, mode="rt", encoding="utf-8") as source:
        text = source.read()
    records: list[dict[str, str]] = []
    for paragraph in text.split("\n\n"):
        if not paragraph.strip():
            continue
        fields: dict[str, str] = {}
        for line in paragraph.splitlines():
            if line.startswith((" ", "\t")):
                continue
            key, separator, value = line.partition(": ")
            if separator:
                fields[key] = value
        records.append(fields)
    return records


def dpkg_field(dpkg_deb: Path, package_path: Path, field: str) -> str:
    process = subprocess.run(
        [str(dpkg_deb), "--field", str(package_path), field],
        check=False,
        capture_output=True,
        text=True,
    )
    if process.returncode != 0:
        fail(f"dpkg-deb failed for {package_path}: {process.stderr.strip()}")
    return process.stdout.strip()


def main() -> int:
    arguments = parse_arguments()
    manifest_path = arguments.manifest.resolve(strict=True)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        fail("unsupported Ubuntu target SDK provenance schema")
    expected_origin = f"https://snapshot.ubuntu.com/ubuntu/{manifest['snapshot_id']}"
    if manifest.get("snapshot_origin") != expected_origin:
        fail("snapshot origin is not the canonical timestamp-addressed Ubuntu service")

    inrelease_inputs = assignments(arguments.inrelease, "InRelease")
    index_inputs = assignments(arguments.index, "Packages index")
    deb_inputs = assignments(arguments.deb, "DEB")
    release_ids = {release["id"] for release in manifest["releases"]}
    index_records = {
        index["id"]: (release, index)
        for release in manifest["releases"]
        for index in release["indexes"]
    }
    package_specs = {package["name"]: package for package in manifest["packages"]}
    if set(inrelease_inputs) != release_ids:
        fail("InRelease input set does not match the manifest")
    if set(index_inputs) != set(index_records):
        fail("Packages index input set does not match the manifest")
    if set(deb_inputs) != set(package_specs):
        fail("DEB input set does not match the manifest")

    keyring = arguments.keyring.resolve(strict=True)
    verify_file(keyring, manifest["archive_keyring"], "Ubuntu archive keyring")
    expected_signers = set(manifest["archive_keyring"]["required_valid_signer_fingerprints"])
    parsed_indexes: dict[str, list[dict[str, str]]] = {}
    observed_signers: set[str] = set()
    for release in manifest["releases"]:
        release_id = release["id"]
        inrelease = inrelease_inputs[release_id]
        verify_file(inrelease, release["inrelease"], f"{release_id} InRelease")
        release_text = verify_inrelease(
            arguments.gpgv.resolve(strict=True), keyring, inrelease, expected_signers
        )
        observed_signers.update(expected_signers)
        signed_entries = release_sha256_entries(release_text)
        for index in release["indexes"]:
            index_path = index_inputs[index["id"]]
            verify_file(index_path, index, f"{index['id']} Packages index")
            signed_path = index["relative_path"]
            expected_entry = (index["sha256"], index["size"])
            if signed_entries.get(signed_path) != expected_entry:
                fail(f"signed Release entry mismatch for {index['id']}")
            parsed_indexes[index["id"]] = package_records(index_path)

    sdk_fields = parse_sdk_manifest(arguments.sdk_manifest.resolve(strict=True))
    verify_sdk_manifest(manifest, sdk_fields)
    for package_name, package in package_specs.items():
        matches = [
            record
            for record in parsed_indexes[package["index_id"]]
            if record.get("Package") == package_name
            and record.get("Version") == package["version"]
            and record.get("Architecture") == package["architecture"]
        ]
        if len(matches) != 1:
            fail(f"expected one signed Packages stanza for {package_name}, found {len(matches)}")
        stanza = matches[0]
        expected_fields = {
            "Filename": package["filename"],
            "Size": str(package["size"]),
            "SHA256": package["sha256"],
        }
        for field, expected in expected_fields.items():
            if stanza.get(field) != expected:
                fail(f"signed {field} mismatch for {package_name}")
        deb_path = deb_inputs[package_name]
        verify_file(deb_path, package, f"{package_name} DEB")
        for field, expected in (
            ("Package", package_name),
            ("Version", package["version"]),
            ("Architecture", package["architecture"]),
        ):
            observed = dpkg_field(arguments.dpkg_deb.resolve(strict=True), deb_path, field)
            if observed != expected:
                fail(f"DEB {field} mismatch for {package_name}: {observed!r} != {expected!r}")

    report = {
        "schema_version": 1,
        "status": "passed",
        "snapshot_id": manifest["snapshot_id"],
        "snapshot_origin": manifest["snapshot_origin"],
        "manifest_sha256": sha256(manifest_path),
        "sdk_manifest_sha256": sha256(arguments.sdk_manifest.resolve(strict=True)),
        "archive_keyring_sha256": sha256(keyring),
        "valid_signer_fingerprints": sorted(observed_signers),
        "release_count": len(release_ids),
        "index_count": len(index_records),
        "package_count": len(package_specs),
    }
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="ascii"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
