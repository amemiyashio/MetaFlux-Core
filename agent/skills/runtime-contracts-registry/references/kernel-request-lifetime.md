# Neutral Kernel Requests and Module Lifetime

Read for the framework-kernel request crossing from a compatibility provider to
the daemon. Start with the actual producer and operation, then inspect the
[client contract](../../../../contracts/protocol/client/v1/README.md) and
[protocol definitions](../../../../contracts/protocol/client/v1/include/metaflux/client/protocol.h).
The [CPU work item](../../../plan/milestone-0.2.0.0-pytorch-cuda-compatibility/work/work-item-0.2.0.2-torch-kernel-intake.md)
owns current accepted scope; a new translation does not automatically expand it.

## Producer-to-consumer path

| Boundary | Source and search term |
| --- | --- |
| Canonical bytes, operation/version admission | [protocol.h](../../../../contracts/protocol/client/v1/include/metaflux/client/protocol.h): `mf_client_kernel_request_init_v1`, `mf_client_kernel_request_validate_v1` |
| Client capability negotiation and control exchange | [fastpath.c](../../../../runtime/client/fastpath/src/fastpath.c): `mf_client_session_connect_capabilities_v1`, `mf_client_session_control_v1` |
| Provider translation and materialization | [Driver provider.c](../../../../plugins/compat/cuda/abi/driver/src/provider.c): `cuModuleLoadData`, `mf_cuda_materialize_pytorch_baseline_locked` |
| Register sealed artifact/source range | [server.cpp](../../../../services/metafluxd/src/server.cpp): `MF_CLIENT_CONTROL_KERNEL_REQUEST_REGISTER_V1`, `ArtifactSourceRange` |
| Retain canonical module and execute | [server.cpp](../../../../services/metafluxd/src/server.cpp): `MF_RING_OPCODE_MODULE_LOAD`, `prepared_module`, `executor_name` |
| Protocol rejection evidence | [protocol.c](../../../../contracts/protocol/client/v1/tests/protocol.c) |

Read each owning source before changing fields. Reuse the existing neutral
request when the missing behavior is provider admission or backend semantics;
do not introduce a parallel representation for the same operation.

## Negotiation and lifetime

Capability bit 11 and control opcode 17 select `KERNEL_REQUEST_REGISTER`.
Clients depending on it require the capability during negotiation; an older
runtime rejects before request traffic, without silent downgrade to raw artifacts.
The control carries one write-sealed payload FD, no extra flags, and its runtime
context ID. The payload's fixed little-endian header carries profile, operation,
operation ABI version, Kernel IR schema version, lifetime and source range.
Keep reserved bytes zero and validate sizes, bounds and versions before mapping
the operation to retained state. The canonical protocol owns the exact offsets
and accepted values; do not copy them into private consumer definitions.

Each accepted operation has one unique protocol value and validation coverage.
No CUDA, PyTorch, MLIR, LLVM, Vulkan, target or native-layout type crosses this
neutral wire. Profile/operation ABI, Kernel IR schema and backend ABI retain
their distinct version roles.

Registration creates an immutable artifact. Its source remains valid through
successful `MODULE_LOAD`; the daemon then retains canonical Kernel IR in the
module, which remains valid until `MODULE_UNLOAD`. Releasing the artifact after
load completion does not release that module. Early release, stale generation,
unknown operation, omitted capability, malformed header, FD/seal mismatch and
context mismatch use the existing typed control/ring status paths.

Keep registration, executable materialization, launch acceptance and actual
completion distinct. The current library path may defer executable module load
until descriptor admission. A registered placeholder or an operation-specific
daemon result does not establish generic interpreter/compiled execution.

## Complete the affected behavior

Compose [$pytorch-cuda-profile](../../pytorch-cuda-profile/SKILL.md) skill for
stock profile translation, [$cuda-driver-abi-compatibility](../../cuda-driver-abi-compatibility/SKILL.md) skill
for Driver errors, and [$daemon-execution-runtime](../../daemon-execution-runtime/SKILL.md) skill
for service-owned retention and dispatch. Canonical source meaning, conversion
mechanics and target execution stay with their PTX/compiler/backend experts.
Implement source, protocol projections and affected consumers together only when
the contract changes. Preserve argument/memory/module/context lifetime through
completion and teardown.

Select the protocol negatives and the real
[frontier runner](../../../../tests/compatibility/run_pytorch_cuda_cpu_frontier.py)
case that exercises this crossing. Bind results to actual per-request executor
completion and mode-specific provenance/cache identity; client output, daemon
launch counters and harness self-tests alone do not supply that evidence.
