# Notes

The control descriptor is the lease owner and can map only the paired queue at
offset zero. The data descriptor remains the payload and registered-memory
owner. `CdevWorkerSession` therefore exposes queue pointers plus an optional
caller-supplied payload pointer but never infers or maps payload memory.

The `/dev/null` regression verifies deterministic `ENOTTY` to
`MF_SHARED_NOT_SUPPORTED` mapping and invalid expected view rejection. Live
lease and payload/object-table activation still require the cdev kernel device
environment.
