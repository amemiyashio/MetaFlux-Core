# cuBLAS Compatibility Provider

This C17 preload provider implements the pinned public cuBLAS surface needed by
stock PyTorch without executing vendor cuBLAS. It converts accepted calls into
MetaFlux Kernel Requests and invokes the active `libcuda.so.1` Driver ABI. The
Driver provider owns device-pointer validation, stream ordering, daemon
submission, and result completion; this library never reads or writes tensor
buffers.

The first profile accepts host-pointer-mode float32 `cublasSgemm_v2` with
`alpha=1` and `beta=0` or `beta=1`. The latter marks the destination as
read/write and covers stock `torch.addmm`; `beta=0` also covers ordinary
`torch.matmul` and bias-free `torch.nn.functional.linear`. Other scalar
combinations return a typed cuBLAS status and do not fall through to
provider-local computation.

The profile also owns the exact cuBLASLt object, preference, heuristic, and
matmul sequence emitted by the pinned client for float32
`torch.nn.functional.linear` with bias. It accepts column-major `T/N` layouts,
host `alpha=1`, `beta=0`, an in-place C/D output, and the bias epilogue, then
submits the fourth device buffer and neutral matmul descriptor to the daemon.
Other layouts, scalar combinations, batched forms, data types, algorithms, and
epilogues fail with a typed status before submission.
