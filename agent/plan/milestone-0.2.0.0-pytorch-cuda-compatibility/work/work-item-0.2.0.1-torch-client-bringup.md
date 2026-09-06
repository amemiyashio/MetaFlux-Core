---
id: work-item-0.2.0.1
delivery: 0.2.0.1
milestone: milestone-0.2.0.0
status: Draft
area: compat.cuda
depends_on: [work-item-0.1.0.4]
updated: 2026-09-06
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

Third-pass refinement (2026-09-07): serving the a094 table with an
interface-version header (64) plus size/count entries (512, 14) satisfies
the loader's version gate; the failure stays at the post-attribute-sweep
internal validation. The NOT_INITIALIZED(3) does not originate from the
provider's status mapper or require_locked paths (both instrumented, zero
hits) — it is generated inside libcudart's post-attribute-sweep
initialization validation. Next diagnostic layer: trace all provider
function returns during the post-sweep window to find which driver state
cudart rejects.

Debugging aids kept in-tree: `METAFLUX_TRACE_STUBS=1` logs typed stub
entries, attribute results (`MF_ATTR`), require_locked failures, and export
table UUIDs (`MF_TABLE_UUID`). The zero-filled and NULL export-table
variants both crash libcudart's reader and must not be used.

Second-table boundary named (2026-09-07, second pass): the second internal
table `a094798c-2e74-2e74-93f2-0800200c0a66` is the post-attribute-sweep
blocker. Serving it with an all-success ops table still segfaults libcudart
(its entries have real semantics: output pointers and callbacks), so the
provider returns NOT_FOUND for it and `cudaGetDeviceCount` fails cleanly
with error 500. Converging this table requires reverse-engineering each
entry's semantics from libcudart's reads of the region (gdb watchpoint
workflow established in this iteration) — the remaining scope of this work
item alongside the kernel-intake strategy.

Second-pass refinement (2026-09-07, latest): with driver version 12060
(exactly matching the baseline runtime) the failure signature changes from
NOT_FOUND to the post-sweep initialization validation, confirming the
version-compatibility boundary is now clean. The remaining work is scoped:
libcudart expects the vtable callback to populate the 0x408-byte
sub-structure at table+8 (cudart zeroes it before the call and reads it
afterwards); the sub-structure layout (function pointers vs data fields)
must be mapped by disassembling libcudart's reads of that region, then the
provider callback fills the entries the probe path requires. This is the
next iteration's full scope; the driver-version constant, attribute table,
and vtable callback from this iteration are prerequisites that are already
in place.

## Exit Gate

`pytorch_cuda_probe.py --profile baseline --require-stage runtime-copy`
passes against a stock daemon, the ABI/smoke gates stay green, and the traced
init sequence with the implemented entry points is recorded beside this work
item.
