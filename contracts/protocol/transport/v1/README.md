# Transport Protocol v1

The candidate M0110 transport envelope is authored by
[`schema/manifest.json`](schema/manifest.json) and its hashed definitions. The
same little-endian records are carried by memfd, cdev, vfio-user, and later
network transports without leaking provider or backend types. Generated C/C++
projections are emitted into the build tree; this directory contains no second
hand-maintained layout. The candidate remains ABI `0.x` until W0114 evidence
freezes the extension namespace as `v1`.

M0120 lifecycle semantics live in the one-way extension at
`schema/extensions/lifecycle/v1/`. Its manifest imports this base by content
hash and the lifecycle model owns generation-candidate, epoch-retirement,
tombstone, and provider-view rules without changing the M0110 root.
