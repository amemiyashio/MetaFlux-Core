"""Invoke the canonical CMake build-directory policy without creating files."""

from __future__ import annotations

from pathlib import Path
import subprocess


POLICY_MODULE = Path(__file__).resolve().parents[1] / "cmake" / "MetaFluxBuildDirectory.cmake"


def validate_build_directory(repository: Path, directory: Path) -> Path:
    """Validate through CMake, then return the resolved build directory."""
    try:
        result = subprocess.run(
            [
                "cmake",
                f"-DMETAFLUX_SOURCE_DIR={repository}",
                f"-DMETAFLUX_BINARY_DIR={directory}",
                "-P",
                str(POLICY_MODULE),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
    except OSError as error:
        raise ValueError(f"CMake build-directory validation could not run: {error}") from error
    if result.returncode != 0:
        raise ValueError(
            result.stderr or result.stdout
            or f"CMake build-directory validation exited with {result.returncode}"
        )
    try:
        return directory.resolve()
    except OSError as error:
        raise ValueError(f"Build-directory resolution failed: {error}") from error
