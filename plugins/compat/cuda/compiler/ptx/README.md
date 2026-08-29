# CUDA PTX Frontend

This directory owns the CUDA PTX input adapter into the ecosystem-neutral
compiler core. It is deliberately absent from installed daemon artifacts until
the compiler worker owns frontend registration and execution.

The frontend may depend on compiler contracts, but it must not depend on an
execution backend or application-side CUDA provider DSO.

## PTX 9.0 / sm_70 manifest

The implemented slice accepts one optional-visible `.entry` for target `sm_70`
with 64-bit global addressing. The machine-readable authority is
`manifest/capabilities.json` plus the one-object-per-line
`manifest/forms.jsonl`; `corpus/index.json` binds every positive, malformed,
unsupported, and edge fixture by SHA-256.

The bounded forms cover x/y components of `%tid`, `%ntid`, `%ctaid`, and
`%nctaid`; modulo-u32 `add`/`sub`/low `mul`/low `mad`; explicit
`add/sub/mul/mad/fma.rn.f32`; two explicit conversion forms; ordered
comparisons; the final-return predicate branch; guarded global/shared u32
stores; exact global f32 bit loads/stores; static shared u32 storage; and
unconditional `bar.sync 0`. Global and shared addresses retain distinct Kernel
IR provenance.

Registers are single-assignment in this schema. Unsupported versions, targets,
opcodes, modifiers, address spaces, register reuse, symbols, types, and malformed
tokens return stable source-located diagnostics. Atomics, warp/cluster forms,
the z dimension, `approx`, `ftz`, saturation, dynamic shared storage, generic
pointers, and predicated arithmetic/barriers are explicitly excluded.
