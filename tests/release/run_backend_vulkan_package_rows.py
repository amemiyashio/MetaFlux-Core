#!/usr/bin/env python3
"""Real dpkg/rpm install/upgrade/uninstall rows for metaflux-backend-vulkan.

Runs the packaged artifacts inside digest-pinned, locally present distribution
images. One network-enabled seed container per distribution downloads the
distro loader dependency (libvulkan1 / vulkan-loader) into the artifact
directory; every actual row then runs with --pull=never --network=none and
installs through the real package manager: fresh install, a real
0.0.0 -> current package-manager upgrade, and removal with file assertions.

Exit codes: 0 pass, 77 skip (podman unavailable), 1 failure.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

DIGEST_PATTERN = re.compile(r".+@sha256:[0-9a-f]{64}\Z")
UBUNTU_SEED_COMMAND = (
    "apt-get update -qq && cd /artifacts && apt-get download libvulkan1"
)
ROCKY_SEED_COMMAND = (
    "dnf -y install dnf-plugins-core"
    " && dnf download -y --destdir /artifacts vulkan-loader.x86_64"
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--deb", required=True, type=Path)
    parser.add_argument("--rpm", required=True, type=Path)
    parser.add_argument("--tar", required=True, dest="tarball", type=Path)
    parser.add_argument("--upgrade-deb", required=True, type=Path)
    parser.add_argument("--upgrade-rpm", required=True, type=Path)
    parser.add_argument("--upgrade-tar", required=True, type=Path)
    parser.add_argument("--ubuntu-20-image", required=True)
    parser.add_argument("--rocky-9-image", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--podman", default="podman")
    return parser.parse_args()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_image_ref(reference: str) -> str:
    if DIGEST_PATTERN.fullmatch(reference) is None:
        raise ValueError(
            f"image reference must be digest-pinned (name@sha256:<64hex>): {reference}"
        )
    return reference


def image_local_digest(podman: str, reference: str) -> str | None:
    result = subprocess.run(
        [podman, "image", "inspect", "--format", "{{.Digest}}", reference],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    return result.stdout.strip() or None


def run_command(
    podman: str,
    image: str,
    command: str,
    mounts: dict[str, str],
    *,
    network_none: bool = True,
) -> dict[str, Any]:
    argv = [podman, "run", "--rm", "--pull=never"]
    if network_none:
        argv.append("--network=none")
    for container_path, host_path in mounts.items():
        argv.extend(("-v", f"{host_path}:{container_path}"))
    argv.extend((image, "sh", "-c", command))
    result = subprocess.run(argv, capture_output=True, text=True)
    return {
        "image": reference_short(image),
        "command": " ".join(argv),
        "returncode": result.returncode,
        "stdout": result.stdout[-4000:],
        "stderr": result.stderr[-4000:],
    }


def reference_short(image: str) -> str:
    return image.replace("@sha256:", "@sha256:")[:96]


def require_success(step: dict[str, Any], label: str) -> None:
    if step["returncode"] != 0:
        raise RuntimeError(f"row '{label}' failed: {step['stderr'] or step['stdout']}")


def main() -> int:
    arguments = parse_arguments()
    podman = shutil.which(arguments.podman)
    if podman is None:
        print("SKIP: podman is unavailable; backend-vulkan package rows not run", file=sys.stderr)
        return 77

    for artifact in (
        arguments.deb,
        arguments.rpm,
        arguments.tarball,
        arguments.upgrade_deb,
        arguments.upgrade_rpm,
        arguments.upgrade_tar,
    ):
        if not artifact.is_file():
            raise ValueError(f"missing package artifact: {artifact}")
    ubuntu_image = validate_image_ref(arguments.ubuntu_20_image)
    rocky_image = validate_image_ref(arguments.rocky_9_image)
    images = {}
    for label, reference in (("ubuntu-20.04", ubuntu_image), ("rockylinux-9", rocky_image)):
        local = image_local_digest(podman, reference)
        if local is None:
            print(
                f"SKIP: image not present locally: {reference} "
                "(acquisition is a separately recorded operator step)",
                file=sys.stderr,
            )
            return 77
        images[label] = {"reference": reference, "local_digest": local}

    arguments.output_dir.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="metaflux-backend-vulkan-rows-") as tmp:
        artifacts = Path(tmp) / "artifacts"
        artifacts.mkdir()
        for source in (
            arguments.deb,
            arguments.rpm,
            arguments.tarball,
            arguments.upgrade_deb,
            arguments.upgrade_rpm,
            arguments.upgrade_tar,
        ):
            shutil.copyfile(source, artifacts / source.name)

        # Seed step (network-enabled, recorded): fetch the distro loader
        # dependency so every qualification row itself stays offline.
        seed_ubuntu = run_command(
            podman,
            ubuntu_image,
            UBUNTU_SEED_COMMAND,
            {"/artifacts": str(artifacts)},
            network_none=False,
        )
        require_success(seed_ubuntu, "ubuntu-20.04 seed libvulkan1")
        seed_rocky = run_command(
            podman,
            rocky_image,
            ROCKY_SEED_COMMAND,
            {"/artifacts": str(artifacts)},
            network_none=False,
        )
        require_success(seed_rocky, "rockylinux-9 seed vulkan-loader")
        rows.extend(
            [
                {"row": "seed-ubuntu-libvulkan1", **seed_ubuntu},
                {"row": "seed-rocky-vulkan-loader", **seed_rocky},
            ]
        )

        library_path = "/usr/lib/metaflux/backends/libmetaflux_vulkan_backend.so"
        deb_version = re.fullmatch(
            r"metaflux-backend-vulkan_(.+)_amd64\.deb", arguments.deb.name
        ).group(1)
        rpm_version = re.fullmatch(
            r"metaflux-backend-vulkan-(.+)-[0-9]+\.x86_64\.rpm", arguments.rpm.name
        ).group(1)
        prior_deb = f"/artifacts/{arguments.upgrade_deb.name}"
        current_deb = f"/artifacts/{arguments.deb.name}"
        prior_rpm = f"/artifacts/{arguments.upgrade_rpm.name}"
        current_rpm = f"/artifacts/{arguments.rpm.name}"
        prior_tar = f"/artifacts/{arguments.upgrade_tar.name}"
        current_tar = f"/artifacts/{arguments.tarball.name}"

        # Each lifecycle runs inside ONE offline container so the package
        # manager database persists across install, upgrade, and removal.
        ubuntu_lifecycle = run_command(
            podman,
            ubuntu_image,
            "dpkg -i /artifacts/libvulkan1*.deb && "
            f"dpkg -i {prior_deb} && "
            f"test -f {library_path} && "
            f"dpkg -i {current_deb} && "
            f"test -f {library_path} && "
            "test \"$(dpkg-query -W -f='${Version}' metaflux-backend-vulkan)\""
            f" = \"{deb_version}\" && "
            "dpkg -r metaflux-backend-vulkan && "
            f"! test -e {library_path} && "
            "! dpkg -s metaflux-backend-vulkan 2>/dev/null",
            {"/artifacts": str(artifacts)},
        )
        require_success(ubuntu_lifecycle, "ubuntu-20.04 dpkg lifecycle")
        rows.append({"row": "ubuntu-20.04-dpkg-install-upgrade-uninstall", **ubuntu_lifecycle})

        rocky_lifecycle = run_command(
            podman,
            rocky_image,
            "rpm -Uvh /artifacts/vulkan-loader-*.el9.x86_64.rpm && "
            f"rpm -Uvh {prior_rpm} && "
            f"test -f {library_path} && "
            f"rpm -Uvh {current_rpm} && "
            f"test -f {library_path} && "
            "test \"$(rpm -q --qf '%{VERSION}' metaflux-backend-vulkan)\""
            f" = \"{rpm_version}\" && "
            "rpm -e metaflux-backend-vulkan && "
            f"! test -e {library_path} && "
            "! rpm -q metaflux-backend-vulkan >/dev/null 2>&1",
            {"/artifacts": str(artifacts)},
        )
        require_success(rocky_lifecycle, "rockylinux-9 rpm lifecycle")
        rows.append({"row": "rockylinux-9-rpm-install-upgrade-uninstall", **rocky_lifecycle})

        tar_row = run_command(
            podman,
            ubuntu_image,
            "mkdir -p /tmp/target && "
            f"tar -xzf {prior_tar} -C /tmp/target && "
            f"tar -xzf {current_tar} -C /tmp/target && "
            f"test -f /tmp/target{library_path} && "
            "rm -rf /tmp/target/usr/lib/metaflux/backends && "
            f"! test -e /tmp/target{library_path}",
            {"/artifacts": str(artifacts)},
        )
        require_success(tar_row, "tar overlay upgrade + removal")
        rows.append({"row": "tar-overlay-upgrade-remove", **tar_row})

        report = {
            "gate": "backend-vulkan-package-rows",
            "status": "passed",
            "packages": {
                path.name: {"sha256": sha256(path), "size": path.stat().st_size}
                for path in (
                    arguments.deb,
                    arguments.rpm,
                    arguments.tarball,
                    arguments.upgrade_deb,
                    arguments.upgrade_rpm,
                    arguments.upgrade_tar,
                )
            },
            "images": images,
            "rows": rows,
        }
        output = arguments.output_dir / "backend-vulkan-package-rows.json"
        output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        print(json.dumps({"status": "ok", "rows": len(rows)}, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        print(f"run_backend_vulkan_package_rows: error: {error}", file=sys.stderr)
        raise SystemExit(1)
