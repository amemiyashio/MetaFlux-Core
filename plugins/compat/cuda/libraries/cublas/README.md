# cuBLAS Compatibility Provider

This C17 preload provider implements the pinned public cuBLAS surface needed by
stock PyTorch without executing vendor cuBLAS. It converts accepted calls into
MetaFlux Kernel Requests and invokes the active `libcuda.so.1` Driver ABI. The
Driver provider owns device-pointer validation, stream ordering, daemon
submission, and result completion; this library never reads or writes tensor
buffers.

The first profile accepts host-pointer-mode float32 `cublasSgemm_v2` with
`alpha=1` and `beta=0`. Other parameter combinations return a typed cuBLAS
status and do not fall through to provider-local computation.
