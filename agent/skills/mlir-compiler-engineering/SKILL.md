---
name: mlir-compiler-engineering
description: Implement or review the actual Kernel IR emitter, MLIR conversion, target pipeline mechanics, legality, diagnostics and compiler-epoch behavior. Use for CPU or Vulkan compiler changes; worker processes, artifact persistence, source meaning and backend policy/runtime have separate owners.
---

# MLIR Compiler Engineering

For an implementation request, carry the semantic family through the first
missing compiler step to the owning backend's executor. For analysis, review or
benchmarking, report evidence within that requested mode; do not infer a new pass
architecture or implementation request.

Start with the selected target's current source. CPU `compile_kernel` and
`MlirEmitter` are in
[`compiler.cpp`](../../../plugins/backend/cpu/compiler/src/compiler.cpp);
Vulkan `lower_kernel` and `emit_actual_spirv` are in
[`lowering.cpp`](../../../plugins/backend/vulkan/compiler/src/lowering.cpp).
Find the first rejected or divergent representation; implementation changes that owner.
Do not infer an implemented dialect/pass from an architectural diagram.

| Task | Read only the relevant guide |
| --- | --- |
| CPU emitter or Vulkan emission/pipeline change | [Actual target paths](references/target-lowering.md) |
| New Kernel IR representation or verifier/type invariant | [Kernel IR boundary](references/kernel-ir-boundary.md) |
| An actual dialect conversion, TypeConverter or materialization | [Dialect conversion](references/dialect-conversion.md) |
| First failing IR, compiler crash/nondeterminism or pipeline identity | [Debugging and versioning](references/debugging-and-versioning.md) |
| Worker spawn/IPC/deadline/cleanup | [$compiler-worker-isolation](../compiler-worker-isolation/SKILL.md) skill |
| Artifact keys, persistent publication, quotas or pins | [$compiler-artifact-cache](../compiler-artifact-cache/SKILL.md) skill |

The pinned epoch descriptor owns LLVM/MLIR API behavior. Check those installed
headers/source before relying on an API; latest online documentation is guidance,
not proof of the pinned LLVM/MLIR 22.1.8 contract.

Kernel IR is the neutral boundary. Put source syntax in the frontend, structural
and cross-operation meaning in its verifier, and target feature/limit rejection
at target lowering. ODS and conversion patterns apply where the chosen path
actually uses them; they are not prerequisites for fixing the current emitter.
Preserve legality, type/region/successor consistency and precise diagnostics.

Keep CPU and Vulkan branches separate after any genuinely shared semantics.
The CPU artifact path ends in validated PIC ELF; Vulkan goes through SPIR-V
without an LLVM-IR detour. MLIR, LLVM IR and bytecode remain compiler-epoch-local
implementation details, outside providers, transports and backend C ABIs.

Compose [$ptx-simt-semantics](../ptx-simt-semantics/SKILL.md) skill for semantic/oracle
changes. Compose [$cpu-backend-performance](../cpu-backend-performance/SKILL.md) skill
or [$vulkan-spirv-compute](../vulkan-spirv-compute/SKILL.md) skill for target policy,
lowering implementation/source placement, target validation and runtime behavior.
Conversion mechanics do not transfer those backend responsibilities here.

Use the first divergent IR and a minimized reproducer during implementation.
Select verifier/pass/conversion/end-to-end checks for the actual changed layers,
including illegal inputs; preserve advertised differential and target gates.
Warm cache loading invokes no compiler framework. Supply changed epoch/pipeline
semantics to the artifact-cache owner; target/helper ABI inputs remain backend
owned. Artifact persistence and worker lifecycle are independent of MLIR mechanics.

An implementation delivers executed client/backend behavior with its changed representation,
legality and cache boundaries. A new pass, artifact or compilation counter alone
is not that result. Hand the coherent diff and exact checks to
[$review](../review/SKILL.md) skill; [$verify](../verify/SKILL.md) skill owns formal
per-phase execution and avoids duplicate covering runs.
