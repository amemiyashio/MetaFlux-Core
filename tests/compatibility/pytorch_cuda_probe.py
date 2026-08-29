#!/usr/bin/env python3
"""Report staged PyTorch/CUDA compatibility gaps without claiming M0001 support."""

from __future__ import annotations

import argparse
import hashlib
import importlib
import json
import os
from pathlib import Path
import platform
import re
import sys
from typing import Any, Callable


SCRIPT_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_CLIENT_MANIFEST = REPOSITORY_ROOT / "toolchains" / "pytorch-cuda-clients-1.json"
DEFAULT_PTX_MANIFEST = (
    REPOSITORY_ROOT
    / "plugins"
    / "compat"
    / "cuda"
    / "compiler"
    / "ptx"
    / "manifest"
    / "capabilities.json"
)
PROFILE_NAMES = ("baseline", "frontier")
STAGE_ORDER = (
    "import",
    "driver-enumeration",
    "runtime-copy",
    "artifact-intake",
    "eager-add",
)
NVML_CHECK_ENV = "PYTORCH_NVML_BASED_CUDA_CHECK"


class ManifestError(ValueError):
    pass


class StageGap(RuntimeError):
    def __init__(self, kind: str, message: str, details: dict[str, Any] | None = None):
        super().__init__(message)
        self.kind = kind
        self.details = details or {}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def compact_exception(error: BaseException) -> str:
    text = " ".join(str(error).split())
    if len(text) > 400:
        text = text[:397] + "..."
    return f"{type(error).__name__}: {text}" if text else type(error).__name__


def read_json_object(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ManifestError(f"cannot read {label} {path}: {compact_exception(error)}") from error
    if not isinstance(value, dict):
        raise ManifestError(f"{label} {path} must contain a JSON object")
    return value


def required_string(value: Any, field: str) -> str:
    if not isinstance(value, str) or not value:
        raise ManifestError(f"{field} must be a non-empty string")
    return value


def portable_path(path: Path, repository_root: Path) -> str:
    resolved = path.resolve()
    try:
        return resolved.relative_to(repository_root.resolve()).as_posix()
    except ValueError:
        return str(resolved)


def load_client_profile(path: Path, profile_name: str) -> dict[str, str]:
    manifest = read_json_object(path, "PyTorch client manifest")
    if manifest.get("schema_version") != 1:
        raise ManifestError("PyTorch client manifest schema_version must equal 1")

    python = manifest.get("python")
    profiles = manifest.get("profiles")
    if not isinstance(python, dict):
        raise ManifestError("PyTorch client manifest python must be an object")
    if not isinstance(profiles, dict):
        raise ManifestError("PyTorch client manifest profiles must be an object")
    profile = profiles.get(profile_name)
    if not isinstance(profile, dict):
        raise ManifestError(f"PyTorch client manifest has no {profile_name!r} profile")

    flat_torch_version = profile.get("torch_version")
    torch_object = profile.get("torch")
    nested_torch_version = torch_object.get("version") if isinstance(torch_object, dict) else None
    if flat_torch_version is not None and nested_torch_version is not None:
        if flat_torch_version != nested_torch_version:
            raise ManifestError(
                f"profiles.{profile_name}.torch_version and torch.version disagree"
            )
    torch_version = (
        flat_torch_version if flat_torch_version is not None else nested_torch_version
    )

    return {
        "role": required_string(profile.get("role"), f"profiles.{profile_name}.role"),
        "python": required_string(python.get("version"), "python.version"),
        "torch": required_string(
            torch_version,
            f"profiles.{profile_name}.torch_version or torch.version",
        ),
        "cuda": required_string(
            profile.get("cuda_version"), f"profiles.{profile_name}.cuda_version"
        ),
        "manifest_sha256": sha256_file(path),
    }


def load_d0017_reference(path: Path, repository_root: Path) -> dict[str, Any]:
    capability = read_json_object(path, "D0017 capability manifest")
    forms_path_value = required_string(capability.get("forms_path"), "forms_path")
    corpus_path_value = required_string(capability.get("corpus_index_path"), "corpus_index_path")
    declared_forms_sha = required_string(capability.get("forms_sha256"), "forms_sha256")
    declared_corpus_sha = required_string(
        capability.get("corpus_index_sha256"), "corpus_index_sha256"
    )

    forms_path = (path.parent / forms_path_value).resolve()
    corpus_path = (path.parent / corpus_path_value).resolve()
    try:
        forms_sha = sha256_file(forms_path)
        corpus_sha = sha256_file(corpus_path)
    except OSError as error:
        raise ManifestError(f"cannot hash D0017 reference: {compact_exception(error)}") from error
    if forms_sha != declared_forms_sha:
        raise ManifestError(
            f"D0017 forms digest mismatch: declared {declared_forms_sha}, observed {forms_sha}"
        )
    if corpus_sha != declared_corpus_sha:
        raise ManifestError(
            f"D0017 corpus digest mismatch: declared {declared_corpus_sha}, observed {corpus_sha}"
        )

    target = required_string(capability.get("target"), "target")
    target_match = re.fullmatch(r"sm_([0-9]+)", target)
    minimum_sm = capability.get("minimum_sm")
    if target_match is None or not isinstance(minimum_sm, int) or minimum_sm < 10:
        raise ManifestError("D0017 target/minimum_sm does not encode a CUDA compute capability")
    if int(target_match.group(1)) != minimum_sm:
        raise ManifestError("D0017 target and minimum_sm disagree")

    entries = {
        "capabilities": {
            "path": portable_path(path, repository_root),
            "sha256": sha256_file(path),
        },
        "forms": {
            "path": portable_path(forms_path, repository_root),
            "sha256": forms_sha,
        },
        "corpus_index": {
            "path": portable_path(corpus_path, repository_root),
            "sha256": corpus_sha,
        },
    }
    bundle_bytes = json.dumps(entries, sort_keys=True, separators=(",", ":")).encode("ascii")
    return {
        "decision": "D0017",
        "ptx_isa": required_string(capability.get("ptx_isa"), "ptx_isa"),
        "target": target,
        "kernel_ir_schema": capability.get("kernel_ir_schema"),
        "entries": entries,
        "bundle_sha256": hashlib.sha256(bundle_bytes).hexdigest(),
        "compute_capability": f"{minimum_sm // 10}.{minimum_sm % 10}",
    }


def stage_result(name: str, status: str, **fields: Any) -> dict[str, Any]:
    return {"name": name, "status": status, **fields}


def import_stage(
    state: dict[str, Any],
    expected: dict[str, str],
    importer: Callable[[], Any],
    python_version: str,
    nvml_env_was_set: bool,
) -> dict[str, Any]:
    try:
        torch = importer()
    except Exception as error:
        raise StageGap("torch-import-failed", compact_exception(error)) from error

    observed = {
        "python": python_version,
        "torch": str(getattr(torch, "__version__", "")),
        "cuda": str(getattr(getattr(torch, "version", None), "cuda", "")),
    }
    state["torch"] = torch
    state["observed_client"] = observed
    mismatches = [
        f"{name}: expected {expected[name]!r}, observed {observed[name]!r}"
        for name in ("python", "torch", "cuda")
        if observed[name] != expected[name]
    ]
    details = {
        "observed": observed,
        "nvml_availability_check_disabled": True,
        "nvml_environment_was_set": nvml_env_was_set,
    }
    if mismatches:
        raise StageGap("client-identity-mismatch", "; ".join(mismatches), details)
    return details


def driver_enumeration_stage(state: dict[str, Any], device_index: int) -> dict[str, Any]:
    torch = state["torch"]
    count_api = getattr(getattr(torch, "_C", None), "_cuda_getDeviceCount", None)
    if not callable(count_api):
        raise StageGap(
            "driver-enumeration-entry-missing",
            "torch._C._cuda_getDeviceCount is not callable",
        )
    try:
        count = int(count_api())
    except Exception as error:
        raise StageGap("driver-enumeration-failed", compact_exception(error)) from error
    if count <= 0:
        raise StageGap("no-cuda-device", f"driver path reported {count} CUDA devices")
    if device_index < 0 or device_index >= count:
        raise StageGap(
            "device-index-out-of-range",
            f"selected device {device_index}, but driver path reported {count} devices",
        )

    devices: list[dict[str, Any]] = []
    try:
        for index in range(count):
            properties = torch.cuda.get_device_properties(index)
            major = int(properties.major)
            minor = int(properties.minor)
            devices.append(
                {
                    "index": index,
                    "name": str(properties.name),
                    "compute_capability": f"{major}.{minor}",
                    "total_memory": int(properties.total_memory),
                }
            )
        torch.cuda.set_device(device_index)
    except Exception as error:
        raise StageGap("driver-device-properties-failed", compact_exception(error)) from error

    state["devices"] = devices
    state["device"] = f"cuda:{device_index}"
    state["selected_compute_capability"] = devices[device_index]["compute_capability"]
    return {
        "api": "torch._C._cuda_getDeviceCount",
        "nvml_substitution": False,
        "selected_device": device_index,
        "devices": devices,
    }


def runtime_copy_stage(state: dict[str, Any]) -> dict[str, Any]:
    torch = state["torch"]
    values = [1, -2, 3, 0x01020304]
    try:
        source = torch.tensor(values, dtype=torch.int32, device="cpu")
        device = torch.empty_like(source, device=state["device"])
        device.copy_(source, non_blocking=False)
        roundtrip = torch.empty_like(source, device="cpu")
        roundtrip.copy_(device, non_blocking=False)
        torch.cuda.synchronize(state["device"])
        observed = [int(value) for value in roundtrip.tolist()]
    except Exception as error:
        raise StageGap("runtime-copy-failed", compact_exception(error)) from error
    if observed != values:
        raise StageGap(
            "runtime-copy-data-mismatch",
            f"expected {values!r}, observed {observed!r}",
        )
    return {
        "operation": "contiguous int32 HtoD/DtoH copy",
        "bytes_each_direction": len(values) * 4,
        "intentional_kernel_launches": 0,
    }


def artifact_intake_stage(state: dict[str, Any]) -> dict[str, Any]:
    torch = state["torch"]
    arch_list_api = getattr(torch.cuda, "get_arch_list", None)
    sleep_api = getattr(torch.cuda, "_sleep", None)
    if not callable(arch_list_api):
        raise StageGap(
            "artifact-arch-list-entry-missing",
            "torch.cuda.get_arch_list is not callable",
        )
    if not callable(sleep_api):
        raise StageGap("artifact-smoke-entry-missing", "torch.cuda._sleep is not callable")

    try:
        architectures = sorted(str(value) for value in arch_list_api())
    except Exception as error:
        raise StageGap("artifact-arch-list-failed", compact_exception(error)) from error
    capability = state["selected_compute_capability"].replace(".", "")
    expected_entry = f"sm_{capability}"
    details = {
        "wheel_architectures": architectures,
        "selected_artifact_entry": expected_entry,
        "operation": "torch.cuda._sleep(1) bundled-kernel smoke",
    }
    if expected_entry not in architectures:
        raise StageGap(
            "artifact-for-advertised-cc-missing",
            f"wheel architecture list has no exact {expected_entry} entry",
            details,
        )

    try:
        sleep_api(1)
        torch.cuda.synchronize(state["device"])
    except Exception as error:
        raise StageGap("artifact-intake-failed", compact_exception(error), details) from error
    return details


def eager_add_stage(state: dict[str, Any]) -> dict[str, Any]:
    torch = state["torch"]
    left_values = [1, -2, 30, 400]
    right_values = [5, 7, -10, 20]
    expected = [6, 5, 20, 420]
    try:
        left = torch.tensor(left_values, dtype=torch.int32, device=state["device"])
        right = torch.tensor(right_values, dtype=torch.int32, device=state["device"])
        result = torch.add(left, right)
        torch.cuda.synchronize(state["device"])
        observed = [int(value) for value in result.to("cpu").tolist()]
    except Exception as error:
        raise StageGap("eager-add-failed", compact_exception(error)) from error
    if observed != expected:
        raise StageGap("eager-add-data-mismatch", f"expected {expected!r}, observed {observed!r}")
    return {"operation": "stock torch.add int32", "elements": len(expected)}


def run_probe(
    profile_name: str,
    client_manifest_path: Path,
    ptx_manifest_path: Path,
    device_index: int = 0,
    *,
    importer: Callable[[], Any] | None = None,
    python_version: str | None = None,
    repository_root: Path = REPOSITORY_ROOT,
) -> dict[str, Any]:
    expected = load_client_profile(client_manifest_path, profile_name)
    d0017 = load_d0017_reference(ptx_manifest_path, repository_root)
    state: dict[str, Any] = {}
    report: dict[str, Any] = {
        "schema_version": 1,
        "probe": "metaflux-pytorch-cuda-gap-probe",
        "scope": "diagnostic-only-not-m0001-compatibility-evidence",
        "profile": profile_name,
        "expected_client": expected,
        "d0017": {key: value for key, value in d0017.items() if key != "compute_capability"},
        "advertised_compute_capability": {
            "d0017": d0017["compute_capability"],
            "observed": None,
        },
        "stages": [],
        "first_gap": None,
    }

    actual_importer = importer or (lambda: importlib.import_module("torch"))
    actual_python_version = python_version or platform.python_version()
    nvml_env_was_set = NVML_CHECK_ENV in os.environ
    previous_nvml_env = os.environ.pop(NVML_CHECK_ENV, None)
    stage_functions: dict[str, Callable[[], dict[str, Any]]] = {
        "import": lambda: import_stage(
            state,
            expected,
            actual_importer,
            actual_python_version,
            nvml_env_was_set,
        ),
        "driver-enumeration": lambda: driver_enumeration_stage(state, device_index),
        "runtime-copy": lambda: runtime_copy_stage(state),
        "artifact-intake": lambda: artifact_intake_stage(state),
        "eager-add": lambda: eager_add_stage(state),
    }

    try:
        for name in STAGE_ORDER:
            first_gap = report["first_gap"]
            if first_gap is not None:
                report["stages"].append(
                    stage_result(name, "blocked", blocked_by=first_gap["stage"])
                )
                continue
            try:
                details = stage_functions[name]()
                report["stages"].append(stage_result(name, "passed", details=details))
            except StageGap as gap:
                first_gap = {
                    "stage": name,
                    "kind": gap.kind,
                    "message": str(gap),
                }
                if gap.details:
                    first_gap["details"] = gap.details
                report["first_gap"] = first_gap
                report["stages"].append(stage_result(name, "gap", **first_gap))
            except Exception as error:
                first_gap = {
                    "stage": name,
                    "kind": "unexpected-probe-exception",
                    "message": compact_exception(error),
                }
                report["first_gap"] = first_gap
                report["stages"].append(stage_result(name, "gap", **first_gap))
    finally:
        if nvml_env_was_set:
            assert previous_nvml_env is not None
            os.environ[NVML_CHECK_ENV] = previous_nvml_env
        else:
            os.environ.pop(NVML_CHECK_ENV, None)

    report["observed_client"] = state.get("observed_client")
    report["advertised_compute_capability"]["observed"] = state.get(
        "selected_compute_capability"
    )
    passed = [stage["name"] for stage in report["stages"] if stage["status"] == "passed"]
    report["reached_stage"] = passed[-1] if passed else None
    report["result"] = "complete" if report["first_gap"] is None else "gap"
    return report


def required_stage_exit_code(report: dict[str, Any], required_stage: str | None) -> int:
    if required_stage is None:
        return 0
    stage = next(item for item in report["stages"] if item["name"] == required_stage)
    return 0 if stage["status"] == "passed" else 1


def parse_arguments(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Diagnose staged PyTorch/CUDA gaps. Success is not an M0001 compatibility claim."
        )
    )
    parser.add_argument("--profile", required=True, choices=PROFILE_NAMES)
    parser.add_argument("--client-manifest", type=Path, default=DEFAULT_CLIENT_MANIFEST)
    parser.add_argument("--ptx-manifest", type=Path, default=DEFAULT_PTX_MANIFEST)
    parser.add_argument("--device-index", type=int, default=0)
    parser.add_argument("--require-stage", choices=STAGE_ORDER)
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_arguments(argv)
    try:
        report = run_probe(
            args.profile,
            args.client_manifest,
            args.ptx_manifest,
            args.device_index,
        )
    except ManifestError as error:
        report = {
            "schema_version": 1,
            "probe": "metaflux-pytorch-cuda-gap-probe",
            "scope": "diagnostic-only-not-m0001-compatibility-evidence",
            "fatal_error": str(error),
        }
        print(json.dumps(report, sort_keys=True, separators=(",", ":")))
        return 2
    print(json.dumps(report, sort_keys=True, separators=(",", ":")))
    return required_stage_exit_code(report, args.require_stage)


if __name__ == "__main__":
    raise SystemExit(main())
