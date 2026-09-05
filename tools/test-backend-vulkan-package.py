#!/usr/bin/env python3
"""Host-independent metaflux-backend-vulkan package construction gate.

Builds the package with packaging/build.py --kind backend-vulkan from a
generic release build tree and re-verifies the produced artifacts. Skips
with exit 77 when no generic tree with the packaged shared backend exists;
container install rows live in tests/release/run_backend_vulkan_package_rows.py.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parent.parent
BUILD_SCRIPT = REPOSITORY / "packaging" / "build.py"
DEFAULT_GENERIC_TREE = (
    REPOSITORY / "tmp" / "build" / "generic-release"
)
SHARED_LIBRARY_NAME = "libmetaflux_vulkan_backend.so"
ALLOWED_NEEDED = frozenset(
    {
        "ld-linux-x86-64.so.2",
        "libc.so.6",
        "libm.so.6",
        "libdl.so.2",
        "libpthread.so.0",
        "librt.so.1",
        "libvulkan.so.1",
    }
)
GLIBC_SYMBOL_PATTERN = re.compile(r"\bGLIBC_(\d+)\.(\d+)\b")


def skip(message: str) -> int:
    print(f"SKIP: {message}", file=sys.stderr)
    return 77


def run(command: list[str]) -> str:
    result = subprocess.run(command, capture_output=True, text=True)
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"{result.stdout}\n{result.stderr}"
        )
    return result.stdout


def locate_shared_library(tree: Path) -> Path | None:
    matches = sorted(tree.rglob(SHARED_LIBRARY_NAME))
    return matches[0] if matches else None


def readelf_needed(readelf: str, path: Path) -> set[str]:
    dynamic = run([readelf, "-d", "-W", str(path)])
    return set(re.findall(r"\(NEEDED\).*?\[([^]]+)\]", dynamic))


def highest_glibc(readelf: str, path: Path) -> str | None:
    versions = [
        (int(major), int(minor))
        for major, minor in GLIBC_SYMBOL_PATTERN.findall(
            run([readelf, "--version-info", "-W", str(path)])
        )
    ]
    if not versions:
        return None
    major, minor = max(versions)
    return f"GLIBC_{major}.{minor}"


def main() -> int:
    if shutil.which("rpmbuild") is None:
        nix = shutil.which("nix")
        if nix is None:
            return skip("rpmbuild and nix are unavailable; cannot build rpm")
        result = subprocess.run(
            [nix, "develop", ".#release", "--command", "python3", "-B",
             str(Path(__file__).resolve()), *sys.argv[1:]],
            cwd=str(REPOSITORY),
        )
        return result.returncode

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=None)
    arguments = parser.parse_args()

    tree = Path(os.environ.get("METAFLUX_GENERIC_BUILD_DIR", "") or DEFAULT_GENERIC_TREE)
    if not (tree / "CMakeCache.txt").is_file():
        return skip(
            f"no generic release build tree at {tree}; run tools/build-generic-release.sh"
        )
    if locate_shared_library(tree) is None:
        return skip(
            f"generic tree {tree} has no packaged shared Vulkan backend "
            "(METAFLUX_VULKAN_BACKEND_SHARED=ON)"
        )

    readelf = os.environ.get("METAFLUX_READELF", "readelf")
    with tempfile.TemporaryDirectory(prefix="metaflux-backend-vulkan-pkg-") as tmp:
        output_dir = Path(tmp) / "packages"
        run(
            [
                sys.executable,
                "-B",
                str(BUILD_SCRIPT),
                "--build-dir",
                str(tree),
                "--output-dir",
                str(output_dir),
                "--kind",
                "backend-vulkan",
            ]
        )
        deb = next(output_dir.glob("metaflux-backend-vulkan_*_amd64.deb"))
        rpm = next(output_dir.glob("metaflux-backend-vulkan-*.x86_64.rpm"))
        tarball = next(output_dir.glob("metaflux-backend-vulkan-*.tar.gz"))

        control = run(["dpkg-deb", "--info", str(deb)])
        assert "Package: metaflux-backend-vulkan" in control, control
        assert "libvulkan1" in control, control
        contents = run(["dpkg-deb", "--contents", str(deb)])
        for token in (
            SHARED_LIBRARY_NAME,
            "metaflux/backend/vulkan.h",
            "metaflux/backend/api.h",
            "metaflux-backend-vulkan/README.md",
        ):
            assert token in contents, f"deb is missing {token}: {contents}"

        rpm_requires = run(["rpm", "-qp", "--requires", str(rpm)])
        assert "vulkan-loader" in rpm_requires, rpm_requires
        rpm_contents = run(["rpm", "-qpl", str(rpm)])
        assert SHARED_LIBRARY_NAME in rpm_contents, rpm_contents

        tar_members = run(["tar", "-tzf", str(tarball)])
        assert SHARED_LIBRARY_NAME in tar_members, tar_members

        extract = Path(tmp) / "deb-data"
        extract.mkdir()
        fs_tar = Path(tmp) / "data.tar"
        with fs_tar.open("wb") as sink:
            subprocess.run(
                ["dpkg-deb", "--fsys-tarfile", str(deb)],
                check=True,
                stdout=sink,
            )
        subprocess.run(
            ["tar", "-xf", str(fs_tar), "-C", str(extract)],
            check=True,
        )
        library = extract / "usr/lib/metaflux/backends" / SHARED_LIBRARY_NAME
        needed = readelf_needed(readelf, library)
        unexpected = sorted(needed - ALLOWED_NEEDED)
        assert not unexpected, f"unexpected NEEDED entries: {unexpected}"
        assert "libstdc++.so.6" not in needed, needed
        highest = highest_glibc(readelf, library)
        if highest is not None:
            major, minor = (int(part) for part in highest.removeprefix("GLIBC_").split("."))
            assert (major, minor) <= (2, 31), highest

        report = {
            "gate": "backend-vulkan-package",
            "status": "passed",
            "generic_tree": str(tree),
            "deb": {"path": deb.name, "size": deb.stat().st_size},
            "rpm": {"path": rpm.name, "size": rpm.stat().st_size},
            "tar": {"path": tarball.name, "size": tarball.stat().st_size},
            "shared_library_needed": sorted(needed),
            "highest_glibc_symbol": highest,
        }
        if arguments.output is not None:
            arguments.output.parent.mkdir(parents=True, exist_ok=True)
            arguments.output.write_text(
                json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
            )
        print(json.dumps({"status": "ok", "gate": "backend-vulkan-package"}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"test-backend-vulkan-package: error: {error}", file=sys.stderr)
        raise SystemExit(1)
