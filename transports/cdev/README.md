# Character-Device Transport

Local transport over `/dev/metafluxN` mappings and the `/dev/metafluxctl` worker
broker. Setup, registration, teardown, and blocking waits are cold operations;
active queues use shared descriptors and timelines directly.

When implemented, this directory splits into `client/` and `worker/` halves per
the transport halves convention (D0010); the kernel counterpart lives under
`kernel/` against the UAPI in `contracts/uapi/linux/`.
