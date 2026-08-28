# Backend Plugin ABI v1

Home of the bootstrap `mf_backend_api_v1` in-process C function table. This zone
may use sized pointers and extension chains only with explicit lifetime,
threading, and allocator ownership rules. It contains no C++ ABI.

The current header is a boundary fixture, not the complete backend API. v1
becomes stable only after the CPU vertical slice and ABI qualification pass.
