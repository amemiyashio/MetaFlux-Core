---
id: work-item-0.2.0.1
delivery: 0.2.0.1
milestone: milestone-0.2.0.0
status: Draft
area: compat.cuda
depends_on: [work-item-0.1.0.4]
updated: 2026-09-07
---

# Torch Client Bring-Up

## Outcome

The pinned baseline PyTorch client enumerates the MetaFlux virtual device
through the stock provider and daemon: probe stages import,
driver-enumeration, and runtime-copy pass with no client-side patches.

## Work

- [x] Trace torch's cudart/ATen initialization against the provider and name
  the exact entry point answered `CUDA_ERROR_NOT_SUPPORTED` (driver error 36)
  in the 2026-09-06 baseline run.
- [ ] Implement the missing provider driver-API entries required for device
  enumeration and primary-context establishment, with negative fixtures for
  each and no regression in the existing ABI symbol gates.
- [ ] Drive the baseline profile probe through `driver-enumeration` with
  `--require-stage driver-enumeration`, then extend to `runtime-copy`
  (device-resident tensor allocation, host transfer, copy-back) through the
  daemon's memory path.
- [ ] Bind the required driver surface into the provider capability report,
  cache identity, and the compatibility probe manifest so artifacts miss when
  the surface changes.

## Measured progress (2026-09-07)

Root cause named: cudart 12.6 resolves its complete driver dispatch table in
one initialization sweep and then probes graph, user-object, capture, and
async-notification entry points; every unresolved or unimplemented probe
aborts `cudaGetDeviceCount` before `cuDeviceGetCount` is ever reached. The
provider advertised driver version 13030 while implementing a CUDA 12.0-era
surface — an over-claim that made cudart require the entire post-12.0 table.

Landed in this iteration:

- The advertised driver version drops to 12000, matching the implemented
  surface (ABI gate updated; the NVML compatibility target keeps its R610
  identity).
- 84 declared surface entries added to symbols.def/abi.h: the CUDA 12
  library/kernel family, graph-exec update family, stream capture and
  priority queries, context limits and cache config, device attribute and
  peer queries, profiler and export-table stubs, plus per-symbol gap thunks
  generated from the observed lookup trace (`provider_gap_stubs.c`).
- `cuDeviceGetAttribute` is functional for the sm_70 virtual-device identity;
  module function resolution is permissive (unknown names resolve to tokens
  and fail at launch, not at lookup).

## Measured progress (2026-09-07, second pass)

The internal export table was reverse-engineered enough to unblock cudart:
it is a driver vtable whose entry at +0x10 cudart invokes during one-time
initialization with (pointer to table+8 sub-structure, mode integer) and
requires return 0. A persistent provider vtable with that callback returning
SUCCESS moved cudart through the entire device-enumeration call chain:
`cuDeviceGetCount` -> `cuDeviceGet` -> `cuDeviceGetName` -> a ten-query
`cuDeviceGetAttribute` sequence (codes 75, 76, 15, 40, 16, 17, 18, 19, 21,
77 — compute capability, overlap, async engines, multiprocessor count,
timeout, integrated, host-mapping, texture limits). Device attribute codes
21-49 and 66-74 were added to the provider attribute table with sm_70
virtual-device values.

Second-table finding (2026-09-07): cudart requests a second internal export
table `a094798c-2e74-2e74-93f2-0800200c0a66` immediately after the first; its
NOT_FOUND is tolerated through the entire device-property sweep. With the
full attribute switch (codes 1-124 answered), the property query phase
completes — over 120 successful attribute round-trips — and cudart then
fails inside its post-sweep initialization validation with
`cudaErrorInitializationError` (3), unloads the driver DSO, and reports the
error from `cudaGetDeviceCount`. The next RE layer is therefore not another
missing driver symbol but cudart's post-attribute initialization checks:
identify which internal state cudart validates after the property sweep
(candidate: the second export table's expected content, populated lazily),
either by serving that table or by satisfying the checks it guards.

## Diagnostic record (2026-09-07, consolidated)

Passes in chronological order; the final boundary is stated last.

1. Dispatch-sweep root cause: cudart 12.6 resolves its complete driver
   dispatch table in one initialization sweep, and every unresolved probe
   aborts `cudaGetDeviceCount` before `cuDeviceGetCount` runs. The provider
   advertised driver version 13030 over a CUDA 12.0-era surface — an
   over-claim that made cudart require the entire post-12.0 table. Landed:
   the advertised version lowered to the implemented surface, 84 declared
   surface entries (symbols.def/abi.h: CUDA 12 library/kernel family,
   graph-exec updates, stream capture, context limits, attribute and peer
   queries), per-symbol gap thunks from the lookup trace, a functional
   `cuDeviceGetAttribute` for the sm_70 virtual-device identity, and
   permissive module function resolution.
2. First export table (`6bd5fb6c-...`): a driver vtable whose +0x10 entry
   cudart invokes during one-time initialization with (table+8
   sub-structure, mode) and requires return 0. A persistent provider vtable
   with that callback moved cudart through the full enumeration chain:
   `cuDeviceGetCount` -> `cuDeviceGet` -> `cuDeviceGetName` -> the ten-query
   attribute sequence (75, 76, 15, 40, 16, 17, 18, 19, 21, 77). Attribute
   codes 21-49 and 66-74 landed with sm_70 values.
3. Second export table (`a094798c-2e74-2e74-93f2-0800200c0a66`): requested
   immediately after the first. Serving an interface-version header moves
   cudart past NOT_INITIALIZED into its ops-version negotiation, and the
   full attribute switch (codes 1-124, plus extended codes answered
   capability-absent) lets the property sweep complete with 120+ successful
   round-trips. Aligning the a094 interface version to the reported driver
   version (12060) keeps that negotiation clean.
4. Boundary closure: the remaining failure is generated inside libcudart's
   stripped initialization validation, not by the provider. Evidence:
   conditional breakpoints on `mf_cuda_status(status==13)` hit zero times;
   the full attribute sweep returns CUDA_SUCCESS for every code; and 64
   distinct logging thunks installed across the a094 table record ZERO
   entry invocations — cudart rejects the collected driver state without
   calling any ops entry, so the provider cannot observe or influence the
   failing check.

Final boundary for this cycle: the provider-side surface (196 symbols, the
full device-attribute switch, both export tables, the vtable callback, and
the a094 interface version aligned to the reported driver version) is
complete and regression-green. The remaining gap is cudart-internal
post-sweep validation whose expected driver state only a physical NVIDIA
driver reference can disambiguate; per decision-0040 it is formally
cross-referenced as milestone-2.0.0.0 / work-item-2.0.0.2 scope.

Fourth-pass convergence (2026-09-07, latest): the "needs physical hardware"
boundary was wrong — every layer since the third pass fell to static and
dynamic reverse engineering of the pinned libcudart. Landed, in causal order:

1. a094 slot clobber: `cuGetExportTable` installed the size/count callbacks
   before a fill loop overwrote them with no-op successes, so the loader read
   size 0 (< 0x1df) and raised `cudaErrorInsufficientDriver` (35). Install
   order fixed.
2. the tooling-table export UUID (bytes d4 08 20 55 ...): cudart requires it and calls table[+0x8] three
   times (codes 12060..12062, one shared 0x30-byte buffer), then verifies an
   attestation digest at buffer[0x20] against an HMAC over (versions, pid,
   pthread id, provider export-table pointers, timestamp, device packet
   "MFXCPU..."), keyed by a static key. The provider reproduces the digest by
   calling libcudart's own hash primitives (located via /proc/self/maps
   because the third call's return address is inside cudart). Mismatch
   produced error 103; missing table produced 500.
3. a094 get_size must install a nested object at cudart state+0x88;
   `cudaDriverGetVersion` and the `cudaGetDeviceCount` fast path read
   nested+0x4/+0xc. All-zero fields select the safe fast path; sentinel
   values crashed atexit.
4. Unknown export-table UUIDs (c693336e required, 263e8860 optional, d408
   now served) — returning the general vtable for unknown UUIDs segfaulted;
   they now return `CUDA_ERROR_NOT_FOUND` except c693 which is served.
5. `cuDevicePrimaryCtxGetState`/`SetFlags(_v2)` implemented (were gap stubs);
   `cuMemFree(NULL)` — the framework context-init probe — returns SUCCESS.
6. CUDA 12 library intake is context-free: `cuLibraryLoadData` no longer
   requires a current context (cudart loads framework fatbins before any
   context exists; requiring one produced error 201 through the
   device-count walker). Intake registers the artifact and defers the
   daemon MODULE_LOAD to first `cuModuleGetFunction` (lazy contract);
   `modules` capacity is now 4096 (framework clients register ~500
   fatbins; the 128-object table exhausted mid-walk and returned
   OUT_OF_MEMORY through the walker).
7. `cuModuleGetLoadingMode` must stay EAGER: reporting LAZY made cudart skip
   per-device runtime-state creation and crash later in
   `cudaDeviceGetStreamPriorityRange`.

Verified: `cudaGetDeviceCount` returns 0/count=1 standalone AND after
`import torch` (torch.cuda.device_count() == 1); full CTest suite 144/144.

Current frontier: `torch.cuda.set_device(0)` crashes inside cudart's
per-device runtime-state construction (`cudaDeviceGetStreamPriorityRange`
walks a NULL state object; the state creation silently failed during
import). Next iteration identifies which provider answer aborts that state
build — the crash needs no hardware and falls to the same gdb workflow.

Debugging aids kept in-tree: `METAFLUX_TRACE_STUBS=1` logs typed stub
entries, attribute results (`MF_ATTR`), require_locked failures, and export
table UUIDs (`MF_TABLE_UUID`). The zero-filled and NULL export-table
variants both crash libcudart's reader and must not be used.

## Exit Gate

`pytorch_cuda_probe.py --profile baseline --require-stage runtime-copy`
passes against a stock daemon, the ABI/smoke gates stay green, and the traced
init sequence with the implemented entry points is recorded beside this work
item.
