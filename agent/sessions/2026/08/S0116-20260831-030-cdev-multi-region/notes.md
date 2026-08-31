# Notes

The region table is intentionally kernel-private. The frozen
`mf_uapi_memory_v0` record remains the per-registration request/reply, and
`max_regions` already existed in the negotiation record. Region ownership is
removed under `mf_cdev_lock`; all DMA unmap, SG free, dirty-unpin, memlock
release, and `mmput` work runs after unlocking.
