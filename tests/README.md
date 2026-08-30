# Cross-Component Tests

Component unit tests stay beside their owners (for example
`runtime/core/tests/`); build them with the component they qualify. This
directory contains only tests that cross ownership boundaries: contract and ABI
fixtures, end-to-end integration, compatibility, fault recovery, performance
qualification, and architecture checks such as the component dependency-graph
gate (`tools/check-component-graph.py`), which runs as
`metaflux.architecture.component-graph`.

The suite verifies consistent device identity across visible interfaces,
provider-only and core-only build boundaries, deterministic AOT/JIT/interpreter
results, transport teardown, and the published latency and throughput budgets.
Performance smoke tests may run in CI; strict latency gates run on controlled
bare-metal workers.

Provider ABI qualification checks exact exported symbols and symbol versions,
forbidden `DT_NEEDED` entries, and SONAMEs independently for CUDA and NVML. A
separate test loads both providers simultaneously in one process. Backend
fixtures are exercised only through their C function table. CTest labels remain
stable so CTest and CI drivers may build the test graph once and report ABI,
unit, integration, and performance-smoke results independently as the suite
grows.

`cuda_add_copy.c` is the application-side milestone fixture: it includes only a
frozen official `cuda.h` and uses ordinary CUDA Driver calls. The separate Python
runner owns daemon startup, socket and provider selection, execution-mode input,
timeouts, and cleanup so one unchanged application binary qualifies every CPU
execution mode.

The [M0100 performance harness](performance/README.md) replaces the old
bootstrap-call timing placeholder with active memfd ring, managed CUDA Add/Copy,
and managed NVML measurements. It archives raw samples and an exact host,
placement, toolchain, source, and binary fingerprint. CI runs only its smoke
classification; controlled Intel/AMD reference runs and every missing baseline
remain explicit machine-readable qualification rows.

The [provider package matrix](release/README.md) runs digest-pinned D0012
distribution images without network access and archives fresh-install, upgrade,
removal, private-path, dynamic-loader, and vendor-coexistence evidence.

`compatibility/run_nvidia_smi_acceptance.py` runs the exact frozen R535, R550,
R570, R580, and R610 stock binaries against one production daemon/provider pair.
It qualifies `-L`, summary, core CSV, `compute-apps`, `-q`, and XML views and
rejects unavailable process data instead of treating command success as coverage.
The explicit `--synthetic-cpu-topology` option creates an affinity-derived,
single-node topology fixture for build sandboxes that do not expose `/sys`; the
evidence marks that fixture, and normal daemon startup retains strict host
topology discovery. `--elf-loader` permits an immutable stock binary to run in
an isolated sandbox whose filesystem lacks its recorded interpreter path; the
runner still validates the original binary digest and records the selected
loader path and digest in the evidence.
