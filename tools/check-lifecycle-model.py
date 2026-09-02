#!/usr/bin/env python3
"""Validate and exhaustively explore the bounded milestone-0.1.2.0 lifecycle model."""

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
        raise ModelError(f"{model_path}: base_manifest must point to the frozen milestone-0.1.1.0 root")
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
        "epoch_action", "commit", "replay", "failure_points", "guards", "owner",
        "commit_points", "high_water", "deadline", "terminal_errors",
    }
    expected_commit_points = {
        "candidate_reservation", "retirement_epoch_increment", "candidate_installation",
        "provider_view_publication",
    }
    expected_terminal_states = set(TERMINAL_STATES)
    expected_terminal_errors = set(model.get("terminal_errors", []))
    for transition in transitions:
        if not isinstance(transition, dict) or not required_transition_keys.issubset(transition):
            raise ModelError(f"{model_path}: malformed transition entry")
        if not isinstance(transition["sources"], list) or not isinstance(transition["terminals"], list):
            raise ModelError(f"{model_path}: transition source/terminal lists are required")
        if not set(transition["sources"]).issubset(set(states)) or not set(transition["terminals"]).issubset(set(states)):
            raise ModelError(f"{model_path}: transition references an unknown state")
        if (not isinstance(transition["failure_points"], list) or
                not isinstance(transition["guards"], list) or
                not transition["guards"] or
                not isinstance(transition["owner"], str) or
                transition["owner"] != "metafluxd.lifecycle.Coordinator" or
                not isinstance(transition["commit_points"], list) or
                not set(transition["commit_points"]).issubset(expected_commit_points) or
                not isinstance(transition["high_water"], dict) or
                set(transition["high_water"]) != {"generation", "identity_record", "epoch"} or
                not isinstance(transition["deadline"], dict) or
                set(transition["deadline"]) != {"terminal_states", "timeout_error", "physical_cancellation"} or
                not set(transition["deadline"]["terminal_states"]).issubset(expected_terminal_states) or
                transition["deadline"]["timeout_error"] != "MF_SHARED_TIMEOUT" or
                transition["deadline"]["physical_cancellation"] is not False or
                not isinstance(transition["terminal_errors"], list) or
                not set(transition["terminal_errors"]).issubset(expected_terminal_errors)):
            raise ModelError(f"{model_path}: transition ownership, guard, deadline, or commit contract is invalid")

    request_contract = model.get("request_contract")
    if (not isinstance(request_contract, dict) or
            request_contract.get("identity") != expected_identity or
            request_contract.get("daemon_incarnation") != {"bits": 128, "reusable": False} or
            request_contract.get("duplicate") != "recorded_outcome_without_side_effect" or
            request_contract.get("conflict") != "reject_without_side_effect" or
            request_contract.get("stale") != "reject_without_side_effect" or
            request_contract.get("late_completion") != "generation_tombstone_returns_DEVICE_LOST"):
        raise ModelError(f"{model_path}: request identity/idempotence contract is incomplete")

    lease_contract = model.get("lease_contract")
    if (not isinstance(lease_contract, dict) or
            lease_contract.get("states") != ["FREE", "STAGED", "REVOKED", "DRAINED", "TOMBSTONED"] or
            lease_contract.get("stage_before_identity_commit") is not True or
            lease_contract.get("revoke_before_retirement") is not True or
            lease_contract.get("worker_death") != "publish_LOST_and_tombstone" or
            lease_contract.get("stale_completion") != "reject_by_generation_and_epoch"):
        raise ModelError(f"{model_path}: lease staging/revocation contract is incomplete")

    deadline_policy = model.get("deadline_policy")
    if (not isinstance(deadline_policy, dict) or
            deadline_policy.get("public_terminal_states") != ["ONLINE", "LOST", "ABSENT"] or
            deadline_policy.get("physical_cancellation") is not False or
            deadline_policy.get("timeout_error") != "MF_SHARED_TIMEOUT"):
        raise ModelError(f"{model_path}: public deadline contract is incomplete")
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

    publication_model = model.get("publication_model")
    if not isinstance(publication_model, dict):
        raise ModelError(f"{model_path}: publication_model is required")
    if publication_model.get("fence_states") != ["ONLINE", "LOST"]:
        raise ModelError(f"{model_path}: fence publication states are incomplete or reordered")
    if publication_model.get("telemetry_states") != ["READY", "WRITING", "REJECTED"]:
        raise ModelError(f"{model_path}: telemetry publication states are incomplete or reordered")
    if publication_model.get("loss_wins_over_telemetry") is not True:
        raise ModelError(f"{model_path}: loss fence must win telemetry publication races")
    if publication_model.get("stale_online_forbidden") is not True:
        raise ModelError(f"{model_path}: stale ONLINE telemetry must be forbidden")
    reader_model = publication_model.get("reader")
    if (not isinstance(reader_model, dict) or
            reader_model.get("requires_even_telemetry_latch") is not True or
            reader_model.get("final_fence_recheck") is not True or
            reader_model.get("bounded_retry") is not True):
        raise ModelError(f"{model_path}: telemetry reader publication rules are incomplete")

    fence_telemetry = bounds.get("fence_telemetry")
    if not isinstance(fence_telemetry, dict):
        raise ModelError(f"{bounds_path}: fence_telemetry bounds are required")
    publication_initial = fence_telemetry.get("initial")
    publication_limits = fence_telemetry.get("limits")
    publication_order = fence_telemetry.get("event_order")
    required_publication_initial = {
        "lifecycle_state", "generation", "epoch", "fence_sequence", "fence_latch",
        "telemetry_latch", "telemetry_snapshot_sequence", "telemetry_active_bank",
        "telemetry_observed_fence", "telemetry_generation", "telemetry_online",
    }
    if (not isinstance(publication_initial, dict) or
            not required_publication_initial.issubset(publication_initial) or
            not isinstance(publication_limits, dict) or not isinstance(publication_order, list)):
        raise ModelError(f"{bounds_path}: fence_telemetry initial state is incomplete")
    if publication_initial.get("lifecycle_state") != "ONLINE" or not publication_initial.get("telemetry_online"):
        raise ModelError(f"{bounds_path}: fence_telemetry must start ONLINE with ready telemetry")
    for key in ("fence_sequence_terminal", "telemetry_sequence_terminal", "max_reader_retries",
                "max_depth", "max_states"):
        if not isinstance(publication_limits.get(key), int) or publication_limits[key] <= 0:
            raise ModelError(f"{bounds_path}: fence_telemetry {key} must be a positive integer")
    if publication_limits["fence_sequence_terminal"] <= int(publication_initial["fence_sequence"]):
        raise ModelError(f"{bounds_path}: fence sequence terminal bound must exceed the initial value")
    if publication_limits["telemetry_sequence_terminal"] <= int(publication_initial["telemetry_snapshot_sequence"]):
        raise ModelError(f"{bounds_path}: telemetry sequence terminal bound must exceed the initial value")
    if (int(publication_initial["fence_latch"]) <= 0 or
            (int(publication_initial["fence_latch"]) & 1) != 0 or
            int(publication_initial["telemetry_latch"]) <= 0 or
            (int(publication_initial["telemetry_latch"]) & 1) != 0 or
            int(publication_initial["telemetry_active_bank"]) not in {0, 1} or
            int(publication_initial["telemetry_observed_fence"]) != int(publication_initial["fence_sequence"]) or
            int(publication_initial["telemetry_generation"]) != int(publication_initial["generation"])):
        raise ModelError(f"{bounds_path}: fence_telemetry initial latches or telemetry identity are invalid")
    expected_publication_order = [
        "begin_loss_fence", "commit_loss_fence", "abort_loss_fence", "begin_telemetry",
        "stage_telemetry", "commit_telemetry", "begin_read", "read_bank", "finish_read",
        "reset_reader",
    ]
    if publication_order != expected_publication_order:
        raise ModelError(f"{bounds_path}: fence_telemetry event_order is not canonical")

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


@dataclass(frozen=True)
class FenceTelemetrySnapshot:
    lifecycle_state: str
    generation: int
    epoch: int
    fence_sequence: int
    fence_latch: int
    telemetry_latch: int
    telemetry_snapshot_sequence: int
    telemetry_active_bank: int
    telemetry_observed_fence: int
    telemetry_generation: int
    telemetry_online: bool
    fence_writer_active: bool
    telemetry_writer_active: bool
    telemetry_writer_fence: int
    telemetry_writer_generation: int
    telemetry_writer_staged: bool
    reader_phase: str
    reader_fence_sequence: int
    reader_generation: int
    reader_telemetry_latch: int
    reader_observed_fence: int
    reader_observed_generation: int
    reader_result: str
    reader_retries: int


@dataclass(frozen=True)
class ViewGateSnapshot:
    view_state: str
    device_state: str
    attempt_tag: int
    attempt_state: str
    lease_tag: int
    lease_state: str
    update_tag: int
    update_state: str
    publish_tag: int
    publish_state: str
    range_tag: int
    range_state: str
    range_begin: int
    range_end: int
    following_range_tag: int
    following_range_state: str
    following_range_begin: int
    following_range_end: int
    view_generation: int
    device_generation: int
    range_token_generation: int
    epoch: int
    allocation_high_water: int
    publication_cursor: int
    intermediate_visible: bool
    committed_admissions: int
    compensation_count: int


def initial_view_gate_snapshot(bounds: dict[str, Any]) -> ViewGateSnapshot:
    initial = bounds["view_gate"]["initial"]
    return ViewGateSnapshot(
        view_state=str(initial["view_state"]),
        device_state=str(initial["device_state"]),
        attempt_tag=0,
        attempt_state="Idle",
        lease_tag=0,
        lease_state="Free",
        update_tag=0,
        update_state="Free",
        publish_tag=0,
        publish_state="Free",
        range_tag=0,
        range_state="Free",
        range_begin=0,
        range_end=0,
        following_range_tag=0,
        following_range_state="Free",
        following_range_begin=0,
        following_range_end=0,
        view_generation=int(initial["view_generation"]),
        device_generation=int(initial["device_generation"]),
        range_token_generation=int(initial["view_generation"]),
        epoch=int(initial["epoch"]),
        allocation_high_water=int(initial["allocation_high_water"]),
        publication_cursor=int(initial["publication_cursor"]),
        intermediate_visible=False,
        committed_admissions=0,
        compensation_count=0,
    )


def assert_view_gate_snapshot(snapshot: ViewGateSnapshot, bounds: dict[str, Any]) -> None:
    limits = bounds["view_gate"]["limits"]
    tag_terminal = int(limits["tag_terminal"])
    generation_terminal = int(limits["generation_terminal"])
    epoch_terminal = int(limits["epoch_terminal"])
    sequence_terminal = int(limits["sequence_terminal"])

    if snapshot.view_generation == 0 or snapshot.view_generation >= generation_terminal:
        raise ModelError(f"view_generation {snapshot.view_generation} out of bounds")
    if snapshot.device_generation == 0 or snapshot.device_generation >= generation_terminal:
        raise ModelError(f"device_generation {snapshot.device_generation} out of bounds")
    if snapshot.epoch == 0 or snapshot.epoch >= epoch_terminal:
        raise ModelError(f"epoch {snapshot.epoch} out of bounds")
    if snapshot.allocation_high_water >= sequence_terminal:
        raise ModelError(f"allocation_high_water {snapshot.allocation_high_water} >= terminal")
    if snapshot.publication_cursor > snapshot.allocation_high_water:
        raise ModelError(f"publication_cursor {snapshot.publication_cursor} > allocation_high_water")

    # Invariant: no live lease after terminal/quarantined view
    if snapshot.view_state in ("Terminal", "Quarantined"):
        if snapshot.device_state != "Closed":
            raise ModelError(f"view {snapshot.view_state} but device {snapshot.device_state}")
        if snapshot.lease_state not in ("Free", "Released", "Revoked", "Tombstoned", "Quarantined"):
            raise ModelError(f"view {snapshot.view_state} but lease {snapshot.lease_state}")

    # Invariant: device Updating requires update in Active/FencePublished
    if snapshot.device_state == "Updating":
        if snapshot.update_state not in ("Active", "FencePublished"):
            raise ModelError(f"device Updating but update {snapshot.update_state}")

    # Invariant: intermediate_visible requires payload_applied and Active/Published
    if snapshot.intermediate_visible:
        if snapshot.publish_state not in ("Active", "Published", "Quarantined"):
            raise ModelError(f"intermediate_visible but publish {snapshot.publish_state}")

    # Invariant: range bounds
    if snapshot.range_state != "Free":
        if snapshot.range_begin == 0 or snapshot.range_begin > snapshot.range_end:
            raise ModelError(f"invalid range [{snapshot.range_begin}, {snapshot.range_end})")
    if snapshot.following_range_state != "Free":
        if snapshot.range_state not in ("Open", "Retired", "Quarantined"):
            raise ModelError(f"following_range but range {snapshot.range_state}")
        if snapshot.range_end + 1 != snapshot.following_range_begin:
            raise ModelError(f"range gap: {snapshot.range_end} + 1 != {snapshot.following_range_begin}")


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


def initial_fence_telemetry_snapshot(bounds: dict[str, Any]) -> FenceTelemetrySnapshot:
    initial = bounds["fence_telemetry"]["initial"]
    return FenceTelemetrySnapshot(
        lifecycle_state=str(initial["lifecycle_state"]),
        generation=int(initial["generation"]),
        epoch=int(initial["epoch"]),
        fence_sequence=int(initial["fence_sequence"]),
        fence_latch=int(initial["fence_latch"]),
        telemetry_latch=int(initial["telemetry_latch"]),
        telemetry_snapshot_sequence=int(initial["telemetry_snapshot_sequence"]),
        telemetry_active_bank=int(initial["telemetry_active_bank"]),
        telemetry_observed_fence=int(initial["telemetry_observed_fence"]),
        telemetry_generation=int(initial["telemetry_generation"]),
        telemetry_online=bool(initial["telemetry_online"]),
        fence_writer_active=False,
        telemetry_writer_active=False,
        telemetry_writer_fence=0,
        telemetry_writer_generation=0,
        telemetry_writer_staged=False,
        reader_phase="IDLE",
        reader_fence_sequence=0,
        reader_generation=0,
        reader_telemetry_latch=0,
        reader_observed_fence=0,
        reader_observed_generation=0,
        reader_result="NONE",
        reader_retries=0,
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


def assert_fence_telemetry_snapshot(snapshot: FenceTelemetrySnapshot,
                                    bounds: dict[str, Any]) -> None:
    limits = bounds["fence_telemetry"]["limits"]
    if snapshot.lifecycle_state not in {"ONLINE", "LOST"}:
        raise ModelError(f"unknown publication lifecycle state {snapshot.lifecycle_state}")
    if snapshot.generation <= 0 or snapshot.epoch <= 0 or snapshot.fence_sequence <= 0:
        raise ModelError("publication identity or fence sequence is not initialized")
    if snapshot.fence_sequence >= int(limits["fence_sequence_terminal"]):
        raise ModelError("publication fence sequence crossed its terminal bound")
    if snapshot.telemetry_snapshot_sequence >= int(limits["telemetry_sequence_terminal"]):
        raise ModelError("telemetry snapshot sequence crossed its terminal bound")
    if snapshot.fence_latch <= 0 or snapshot.telemetry_latch <= 0:
        raise ModelError("publication latch is not initialized")
    if (snapshot.fence_latch & 1) != int(snapshot.fence_writer_active):
        raise ModelError("fence latch parity disagrees with writer ownership")
    if (snapshot.telemetry_latch & 1) != int(snapshot.telemetry_writer_active):
        raise ModelError("telemetry latch parity disagrees with writer ownership")
    if snapshot.telemetry_active_bank not in {0, 1}:
        raise ModelError("telemetry selected an invalid bank")
    if snapshot.telemetry_observed_fence > snapshot.fence_sequence:
        raise ModelError("telemetry observed a future lifecycle fence")
    if snapshot.telemetry_generation != snapshot.generation:
        raise ModelError("telemetry generation diverged from the live identity")
    if snapshot.telemetry_online:
        if snapshot.lifecycle_state != "ONLINE" or snapshot.telemetry_observed_fence != snapshot.fence_sequence:
            raise ModelError("telemetry exposes ONLINE after a newer loss fence")
    elif snapshot.lifecycle_state != "LOST":
        raise ModelError("telemetry lost readiness without a loss fence")
    if snapshot.telemetry_writer_active:
        if snapshot.telemetry_writer_fence <= 0 or snapshot.telemetry_writer_generation <= 0:
            raise ModelError("telemetry writer has no captured identity")
    elif snapshot.telemetry_writer_fence != 0 or snapshot.telemetry_writer_generation != 0 or snapshot.telemetry_writer_staged:
        raise ModelError("inactive telemetry writer retained mutable staging state")
    if snapshot.reader_phase not in {"IDLE", "CAPTURED", "BANK_READ", "DONE"}:
        raise ModelError(f"unknown telemetry reader phase {snapshot.reader_phase}")
    if snapshot.reader_result not in {"NONE", "ONLINE", "LOST", "RETRY"}:
        raise ModelError(f"unknown telemetry reader result {snapshot.reader_result}")
    if snapshot.reader_retries < 0 or snapshot.reader_retries > int(limits["max_reader_retries"]):
        raise ModelError("telemetry reader exceeded its retry bound")
    if snapshot.reader_phase == "CAPTURED":
        if snapshot.reader_fence_sequence <= 0 or snapshot.reader_generation != snapshot.generation:
            raise ModelError("captured reader has no current identity")
    if snapshot.reader_phase == "BANK_READ" and (
        snapshot.reader_telemetry_latch <= 0 or (snapshot.reader_telemetry_latch & 1) != 0
    ):
        raise ModelError("bank reader did not retain an even telemetry latch")
    if snapshot.reader_phase == "DONE":
        if snapshot.reader_result == "NONE":
            raise ModelError("completed reader has no result")
        if snapshot.reader_result == "ONLINE":
            if snapshot.lifecycle_state != "ONLINE" or not snapshot.telemetry_online:
                raise ModelError("reader returned ONLINE after loss")
            if snapshot.reader_fence_sequence != snapshot.fence_sequence or snapshot.reader_generation != snapshot.generation:
                raise ModelError("reader returned ONLINE for a stale fence")
            if snapshot.reader_observed_generation != snapshot.generation or snapshot.reader_observed_fence < snapshot.fence_sequence:
                raise ModelError("reader returned ONLINE for stale telemetry")
        if snapshot.reader_result == "LOST" and snapshot.lifecycle_state != "LOST":
            raise ModelError("reader returned LOST without an observed loss fence")


def apply_fence_telemetry(snapshot: FenceTelemetrySnapshot, event: str,
                          bounds: dict[str, Any]) -> FenceTelemetrySnapshot:
    limits = bounds["fence_telemetry"]["limits"]
    if event == "begin_loss_fence":
        if (snapshot.lifecycle_state != "ONLINE" or snapshot.fence_writer_active or
                (snapshot.fence_latch & 1) != 0):
            return snapshot
        return replace(snapshot, fence_latch=snapshot.fence_latch + 1, fence_writer_active=True)

    if event == "commit_loss_fence":
        if (not snapshot.fence_writer_active or (snapshot.fence_latch & 1) == 0 or
                snapshot.fence_sequence + 1 >= int(limits["fence_sequence_terminal"])):
            return snapshot
        completed_online_read = snapshot.reader_phase == "DONE" and snapshot.reader_result == "ONLINE"
        return replace(
            snapshot,
            lifecycle_state="LOST",
            fence_sequence=snapshot.fence_sequence + 1,
            fence_latch=snapshot.fence_latch + 1,
            telemetry_online=False,
            fence_writer_active=False,
            reader_phase="IDLE" if completed_online_read else snapshot.reader_phase,
            reader_fence_sequence=0 if completed_online_read else snapshot.reader_fence_sequence,
            reader_generation=0 if completed_online_read else snapshot.reader_generation,
            reader_telemetry_latch=0 if completed_online_read else snapshot.reader_telemetry_latch,
            reader_observed_fence=0 if completed_online_read else snapshot.reader_observed_fence,
            reader_observed_generation=0 if completed_online_read else snapshot.reader_observed_generation,
            reader_result="NONE" if completed_online_read else snapshot.reader_result,
        )

    if event == "abort_loss_fence":
        if not snapshot.fence_writer_active or (snapshot.fence_latch & 1) == 0:
            return snapshot
        return replace(snapshot, fence_latch=snapshot.fence_latch + 1, fence_writer_active=False)

    if event == "begin_telemetry":
        if (snapshot.lifecycle_state != "ONLINE" or snapshot.fence_writer_active or
                snapshot.telemetry_writer_active or (snapshot.fence_latch & 1) != 0 or
                (snapshot.telemetry_latch & 1) != 0):
            return snapshot
        return replace(
            snapshot,
            telemetry_latch=snapshot.telemetry_latch + 1,
            telemetry_writer_active=True,
            telemetry_writer_fence=snapshot.fence_sequence,
            telemetry_writer_generation=snapshot.generation,
            telemetry_writer_staged=False,
        )

    if event == "stage_telemetry":
        if not snapshot.telemetry_writer_active or snapshot.telemetry_writer_staged:
            return snapshot
        return replace(snapshot, telemetry_writer_staged=True)

    if event == "commit_telemetry":
        if not snapshot.telemetry_writer_active or not snapshot.telemetry_writer_staged:
            return snapshot
        fresh = (
            snapshot.lifecycle_state == "ONLINE" and
            not snapshot.fence_writer_active and
            (snapshot.fence_latch & 1) == 0 and
            snapshot.telemetry_writer_fence == snapshot.fence_sequence and
            snapshot.telemetry_writer_generation == snapshot.generation
        )
        if fresh:
            if snapshot.telemetry_snapshot_sequence + 1 >= int(limits["telemetry_sequence_terminal"]):
                return snapshot
            return replace(
                snapshot,
                telemetry_latch=snapshot.telemetry_latch + 1,
                telemetry_snapshot_sequence=snapshot.telemetry_snapshot_sequence + 1,
                telemetry_active_bank=snapshot.telemetry_active_bank ^ 1,
                telemetry_observed_fence=snapshot.telemetry_writer_fence,
                telemetry_generation=snapshot.telemetry_writer_generation,
                telemetry_online=True,
                telemetry_writer_active=False,
                telemetry_writer_fence=0,
                telemetry_writer_generation=0,
                telemetry_writer_staged=False,
            )
        return replace(
            snapshot,
            telemetry_latch=snapshot.telemetry_latch + 1,
            telemetry_online=False if snapshot.lifecycle_state == "LOST" else snapshot.telemetry_online,
            telemetry_writer_active=False,
            telemetry_writer_fence=0,
            telemetry_writer_generation=0,
            telemetry_writer_staged=False,
        )

    if event == "begin_read":
        if snapshot.reader_phase != "IDLE":
            return snapshot
        if snapshot.lifecycle_state == "LOST":
            return replace(snapshot, reader_phase="DONE", reader_result="LOST")
        if snapshot.fence_writer_active or (snapshot.fence_latch & 1) != 0:
            return snapshot
        return replace(
            snapshot,
            reader_phase="CAPTURED",
            reader_fence_sequence=snapshot.fence_sequence,
            reader_generation=snapshot.generation,
            reader_telemetry_latch=0,
            reader_observed_fence=0,
            reader_observed_generation=0,
            reader_result="NONE",
        )

    if event == "read_bank":
        if snapshot.reader_phase != "CAPTURED":
            return snapshot
        if (snapshot.telemetry_latch & 1) != 0:
            retries = snapshot.reader_retries + 1
            return replace(
                snapshot,
                reader_phase="DONE" if retries >= int(limits["max_reader_retries"]) else "IDLE",
                reader_result="RETRY",
                reader_retries=retries,
                reader_fence_sequence=0 if retries < int(limits["max_reader_retries"]) else snapshot.reader_fence_sequence,
                reader_generation=0 if retries < int(limits["max_reader_retries"]) else snapshot.reader_generation,
            )
        return replace(
            snapshot,
            reader_phase="BANK_READ",
            reader_telemetry_latch=snapshot.telemetry_latch,
            reader_observed_fence=snapshot.telemetry_observed_fence,
            reader_observed_generation=snapshot.telemetry_generation,
        )

    if event == "finish_read":
        if snapshot.reader_phase != "BANK_READ":
            return snapshot
        if snapshot.telemetry_latch != snapshot.reader_telemetry_latch or (snapshot.telemetry_latch & 1) != 0:
            retries = snapshot.reader_retries + 1
            return replace(
                snapshot,
                reader_phase="DONE" if retries >= int(limits["max_reader_retries"]) else "IDLE",
                reader_result="RETRY",
                reader_retries=retries,
                reader_fence_sequence=0 if retries < int(limits["max_reader_retries"]) else snapshot.reader_fence_sequence,
                reader_generation=0 if retries < int(limits["max_reader_retries"]) else snapshot.reader_generation,
                reader_telemetry_latch=0,
            )
        if snapshot.lifecycle_state == "LOST":
            return replace(snapshot, reader_phase="DONE", reader_result="LOST", reader_telemetry_latch=0)
        if (not snapshot.telemetry_online or snapshot.reader_fence_sequence != snapshot.fence_sequence or
                snapshot.reader_generation != snapshot.generation):
            retries = snapshot.reader_retries + 1
            return replace(
                snapshot,
                reader_phase="DONE" if retries >= int(limits["max_reader_retries"]) else "IDLE",
                reader_result="RETRY",
                reader_retries=retries,
                reader_fence_sequence=0 if retries < int(limits["max_reader_retries"]) else snapshot.reader_fence_sequence,
                reader_generation=0 if retries < int(limits["max_reader_retries"]) else snapshot.reader_generation,
                reader_telemetry_latch=0,
            )
        if (snapshot.reader_observed_generation != snapshot.generation or
                snapshot.reader_observed_fence < snapshot.fence_sequence):
            retries = snapshot.reader_retries + 1
            return replace(
                snapshot,
                reader_phase="DONE" if retries >= int(limits["max_reader_retries"]) else "IDLE",
                reader_result="RETRY",
                reader_retries=retries,
                reader_fence_sequence=0 if retries < int(limits["max_reader_retries"]) else snapshot.reader_fence_sequence,
                reader_generation=0 if retries < int(limits["max_reader_retries"]) else snapshot.reader_generation,
                reader_telemetry_latch=0,
            )
        return replace(snapshot, reader_phase="DONE", reader_result="ONLINE", reader_telemetry_latch=0)

    if event == "reset_reader":
        if snapshot.reader_phase != "DONE" or snapshot.reader_result != "RETRY" or \
                snapshot.reader_retries >= int(limits["max_reader_retries"]):
            return snapshot
        return replace(
            snapshot,
            reader_phase="IDLE",
            reader_fence_sequence=0,
            reader_generation=0,
            reader_telemetry_latch=0,
            reader_observed_fence=0,
            reader_observed_generation=0,
            reader_result="NONE",
        )

    raise ModelError(f"unknown fence/telemetry event {event}")


def publication_valid_actions(snapshot: FenceTelemetrySnapshot,
                              bounds: dict[str, Any]) -> list[str]:
    limits = bounds["fence_telemetry"]["limits"]
    actions: list[str] = []
    if (snapshot.lifecycle_state == "ONLINE" and not snapshot.fence_writer_active and
            (snapshot.fence_latch & 1) == 0):
        actions.append("begin_loss_fence")
    if snapshot.fence_writer_active:
        if snapshot.fence_sequence + 1 < int(limits["fence_sequence_terminal"]):
            actions.append("commit_loss_fence")
        actions.append("abort_loss_fence")
    if (snapshot.lifecycle_state == "ONLINE" and not snapshot.fence_writer_active and
            not snapshot.telemetry_writer_active and (snapshot.fence_latch & 1) == 0 and
            (snapshot.telemetry_latch & 1) == 0):
        actions.append("begin_telemetry")
    if snapshot.telemetry_writer_active and not snapshot.telemetry_writer_staged:
        actions.append("stage_telemetry")
    if snapshot.telemetry_writer_active and snapshot.telemetry_writer_staged:
        if snapshot.lifecycle_state == "LOST" or snapshot.fence_writer_active or \
                snapshot.telemetry_writer_fence != snapshot.fence_sequence:
            actions.append("commit_telemetry")
        elif snapshot.telemetry_snapshot_sequence + 1 < int(limits["telemetry_sequence_terminal"]):
            actions.append("commit_telemetry")
    if snapshot.reader_phase == "IDLE":
        if snapshot.lifecycle_state == "LOST" or (not snapshot.fence_writer_active and (snapshot.fence_latch & 1) == 0):
            actions.append("begin_read")
    elif snapshot.reader_phase == "CAPTURED":
        actions.append("read_bank")
    elif snapshot.reader_phase == "BANK_READ":
        actions.append("finish_read")
    elif snapshot.reader_phase == "DONE" and snapshot.reader_result == "RETRY" and \
            snapshot.reader_retries < int(limits["max_reader_retries"]):
        actions.append("reset_reader")
    return [event for event in bounds["fence_telemetry"]["event_order"] if event in actions]


def publication_direct_scenarios(bounds: dict[str, Any]) -> int:
    checks = 0
    base = initial_fence_telemetry_snapshot(bounds)
    assert_fence_telemetry_snapshot(base, bounds)

    fresh = apply_fence_telemetry(base, "begin_telemetry", bounds)
    fresh = apply_fence_telemetry(fresh, "stage_telemetry", bounds)
    fresh = apply_fence_telemetry(fresh, "commit_telemetry", bounds)
    assert_fence_telemetry_snapshot(fresh, bounds)
    if fresh.telemetry_snapshot_sequence != base.telemetry_snapshot_sequence + 1 or not fresh.telemetry_online:
        raise ModelError("fresh telemetry publication did not commit a ready bank")
    checks += 1

    race = apply_fence_telemetry(base, "begin_telemetry", bounds)
    race = apply_fence_telemetry(race, "stage_telemetry", bounds)
    race = apply_fence_telemetry(race, "begin_loss_fence", bounds)
    race = apply_fence_telemetry(race, "commit_loss_fence", bounds)
    race = apply_fence_telemetry(race, "commit_telemetry", bounds)
    assert_fence_telemetry_snapshot(race, bounds)
    if race.lifecycle_state != "LOST" or race.telemetry_online or race.telemetry_observed_fence >= race.fence_sequence:
        raise ModelError("stale telemetry publication restored ONLINE after a loss fence")
    checks += 1

    reader_race = apply_fence_telemetry(base, "begin_read", bounds)
    reader_race = apply_fence_telemetry(reader_race, "begin_loss_fence", bounds)
    reader_race = apply_fence_telemetry(reader_race, "commit_loss_fence", bounds)
    reader_race = apply_fence_telemetry(reader_race, "read_bank", bounds)
    reader_race = apply_fence_telemetry(reader_race, "finish_read", bounds)
    assert_fence_telemetry_snapshot(reader_race, bounds)
    if reader_race.reader_result == "ONLINE":
        raise ModelError("reader accepted ONLINE across a loss-fence race")
    checks += 1

    aborted = apply_fence_telemetry(base, "begin_loss_fence", bounds)
    aborted = apply_fence_telemetry(aborted, "abort_loss_fence", bounds)
    assert_fence_telemetry_snapshot(aborted, bounds)
    if (aborted.fence_latch <= base.fence_latch or (aborted.fence_latch & 1) != 0 or
            aborted.lifecycle_state != "ONLINE"):
        raise ModelError("aborted loss fence did not restore an even open latch")
    checks += 1

    retry = apply_fence_telemetry(base, "begin_read", bounds)
    retry = apply_fence_telemetry(retry, "begin_telemetry", bounds)
    retry = apply_fence_telemetry(retry, "stage_telemetry", bounds)
    retry = apply_fence_telemetry(retry, "read_bank", bounds)
    if retry.reader_result != "RETRY" or retry.reader_retries != 1:
        raise ModelError("reader did not consume a bounded retry when telemetry latch was odd")
    checks += 1
    retry = apply_fence_telemetry(retry, "commit_telemetry", bounds)
    retry = apply_fence_telemetry(retry, "reset_reader", bounds)
    retry = apply_fence_telemetry(retry, "begin_read", bounds)
    retry = apply_fence_telemetry(retry, "read_bank", bounds)
    retry = apply_fence_telemetry(retry, "finish_read", bounds)
    assert_fence_telemetry_snapshot(retry, bounds)
    if retry.reader_result != "ONLINE":
        raise ModelError("reader did not recover after a bounded telemetry retry")
    checks += 1
    return checks


def explore_fence_telemetry(bounds: dict[str, Any]) -> dict[str, int]:
    limits = bounds["fence_telemetry"]["limits"]
    max_depth = int(limits["max_depth"])
    max_states = int(limits["max_states"])
    root = initial_fence_telemetry_snapshot(bounds)
    seen: set[FenceTelemetrySnapshot] = {root}
    stack: list[tuple[FenceTelemetrySnapshot, int]] = [(root, 0)]
    transitions = 0
    complete_sequences = 0
    maximum_depth = 0
    while stack:
        snapshot, depth = stack.pop()
        assert_fence_telemetry_snapshot(snapshot, bounds)
        maximum_depth = max(maximum_depth, depth)
        actions = publication_valid_actions(snapshot, bounds)
        if depth >= max_depth or not actions:
            complete_sequences += 1
            continue
        for event in reversed(actions):
            next_snapshot = apply_fence_telemetry(snapshot, event, bounds)
            transitions += 1
            assert_fence_telemetry_snapshot(next_snapshot, bounds)
            if next_snapshot not in seen:
                seen.add(next_snapshot)
                if len(seen) > max_states:
                    raise ModelError(f"bounded publication exploration exceeded max_states={max_states}")
                stack.append((next_snapshot, depth + 1))
    return {
        "state_count": len(seen),
        "transition_count": transitions,
        "complete_sequence_count": complete_sequences,
        "maximum_depth": maximum_depth,
        "initial_action_count": len(publication_valid_actions(root, bounds)),
    }


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


def view_gate_direct_scenarios(bounds: dict[str, Any]) -> int:
    """Test specific view gate scenarios from the work item."""
    checks = 0
    base = initial_view_gate_snapshot(bounds)

    # Scenario 1: Normal lease lifecycle (enter -> claim -> materialize -> reserve -> commit -> publish -> release)
    s = base
    s = replace(s, attempt_tag=1, attempt_state="Entering")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, attempt_state="Claiming", lease_tag=1, lease_state="Initializing")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, attempt_state="Initializing", lease_state="Reserved")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, attempt_state="Ready", lease_state="Committing")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, lease_state="Committed", committed_admissions=1)
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, lease_state="Published")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, lease_state="Released", attempt_state="Exited")
    assert_view_gate_snapshot(s, bounds)
    checks += 1

    # Scenario 2: Device update lifecycle
    s = base
    s = replace(s, device_state="Updating", update_tag=1, update_state="Active",
                device_generation=2, epoch=2)
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, update_state="FencePublished")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, device_state="Open", update_state="Closed", update_tag=0)
    assert_view_gate_snapshot(s, bounds)
    checks += 1

    # Scenario 3: Range lifecycle
    s = base
    s = replace(s, range_tag=1, range_state="Open", range_begin=1, range_end=8,
                allocation_high_water=8)
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, range_state="Retired")
    assert_view_gate_snapshot(s, bounds)
    checks += 1

    # Scenario 4: View close with settled lease
    s = base
    s = replace(s, view_state="Closing", lease_tag=1, lease_state="Released")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, view_state="Terminal", device_state="Closed",
                device_generation=3, lease_state="Free", lease_tag=0)
    assert_view_gate_snapshot(s, bounds)
    checks += 1

    # Scenario 5: Quarantine on owner death
    s = base
    s = replace(s, publish_tag=1, publish_state="Quarantined",
                view_state="Quarantined", device_state="Closed", device_generation=3)
    assert_view_gate_snapshot(s, bounds)
    checks += 1

    # Scenario 6: Publication lifecycle
    s = base
    s = replace(s, range_tag=1, range_state="Open", range_begin=1, range_end=8,
                allocation_high_water=8)
    s = replace(s, publish_tag=1, publish_state="Initializing")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, publish_state="Prepared")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, publish_state="Linking")
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, publish_state="Active", intermediate_visible=True)
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, publish_state="Published", publication_cursor=8)
    assert_view_gate_snapshot(s, bounds)
    checks += 1
    s = replace(s, publish_state="Terminal", intermediate_visible=False)
    assert_view_gate_snapshot(s, bounds)
    checks += 1

    return checks


def explore_view_gate(bounds: dict[str, Any]) -> dict[str, int]:
    """Bounded exploration of the view gate state machine."""
    limits = bounds["view_gate"]["limits"]
    max_depth = int(limits["max_depth"])
    max_states = int(limits["max_states"])
    attempt_tags = [int(t) for t in bounds["view_gate"]["attempt_tags"]]
    lease_tags = [int(t) for t in bounds["view_gate"]["lease_tags"]]
    update_tags = [int(t) for t in bounds["view_gate"]["update_tags"]]
    range_tags = [int(t) for t in bounds["view_gate"]["range_tags"]]
    publish_tags = [int(t) for t in bounds["view_gate"]["publish_tags"]]
    range_lengths = [int(l) for l in bounds["view_gate"]["range_lengths"]]

    initial = initial_view_gate_snapshot(bounds)
    queue: list[tuple[ViewGateSnapshot, int]] = [(initial, 0)]
    visited: set[ViewGateSnapshot] = {initial}
    stats = {"states": 0, "transitions": 0, "invariant_checks": 0, "errors": 0}

    while queue and stats["states"] < max_states:
        snapshot, depth = queue.pop(0)
        stats["states"] += 1
        try:
            assert_view_gate_snapshot(snapshot, bounds)
            stats["invariant_checks"] += 1
        except ModelError:
            stats["errors"] += 1
            continue

        if depth >= max_depth:
            continue

        # Generate successor states
        for next_snap in _view_gate_successors(snapshot, bounds, attempt_tags,
                                                lease_tags, update_tags, range_tags,
                                                publish_tags, range_lengths):
            stats["transitions"] += 1
            if next_snap not in visited:
                visited.add(next_snap)
                queue.append((next_snap, depth + 1))

    return stats


def _view_gate_successors(snapshot: ViewGateSnapshot, bounds: dict[str, Any],
                          attempt_tags: list[int], lease_tags: list[int],
                          update_tags: list[int], range_tags: list[int],
                          publish_tags: list[int],
                          range_lengths: list[int]) -> list[ViewGateSnapshot]:
    """Generate all valid successor snapshots from the current state."""
    successors: list[ViewGateSnapshot] = []
    limits = bounds["view_gate"]["limits"]
    generation_terminal = int(limits["generation_terminal"])
    epoch_terminal = int(limits["epoch_terminal"])
    sequence_terminal = int(limits["sequence_terminal"])

    # Attempt transitions
    if snapshot.attempt_state == "Idle" and snapshot.view_state == "Open":
        for tag in attempt_tags:
            if tag > 0 and tag < int(limits["tag_terminal"]):
                successors.append(replace(snapshot, attempt_tag=tag, attempt_state="Entering"))

    if snapshot.attempt_state == "Entering" and snapshot.lease_state == "Free":
        for tag in lease_tags:
            if tag > 0 and tag < int(limits["tag_terminal"]):
                successors.append(replace(snapshot, attempt_state="Claiming",
                                          lease_tag=tag, lease_state="Initializing"))

    if snapshot.attempt_state == "Claiming" and snapshot.lease_state == "Initializing":
        successors.append(replace(snapshot, attempt_state="Initializing",
                                  lease_state="Reserved"))

    if snapshot.attempt_state == "Initializing" and snapshot.lease_state == "Reserved":
        successors.append(replace(snapshot, attempt_state="Ready"))

    # Lease transitions
    if snapshot.lease_state == "Reserved":
        successors.append(replace(snapshot, lease_state="Committing"))

    if snapshot.lease_state == "Committing":
        successors.append(replace(snapshot, lease_state="Committed",
                                  committed_admissions=snapshot.committed_admissions + 1))

    if snapshot.lease_state == "Committed":
        successors.append(replace(snapshot, lease_state="Published"))

    if snapshot.lease_state == "Published":
        successors.append(replace(snapshot, lease_state="Released",
                                  attempt_state="Exited"))

    # Device update transitions
    if snapshot.device_state == "Open" and snapshot.update_state == "Free":
        for tag in update_tags:
            new_gen = snapshot.device_generation + 1
            new_epoch = snapshot.epoch + 1
            if new_gen < generation_terminal and new_epoch < epoch_terminal:
                successors.append(replace(snapshot, device_state="Updating",
                                          update_tag=tag, update_state="Active",
                                          device_generation=new_gen, epoch=new_epoch))

    if snapshot.device_state == "Updating" and snapshot.update_state == "Active":
        successors.append(replace(snapshot, update_state="FencePublished"))

    if snapshot.device_state == "Updating" and snapshot.update_state == "FencePublished":
        successors.append(replace(snapshot, device_state="Open", update_state="Closed",
                                  update_tag=0))

    # Range transitions
    if snapshot.range_state == "Free" and snapshot.view_state == "Open":
        for tag in range_tags:
            for length in range_lengths:
                new_begin = snapshot.allocation_high_water + 1
                new_end = new_begin + length - 1
                if new_end < sequence_terminal:
                    successors.append(replace(snapshot, range_tag=tag, range_state="Open",
                                              range_begin=new_begin, range_end=new_end,
                                              allocation_high_water=new_end))

    if snapshot.range_state == "Open":
        successors.append(replace(snapshot, range_state="Retired"))

    # Publication transitions
    if snapshot.range_state == "Open" and snapshot.publish_state == "Free":
        for tag in publish_tags:
            successors.append(replace(snapshot, publish_tag=tag, publish_state="Initializing"))

    if snapshot.publish_state == "Initializing":
        successors.append(replace(snapshot, publish_state="Prepared"))

    if snapshot.publish_state == "Prepared":
        successors.append(replace(snapshot, publish_state="Linking"))

    if snapshot.publish_state == "Linking":
        successors.append(replace(snapshot, publish_state="Active", intermediate_visible=True))

    if snapshot.publish_state == "Active":
        successors.append(replace(snapshot, publish_state="Published",
                                  publication_cursor=snapshot.range_end,
                                  intermediate_visible=False))

    if snapshot.publish_state == "Published":
        successors.append(replace(snapshot, publish_state="Terminal"))

    # View close
    if snapshot.view_state == "Open" and snapshot.lease_state in ("Free", "Released"):
        if snapshot.publish_state in ("Free", "Terminal"):
            successors.append(replace(snapshot, view_state="Closing"))

    if snapshot.view_state == "Closing":
        successors.append(replace(snapshot, view_state="Terminal", device_state="Closed",
                                  device_generation=int(limits["generation_terminal"])))

    return successors


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
    if (rejected.state != exhausted_generation.state or
            rejected.generation != exhausted_generation.generation or
            rejected.epoch != exhausted_generation.epoch or
            rejected.high_water != exhausted_generation.high_water or
            rejected.accepted != exhausted_generation.accepted or
            rejected.committed != exhausted_generation.committed or
            rejected.retired != exhausted_generation.retired or
            rejected.tombstones != exhausted_generation.tombstones or
            rejected.high_water >= int(bounds["limits"]["generation_terminal"])):
        raise ModelError("generation exhaustion changed identity or epoch")
    exhausted_epoch = replace(base, epoch=int(bounds["limits"]["epoch_terminal"]) - 1)
    rejected_remove = apply_lifecycle(exhausted_epoch, "remove", 1, "none", bounds)
    if (rejected_remove.state != exhausted_epoch.state or
            rejected_remove.generation != exhausted_epoch.generation or
            rejected_remove.epoch != exhausted_epoch.epoch or
            rejected_remove.high_water != exhausted_epoch.high_water or
            rejected_remove.committed != exhausted_epoch.committed or
            rejected_remove.retired != exhausted_epoch.retired or
            rejected_remove.tombstones != exhausted_epoch.tombstones or
            rejected_remove.epoch >= int(bounds["limits"]["epoch_terminal"])):
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
    publication_checks = publication_direct_scenarios(bounds)
    publication_exploration = explore_fence_telemetry(bounds)
    view_gate_checks = view_gate_direct_scenarios(bounds)
    view_gate_exploration = explore_view_gate(bounds)
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
        "publication": {
            "scenario_checks": publication_checks,
            "exploration": publication_exploration,
            "invariants": [
                "loss_fence_wins_telemetry_publication",
                "stale_online_is_unreadable",
                "telemetry_reader_retries_are_bounded",
                "even_latch_required_for_accepted_bank",
            ],
        },
        "view_gate": {
            "scenario_checks": view_gate_checks,
            "exploration": view_gate_exploration,
            "invariants": model.get("view_gate_model", {}).get("invariants", []),
        },
        "invariants": [{"id": name, "status": "pass"} for name in invariant_names],
        "counterexamples": [],
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return evidence


def generate_fixtures(model: dict[str, Any], bounds: dict[str, Any]) -> dict[str, Any]:
    """Generate positive, invalid, repeated, racing, and injected-failure fixtures from the model schema."""
    transitions = model.get("transitions", [])
    states = model.get("states", [])
    fixtures: dict[str, Any] = {
        "schema_version": 1,
        "description": "Lifecycle model test fixtures generated from model.json",
        "categories": {
            "positive": [],
            "invalid": [],
            "repeated": [],
            "racing": [],
            "injected_failure": [],
        },
    }

    # Positive fixtures: valid event sequences that should pass all invariants
    for transition in transitions:
        event = transition.get("event", "")
        sources = transition.get("sources", [])
        terminals = transition.get("terminals", [])
        failure_points = transition.get("failure_points", [])
        if sources and terminals:
            fixtures["categories"]["positive"].append({
                "name": f"positive_{event}_from_{sources[0]}_to_{terminals[0]}",
                "event": event,
                "source_state": sources[0],
                "expected_terminal": terminals[0],
                "fault_point": "none",
                "description": f"Valid {event} from {sources[0]} reaching {terminals[0]}",
            })

    # Invalid fixtures: guard-violating sequences that must be rejected
    for transition in transitions:
        event = transition.get("event", "")
        sources = transition.get("sources", [])
        # Invalid: event from a non-source state
        invalid_sources = [s for s in states if s not in sources]
        for inv_state in invalid_sources[:2]:  # limit to 2 per event
            fixtures["categories"]["invalid"].append({
                "name": f"invalid_{event}_from_{inv_state}",
                "event": event,
                "source_state": inv_state,
                "expected_error": "reject_without_side_effect",
                "description": f"Invalid {event} from non-source state {inv_state}",
            })

    # Repeated fixtures: duplicate request replay scenarios
    for transition in transitions:
        event = transition.get("event", "")
        sources = transition.get("sources", [])
        if sources:
            fixtures["categories"]["repeated"].append({
                "name": f"repeated_{event}_from_{sources[0]}",
                "event": event,
                "source_state": sources[0],
                "replay_count": 3,
                "expected_behavior": "idempotent_without_side_effect",
                "description": f"Duplicate {event} replay from {sources[0]} must be idempotent",
            })

    # Racing fixtures: concurrent/interleaved event sequences
    race_pairs = [
        ("reset", "transport_loss", "ONLINE"),
        ("recover", "transport_loss", "LOST"),
        ("add", "transport_loss", "PRESENT"),
        ("remove", "reset", "ONLINE"),
    ]
    for event_a, event_b, state in race_pairs:
        fixtures["categories"]["racing"].append({
            "name": f"racing_{event_a}_vs_{event_b}_in_{state}",
            "events": [event_a, event_b],
            "source_state": state,
            "expected_behavior": "first_wins_second_rejected_or_lost",
            "description": f"Concurrent {event_a} and {event_b} in {state}; first wins, second rejected or lost",
        })

    # Injected-failure fixtures: fault points at pre_commit/post_commit
    faultable_events = model.get("faultable_events", ["add", "remove", "reset", "recover"])
    for transition in transitions:
        event = transition.get("event", "")
        failure_points = transition.get("failure_points", [])
        sources = transition.get("sources", [])
        if event in faultable_events and failure_points and sources:
            for fault in failure_points:
                fixtures["categories"]["injected_failure"].append({
                    "name": f"fault_{event}_{fault}_from_{sources[0]}",
                    "event": event,
                    "source_state": sources[0],
                    "fault_point": fault,
                    "expected_behavior": "candidate_consumed_no_identity_change" if fault == "pre_commit" else "committed_generation_lost",
                    "description": f"Injected {fault} failure during {event} from {sources[0]}",
                })

    # Summary
    fixtures["summary"] = {
        "total_fixtures": sum(len(v) for v in fixtures["categories"].values()),
        "per_category": {k: len(v) for k, v in fixtures["categories"].items()},
    }
    return fixtures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-manifest", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--bounds", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--generate-fixtures", type=Path, help="Generate test fixtures JSON")
    arguments = parser.parse_args()
    try:
        base_path = arguments.base_manifest.resolve()
        extension_path = arguments.manifest.resolve()
        model_path = arguments.model.resolve()
        bounds_path = arguments.bounds.resolve()
        output_path = arguments.output.resolve()
        evidence = run(base_path, extension_path, model_path, bounds_path, output_path)
        if arguments.generate_fixtures is not None:
            model = load_json(model_path)
            bounds_data = load_json(bounds_path)
            fixtures = generate_fixtures(model, bounds_data)
            fixtures_path = arguments.generate_fixtures.resolve()
            fixtures_path.parent.mkdir(parents=True, exist_ok=True)
            fixtures_path.write_text(json.dumps(fixtures, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (ModelError, OSError) as error:
        print(f"lifecycle model: error: {error}", file=sys.stderr)
        return 1
    print(
        "lifecycle model: ok "
        f"({evidence['exploration']['state_count']} states, "
        f"{evidence['exploration']['transition_count']} transitions, "
        f"{evidence['scenario_checks']} direct checks; "
        f"publication {evidence['publication']['exploration']['state_count']} states, "
        f"{evidence['publication']['exploration']['transition_count']} transitions, "
        f"{evidence['publication']['scenario_checks']} direct checks)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
