# Cross-Component Tests

Component unit tests stay beside their owners. This directory contains only
tests that cross ownership boundaries: contract and ABI fixtures, end-to-end
integration, compatibility, fault recovery, and performance qualification.

The suite verifies consistent device identity across visible interfaces,
provider-only and core-only build boundaries, deterministic AOT/JIT/interpreter
results, transport teardown, and the published latency and throughput budgets.
Performance smoke tests may run in Nix CI; strict latency gates run on controlled
bare-metal workers.

Provider ABI qualification checks exact exported symbols and symbol versions,
forbidden `DT_NEEDED` entries, and SONAMEs independently for CUDA and NVML. A
separate test loads both providers simultaneously in one process. Backend
fixtures are exercised only through their C function table. CTest labels remain
stable so Nix may build the test graph once and report ABI, unit, integration,
and performance-smoke results independently as the suite grows.
