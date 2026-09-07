---
id: work-item-0.2.0.2
delivery: 0.2.0.2
milestone: milestone-0.2.0.0
status: Draft
area: compiler.cpu
depends_on: [work-item-0.2.0.1]
updated: 2026-09-06
---

# Torch Kernel Intake and Eager Execution

## Outcome

torch CUDA kernels reach the managed execution pipeline and eager operations
(add, mul, matmul, reduction) run through the daemon with results bit-exact
against the torch CPU reference.

## Work

- [ ] Close decision item 1: pick the kernel-intake strategy — an
  arch-pinned PTX-bearing client profile (client build emits `compute_70`
  PTX that rides the frozen decision-0017 pipeline) versus the deferred
  cubin/SASS intake worker — and record the governing decision with
  rationale and rejected alternatives.
- [ ] If torch's PTX forms exceed the frozen capability/instruction-form
  manifests, run the decision-0017 manifest/corpus revision with parser,
  verifier, Kernel IR, interpreter, lowering, and differential evidence
  advancing together, plus the compiler-epoch and cache-identity bumps.
- [ ] Load torch modules at corpus volume through the module path: many
  kernels per module, warm-cache hits, and per-kernel function resolution.
- [ ] Execute the eager-operation corpus end-to-end (probe stages
  `artifact-intake` and `eager-add`, then an extended op list) and verify
  bit-exact against the torch CPU reference; archive raw samples under
  `tmp/outputs/`.
- [ ] Keep the frontier profile a recorded gap list: no frontier claim without
  its own intake evidence.

## Exit Gate

The baseline profile reaches probe stage `complete`; the extended eager
corpus is bit-exact through a stock daemon on the CPU backend; any manifest
revision is bound by its own hashes, epoch bump, and green differential
gates.


## Measured progress (2026-09-08)

Groundwork from the enumeration work item: real fatbin-size parsing, per-
module blob retention, cubin ELF symbol-table kernel-name extraction, the
CUDA 12 kernel-query surface (GetKernelCount, EnumerateKernels,
KernelGetName, registry-backed GetKernel), and a launch-time semantic router
(`sleep_kernel` no-op; elementwise AddFunctor int32 host add through the
copy path). Wheel survey: 387 fatbins, 2714 entries, all kind=2 cubins, no
PTX — daemon PTX compilation cannot serve client kernels; the semantic
profile (or a future SASS worker) is the only viable strategy.

Blocking frontier (negative finding): `torch.cuda._sleep(1)` fails with
cudaErrorInvalidDeviceFunction while the provider observes ZERO driver
calls — no launch, no kernel query, no load. cudart 12.6 resolves
registered functions entirely from its own registration metadata and the
negative decision is cached before any driver contact, in both loading
modes. The container state (g+0x68) and its vtable were exonerated: the
lookup/create path completes successfully. Next: attach to cudart's
registered-function records at __cudaRegisterFunction time and step the
launch-time resolution to find which internal field fails validation
(the entry walker at 0x40380 and the 0x426xx accessor are the anchors).

Refined frontier (2026-09-08, second pass): with entry traces on all four
kernel-query APIs plus cuLaunchKernel, a torch `_sleep` launch under either
loading mode shows ZERO driver calls between device init and the 701 — the
registered-function hash lookup inside cudart's cudaLaunchKernel misses
before any module load or kernel query. Hard evidence: the host pointer
torch passes at launch (0x7fff01528300) IS present in the
__cudaRegisterFunction stream (`_ZN2at4cuda40_GLOBAL__N__758183f4_8_Sleep_
cu_088d913f11spin_kernelEl`, at::cuda spin_kernel<long> from Sleep.cu), and
cudaLaunchKernel (0x75800) is entered but returns 701 from an internal
branch. Next: single-step 0x75800 from entry to its 0x2bd-producing branch
with a completion-aware stepping loop (the launcher body is large; the
previous 2M-step loop timed out mid-body), then satisfy the hash-lookup
input that misses — likely a provider-observable state the registration
path records and the launch path re-validates.

Registration-vs-launch comparison (2026-09-08, third pass): the launch host
pointer 0x7fff01528300 IS present in the __cudaRegisterFunction stream as
`_ZN2at4cuda40_GLOBAL__N__758183f4_8_Sleep_cu_088d913f11spin_kernelEl`
(at::cuda spin_kernel<long> — the earlier "sleep" grep missed it by case).
So registration succeeds and the launch reaches cudaLaunchKernel's
registered-function lookup, which then fails internally: 701 has no
immediate anywhere in the binary (no `mov/cmp $0x2bd`), the two rodata
error-table copies (0x8d764, 0xac258) are never read during the failing
launch (hardware read watchpoints silent), and the read watchpoint plus a
6M-instruction native stepping loop over cudaLaunchKernel did not reach a
return. The error is produced by internal logic (likely computed or
enum-cached) with no trappable producer.

Current green state holds under LAZY loading: set_device, device_count,
both probe stages, 144/144 CTest. Next options: (a) gdb scripting at the
__cudaRegisterFunction record level (break the record constructor, diff a
bound-working vs bound-failing kernel's records); (b) accept the artifact
boundary for milestone-0.2.0.0 and move the semantic-launch strategy to a
dedicated design decision; (c) SASS worker feasibility spike. Each is a
full iteration; the enumeration and copy surfaces stay green regardless.

Refined root-cause (2026-09-08, fourth pass): cudaLaunchKernel (0x75800)
branches on nested+0x34c — zero takes the fast path into the device
initializer (12f50), which reaches the container accessor 425f0; the
lookup/create round-trip through c693[+0x10] ends with the container's
per-device map insert (3a670, keyed on the current context) returning
failure for our zero state blob, and that failure surfaces as 701. The
complete fix requires implementing the driver-side container-map semantics:
the state blob must be created, keyed, and registered through 3a670's map
on first use (keyed by the primary context), matching what the real driver
does between retain and launch. This is the concrete final piece of the
container contract; everything before it (vtable polarity, stateful
lookup, retain-current, LAZY mode) is landed and green.
