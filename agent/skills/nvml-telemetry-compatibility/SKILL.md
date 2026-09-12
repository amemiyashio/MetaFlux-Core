---
name: nvml-telemetry-compatibility
description: Implement or review NVML queries, versioned lifecycle, process reporting and policy setters from real runtime telemetry, with stock nvidia-smi qualification. Own NVML field/error mapping rather than CUDA execution or shared lifecycle mechanisms.
---

# NVML Telemetry Compatibility

Start with one requested stock-tool query or setter. Find its entry in
[NVML symbols](../../../plugins/compat/cuda/management/nvml/symbols.def) and
[provider behavior](../../../plugins/compat/cuda/management/nvml/src/provider.c),
then trace its real producer, snapshot and API mapping. The
[provider field table](../../../plugins/compat/cuda/management/nvml/README.md)
owns current provenance and units. A new producer belongs in its runtime/backend
owner; plausible data or an additional N/A result is not measured telemetry.

Read the selected NVML header/stock-tool manifest and relevant work item. Use
the shared [implementation guidance](../review/references/implementation-guidance.md)
for implementation; explicit analysis or review-only requests stay read-only.

| Task | Read |
| --- | --- |
| Init/shutdown, aliases, handles, structure versions or count/fill queries | [Lifecycle and versioning](references/lifecycle-and-versioning.md) |
| Getter provenance, process rows, freshness, policy setter or runtime consumer | [Telemetry contracts and source path](references/telemetry-contracts.md) |
| Stock nvidia-smi views, CSV/XML output or polling measurements | [Qualification](references/nvidia-smi-qualification.md) |

Keep these invariants visible:

- DSO loading remains inert. Initialization reference counting and shutdown
  are thread-safe; entry points share one committed process view and mode.
- Read metrics from one coherent published snapshot. Validate generation,
  liveness and effective policy against the lifecycle fence, with no stale
  telemetry fallback after loss or close.
- A setter returns success only after requested policy is effective in that
  fence. A control acknowledgement alone does not establish completion.
- NVML captures canonical membership at zero-to-one init. CUDA filtering never
  reorders NVML; count/order parity applies only to unfiltered providers with
  the same view revision. Match common live devices by `(UUID, generation)`.
- Validate caller size/version and count/fill capacity before writing. Preserve
  per-field errors, units and reuse-safe PID/start-time process identity.
- Physical fields without a real producer return their typed unsupported
  status and stock-tool N/A presentation. Never fabricate clocks, temperature,
  power, utilization or other measurements.

Compose [$runtime-contracts-registry](../runtime-contracts-registry/SKILL.md) skill
for shared snapshot/view changes and
[$device-lifecycle-resilience](../device-lifecycle-resilience/SKILL.md) skill for
reset/replacement. CUDA object and execution behavior belongs to
[$cuda-driver-abi-compatibility](../cuda-driver-abi-compatibility/SKILL.md) skill;
PCI configuration identity belongs to
[$pcie-vpci-device-model](../pcie-vpci-device-model/SKILL.md) skill.

Return the changed API/field mapping, producer and freshness/error behavior,
affected target versions, actual stock-tool result and selected checks.
Implement the missing path before broad qualification. Use the selected topic's
negative and concurrency cases for the changed behavior; expand to the required
target/view matrix when that qualification is the task. Latency claims need
raw measurements, not unit-test success.
