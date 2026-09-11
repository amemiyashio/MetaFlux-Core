---
status: Current
updated: 2026-09-10
---

# Durable Constraints

Canonical product sources are [milestone-0.1.0.0](../plan/milestone-0.1.0.0-core-foundation/plan.md),
[work-item-0.1.0.3](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.3-compiler-cpu.md), and
[work-item-0.1.0.6](../plan/milestone-0.1.0.0-core-foundation/work/work-item-0.1.0.6-modes-release.md). Tool
identity and provisioning boundaries live in
[`toolchains/README.md`](../../toolchains/README.md).

- Product release identity comes from the root [`VERSION`](../../VERSION) and
  remains standard three-part SemVer. Four-part delivery coordinates and their
  full-word milestone/work-item identifiers follow
  [`docs/release-versioning.md`](../../docs/release-versioning.md) (decision-0024).
- Target Linux x86_64 and glibc. Kernel work uses the target kernel's Kbuild.
- The userspace glibc floor is 2.31 (Ubuntu 20.04, decision-0009). Provider `DT_NEEDED`
  is restricted to `libc.so.6` plus `libpthread.so.0` and `libdl.so.2` only
  where a pre-2.34 target requires them; the kernel-module validation matrix is
  independent of this floor.
- The `v0.1.0` generic release matrix is Ubuntu 20.04.6, Ubuntu 22.04.5,
  Ubuntu 24.04.4, and Rocky Linux 9.8 (decision-0012). Native NixOS VM/package
  qualification belongs to the `v0.3.0` support expansion. Per-run image and
  update digests remain required evidence.
- Application-side providers and client fast path use C17 and keep LLVM/MLIR,
  Python, systemd, and the C++ runtime out of the provider closure.
- Runtime services, compiler code, scheduler, and execution backends use C++20.
- Generic daemon release builds statically link the required MLIR/LLVM component
  closure and do not require `libMLIR` or `libLLVM` at runtime (decision-0019). The
  shared-framework switch is qualification-only.
- Cross-component plugin boundaries use versioned C ABIs. Encoded/shared/UAPI
  records follow the narrower rules in the [contracts index](../../contracts/README.md).
- Compiler epoch 1 tool identity is defined by the
  [toolchain index](../../toolchains/README.md#compiler-epoch-1-decision-0018) and
  [`compiler-epoch-1.json`](../../toolchains/compiler-epoch-1.json): Clang,
  MLIR, LLD, and LLVM 22.1.8 plus the single decision-0018 correctness backport.
- Compiler epoch 1 accepts only the decision-0017 PTX 9.0/sm_70 capability and
  instruction-form manifests bound to their checked-in 17-fixture semantic
  corpus. Parser, verifier, interpreter, target lowering, and differential
  evidence advance together for any future manifest revision.
- CUDA Driver and NVML ABI sources come only from the manifest-epoch-1 exact
  R535/R550/R570/R580/R610 package and extracted-header digests; changing a
  family requires an explicit manifest epoch bump (decision-0016).
- Nix only fixes and provides declared tool versions. Git owns source identity,
  CMake/Ninja own configure and build, CTest and test harnesses own testing,
  `packaging/` owns release artifacts, Git owns committed evidence, and invoking
  tools or host operators own cleanup and garbage collection (decision-0022).
  Nix declarations are strictly limited to tool version/source/input/patch/hash
  identity, materialization, and shell exposure; they do not encode project
  workflows, Agent execution policy, evidence, cleanup, or host installation.
  Fixed means clear and reproducibly stable for the current revision, not
  permanently immutable; governed manifest/lock updates may advance versions.
- Agent startup is Nix-first (decision-0031) and agent identity follows
  decision-0034. The Nix shell is the required execution environment for
  every tool invocation: `nix develop . --ignore-environment --keep HOME --keep USER --command ...` is the full Git-aware
  repository entry, and `nix shell .#<tool-output> --command ...` is the
  preferred form when a workflow needs one named tool output without the
  whole development shell. Ambient host execution is never preferred; it is
  only a recorded prerequisite after a proved Nix gap (decision-0036).
  [$main](../skills/main/SKILL.md) skill runs inside the Git-aware Nix
  environment and derives its ephemeral subject only from the harness name
  already emitted in the current conversation. It does not scan PATH, walk
  processes, or probe an executable or `--version`.
  Model/provider/template/backend/build/prompt/session/thread,
  repository prose, Git configuration, and user labels that are not that
  harness name are invalid inputs and are never output fields. A missing
  declaration requires the conversation-emitted harness name instead of
  PATH-order selection. Except for host Git/Nix bootstrap, every
  executable and tool/version/capability probe runs through the Git-aware
  `nix develop . --ignore-environment --keep HOME --keep USER --command ...` environment before ambient host inspection.
  Shell grammar for repository work runs under the Nix-provided bash
  (`nix develop . --ignore-environment --keep HOME --keep USER --command bash -c '...'`); the ambient host shell never
  executes repository tools.
  [Clean initialization](../../toolchains/README.md#clean-tool-environment-decision-0057)
  owns inherited-environment isolation; single-tool forms run only after that entry.
  The caller harness is not pinned or installed as a repository Nix tool.
  Newly required tools are added to the repository Nix declaration first. Only
  a confirmed Nix materialization gap permits decision-0032 host escalation through
  [$manage-host-privilege](../skills/manage-host-privilege/SKILL.md) skill (decision-0036): an exact pacman package is installed
  through the
  package-name-only root helper, then
  its exact host executable may run from inside the Nix entry environment. This
  host copy is a local prerequisite, not declared repeatable tool identity or
  release evidence. MetaFlux driver privilege uses the separate decision-0032 action
  allowlist for current module lifecycle, kernel-log/kmemleak inspection, and
  the named live cdev qualification binary under the configured repository
  root. That skill is the sole owner for sudo/su, root-helper, persistent-grant,
  and revocation policy. Neither path permits arbitrary root commands or
  credential persistence.
- Agent execution follows decision-0033 as amended by decisions 0037 and 0054.
  `agent/goal.json` stores only the active Epoch, Batch, product target,
  objective, research-only reference prerequisites, and
  planned/integrated/deferred lanes. Goal schema v4 binds every lane to one work
  item. [$main](../skills/main/SKILL.md) skill dispatches
  [$epoch](../skills/epoch/SKILL.md) skill, [$batch](../skills/batch/SKILL.md) skill,
  or [$iteration](../skills/iteration/SKILL.md) skill. Workers deliver schema-v2
  exact candidates and actual content-bound verification receipts. The acceptance controller alone
  advances daily accepted state after fresh integration verification. Slice
  acceptance preserves the lane/work item/target and allocates the Batch maximum
  Iteration plus one; complete acceptance requires the full Exit Gate. Selection
  uses dependencies and lane array order, never Iteration priority. Product
  source and test mutation prefers a parent briefing, a bounded coding
  subagent, and parent conversational review against drift. The parent does not
  start the next coding-subagent dispatch or Iteration cycle until that review
  accepts the dispatched briefing goal. Coding subagents never edit
  `goal.json`, integrate, govern, commit, or push. Conversation, actor,
  branch, worktree, timeline, activity, and review notes are not repository
  authorities.
- Breaking repository governance uses an explicitly requested [$epoch](../skills/epoch/SKILL.md) skill
  cutover. It directly rewrites all affected current authority, promotes useful
  knowledge through knowledge promotion, deletes obsolete surfaces, and publishes the next
  Epoch only after complete regression. There is no dual-write, alias, old-format
  parser, migration ledger, or compatibility route; Git history is the recovery
  boundary. Maintenance, Epoch activation, and Batch acceptance automatically
  publish their exact guarded commit through [$main](../skills/main/SKILL.md) skill unless the user limits
  publication (decisions 0038 and 0054). Worker candidates go to [$batch](../skills/batch/SKILL.md) skill without
  pushing. Failed publication recovers the same commit; changed remote ancestry
  requires the application's new exact execution context.
- Research-only upstream sources live under `references/` as manifest-backed
  exact submodule gitlinks (decision-0047). Ordinary clone, build, test,
  package, and release workflows never recurse into them. A lane may require
  explicit materialization before implementation, but the checkout and its
  notes are not product truth or acceptance evidence. Relied-upon claims are
  promoted to one canonical Core owner; any build or qualification input is
  separately pinned by `toolchains/`.
- A tool newly required by a repeatable workflow is versioned in the Nix-provided
  tool environment before use; this does not transfer workflow semantics or
  outputs to Nix. decision-0022 supersedes the broader decision-0021 wording.
- glibc is the only libc: every MetaFlux runtime, release, and qualification
  artifact builds and runs against glibc, and no workflow may introduce musl or
  any other libc replacement (user mandate, performance-first). Guest
  qualification binaries ship with the glibc runtime they need (dynamic
  binaries plus the loader and libraries in the guest image) instead of
  switching libc. Instrumentation or bootstrap defects inside a glibc static
  link are fixed within the glibc toolchain, never by changing libc.
- Physical-hardware gates are isolated in milestone-2.0.0.0 (decision-0040):
  Intel x86_64 host rows, physical NVIDIA binding/performance evidence, and
  physical dual-driver Vulkan rows (including the external-memory freeze and
  validation soak) are `v2.0.0` scope. `v0.1.x` milestones, milestone-1.0.0.0,
  and the active-session goal never gate on hardware the executing fleet
  cannot provide; agent plans must defer such rows by referencing
  milestone-2.0.0.0 instead of leaving them as open blockers.
- Podman is the approved container runtime for repository qualification and is
  exposed by the release development shell; agents use podman through
  `nix develop .#release` and never install or invoke a second ambient runtime.
  The repository `docker/` directory is the approved home for container build
  contexts and Dockerfiles owned by repository qualification. Release rows keep
  the established transport policy: digest-pinned image references, `--pull=never`,
  and `--network=none` (see [`tests/release/README.md`](../../tests/release/README.md));
  base-image acquisition remains a separately recorded operator step.
- Artifact downloads follow the configured-timezone, adjacent-timezone, then
  canonical route while frozen upstream identity remains authoritative; route
  details are per-run evidence (decision-0020).
- Rust and handwritten assembly are outside compiler epoch 1 unless a later
  measured decision explicitly changes that boundary.
- The runtime and compiler core remain ecosystem-neutral. Compatibility plugins
  do not depend on concrete execution backends.
- The stock PyTorch CUDA baseline must negotiate `KERNEL_REQUEST_REGISTER`
  capability bit 11 before it submits its v1 neutral kernel request. The sealed
  request carries only the baseline operation/profile and Kernel IR versions;
  source remains live through module load, after which daemon-owned canonical
  Kernel IR outlives the client artifact until module unload (decision-0048).
- The baseline's profile-specific cudart table UUIDs and slot behavior are
  observations, never CUDA Driver guarantees. Any reached but unclassified slot
  returns `CUDA_ERROR_NOT_SUPPORTED`; it may not synthesize a successful result
  or leave output undefined (decision-0049).
- The stock PyTorch interpreter row records source provenance and explicit
  compiler/cache non-use. Its JIT/AOT rows and the CPU compiled subset require
  actual compiled-artifact/cache identity; decision-0050 is mode-specific,
  refined by decision-0055. Declared counts and compilation counters do not
  replace per-request actual executor evidence for full CPU-profile acceptance.
- The PyTorch CUDA qualification boundary follows decision-0055: preserve
  stock source/wheel/API, require actual physical AMD GPU submission/completion
  for a Vulkan claim, and fix the backend before visible context resources
  succeed. A daemon-native CPU result or per-kernel CPU fallback is not GPU
  success. Broader applications require declared scope and separate evidence.
- Library-backed PyTorch operations use only qualified MetaFlux ABI shims that
  translate accepted calls into versioned neutral requests for daemon-owned
  execution. No compatibility provider vendors or executes an upstream GPU
  library, reads tensor inputs, or materializes results. The current boundary is
  limited to decision-0053's pinned float32 SGEMM and cuBLASLt bias-linear
  profiles; other valid configurations fail before submission until separately
  specified and qualified.
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
  loader search paths never participate (decision-0013).
- Mutable compiler cache content is peer-credential-UID isolated, quota reserved
  before compilation, atomically published, and deterministically evicted; the
  administrator AOT tier is separate and read-only (decision-0014). Those
  installed roots remain `/var/cache/metaflux/compiler` and
  `/var/lib/metaflux/aot`.
- Build and product-test scratch live under `tmp/` at the repository
  root (decision-0042): CMake/Ninja trees in `tmp/build/<preset>`,
  debug-kernel overlays in `tmp/build/debug-kernel`, measurement dumps and
  checker JSON in `tmp/outputs/<name>`, retained work directories in
  `tmp/work/<name>`. The invoking tool owns cleanup. Do not write
  `build/`, `.cache/`, `outputs/`, `../.metaflux-build`, or
  `../.metaflux-evidence`. Guest or container `/tmp` inside a qualification
  image is that image's filesystem. Decision-0054 adds the narrow Git-ignored
  `agent/tmp/main/` home for current controller state, verification receipts,
  and pending transactions; it is excluded from authority scans and forbidden
  from tracking. It is neither authorization nor product-completion evidence.
  Nix store GC remains host-operator
  ownership (decision-0022).
- CPU execution uses effective physical cores and NUMA-local pools, does not
  oversubscribe, schedules indivisible CTAs, and keeps cross-node stealing off
  by default (decision-0015).
- milestone-0.1.0.0 / `v0.1.0` host evidence is AMD x86_64. Intel x86_64 support
  qualification belongs to milestone-2.0.0.0 / `v2.0.0` (decision-0040).
- Generic packages do not overwrite vendor-owned libraries or device nodes and
  do not require Nix store paths at runtime.
- milestone-0.1.0.0 performance budgets remain provisional throughout `v0.1.0`. Promotion
  to binding budgets requires the physical NVIDIA H2D/D2H and passthrough
  evidence owned by milestone-2.0.0.0 / `v2.0.0`; an AMD-only host run cannot promote them.
  Canonical target values and measurement rules remain in
  [milestone-0.1.0.0](../plan/milestone-0.1.0.0-core-foundation/plan.md).

When a task would relax one of these constraints, create or update a canonical
architecture decision before implementation.
