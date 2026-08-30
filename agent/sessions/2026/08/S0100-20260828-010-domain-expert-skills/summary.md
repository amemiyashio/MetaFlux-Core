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

## roast

### light roasts

- Von Neumann and Harvard memory-architecture terminology ->
  agent/skills/cpu-backend-performance/references/cpu-memory-architecture.md
  (bundled quick validation passed for all 15 skill packages)

### medium roasts

- Ten-domain-skill ownership and composition routing -> agent/skills/README.md
  (15 unique Active packages and seven composition routes were recorded)
- Thirty single-skill and five composition routing cases ->
  agent/skills/trigger-evals.md (static trigger assertions passed for 30 single
  and 5 composition cases)
- CUDA Driver ABI review procedure ->
  agent/skills/cuda-driver-abi-compatibility/SKILL.md (bundled quick validation
  passed for all 15 skill packages)
- NVML telemetry compatibility review procedure ->
  agent/skills/nvml-telemetry-compatibility/SKILL.md (bundled quick validation
  passed for all 15 skill packages)
- PTX/SIMT semantics review procedure -> agent/skills/ptx-simt-semantics/SKILL.md
  (bundled quick validation passed for all 15 skill packages)
- MLIR compiler-engineering review procedure ->
  agent/skills/mlir-compiler-engineering/SKILL.md (bundled quick validation
  passed for all 15 skill packages)
- CPU backend performance review procedure ->
  agent/skills/cpu-backend-performance/SKILL.md (bundled quick validation passed
  for all 15 skill packages)
- Linux device UAPI review procedure ->
  agent/skills/linux-device-driver-uapi/SKILL.md (bundled quick validation passed
  for all 15 skill packages)
- vfio-user GPU virtualization review procedure ->
  agent/skills/gpu-virtualization-vfio-user/SKILL.md (bundled quick validation
  passed for all 15 skill packages)
- PCIe/vPCI device-model review procedure ->
  agent/skills/pcie-vpci-device-model/SKILL.md (bundled quick validation passed
  for all 15 skill packages)
- Cross-layer lifecycle-resilience review procedure ->
  agent/skills/device-lifecycle-resilience/SKILL.md (bundled quick validation
  passed for all 15 skill packages)
- Vulkan/SPIR-V compute review procedure ->
  agent/skills/vulkan-spirv-compute/SKILL.md (bundled quick validation passed for
  all 15 skill packages)

### dark roasts

- none.

## session-only

- none.

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
