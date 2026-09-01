# memfd Transport

Local userspace fallback transport for the `v0.1.x` compatibility line. It uses
the common device protocol and generation semantics and is selected only during
provider initialization under the documented fallback rules.

This transport has no doorbell primitive: cold operations may syscall freely and
one active-queue dispatch may perform at most one wake syscall. The zero-syscall
steady-state obligation applies only to the milestone-0.1.1.0 doorbell transports, as
recorded in the control and data plane architecture record.

The client half is the existing C17 `runtime/client/fastpath` implementation;
`client/` registers that ownership without copying its shared-memory code. The
C++ `worker/` half now mirrors the runtime lifecycle coordinator. It stages
generation and epoch replacements, rejects new work while quiescing, drains
in-flight submissions before commit, and leaves lost or removed generations
unusable. It does not publish identity or implement a second ring protocol.

This is a lifecycle adapter fixture, not the milestone-0.1.2.0 qualification gate. QMP and
daemon-restart normalization, provider-view freeze, repeated-cycle evidence,
and the production memfd worker wiring remain in work-item-0.1.2.2/work-item-0.1.2.3.
