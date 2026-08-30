# Summary

Added ten repository-scoped Codex expert skills covering the systems boundaries
needed by M0100-M0130: CUDA Driver ABI, NVML telemetry, PTX/SIMT semantics, MLIR
compiler engineering, x86 CPU execution, Linux device UAPI, vfio-user GPU
virtualization, PCIe/vPCI presentation, cross-layer lifecycle resilience, and
Vulkan/SPIR-V compute.

Each package uses the standard Codex shape with English `SKILL.md` instructions,
`agents/openai.yaml`, and narrowly loaded references. Source semantics, compiler
mechanism, target execution, transport, presentation, and lifecycle have distinct
owners and explicit composition routes. The repository now exposes 15 unique
Active skills through `.agents/skills`.

## Changed paths

- `agent/skills/README.md`: all 15 Active packages plus seven composition routes.
- `agent/skills/trigger-evals.md`: 30 single-skill and five multi-skill routing
  cases, including the required PTX-to-CPU, vfio-user reset, and Vulkan lowering
  compositions.
- `agent/skills/{cuda-driver-abi-compatibility,nvml-telemetry-compatibility,
  ptx-simt-semantics,mlir-compiler-engineering,cpu-backend-performance}/`: M0100
  ABI, semantics, compiler, and CPU expertise.
- `agent/skills/{linux-device-driver-uapi,gpu-virtualization-vfio-user,
  pcie-vpci-device-model,device-lifecycle-resilience}/`: M0110/M0120 kernel,
  transport, presentation, and lifecycle expertise.
- `agent/skills/vulkan-spirv-compute/`: M0130 target/runtime expertise paired
  with the cross-backend MLIR skill.
- This session, `progress/current.md`, and `P20260828-010`: exact work record and
  protected handoff checkpoint.

## Verification

| Command/gate | Result |
| --- | --- |
| Bundled `skill-creator/scripts/quick_validate.py` over all packages | 15/15 skills passed |
| Static package and trigger assertions | 15 unique packages, 38 new references, 30 single and 5 composition cases |
| `git diff --cached --check` | Passed for the 60-file content change |
| `python3 tools/check-agent-records.py .` | Passed before content commit |
| `nix flake check path:. -L` | All checks passed |
| CTest | Not run; content changes are confined to `agent/` |

## Decisions and experience

- No DNNNN decision or ENNNN experience record changed. Exact ABI, protocol,
  feature, target, and support versions remain governed by active milestone
  plans, compiler epoch 1, pinned headers, and the existing open-decision ledger.

## Distillation

- Promoted: ten-expert ownership, composition routing, trigger corpus, and
  source-backed checklists -> `agent/skills/` (session verification above).
- Promoted: Von Neumann and Harvard terminology -> CPU memory-architecture
  reference and its address-space, cache, coherence, NUMA, ordering, MMIO, and
  DMA checks (session verification above).
- Session-only: none; the terminology did not become a duplicate standalone skill.

## Unresolved items

- No skill-matrix work remains. Existing M0100-M0130 open decisions and workstream
  statuses are unchanged; these skills guide future implementation and do not
  certify that fixtures are functional providers, transports, or backends.

## Handoff

Start with `python3 tools/check-agent-records.py .`, then follow `$start-work` and
the composition table in the [skill catalog](../../../../skills/README.md). Read
[current progress](../../../../progress/current.md) and
[P20260828-010](../../../../progress/checkpoints/2026/P20260828-010-domain-expert-skills.md)
before beginning the next product slice.
