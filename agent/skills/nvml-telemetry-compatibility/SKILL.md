---
name: nvml-telemetry-compatibility
description: Implement or review libnvidia-ml.so compatibility for NVML lifecycle, versioned APIs, count/fill queries, telemetry snapshots, process reporting, and stock nvidia-smi qualification. Use for milestone-0.1.0.0 NVML provider work or NVML-visible milestone-0.1.2.0 lifecycle behavior. Do not use for CUDA execution, PTX lowering, or invented physical telemetry.
---

# NVML Telemetry Compatibility

## Implementation Focus

For an implementation request, use the shared
[implementation guidance](../review/references/implementation-guidance.md).
Select the affected inputs and obligations below; broad qualification lists
do not make every invocation a new inventory or full-suite run.

Start from a requested stock-tool query or setter and trace it to the actual
producer, snapshot and API mapping. Implement that path and its freshness/error
behavior together. Reuse the symbol and field matrices; a missing producer
needs an implementation in its owning layer or an explicit unsupported field,
not plausible data. An additional N/A result is not new measured telemetry.

## Inputs

- The active milestone/work item and selected NVML header/version matrix.
- The NVML symbol manifest, registry snapshot schema, policy setter contract,
  telemetry producers, and exact `nvidia-smi` versions/views under test.
- CUDA identity evidence when cross-provider parity is in scope.

Treat telemetry provenance as an input. A field without a real producer is not a
license to synthesize a plausible value.

## Routing

- Use [lifecycle and versioning](references/lifecycle-and-versioning.md) for
  initialization, shutdown, aliases, structures, and count/fill APIs.
- Use [telemetry contracts](references/telemetry-contracts.md) for snapshot
  freshness, identity, utilization, memory, process, and setter behavior.
- Use [nvidia-smi qualification](references/nvidia-smi-qualification.md) for
  stock-tool coverage and evidence.
- Route CUDA object/execution behavior to `$cuda-driver-abi-compatibility`, PCI
  config identity to `$pcie-vpci-device-model`, and reset generation behavior to
  `$device-lifecycle-resilience`.
- Compose `$runtime-contracts-registry` when process-view membership, ordering,
  `registry_view_id`, or provider freeze rules change. This skill owns NVML-visible
  mapping/errors and consumes the shared-view contract rather than redefining it.

## Workflow

1. Freeze the selected NVML headers and stock-tool matrix; generate the complete
   target symbol and structure manifest before implementing behavior.
2. Define one thread-safe initialization reference count and shutdown state
   shared by all entry points. Loading the DSO itself remains inert.
3. Map each API to a registry or telemetry field with source, unit, update
   interval, age policy, permission, unsupported behavior, and per-field error.
4. Implement versioned handles/structures and count-then-fill contracts with
   exact null, zero-capacity, short-capacity, and concurrent-change semantics.
5. Read metrics getters from an immutable shared telemetry snapshot, but validate
   device liveness/generation and effective policy against the runtime lifecycle
   fence with no stale fallback. Route setters through the control plane and
   acknowledge only after policy is effective in that fence.
6. Enumerate the membership revision captured at NVML's zero-to-one init in
   canonical registry order. Default unfiltered CUDA parity applies only when both
   providers captured the same process-view revision. Under a CUDA filter or
   different lifecycle revision, require no count/ordinal parity and correlate
   only common live incarnations by `(UUID, generation)`; UUID/logical ID tracks
   persistent identity and BDF alone is insufficient. Return
   `NVML_ERROR_NOT_SUPPORTED` and expose `N/A` for unsupported physical fields
   instead of fabricating measurements.
7. Drive implementation and tests from the stock `nvidia-smi` call traces and
   selected manifest, including XML/CSV structure and per-field failures.

## Output

Select the applicable outputs for the requested task:

- A version/symbol/structure matrix and API-to-field provenance table.
- Lifecycle, snapshot freshness, count/fill, and setter completion semantics.
- A stock-tool coverage matrix listing supported views and explicit `N/A`
  fields.
- Qualification commands and results for every target header/tool family.

## Verification

- Compare exact symbols, aliases, structure layouts, and provider dependencies
  with the pinned manifest.
- Test repeated/concurrent init and shutdown, pre-init/post-shutdown calls,
  invalid handles, short buffers, count changes, and per-field errors.
- Run supported stock `nvidia-smi -L`, summary, CSV, `compute-apps`, and required
  `-q/-x` queries for zero, one, and multiple devices.
- Verify default, unfiltered CUDA and NVML count/order when they capture the same
  process-view revision. With `CUDA_VISIBLE_DEVICES` or different provider init
  revisions, verify common live incarnations by `(UUID, generation)` and do not
  require ordinal/count parity. Never treat UUID, logical ID, or BDF alone as a
  live-incarnation match after replacement.
- Measure hot getter and polling overhead from archived raw samples; do not infer
  performance from a unit test.
