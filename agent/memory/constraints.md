---
status: Current
updated: 2026-08-31
---

# Durable Constraints

Canonical product sources are [M0100](../plan/M0100-core-foundation/plan.md),
[W0103](../plan/M0100-core-foundation/work/W0103-compiler-cpu.md), and
[W0106](../plan/M0100-core-foundation/work/W0106-modes-release.md). Tool
identity and provisioning boundaries live in
[`toolchains/README.md`](../../toolchains/README.md).

- Product release identity comes from the root [`VERSION`](../../VERSION) and
  remains standard three-part SemVer. Four-part delivery coordinates and their
  derived M/W/S identifiers follow
  [`docs/release-versioning.md`](../../docs/release-versioning.md) (D0024).
- Target Linux x86_64 and glibc. Kernel work uses the target kernel's Kbuild.
- The userspace glibc floor is 2.31 (Ubuntu 20.04, D0009). Provider `DT_NEEDED`
  is restricted to `libc.so.6` plus `libpthread.so.0` and `libdl.so.2` only
  where a pre-2.34 target requires them; the kernel-module validation matrix is
  independent of this floor.
- The `v0.1.0` generic release matrix is Ubuntu 20.04.6, Ubuntu 22.04.5,
  Ubuntu 24.04.4, and Rocky Linux 9.8 (D0012). Native NixOS VM/package
  qualification belongs to the `v0.2.0` support expansion. Per-run image and
  update digests remain required evidence.
- Application-side providers and client fast path use C17 and keep LLVM/MLIR,
  Python, systemd, and the C++ runtime out of the provider closure.
- Runtime services, compiler code, scheduler, and execution backends use C++20.
- Generic daemon release builds statically link the required MLIR/LLVM component
  closure and do not require `libMLIR` or `libLLVM` at runtime (D0019). The
  shared-framework switch is qualification-only.
- Cross-component plugin boundaries use versioned C ABIs. Encoded/shared/UAPI
  records follow the narrower rules in the [contracts index](../../contracts/README.md).
- Compiler epoch 1 tool identity is defined by the
  [toolchain index](../../toolchains/README.md#compiler-epoch-1-d0018) and
  [`compiler-epoch-1.json`](../../toolchains/compiler-epoch-1.json): Clang,
  MLIR, LLD, and LLVM 22.1.8 plus the single D0018 correctness backport.
- Compiler epoch 1 accepts only the D0017 PTX 9.0/sm_70 capability and
  instruction-form manifests bound to their checked-in 17-fixture semantic
  corpus. Parser, verifier, interpreter, target lowering, and differential
  evidence advance together for any future manifest revision.
- CUDA Driver and NVML ABI sources come only from the manifest-epoch-1 exact
  R535/R550/R570/R580/R610 package and extracted-header digests; changing a
  family requires an explicit manifest epoch bump (D0016).
- Nix only fixes and provides declared tool versions. Git owns source identity,
  CMake/Ninja own configure and build, CTest and test harnesses own testing,
  `packaging/` owns release artifacts, sessions own durable work evidence, and
  invoking tools or host operators own cleanup and garbage collection (D0022).
- Agent startup is Nix-first (D0031). The stable harness subject comes only from
  active system/developer runtime instruction context; Codex declares exactly
  `codex`. Model/template/backend/build/CLI/session/thread/prompt labels and
  identity inference are invalid. Except for host Git/Nix bootstrap, every
  executable and tool/version/capability probe runs through the Git-aware
  `nix develop . --command ...` environment before ambient host inspection.
  SC0008 provides no compatibility route for the superseded startup order.
- Breaking replacements of established semantics, identifiers, constraints,
  record shapes, or authority use a decision-bound `SCNNNN` migration (D0025).
  The current checkout synchronizes every affected record, while historical
  commands, outputs, counts, revisions, hashes, provenance, and observed facts
  remain unchanged. Terminal sessions and checkpoints are otherwise protected.
- Durable work has one machine execution focus (D0029). It names one exact
  `in_progress` owner and either one dependency-valid product work item with its
  canonical Exit Gate or one decision-authorized governance migration with a
  dependency-valid resume target. Session scaffolding and local testability do
  not claim or redirect focus. Content commits declare the exact owner through
  `METAFLUX_SESSION_ID`; focus transfer is an atomic record-only close/install.
- The current execution-governance epoch is D0029. Focus and its owner use
  schema version 2 and declare that exact epoch. Schema version 1 and pre-epoch
  sessions cannot own focus, be upgraded in place, or provide fallback task
  context. SC0007 liquidates their detailed ledgers from the current tree after
  `$roast` verifies that only already-promoted medium and dark claims remain in
  their canonical owners; a compact ID tombstone is administrative resolution,
  not compatibility or knowledge storage.
- A tool newly required by a repeatable workflow is versioned in the Nix-provided
  tool environment before use; this does not transfer workflow semantics or
  outputs to Nix. D0022 supersedes the broader D0021 wording.
- Artifact downloads follow the configured-timezone, adjacent-timezone, then
  canonical route while frozen upstream identity remains authoritative; route
  details are per-run evidence (D0020).
- Rust and handwritten assembly are outside compiler epoch 1 unless a later
  measured decision explicitly changes that boundary.
- The runtime and compiler core remain ecosystem-neutral. Compatibility plugins
  do not depend on concrete execution backends.
- Local managed providers select cdev before memfd. cdev uses the Unix daemon
  session as its object-table control plane and the cdev queue as its steady-state
  data plane, bound to one session/view/generation for the provider initialization
  epoch. Memfd fallback is limited to pre-success cdev `ENOENT`, `ENODEV`, or an
  explicit ABI incompatibility; permission, malformed/integrity, and policy
  failures are terminal. Kernel registered-memory handles remain opaque and are
  not host pointers; CPU backend payload mappings require an explicit binding.
- Mutable provider state belongs to one negotiated shared view, not per-DSO
  globals. Statically embedded fast-path code remains stateless.
- Vendor CUDA/NVML passthrough loads one validated same-build pair by canonical
  absolute paths from the distribution whitelist or root-owned override; ambient
  loader search paths never participate (D0013).
- Mutable compiler cache content is peer-credential-UID isolated, quota reserved
  before compilation, atomically published, and deterministically evicted; the
  administrator AOT tier is separate and read-only (D0014).
- CPU execution uses effective physical cores and NUMA-local pools, does not
  oversubscribe, schedules indivisible CTAs, and keeps cross-node stealing off
  by default (D0015).
- M0100 / `v0.1.0` host evidence is AMD x86_64. Intel x86_64 support
  qualification belongs to M1000 / `v1.0.0` (D0027, superseding D0023 only for
  the future destination).
- Generic packages do not overwrite vendor-owned libraries or device nodes and
  do not require Nix store paths at runtime.
- M0100 performance budgets remain provisional throughout `v0.1.0`. Promotion
  to binding budgets requires the physical NVIDIA H2D/D2H and passthrough
  evidence owned by M1000 / `v1.0.0`; an AMD-only host run cannot promote them.
  Canonical target values and measurement rules remain in
  [M0100](../plan/M0100-core-foundation/plan.md).

When a task would relax one of these constraints, create or update a canonical
architecture decision before implementation.
