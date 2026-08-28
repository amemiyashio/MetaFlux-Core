# Character-Device Transport

Local transport over `/dev/metafluxN` mappings and the `/dev/metafluxctl` worker
broker. Setup, registration, teardown, and blocking waits are cold operations;
active queues use shared descriptors and timelines directly.
