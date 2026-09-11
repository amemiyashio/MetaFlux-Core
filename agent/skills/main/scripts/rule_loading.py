#!/usr/bin/env python3
"""Load actual rule bodies and bind their versions to one operation.

This is evidence of emitted instructions, not authorization or proof of
comprehension. The host hook supplies the text to the current tool context.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from typing import Any, Iterable, TextIO

import workflow_state as ws

# Ownership requirements, not another skill roster. Available names come from
# the skill packages themselves; the existing state gate validates that roster.
OWNERS = {
    "runtime-contracts-registry": ("runtime/", "contracts/protocol/", "contracts/shared/", "contracts/plugin/", "services/metafluxd/"),
    "cuda-driver-abi-compatibility": ("plugins/compat/cuda/abi/", "plugins/compat/cuda/libraries/", "plugins/compat/cuda/passthrough/", "tests/compatibility/"),
    "nvml-telemetry-compatibility": ("plugins/compat/cuda/management/",),
    "ptx-simt-semantics": ("plugins/compat/cuda/compiler/ptx/", "compiler/core/include/metaflux/compiler/kernel_ir.hpp", "compiler/core/src/kernel_ir.cpp"),
    "mlir-compiler-engineering": ("compiler/",),
    "cpu-backend-performance": ("plugins/backend/cpu/",),
    "linux-device-driver-uapi": ("linux-kernel-drivers/", "contracts/uapi/linux/", "transports/cdev/"),
    "gpu-virtualization-vfio-user": ("transports/vfio-user/", "services/metaflux-vfio-userd/", "contracts/protocol/transport/v1/schema/vfio_user.json"),
    "pcie-vpci-device-model": ("linux-kernel-drivers/vroot/", "linux-kernel-drivers/pci/", "contracts/protocol/transport/v1/schema/extensions/vroot/"),
    "device-lifecycle-resilience": ("runtime/core/", "tests/lifecycle/", "contracts/protocol/transport/v1/schema/extensions/lifecycle/"),
    "vulkan-spirv-compute": ("plugins/backend/vulkan/",),
    "manage-toolchain": ("nix/", "flake.nix", "flake.lock", "toolchains/"),
    "add-component": ("CMakeLists.txt", "cmake/MetaFluxComponentGraph.cmake", "cmake/MetaFluxTargets.cmake"),
}


def workflow(kind: str) -> str:
    ws.require(kind in {"maintenance", "iteration", "batch", "integration", "epoch", "read-only"}, "Unknown rule-loading workflow")
    return {"maintenance": "main", "read-only": "main", "integration": "batch"}.get(kind, kind)


def overlaps(path: str, owner: str) -> bool:
    return path == owner or (owner.endswith("/") and path.startswith(owner)) or (path.endswith("/") and owner.startswith(path))


def selected_skills(kind: str, paths: Iterable[str] = (), skills: Iterable[str] = ()) -> list[str]:
    selected = {"main", workflow(kind), *skills}
    for path in paths:
        selected.update(name for name, owners in OWNERS.items() if any(overlaps(path, owner) for owner in owners))
        # Skill package creation/removal belongs to its workflow. Treating the
        # edited package as a required reader would demand a nonexistent body
        # before creation or after deletion. Additional domain skills are
        # selected explicitly when the package's technical meaning needs them.
    for name in selected:
        ws.require(isinstance(name, str) and re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", name) is not None,
                   "Invalid required skill")
    return sorted(selected, key=lambda name: (name != "main", name))


def required(root: Path, kind: str, paths: Iterable[str] = (), skills: Iterable[str] = ()) -> list[str]:
    selected = selected_skills(kind, paths, skills)
    for name in selected:
        ws.require((root / "agent/skills" / name / "SKILL.md").is_file(), "Required skill is missing: " + name)
    return selected


def rule_paths(root: Path, selected: list[str]) -> list[str]:
    paths = ["AGENTS.md", "agent/skills/main/SKILL.md", "agent/skills/main/references/controller.md"]
    for name in selected:
        path = "agent/skills/" + name + "/SKILL.md"
        if path not in paths:
            paths.append(path)
    return paths


def load(root: Path, kind: str, base_revision: str, *, skills: Iterable[str] = (),
         paths: Iterable[str] = (), output: TextIO = sys.stdout) -> dict[str, Any]:
    ws.exact_commit(root, base_revision)
    selected = required(root, kind, paths, skills)
    values = ws.entries(root)
    files = {}
    bodies = []
    for name in rule_paths(root, selected):
        path = root / name
        ws.require(path.is_file() and not path.is_symlink() and path.resolve().is_relative_to(root.resolve()),
                   "Rule must be a repository-owned regular file: " + name)
        ws.require(name in values, "Rule is missing from candidate inputs: " + name)
        files[name] = list(values[name])
        bodies.append("\n--- " + name + " ---\n" + path.read_text(encoding="utf-8"))
    state_path = ws.local_path(root, "state.json")
    state = ws.read_json(state_path) if state_path.exists() else {}
    certificate = {"schema_version": 1, "base_revision": base_revision, "head": ws.oid(root),
                   "workflow": workflow(kind), "request": None if state.get("commit") else state.get("run"),
                   "skills": selected, "files": files}
    certificate["digest"] = ws.digest(certificate)
    output.write("MetaFlux rules for the current operation. Read these bodies before the next mutation.\n" + "".join(bodies) + "\n")
    output.flush()
    # Failed output or concurrent edits must not leave a successful receipt.
    validate(root, certificate, kind=kind)
    ws.require(ws.oid(root) == certificate["head"], "HEAD changed while loading rules")
    ws.atomic_json(ws.local_path(root, "rules.json"), certificate)
    return certificate


def validate(root: Path, certificate: Any, *, kind: str,
             entries: dict[str, tuple[str, str]] | None = None) -> None:
    ws.require(isinstance(certificate, dict) and set(certificate) ==
               {"schema_version", "base_revision", "head", "workflow", "request", "skills", "files", "digest"},
               "Missing actual rule-loading receipt; use $main skill (main.py load-rules)")
    ws.require(certificate["schema_version"] == 1 and certificate["digest"] ==
               ws.digest({k: v for k, v in certificate.items() if k != "digest"}), "Rule-loading receipt changed")
    ws.require(certificate["workflow"] == workflow(kind), "Loaded rules belong to another workflow")
    selected = certificate["skills"]
    ws.require(isinstance(selected, list) and all(isinstance(s, str) and re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", s) for s in selected)
               and len(set(selected)) == len(selected) and {"main", workflow(kind)} <= set(selected), "Incomplete required skills")
    ws.require(set(certificate["files"]) == set(rule_paths(root, selected)), "Rule-loading file set is incomplete")
    values = ws.entries(root) if entries is None else entries
    ws.exact_commit(root, certificate["base_revision"])
    baseline = ws.entries(root, certificate["base_revision"])
    changed = [name for name in baseline.keys() | values.keys() if baseline.get(name) != values.get(name)]
    ws.require(set(selected_skills(kind, changed)) <= set(selected),
               "Changed paths require additional skill bodies; use $main skill (main.py load-rules --skill NAME)")
    for name, value in certificate["files"].items():
        ws.require(isinstance(value, list) and len(value) == 2 and value[0] in {"100644", "100755"}
                   and values.get(name) == tuple(value), "Loaded rule version changed: " + name + "; use $main skill (main.py load-rules)")


def current(root: Path, *, kind: str | None = None) -> dict[str, Any]:
    path = ws.local_path(root, "rules.json")
    ws.require(path.is_file(), "Load rule bodies with $main skill (main.py load-rules) before preparation or review")
    certificate = ws.read_json(path)
    recorded_kind = {"main": "maintenance"}.get(certificate.get("workflow"), certificate.get("workflow"))
    validate(root, certificate, kind=kind or recorded_kind)
    ws.require(certificate["head"] == ws.oid(root), "Loaded rules have a stale HEAD; use $main skill (main.py load-rules)")
    return certificate
