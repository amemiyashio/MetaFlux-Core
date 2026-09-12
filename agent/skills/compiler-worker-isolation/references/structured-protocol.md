# Structured Compiler IPC

Start at `encode_request`, `decode_request`, `encode_response` and
`decode_response` in
[`compiler_worker_protocol.cpp`](../../../../services/metafluxd/src/compiler_worker_protocol.cpp).
This private, versioned protocol carries structured KIR into the child and a
bounded artifact or diagnostic back. It is not PTX text, MLIR bytecode or a
public provider/backend ABI. Its current success payload is CPU PIC ELF; it
does not establish a Vulkan compiler-worker route.

## Change the complete representation

The request encodes schema/PTX versions, name, parameter types, shared
allocations, registers, operations and source locations. A new KIR type or field
must survive both directions' enum/count/boolean checks and canonical round trip;
inspect `valid_parameter_kind`, `valid_value_kind` and shared opcode validation.
Protocol acceptance is structural decoding; `verify_kernel` and the compiler
still establish semantic legality. Compose
[$ptx-simt-semantics](../../ptx-simt-semantics/SKILL.md) skill for changed meaning
and [$mlir-compiler-engineering](../../mlir-compiler-engineering/SKILL.md) skill
for representation-to-emitter mechanics.

Integers are explicitly little-endian; lengths are checked before allocation
against remaining input and declared bounds. Preserve magic/version, collection
limits, scalar enum and boolean validation, and rejection of trailing bytes.
The header's current request/response maxima are 64 MiB/257 MiB, collection
counts are bounded to `2^20` elements, and diagnostics to 64 KiB. A new
larger field needs coherent encoder, decoder and transport admission; adjusting
one constant does not establish a legal representation or memory budget.

## Accept the complete child result

Success contains ELF bytes, digest, parameter signature and floating-point flag;
failure contains the compiler error, source location and message. No MLIR/LLVM
intermediate text crosses this result. Decode all fields, require exact end of
message, verify the ELF digest, then require the response PID to match the child
the parent spawned. Normal child exit alone is insufficient. Target-specific ELF,
signature/helper and load-time checks remain with the backend, and the parent
alone may publish through the artifact cache after successful preparation.

Preserve malformed/truncated header/body, unknown result kind, invalid enums,
digest mismatch, PID mismatch and oversized-message diagnostics. A compiler
semantic failure is a valid structured failure response, distinct from corrupt
IPC, a signalled child or a nonzero exit. Keep the original source location and
bounded diagnostic payload when mapping it into module-preparation errors.

## Focused evidence

`test_exp_request_protocol` in
[compiler_worker_test.cpp](../../../../services/metafluxd/tests/compiler_worker_test.cpp)
shows a current operation crossing encode/decode with canonical equality,
truncation checks and unknown-opcode rejection. For a new representation, add
its round-trip and nearest precise negative, then run it through a real child
when compiler consumption changes. Reuse the existing worker integration gate
for its registered cases; choose additional compiler/backend checks only for
layers the change actually crosses. A protocol-only change does not require a
new whole-framework corpus or device matrix.
