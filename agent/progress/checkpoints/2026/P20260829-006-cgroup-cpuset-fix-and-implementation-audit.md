---
id: P20260829-006
date: 2026-08-29
status: Recorded
revision: 694272a
trigger: cgroup cpuset fallback fix and M0001 implementation audit breakthrough
---

# Cgroup cpuset fallback fix and M0001 implementation audit

## Outcome

Content revision `694272a` fixes cgroup cpuset fallback for scopes without cpuset
controller and confirms W02/W03/W04 are fully implemented through a comprehensive
audit.

## What changed

- `plugins/backend/cpu/runtime/src/placement.cpp`: Added `read_list_file_up()`
  that walks the cgroup v2 directory hierarchy to find the nearest ancestor with
  `cpuset.cpus.effective` and `cpuset.mems.effective`. When no ancestor has the
  cpuset controller mounted, treats the constraint as unconstrained (fall back to
  `sched_getaffinity` for CPUs, online nodes for memory).
- `W02-contracts-runtime.md`: Marked registry/latch/handle implementation as
  complete (registry_recovery.cpp 2294 lines, runtime.cpp, recovery_model.cpp).
- `W03-compiler-cpu.md`: Marked daemon lifecycle/credentials/cache/AOT as
  complete (server.cpp SO_PEERCRED, compiler_worker_process.cpp isolation,
  artifact_cache.cpp 1394 lines).
- `W04-cuda-provider.md`: Marked fast path routing as complete (dispatch.c
  routes launch/copy/event/sync through fastpath.c shared-memory ring).

## Verification

| Gate | Result |
| --- | --- |
| Dev CTest |59/59 pass |
| ASan CTest |59/59 pass |
| Recovery stress ordinary |50/50 pass, zero failures |
| Recovery stress ASan |20/20 pass, zero failures |
| Daemon integration |5/5 pass |
| Fastpath + provider |18/18 pass |
| Provider co-load | Pass |
| CPU Add/Copy differential | Pass |
| check-agent-records | ok |
| Optimization runner self-test |12/12 pass |
| Measurement runner self-test |12/12 pass |

## AMD reference host

- CPU: AMD Ryzen 7 H 255 w/ Radeon 780M Graphics (AuthenticAMD)
- Cores:8 physical,16 logical (SMT)
- NUMA: single node (node0)
- Affinity:0-15 (all CPUs)
- Memory:30 GiB total
- Kernel:6.18.42-1-cachyos-lts
- glibc:2.42
- Clang:22.1.8
- CMake:4.1.6
- Ninja:1.13.2
- Git:436388e (pre-fix),694272a (post-fix)

## Remaining M0001 work

W01: Intel/AMD dual-host qualification (requires Intel host access).
W02: Multiprocess stress with concurrent admission/lease/release (needs harness).
W06: PGO training, -O2/-O3 comparison, hardening/soak/fuzz archival (requires
clean Git tree run), NixOS native package (requires flake output), extended
audit dimensions (syscall/cache-line/NUMA/asm/relocation/RSS).
