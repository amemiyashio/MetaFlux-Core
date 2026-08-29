# Session Summary

## Objective and outcome

Added two exact, isolated PyTorch CUDA client environments and a staged gap
probe without changing M0001 product claims. The baseline is PyTorch
`2.11.0+cu126`/CUDA 12.6 for the current `sm_70` path; the frontier is PyTorch
`2.13.0+cu132`/CUDA 13.2 for future `sm_80` research. Both are explicit Nix
outputs and remain absent from normal development and release environments.

## Durable changes

- `toolchains/pytorch-cuda-clients-1.json` and its two locks: exact Python,
  PyTorch, CUDA, complete 29-wheel closures, hashes, sizes, and routed URLs.
- `nix/toolchains/pytorch-cuda-client.nix`: offline, hash-checked wheel
  materialization with bounded driver/optional host-transport ELF handling.
- `nix/default.nix` and `nix/shells/default.nix`: named packages and isolated
  on-demand shells; ambient Python and CUDA library paths are cleared.
- `tests/compatibility/pytorch_cuda_probe.py`: import, direct driver
  enumeration, host/device copy, artifact intake, and eager-add gap stages.
- `agent/plan/pytorch-compatibility-roadmap.md` and `manage-toolchain`: preserve
  the M0001/D0017 boundary and govern exact framework-client profiles.

## Verification

| Command/gate | Result |
| --- | --- |
| Full isolated baseline materialization/import | Passed: Python 3.13.15, torch 2.11.0+cu126, CUDA 12.6, includes `sm_70` |
| Full isolated frontier materialization/import | Passed: Python 3.13.15, torch 2.13.0+cu132, CUDA 13.2, starts at `sm_75` |
| Primary and final second-route HEAD/size checks | Passed: 58/58 wheel candidates in each check |
| `python3 -B toolchains/tests/verify_pytorch_cuda_clients.py` | Passed; exact closures, tags, generations, and routing shape |
| Probe self-test and focused CTest | Passed: 7/7 unit cases and 2/2 CTest cases |
| `nix flake show` and both profile dry-runs | Passed; 29 wheels plus one final derivation per profile |
| Recursive default/provider/runtime/release shell scan | Passed; no PyTorch/CUDA client derivation leaked |
| Skill validation, skill routing, and Agent record gates | Passed |

The later URL-route refresh changed only transfer candidates; filenames, sizes,
and SHA-256 identities stayed fixed. Static verification and Nix dry-runs were
rerun after that refresh.

## Cleanup

- Removed: session-owned local Nix stores under the narrow
  `/var/tmp/metaflux-pytorch-*` prefix, `/tmp/metaflux-pip-baseline.err`,
  `/tmp/metaflux-pip-frontier.err`, and `tests/compatibility/__pycache__`.
- Retained: the pre-existing external dev build tree and host `/nix/store`;
  neither is session-owned. No host GC ran. The concurrent uncommitted change in
  `runtime/core/tests/multiprocess_stress.cpp` was left untouched.

## Decisions and experience

- D0017 remains the `sm_70` semantic boundary; D0020 governs timezone-relative
  transfer routing; D0022 keeps Nix limited to tool materialization.
- No new architecture decision or experience ID was needed. Reusable behavior
  was placed directly in the toolchain skill, static gate, and probe tests.

## Distillation

- Distilled into `manage-toolchain`, the cross-release roadmap, exact manifests,
  the static lock verifier, the probe self-tests, current progress, and
  checkpoint P20260829-007.

## Unresolved items

- The five runtime stages have not been qualified against an injected MetaFlux
  CUDA provider. Run the probe from the owning compatibility harness when that
  provider path is ready; a diagnostic result alone is not release evidence.
- `sm_80` publication remains future-milestone work. The frontier profile does
  not change D0017, capability negotiation, provider identity, or M0001 scope.

## Handoff

Read `agent/plan/pytorch-compatibility-roadmap.md`, then enter exactly one client
with `nix develop .#pytorch-baseline` or `nix develop .#pytorch-frontier`. Run
`python tests/compatibility/pytorch_cuda_probe.py --profile baseline` for a
diagnostic report; add `--require-stage STAGE` only when an owning test is ready
to make that stage a gate.
