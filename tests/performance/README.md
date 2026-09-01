# milestone-0.1.0.0 Performance Evidence

This directory owns the reproducible milestone-0.1.0.0 CPU-backed performance harness. The
timed executables use `CLOCK_MONOTONIC_RAW`, allocate their sample arrays before
warm-up, retain every sample, and emit a line-oriented protocol. The Python
driver pins itself before starting any benchmark or daemon, so the ring client,
provider process, daemon, and inherited worker threads begin inside the same
effective CPU affinity.

The current workloads are:

- `milestone_0_1_0_0_ring_benchmark.c`: one active, uncontended memfd-backed ring with an
  immediate submit/consume round trip, waiter-doorbell counters, and raw
  back-to-back clock-read calibration samples.
- `milestone_0_1_0_0_cuda_managed_benchmark.c`: the production CUDA provider and daemon,
  with one initialized Add module followed by warm launch and managed D2D copy
  samples plus direct H2D and D2H samples over at least 16 MiB. Submit timing
  ends immediately when the corresponding asynchronous CUDA API returns;
  completion timing ends separately when `cuStreamSynchronize` returns. Add and
  every Copy direction are validated after measurement.
- `milestone_0_1_0_0_nvml_benchmark.c`: the production NVML provider and daemon, with hot
  memory getter samples and repeated warm shutdown/init cycles.

The driver forces `METAFLUX_MODE=managed` whenever either provider benchmark is
selected, so vendor libraries on a reference host cannot turn a managed
measurement into an auto-mode passthrough result.

When the CUDA benchmark is configured, `direct_host_copy_path` is a mandatory
correctness and path gate even in smoke mode. It requires all four direct H2D/D2H
submit/completion metrics in nanoseconds, a checked workload of at least 16 MiB,
and successful output validation. The daemon's shutdown statistics, rather than
benchmark self-reporting, must show address-space registration and at least
`warmup_count + sample_count + 1` direct operations in each host direction. Each
direction must account for at least that count multiplied by `direct_copy_bytes`;
the extra full-size operation is the H2D setup transfer or D2H correctness
readback. Every staged-host operation and byte counter must remain zero. Missing,
duplicate, malformed, or contradictory fields fail the run.

Before archiving CUDA timing evidence, run
`metaflux.unit.provider.cuda-semantics`. Its test-only path counters prove that
the initialized/cached launch, synchronous/asynchronous Copy, event-record, and
stream-wait entry points acquire the queue/ledger gate but neither the export
dispatch gate, provider state gate, nor provider heap storage, including a
four-thread shared-context submission regression. Timing samples alone are not
used as lock-path proof.

`run_milestone_0_1_0_0_performance.py` writes three atomic artifacts under `--output-dir`:

| Artifact | Contents |
| --- | --- |
| `raw-samples.csv` | Every raw metric sample, including tails and outliers |
| `fingerprint.json` | CPU/microcode, original and pinned affinity, SMT siblings, NUMA/cpuset, frequency policy, kernel/libc, timezone, compiler epoch, build manifest, source revision, and binary hashes |
| `results.json` | p50/p90/p99 distributions, subprocess metadata, provisional comparisons, qualification coverage, and machine-readable skipped reasons |

## Optimization and durability qualification

`run_milestone_0_1_0_0_optimization.py` owns three gates bound to a clean Git `HEAD` and
tree. It refuses dirty tracked or untracked source state, never creates a
per-file source snapshot, never reuses an evidence directory, and refuses any
pre-existing `profraw` file.

- `pgo` configures a Release+ThinLTO `GENERATE` tree, runs each named provider,
  compiler-service, Add/Copy, and managed-performance workload separately,
  associates every workload with the raw files it changed, merges them with the
  pinned `llvm-profdata`, validates covered CUDA, NVML, and compiler-service
  functions, then performs a Release+ThinLTO `USE` rebuild and repeats the same
  tests. Before training, negative checks reject a relative profile path and
  prove that a control-flow change still raises `profile-instr-out-of-date` as
  an error; only the expected cold-translation-unit `profile-instr-unprofiled`
  diagnostic is suppressed during the USE build. The profile evidence binds the
  Git revision and tree, build manifests, exact tests, raw profile set, merged
  profile, and tool hashes. The measured profile remains run evidence and does
  not enter `toolchains/` or a compiler epoch.
- `variants` builds providers at `-O2` and `-O3` with ThinLTO held constant. It
  proves exact dynamic-symbol parity, measures every allocated executable ELF
  section, and alternates repeated regression runs on one pinned CPU. Runtime
  and code-size ratios are observations; the runner does not invent a selection
  threshold.
- `hardening` builds the full selected path with ASan/UBSan. A deterministic
  mutator executes every checked-in PTX seed and every declared mutation class
  through the production parser in isolated processes. The soak gate repeatedly
  runs registry recovery, provider lifecycle, policy, compiler-worker, and all
  Add/Copy execution modes. A missing test, timeout, sanitizer finding, or
  nonzero result fails the gate.

All required build and inspection tools are declared by the Nix development
environment. Nix only materializes those tools; the Python runner owns the
workflow. Start from a clean committed worktree and a fresh compact evidence
directory. `TOOLCHAIN_PREFIX` and `NVIDIA_HEADER_DIR` are absolute paths to the
materialized tool and header inputs:

```sh
nix develop . --command python3 -B tests/performance/run_milestone_0_1_0_0_optimization.py \
  --repository . \
  --output-dir ../.metaflux-evidence/MetaFlux-Core/milestone-0.1.0.0-optimization \
  --toolchain-prefix TOOLCHAIN_PREFIX \
  --nvidia-header-dir NVIDIA_HEADER_DIR \
  --jobs 16 \
  --variant-runs 7 \
  --fuzz-cases 1024 \
  --soak-iterations 10
```

The atomic top-level result is `optimization-evidence.json`. Raw fuzz and soak
rows remain in JSONL files, command stdout/stderr remains in hashed logs, and
`pgo/profile-evidence.json` plus `pgo/milestone-0.1.0.0-measured.profdata` describe the PGO
measurement. The runner checks the clean Git revision and tree again after all
selected stages; any concurrent source change fails the complete run even when
individual commands passed. Build trees and raw profiles live in a system
temporary work directory and are removed after success or failure. Pass
`--work-dir ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-optimization-work` together
with `--keep-work` only for explicit debugging retention. Repeated `--stage`
arguments run `pgo`, `variants`, or `hardening` independently during harness development.
PGO and hardening update an atomic `progress.json` after each completed test, so
a later failure retains compact training, USE, fuzz, and soak evidence without
retaining the build tree.

## Smoke versus binding reference

`smoke` validates the benchmark path and archives observations. It never
promotes a budget even when an observation is below a provisional limit. The
CTest case uses this mode with a short workload:

```sh
nix develop . --command ctest \
  --test-dir ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration \
  -R 'metaflux.performance.milestone-0.1.0.0-(ring|managed)-smoke' \
  --output-on-failure
```

`binding-reference` is intentionally stricter. A run is eligible only when the
milestone budget status is `binding`, the operator declares a controlled host,
the reference role is `amd` or `intel`, and the observed CPU vendor matches that
role. Because the launch budget is one microsecond, the back-to-back
`CLOCK_MONOTONIC_RAW` pair-overhead p99 must be at most 100 ns; the harness
records but never subtracts that overhead. The standard Copy gate additionally
needs the measured same-path native baseline. Missing Intel, physical NVIDIA, trace/audit, stock-tool interference,
passthrough, or native baseline evidence is written as `status: skipped` with a
stable reason, and the binding command exits with status 2 (`incomplete`) rather
than claiming success.

For a controlled AMD reference run after the plan promotes its budgets:

```sh
nix develop . --command python3 -B tests/performance/run_milestone_0_1_0_0_performance.py \
  --mode binding-reference \
  --budget-status binding \
  --reference-host-role amd \
  --controlled-host \
  --output-dir ../.metaflux-evidence/MetaFlux-Core/milestone-0.1.0.0-amd-reference \
  --repository . \
  --milestone-plan agent/plan/milestone-0.1.0.0-core-foundation/plan.md \
  --ring-benchmark ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration/tests/metaflux_milestone_0_1_0_0_ring_benchmark \
  --cuda-benchmark ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration/tests/metaflux_milestone_0_1_0_0_cuda_managed_benchmark \
  --nvml-benchmark ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration/tests/metaflux_milestone_0_1_0_0_nvml_benchmark \
  --daemon ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration/services/metafluxd/metafluxd \
  --provider-dir ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration/plugins/compat/cuda/abi/driver \
  --cuda-provider ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration/plugins/compat/cuda/abi/driver/libcuda.so.1 \
  --nvml-provider ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration/plugins/compat/cuda/management/nvml/libnvidia-ml.so.1 \
  --compiler-epoch toolchains/compiler-epoch-1.json \
  --build-manifest ../.metaflux-build/MetaFlux-Core/milestone-0.1.0.0-integration/metaflux-build-manifest.json \
  --execution-mode warm-jit \
  --warmup 1000 \
  --samples 10000 \
  --copy-bytes 16777216 \
  --native-h2d-baseline-json SAME_PATH_NATIVE_H2D_BASELINE.json \
  --native-d2h-baseline-json SAME_PATH_NATIVE_D2H_BASELINE.json \
  --native-d2d-baseline-json SAME_PATH_NATIVE_D2D_BASELINE.json \
  --native-device-bdf 0000:BB:DD.F \
  --native-cuda-library /ABSOLUTE/PATH/libcuda.so.1 \
  --native-copy-benchmark /ABSOLUTE/PATH/native-copy-benchmark \
  --llvm-readobj /ABSOLUTE/PATH/llvm-readobj
```

Each native Copy baseline JSON uses schema version 2 and identifies workload
`same_path_native_copy`, one explicit `direction` (`h2d`, `d2h`, or `d2d`), the
matching asynchronous CUDA API, completion boundary
`cuStreamSynchronize_return`, provider mode `native`, and clock
`CLOCK_MONOTONIC_RAW`. Host allocation is fixed to `malloc_pageable`, matching
the managed benchmark. It contains matching `copy_bytes`, `warmup_count`,
`sample_count`, fixed stopping rule, `selected_cpu`, `worker_cpu`, `numa_node`,
`cpu_vendor`, `cpu_model`, and `microcode` fields plus exactly `sample_count`
positive integer values in `samples_ns`, with at least 30 samples.

The runner independently resolves the selected physical GPU through procfs and
sysfs. Every baseline must match its UUID, model, BDF, resolved PCIe path,
PCI vendor/device IDs, current and maximum link speed/width, and NUMA node; the
host kernel and NVIDIA driver version-text hash; the resolved native CUDA
library hash and ELF build ID; and the executable native benchmark binary hash.
Its command must exactly match the canonical vector containing direction,
warm-up/sample counts, bytes, BDF, provider path, and pageable allocation mode.
The recorded environment must exactly select the UUID with
`CUDA_VISIBLE_DEVICES`, fix `CUDA_DEVICE_ORDER=PCI_BUS_ID`, and expose only the
resolved provider directory through `LD_LIBRARY_PATH`.

The harness hashes each baseline file, raw sample vector, command, and loading
environment and derives p50 from the vector. A declared summary number is never
trusted. Missing physical identity, an unknown NUMA node, missing build ID, or
an unavailable benchmark/tool artifact keeps a binding run incomplete. H2D and
D2H are separate binding gates; D2D remains an independent observation and
never substitutes for either host direction.

The driver reads and hashes the plan named by `--milestone-plan`. Its canonical
`budgets` front-matter value must be `binding` and must match the declared
`--budget-status`; a command-line override alone never promotes a budget.

milestone-0.1.0.0 / `v0.1.0` therefore runs only the provisional profile. The
`binding-reference` profile, physical NVIDIA H2D/D2H and passthrough evidence,
and any promotion of the numeric budgets belong to milestone-1.0.0.0 / `v1.0.0`.

Run reference measurements on an otherwise idle host. Record Intel and AMD as
separate artifacts; one host never stands in for the other. The client CPU is
the lowest CPU in `sched_getaffinity` unless `--cpu` names another effective CPU.
The daemon CPU defaults to the lowest effective CPU on the same NUMA node that
does not share the client's physical core; `--worker-cpu` may select it
explicitly. Sharing a core or crossing NUMA nodes makes a binding run ineligible.
NUMA policy is recorded from the process mappings and cpuset; allocations use
first touch on the pinned process CPU. A future explicit `mbind`/`numactl` policy
must be a separate factor, not an unrecorded harness change.
