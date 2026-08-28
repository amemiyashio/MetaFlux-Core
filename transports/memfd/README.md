# memfd Transport

Local userspace fallback transport for the v0.1/v0.2 compatibility path. It uses
the common device protocol and generation semantics and is selected only during
provider initialization under the documented fallback rules.

This transport has no doorbell primitive: cold operations may syscall freely and
one active-queue dispatch may perform at most one wake syscall. The zero-syscall
steady-state obligation applies only to the M0002 doorbell transports, as
recorded in the control and data plane architecture record.

When implemented, this directory splits into `client/` and `worker/` halves per
the transport halves convention (D0010).
