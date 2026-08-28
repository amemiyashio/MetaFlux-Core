---
name: nvml-telemetry-compatibility
description: Design or review libnvidia-ml.so compatibility for NVML lifecycle, versioned APIs, count/fill queries, telemetry snapshots, process reporting, and stock nvidia-smi qualification. Use for M0001 NVML provider work. Do not use for CUDA execution, PTX lowering, or invented physical telemetry.
---

# NVML Telemetry Compatibility

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

## Workflow

1. Freeze the selected NVML headers and stock-tool matrix; generate the complete
   target symbol and structure manifest before implementing behavior.
2. Define one thread-safe initialization reference count and shutdown state
   shared by all entry points. Loading the DSO itself remains inert.
3. Map each API to a registry or telemetry field with source, unit, update
   interval, age policy, permission, unsupported behavior, and per-field error.
4. Implement versioned handles/structures and count-then-fill contracts with
   exact null, zero-capacity, short-capacity, and concurrent-change semantics.
5. Read hot getters from an immutable shared snapshot. Route setters through the
   control plane and acknowledge only after policy is effective.
6. Preserve identity and ordering parity with CUDA inside one registry view.
   Return `NVML_ERROR_NOT_SUPPORTED` and expose `N/A` for unsupported physical
   fields instead of fabricating measurements.
7. Drive implementation and tests from the stock `nvidia-smi` call traces and
   selected manifest, including XML/CSV structure and per-field failures.

## Output

Return or implement:

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
- Verify CUDA/NVML UUID, ordinal, BDF, name, memory, generation, and state parity
  within each managed domain.
- Measure hot getter and polling overhead from archived raw samples; do not infer
  performance from a unit test.
