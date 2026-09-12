# Implement a Reusable Semantic Family

Begin with the requested input family: dtype, shape, layout/strides, scalar
parameters, indexing/control behavior and the first unsupported form. Inspect
its actual stock-client artifact and owning profile before choosing an opcode.
An observed fixed six-element profile does not establish runtime shape generality.

## Trace representation before execution

| Layer | Source and required decision |
| --- | --- |
| Accepted PTX syntax and normalization | [parser.cpp](../../../../plugins/compat/cuda/compiler/ptx/src/parser.cpp): `kSupportedForms`, `Parser::parse_instruction` and the affected parser helper |
| Neutral types and operations | [kernel_ir.hpp](../../../../compiler/core/include/metaflux/compiler/kernel_ir.hpp): `ParameterKind`, `ValueKind`, `Opcode`, `Operation` |
| Canonical legality and serialization | [kernel_ir.cpp](../../../../compiler/core/src/kernel_ir.cpp): `operation_contract`, `verify_kernel`, `serialize_kernel` |
| Compiler-worker transport of that representation | [compiler_worker_protocol.cpp](../../../../services/metafluxd/src/compiler_worker_protocol.cpp): `encode_request`, `decode_request`, type/opcode bounds |
| Independent runtime consumption | [interpreter.cpp](../../../../plugins/backend/cpu/runtime/src/interpreter.cpp): `parse_kernel`, argument validation and operation execution |
| Backend lowering | The selected CPU/Vulkan compiler owner; a parser success is not emitted executable coverage |

Check every represented field and decoder when adding a type or operation.
The current neutral scalar/value kinds include binary32, not binary64; extending
an exponential helper alone would not carry float64 through parameter typing,
registers, serialization, worker decoding and both execution paths. The exact
schema/helper/target identity consequences must follow the actual representation
change. Do not add ecosystem or target-specific wire types to solve it.

For shape generality, carry actual dimensions, strides, bounds and iteration
relationships as validated data or reusable control. Enumerate the legal
parameterized family and the nearest excluded configurations. If current
single-assignment/final-return control cannot express it, change and verify that
semantic boundary explicitly; duplicating shape-specialized artifacts does not
establish the requested family. A retained specialization needs truthful admission.

## Preserve observable meaning

Resolve declarations/register classes, labels, address spaces and layouts before
execution. Define each operation's source types, width/sign behavior, overflow,
rounding, NaNs/infinities/subnormals, predicate effects and address provenance.
Then add the verifier conditions and stable source-located diagnostics. Target
limitations are explicit rejection, never a silent approximation of the source.

Use [SIMT control](simt-control.md) only when changing participation/control and
[memory model](memory-model.md) when changing memory/scope/order. Broader atomics,
loops or warp semantics are not prerequisites for an unrelated scalar operation.
Current exclusions remain meaningful negative behavior until separately changed.

## Prove and advertise the form

Define independent deterministic answers or a normative allowed-outcome relation
before using differential output. [Oracle testing](oracle-testing.md) owns the
classification. Preserve binary32 exponential's declared accuracy bound and
special-value classification; mode agreement is distinct from mathematical
accuracy and is not a new correctly-rounded claim.

For each newly advertised form, add a positive and precise negative fixture,
its oracle class and applicable edge/interaction cases. Update the existing
form/capability and fixture-digest owners rather than creating a second inventory.
Verify direct invalid Kernel IR so parser checks do not hide a missing invariant.
Compose the backend owners to carry the form through interpreter, cold JIT,
warm JIT and AOT where advertised; Vulkan joins only within its declared scope.
Keep randomized/metamorphic interactions tied to a semantic question, not an
automatic new full-matrix campaign. Hand the completed semantic family, exact
diagnostics, representation changes and actual evidence to parent review.
