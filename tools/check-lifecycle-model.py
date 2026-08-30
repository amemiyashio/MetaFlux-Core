#!/usr/bin/env python3
"""Validate and exhaustively explore the bounded M0120 lifecycle model."""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import sys
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any, Iterable

CHECKER_VERSION = "1.0"
LIFECYCLE_EVENTS = ("add", "remove", "reset", "transport_loss", "recover")
FAULTABLE_EVENTS = {"add", "remove", "reset", "recover"}
TERMINAL_STATES = {"ABSENT", "ONLINE", "LOST"}


class ModelError(ValueError):
    """An invalid lifecycle input or a failed model invariant."""


def load_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ModelError(f"cannot load {path}: {error}") from error
    if not isinstance(value, dict):
        raise ModelError(f"{path}: expected a JSON object")
    return value


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def repository_root(path: Path) -> Path:
    for parent in (path, *path.parents):
        if (parent / "contracts").is_dir() and (parent / "tools").is_dir():
            return parent
    raise ModelError(f"cannot locate repository root from {path}")


def require_string(document: dict[str, Any], key: str, path: Path) -> str:
    value = document.get(key)
    if not isinstance(value, str) or not value:
        raise ModelError(f"{path}: {key} must be a non-empty string")
    return value


def validate_inputs(base_path: Path, extension_path: Path, model_path: Path,
                    bounds_path: Path) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any], dict[str, str]]:
    base = load_json(base_path)
    extension = load_json(extension_path)
    model = load_json(model_path)
    bounds = load_json(bounds_path)
    root = repository_root(base_path)

    if base.get("id") != "transport.base.v0" or base.get("version") != "0.1":
        raise ModelError(f"{base_path}: expected transport.base.v0 version 0.1")
    if extension.get("id") != "transport.lifecycle-extension.v1" or extension.get("version") != "1.0":
        raise ModelError(f"{extension_path}: expected transport.lifecycle-extension.v1 version 1.0")
    imports = extension.get("imports")
    if not isinstance(imports, list) or len(imports) != 1 or not isinstance(imports[0], dict):
        raise ModelError(f"{extension_path}: exactly one base import is required")
    base_import = imports[0]
    expected_base_rel = base_path.relative_to(root).as_posix()
    if (base_import.get("id"), base_import.get("version"), base_import.get("path")) != (
        "transport.base.v0", "0.1", expected_base_rel
    ):
        raise ModelError(f"{extension_path}: base import does not identify the supplied root manifest")
    base_hash = sha256(base_path)
    if base_import.get("sha256") != base_hash:
        raise ModelError(f"{extension_path}: base manifest content hash mismatch")

    model_ref = extension.get("model")
    if not isinstance(model_ref, dict):
        raise ModelError(f"{extension_path}: model reference is required")
    expected_model_rel = model_path.relative_to(root).as_posix()
    if (model_ref.get("id"), model_ref.get("version"), model_ref.get("path")) != (
        "lifecycle.model.v1", "1.0", expected_model_rel
    ):
        raise ModelError(f"{extension_path}: model reference does not identify the supplied model")
    model_hash = sha256(model_path)
    if model_ref.get("sha256") != model_hash:
        raise ModelError(f"{extension_path}: lifecycle model content hash mismatch")

    if model.get("id") != "lifecycle.model.v1" or model.get("version") != "1.0":
        raise ModelError(f"{model_path}: expected lifecycle.model.v1 version 1.0")
    if model.get("base_manifest") != expected_base_rel:
        raise ModelError(f"{model_path}: base_manifest must point to the frozen M0110 root")
    states = model.get("states")
    if states != ["ABSENT", "PRESENT", "ONLINE", "QUIESCING", "DRAINING", "RESETTING", "LOST"]:
        raise ModelError(f"{model_path}: canonical lifecycle states are incomplete or reordered")
    request_identity = model.get("request_identity")
    expected_identity = [
        "request_id", "source", "operation", "logical_device_uuid",
        "expected_generation", "expected_epoch", "daemon_incarnation_id", "deadline",
    ]
    if request_identity != expected_identity:
        raise ModelError(f"{model_path}: request identity tuple is not canonical")
    transitions = model.get("transitions")
    if not isinstance(transitions, list) or {entry.get("event") for entry in transitions} != set(LIFECYCLE_EVENTS):
        raise ModelError(f"{model_path}: transition set must contain each lifecycle event exactly once")
    required_transition_keys = {
        "event", "sources", "intermediate", "terminals", "generation_action",
        "epoch_action", "commit", "replay", "failure_points",
    }
    for transition in transitions:
        if not isinstance(transition, dict) or not required_transition_keys.issubset(transition):
            raise ModelError(f"{model_path}: malformed transition entry")
        if not isinstance(transition["sources"], list) or not isinstance(transition["terminals"], list):
            raise ModelError(f"{model_path}: transition source/terminal lists are required")
        if not set(transition["sources"]).issubset(set(states)) or not set(transition["terminals"]).issubset(set(states)):
            raise ModelError(f"{model_path}: transition references an unknown state")
        if not isinstance(transition["failure_points"], list):
            raise ModelError(f"{model_path}: failure_points must be a list")
    provider_views = model.get("provider_views")
    if not isinstance(provider_views, dict) or provider_views.get("registry_view_id") != "shared_per_process":
        raise ModelError(f"{model_path}: provider view authority is missing")
    if provider_views.get("matching") != "UUID_and_generation":
        raise ModelError(f"{model_path}: provider matching must use UUID and generation")
    invariants = model.get("invariants")
    if not isinstance(invariants, list) or len(invariants) < 10 or len(set(invariants)) != len(invariants):
        raise ModelError(f"{model_path}: invariant list is incomplete or duplicated")

    if bounds.get("schema_version") != 1 or bounds.get("checker_version") != CHECKER_VERSION:
        raise ModelError(f"{bounds_path}: unsupported bounds/checker version")
    initial = bounds.get("initial")
    limits = bounds.get("limits")
    if not isinstance(initial, dict) or not isinstance(limits, dict):
        raise ModelError(f"{bounds_path}: initial and limits objects are required")
    required_initial = {"state", "generation", "epoch", "generation_high_water", "daemon_incarnation_id", "registry_view_id"}
    if not required_initial.issubset(initial) or initial["state"] != "ONLINE":
        raise ModelError(f"{bounds_path}: initial identity is incomplete")
    for key in ("generation_terminal", "epoch_terminal", "max_depth", "max_states"):
        if not isinstance(limits.get(key), int) or limits[key] <= 0:
            raise ModelError(f"{bounds_path}: {key} must be a positive integer")
    if limits["generation_terminal"] <= initial["generation"] or limits["epoch_terminal"] <= initial["epoch"]:
        raise ModelError(f"{bounds_path}: terminal bounds must exceed initial identity")
    request_ids = bounds.get("request_ids")
    faults = bounds.get("fault_points")
    event_order = bounds.get("event_order")
    if not isinstance(request_ids, list) or not request_ids or any(not isinstance(item, int) or item <= 0 for item in request_ids):
        raise ModelError(f"{bounds_path}: request_ids must be positive integers")
    if faults != ["none", "pre_commit", "post_commit"]:
        raise ModelError(f"{bounds_path}: fault point order is not canonical")
    expected_order = ["add", "remove", "reset", "transport_loss", "recover", "capture_cuda", "capture_nvml", "nvml_reinit", "replay", "conflict"]
    if event_order != expected_order:
        raise ModelError(f"{bounds_path}: event_order is not canonical")

    hashes = {
        "base_manifest": base_hash,
        "extension_manifest": sha256(extension_path),
        "model": model_hash,
        "bounds": sha256(bounds_path),
    }
    return base, extension, model, bounds, hashes


@dataclass(frozen=True)
class Snapshot:
    state: str
    generation: int
    epoch: int
    high_water: int
    accepted: tuple[int, ...]
    committed: tuple[int, ...]
    retired: tuple[int, ...]
    tombstones: tuple[int, ...]
    request_history: tuple[tuple[int, str], ...]
    cuda_initialized: bool
    cuda_membership: tuple[int, ...]
    cuda_lost: tuple[int, ...]
    nvml_initialized: bool
    nvml_membership: tuple[int, ...]
    nvml_lost: tuple[int, ...]
    nvml_init_epoch: int


def ordered(values: Iterable[int]) -> tuple[int, ...]:
    return tuple(sorted(set(values)))


def initial_snapshot(bounds: dict[str, Any]) -> Snapshot:
    initial = bounds["initial"]
    generation = int(initial["generation"])
    return Snapshot(
        state=initial["state"],
        generation=generation,
        epoch=int(initial["epoch"]),
        high_water=int(initial["generation_high_water"]),
        accepted=(generation,),
        committed=(generation,),
        retired=(),
        tombstones=(),
        request_history=(),
        cuda_initialized=False,
        cuda_membership=(),
        cuda_lost=(),
        nvml_initialized=False,
        nvml_membership=(),
        nvml_lost=(),
        nvml_init_epoch=0,
    )


def history_has(snapshot: Snapshot, request_id: int) -> bool:
    return any(item[0] == request_id for item in snapshot.request_history)


def record_request(snapshot: Snapshot, request_id: int, event: str) -> Snapshot:
    return replace(snapshot, request_history=tuple(sorted((*snapshot.request_history, (request_id, event)))))


def mark_provider_loss(snapshot: Snapshot, old_generation: int) -> Snapshot:
    if old_generation <= 0:
        return snapshot
    return replace(
        snapshot,
        cuda_lost=ordered((*snapshot.cuda_lost, old_generation) if old_generation in snapshot.cuda_membership else snapshot.cuda_lost),
        nvml_lost=ordered((*snapshot.nvml_lost, old_generation) if old_generation in snapshot.nvml_membership else snapshot.nvml_lost),
    )


def reserve_candidate(snapshot: Snapshot, limits: dict[str, Any]) -> tuple[Snapshot, int] | None:
    candidate = snapshot.high_water + 1
    if candidate >= int(limits["generation_terminal"]):
        return None
    return replace(snapshot, high_water=candidate, accepted=ordered((*snapshot.accepted, candidate))), candidate


def retire_and_install(snapshot: Snapshot, candidate: int, old_generation: int, state: str,
                       limits: dict[str, Any]) -> Snapshot | None:
    if snapshot.epoch + 1 >= int(limits["epoch_terminal"]):
        return None
    next_snapshot = mark_provider_loss(snapshot, old_generation)
    return replace(
        next_snapshot,
        state=state,
        generation=candidate,
        epoch=snapshot.epoch + 1,
        committed=ordered((*snapshot.committed, candidate)),
        retired=ordered((*snapshot.retired, old_generation)),
        tombstones=ordered(
            (*snapshot.tombstones, old_generation, candidate)
            if state == "LOST"
            else (*snapshot.tombstones, old_generation)
        ),
    )


def apply_lifecycle(snapshot: Snapshot, event: str, request_id: int, fault: str,
                    bounds: dict[str, Any]) -> Snapshot:
    if history_has(snapshot, request_id):
        return snapshot
    limits = bounds["limits"]
    current = snapshot
    if event == "add":
        if snapshot.state != "ABSENT":
            return record_request(snapshot, request_id, event)
        reserved = reserve_candidate(snapshot, limits)
        if reserved is None:
            return record_request(snapshot, request_id, event)
        current, candidate = reserved
        if fault == "pre_commit":
            return record_request(current, request_id, event)
        next_state = "LOST" if fault == "post_commit" else "ONLINE"
        current = replace(
            current,
            state=next_state,
            generation=candidate,
            committed=ordered((*current.committed, candidate)),
            tombstones=ordered((*current.tombstones, candidate)) if next_state == "LOST" else current.tombstones,
        )
        return record_request(current, request_id, event)

    if event == "remove":
        if snapshot.state not in {"ONLINE", "LOST"} or snapshot.generation <= 0:
            return record_request(snapshot, request_id, event)
        if snapshot.epoch + 1 >= int(limits["epoch_terminal"]):
            return record_request(snapshot, request_id, event)
        old_generation = snapshot.generation
        current = mark_provider_loss(snapshot, old_generation)
        current = replace(current, state="ABSENT", generation=0, epoch=snapshot.epoch + 1,
                          retired=ordered((*current.retired, old_generation)),
                          tombstones=ordered((*current.tombstones, old_generation)))
        return record_request(current, request_id, event)

    if event == "reset":
        if snapshot.state != "ONLINE" or snapshot.generation <= 0 or snapshot.epoch + 1 >= int(limits["epoch_terminal"]):
            return record_request(snapshot, request_id, event)
        reserved = reserve_candidate(snapshot, limits)
        if reserved is None:
            return record_request(snapshot, request_id, event)
        current, candidate = reserved
        if fault == "pre_commit":
            return record_request(current, request_id, event)
        old_generation = snapshot.generation
        next_state = "LOST" if fault == "post_commit" else "ONLINE"
        current = retire_and_install(current, candidate, old_generation, next_state, limits)
        if current is None:
            raise ModelError("accepted reset lost its reserved retirement capacity")
        return record_request(current, request_id, event)

    if event == "transport_loss":
        if snapshot.state in {"PRESENT", "ONLINE", "QUIESCING", "DRAINING", "RESETTING"} and snapshot.generation > 0:
            current = mark_provider_loss(snapshot, snapshot.generation)
            current = replace(current, state="LOST", tombstones=ordered((*current.tombstones, current.generation)))
        return record_request(current, request_id, event)

    if event == "recover":
        if snapshot.state != "LOST" or snapshot.generation <= 0 or snapshot.epoch + 1 >= int(limits["epoch_terminal"]):
            return record_request(snapshot, request_id, event)
        reserved = reserve_candidate(snapshot, limits)
        if reserved is None:
            return record_request(snapshot, request_id, event)
        current, candidate = reserved
        if fault == "pre_commit":
            return record_request(current, request_id, event)
        old_generation = snapshot.generation
        next_state = "LOST" if fault == "post_commit" else "ONLINE"
        current = retire_and_install(current, candidate, old_generation, next_state, limits)
        if current is None:
            raise ModelError("accepted recovery lost its reserved retirement capacity")
        return record_request(current, request_id, event)

    raise ModelError(f"unknown lifecycle event {event}")


def apply_view(snapshot: Snapshot, event: str) -> Snapshot:
    if event == "capture_cuda":
        if snapshot.cuda_initialized:
            return snapshot
        membership = (snapshot.generation,) if snapshot.state == "ONLINE" and snapshot.generation > 0 else ()
        return replace(snapshot, cuda_initialized=True, cuda_membership=membership, cuda_lost=())
    if event == "capture_nvml":
        if snapshot.nvml_initialized:
            return snapshot
        membership = (snapshot.generation,) if snapshot.state == "ONLINE" and snapshot.generation > 0 else ()
        return replace(snapshot, nvml_initialized=True, nvml_membership=membership,
                       nvml_lost=(), nvml_init_epoch=max(snapshot.nvml_init_epoch, 1))
    if event == "nvml_reinit":
        membership = (snapshot.generation,) if snapshot.state == "ONLINE" and snapshot.generation > 0 else ()
        return replace(snapshot, nvml_initialized=True, nvml_membership=membership,
                       nvml_lost=(), nvml_init_epoch=snapshot.nvml_init_epoch + 1)
    raise ModelError(f"unknown view event {event}")


def assert_snapshot(snapshot: Snapshot, bounds: dict[str, Any]) -> None:
    limits = bounds["limits"]
    initial = bounds["initial"]
    if snapshot.state not in {"ABSENT", "PRESENT", "ONLINE", "QUIESCING", "DRAINING", "RESETTING", "LOST"}:
        raise ModelError(f"unknown state {snapshot.state}")
    if snapshot.high_water >= int(limits["generation_terminal"]) or snapshot.epoch >= int(limits["epoch_terminal"]):
        raise ModelError("generation or epoch crossed its reserved terminal bound")
    if snapshot.accepted != tuple(range(int(initial["generation"]), snapshot.high_water + 1)):
        raise ModelError("accepted generation candidates are not contiguous and monotonic")
    if not set(snapshot.committed).issubset(set(snapshot.accepted)):
        raise ModelError("a committed generation was never accepted")
    if not set(snapshot.retired).issubset(set(snapshot.committed)):
        raise ModelError("a retired generation was never committed")
    if set(snapshot.retired).intersection({snapshot.generation}):
        raise ModelError("current generation is retired")
    if snapshot.epoch != int(initial["epoch"]) + len(snapshot.retired):
        raise ModelError("epoch did not advance exactly once per retirement")
    if snapshot.state in {"ONLINE", "LOST"}:
        if snapshot.generation <= 0 or snapshot.generation not in snapshot.committed:
            raise ModelError("live state has no committed generation")
    elif snapshot.state == "ABSENT" and snapshot.generation != 0:
        raise ModelError("ABSENT retains a live generation")
    if snapshot.state == "ONLINE" and len({snapshot.generation}) != 1:
        raise ModelError("more than one live generation owner")
    if not set(snapshot.retired).issubset(set(snapshot.tombstones)):
        raise ModelError("retired generation lost its tombstone")
    if snapshot.state == "LOST" and snapshot.generation not in snapshot.tombstones:
        raise ModelError("lost generation did not retain a tombstone")
    if not snapshot.cuda_initialized:
        if snapshot.cuda_membership or snapshot.cuda_lost:
            raise ModelError("CUDA view has state before initialization")
    else:
        if not set(snapshot.cuda_membership).issubset(set(snapshot.committed)):
            raise ModelError("CUDA membership references an uncommitted generation")
        for generation in snapshot.cuda_membership:
            if generation in snapshot.retired or (
                generation == snapshot.generation and snapshot.state == "LOST"
            ):
                if generation not in snapshot.cuda_lost:
                    raise ModelError("CUDA loss did not update the frozen entry")
    if not snapshot.nvml_initialized:
        if snapshot.nvml_membership or snapshot.nvml_lost or snapshot.nvml_init_epoch != 0:
            raise ModelError("NVML view has state before initialization")
    else:
        if snapshot.nvml_init_epoch <= 0:
            raise ModelError("initialized NVML view has no initialization epoch")
        if not set(snapshot.nvml_membership).issubset(set(snapshot.committed)):
            raise ModelError("NVML membership references an uncommitted generation")
        for generation in snapshot.nvml_membership:
            if generation in snapshot.retired or (
                generation == snapshot.generation and snapshot.state == "LOST"
            ):
                if generation not in snapshot.nvml_lost:
                    raise ModelError("NVML loss did not update the frozen entry")
    if set(snapshot.cuda_lost).difference(snapshot.cuda_membership):
        raise ModelError("CUDA loss references a non-member")
    if set(snapshot.nvml_lost).difference(snapshot.nvml_membership):
        raise ModelError("NVML loss references a non-member")
    if snapshot.nvml_init_epoch < 0:
        raise ModelError("NVML initialization epoch wrapped")


def valid_actions(snapshot: Snapshot, bounds: dict[str, Any]) -> list[tuple[str, int | None, str]]:
    actions: list[tuple[str, int | None, str]] = []
    request_ids = bounds["request_ids"]
    source_by_event = {
        "add": {"ABSENT"},
        "remove": {"ONLINE", "LOST"},
        "reset": {"ONLINE"},
        "transport_loss": {"PRESENT", "ONLINE", "QUIESCING", "DRAINING", "RESETTING"},
        "recover": {"LOST"},
    }
    for event in bounds["event_order"]:
        if event in LIFECYCLE_EVENTS:
            if snapshot.state not in source_by_event[event]:
                continue
            for request_id in request_ids:
                if history_has(snapshot, int(request_id)):
                    continue
                faults = bounds["fault_points"] if event in FAULTABLE_EVENTS else ["none"]
                for fault in faults:
                    actions.append((event, int(request_id), fault))
        elif event == "capture_cuda" and not snapshot.cuda_initialized:
            actions.append((event, None, "none"))
        elif event == "capture_nvml" and not snapshot.nvml_initialized:
            actions.append((event, None, "none"))
        elif event == "nvml_reinit" and snapshot.nvml_initialized:
            actions.append((event, None, "none"))
        elif event == "replay":
            actions.extend((event, request_id, "none") for request_id, _ in snapshot.request_history)
        elif event == "conflict":
            actions.extend((event, request_id, "none") for request_id, _ in snapshot.request_history)
    return actions


def apply_action(snapshot: Snapshot, action: tuple[str, int | None, str], bounds: dict[str, Any]) -> Snapshot:
    event, request_id, fault = action
    if event in LIFECYCLE_EVENTS:
        if request_id is None:
            raise ModelError("lifecycle action has no request id")
        return apply_lifecycle(snapshot, event, request_id, fault, bounds)
    if event in {"capture_cuda", "capture_nvml", "nvml_reinit"}:
        return apply_view(snapshot, event)
    if event in {"replay", "conflict"}:
        return snapshot
    raise ModelError(f"unknown action {event}")


def direct_scenarios(bounds: dict[str, Any]) -> int:
    checks = 0
    base = initial_snapshot(bounds)
    assert_snapshot(base, bounds)

    reset_pre = apply_lifecycle(base, "reset", 1, "pre_commit", bounds)
    if (reset_pre.state, reset_pre.generation, reset_pre.epoch, reset_pre.high_water) != ("ONLINE", 1, 1, 2):
        raise ModelError("pre-commit reset changed the old identity or epoch")
    checks += 1
    if apply_lifecycle(reset_pre, "reset", 1, "none", bounds) != reset_pre:
        raise ModelError("duplicate reset was not idempotent")
    checks += 1
    reset_done = apply_lifecycle(reset_pre, "reset", 2, "none", bounds)
    assert_snapshot(reset_done, bounds)
    if (reset_done.generation, reset_done.epoch, reset_done.state) != (3, 2, "ONLINE"):
        raise ModelError("reset did not atomically retire and install")
    checks += 1

    loss = apply_lifecycle(base, "transport_loss", 1, "none", bounds)
    if (loss.state, loss.generation, loss.epoch) != ("LOST", 1, 1):
        raise ModelError("transport loss changed generation or epoch")
    recover_pre = apply_lifecycle(loss, "recover", 2, "pre_commit", bounds)
    if (recover_pre.state, recover_pre.generation, recover_pre.epoch, recover_pre.high_water) != ("LOST", 1, 1, 2):
        raise ModelError("pre-commit recovery changed the lost identity or epoch")
    checks += 2
    successful_recovery = apply_lifecycle(loss, "recover", 2, "none", bounds)
    recovered = apply_lifecycle(successful_recovery, "recover", 2, "none", bounds)
    assert_snapshot(recovered, bounds)
    if recovered.state != "ONLINE" or recovered.generation != 2:
        raise ModelError("recovery did not install a fresh generation")
    if recovered.epoch != 2 or 1 not in recovered.retired:
        raise ModelError("recovery did not retire the lost generation exactly once")
    checks += 2

    removed = apply_lifecycle(base, "remove", 1, "none", bounds)
    added = apply_lifecycle(removed, "add", 2, "post_commit", bounds)
    assert_snapshot(added, bounds)
    if (removed.state, removed.generation, removed.epoch) != ("ABSENT", 0, 2):
        raise ModelError("remove did not reach ABSENT with one retirement")
    if added.state != "LOST" or added.generation in added.retired:
        raise ModelError("add post-commit failure left an invalid current generation")
    checks += 2

    views = apply_view(apply_view(base, "capture_cuda"), "capture_nvml")
    replaced = apply_lifecycle(views, "reset", 1, "none", bounds)
    frozen = apply_view(replaced, "capture_cuda")
    if frozen.cuda_membership != (1,) or 1 not in frozen.cuda_lost:
        raise ModelError("initialized CUDA view admitted a replacement or missed loss")
    if frozen.nvml_membership != (1,) or 1 not in frozen.nvml_lost:
        raise ModelError("initialized NVML view admitted a replacement before reinit")
    if apply_view(frozen, "capture_cuda").cuda_membership != frozen.cuda_membership:
        raise ModelError("initialized CUDA view changed on a repeated capture")
    checks += 2
    reinitialized = apply_view(frozen, "nvml_reinit")
    if reinitialized.nvml_membership != (2,):
        raise ModelError("NVML zero-to-one reinitialization did not see the replacement")
    if reinitialized.nvml_lost or reinitialized.nvml_init_epoch != frozen.nvml_init_epoch + 1:
        raise ModelError("NVML reinitialization did not reset loss or advance its epoch")
    checks += 2

    exhausted_generation = replace(base, high_water=int(bounds["limits"]["generation_terminal"]) - 1,
                                    accepted=tuple(range(1, int(bounds["limits"]["generation_terminal"]))))
    rejected = apply_lifecycle(exhausted_generation, "reset", 1, "none", bounds)
    if rejected.generation != exhausted_generation.generation or rejected.epoch != exhausted_generation.epoch:
        raise ModelError("generation exhaustion changed identity or epoch")
    exhausted_epoch = replace(base, epoch=int(bounds["limits"]["epoch_terminal"]) - 1)
    rejected_remove = apply_lifecycle(exhausted_epoch, "remove", 1, "none", bounds)
    if rejected_remove.state != exhausted_epoch.state or rejected_remove.epoch != exhausted_epoch.epoch:
        raise ModelError("epoch exhaustion changed state or epoch")
    checks += 2
    return checks


def explore(bounds: dict[str, Any]) -> tuple[int, int, int, int, int]:
    max_depth = int(bounds["limits"]["max_depth"])
    max_states = int(bounds["limits"]["max_states"])
    root = initial_snapshot(bounds)
    seen: set[Snapshot] = {root}
    stack: list[tuple[Snapshot, int]] = [(root, 0)]
    transitions = 0
    complete_sequences = 0
    maximum_depth = 0
    while stack:
        snapshot, depth = stack.pop()
        assert_snapshot(snapshot, bounds)
        maximum_depth = max(maximum_depth, depth)
        actions = valid_actions(snapshot, bounds)
        if depth >= max_depth or not actions:
            complete_sequences += 1
            continue
        for action in reversed(actions):
            next_snapshot = apply_action(snapshot, action, bounds)
            if action[0] == "transport_loss" and next_snapshot.generation != snapshot.generation:
                raise ModelError("transport loss changed generation")
            if action[0] == "transport_loss" and next_snapshot.epoch != snapshot.epoch:
                raise ModelError("transport loss changed epoch")
            transitions += 1
            assert_snapshot(next_snapshot, bounds)
            if next_snapshot not in seen:
                seen.add(next_snapshot)
                if len(seen) > max_states:
                    raise ModelError(f"bounded exploration exceeded max_states={max_states}")
                stack.append((next_snapshot, depth + 1))
    return len(seen), transitions, complete_sequences, maximum_depth, len(valid_actions(root, bounds))


def run(base_path: Path, extension_path: Path, model_path: Path, bounds_path: Path,
        output_path: Path) -> dict[str, Any]:
    _base, _extension, model, bounds, hashes = validate_inputs(
        base_path, extension_path, model_path, bounds_path
    )
    scenario_checks = direct_scenarios(bounds)
    states, transitions, complete_sequences, maximum_depth, initial_actions = explore(bounds)
    transition_names = {entry["event"] for entry in model["transitions"]}
    covered = sorted(transition_names.intersection(set(LIFECYCLE_EVENTS)))
    if covered != sorted(LIFECYCLE_EVENTS):
        raise ModelError("not every canonical transition was explored")
    invariant_names = model["invariants"]
    evidence = {
        "schema_version": 1,
        "checker_version": CHECKER_VERSION,
        "status": "pass",
        "inputs": hashes,
        "normalized_command": [
            "python3", "tools/check-lifecycle-model.py",
            "--base-manifest", str(base_path), "--manifest", str(extension_path),
            "--model", str(model_path), "--bounds", str(bounds_path),
            "--output", str(output_path),
        ],
        "exploration": {
            "state_count": states,
            "transition_count": transitions,
            "complete_sequence_count": complete_sequences,
            "maximum_depth": maximum_depth,
            "initial_action_count": initial_actions,
            "bounds": bounds["limits"],
            "event_order": bounds["event_order"],
            "covered_transitions": covered,
        },
        "scenario_checks": scenario_checks,
        "invariants": [{"id": name, "status": "pass"} for name in invariant_names],
        "counterexamples": [],
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return evidence


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-manifest", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--bounds", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    try:
        base_path = arguments.base_manifest.resolve()
        extension_path = arguments.manifest.resolve()
        model_path = arguments.model.resolve()
        bounds_path = arguments.bounds.resolve()
        output_path = arguments.output.resolve()
        evidence = run(base_path, extension_path, model_path, bounds_path, output_path)
    except (ModelError, OSError) as error:
        print(f"lifecycle model: error: {error}", file=sys.stderr)
        return 1
    print(
        "lifecycle model: ok "
        f"({evidence['exploration']['state_count']} states, "
        f"{evidence['exploration']['transition_count']} transitions, "
        f"{evidence['scenario_checks']} direct checks)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
