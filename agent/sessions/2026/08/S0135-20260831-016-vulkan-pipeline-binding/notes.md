# Notes

The binding key is the complete device-bound cache identity, so vendor/device,
device UUID, driver UUID/version, pipeline-cache UUID, target, compiler epochs,
argument ABI, and specialization remain part of the hit. The generation is a
runtime lifetime guard and is intentionally not added to the persistent cache
key; a reset can reuse the same physical identity only after the old binding is
released and a fresh generation is acquired.
