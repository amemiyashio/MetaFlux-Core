# Character-Device Transport

Local transport over `/dev/metafluxN` mappings and the `/dev/metafluxctl` worker
broker. Setup, registration, teardown, and blocking waits are cold operations;
active queues use shared descriptors and timelines directly.

The userspace halves are split per the transport halves convention (D0010). The
kernel counterpart under `kernel/core/` maps one submission and one completion
ring back-to-back from the generated UAPI queue record. The current fixture
advertises queue mmap and the worker-broker bit; eventfd, registered memory, and
daemon-owned replacement generations remain explicit W0112/W0114 follow-up
work.
