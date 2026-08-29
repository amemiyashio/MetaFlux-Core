---
id: P20260829-007
date: 2026-08-29
status: Recorded
revision: b4e78bf
trigger: isolated dual PyTorch CUDA client profiles and staged gap probe
---

# PyTorch CUDA client profiles and staged gap probe

## Outcome

Content revision `7fd83f6` adds two exact, isolated Nix client profiles;
revision `b4e78bf` adds the test-owned staged compatibility probe. Normal
development and release environments remain free of their multi-gigabyte
closures.

## Frozen profiles

| Profile | Python | PyTorch | CUDA | Observed wheel architectures | Role |
| --- | --- | --- | --- | --- | --- |
| `pytorch-baseline` | 3.13.15 | `2.11.0+cu126` | 12.6 | `sm_50 sm_60 sm_70 sm_75 sm_80 sm_86 sm_90` | Current `sm_70` regression probe |
| `pytorch-frontier` | 3.13.15 | `2.13.0+cu132` | 13.2 | `sm_75 sm_80 sm_86 sm_90 sm_100 sm_120` | Future `sm_80` gap probe |

Each profile has 29 transitively complete wheels frozen by filename, byte size,
and SHA-256. Transfer candidates follow D0020 and end at canonical upstream.

## Verification

| Gate | Result |
| --- | --- |
| Full isolated materialization and `import torch` | Both profiles passed exact Python/PyTorch/CUDA identity and CPU int32 add |
| Wheel route HEAD and byte-size preflight | 58/58 primary and 58/58 final second candidates passed |
| Static manifest verifier | Passed exact closure, tag, platform, CUDA generation, and routing checks |
| Probe self-test | 7/7 passed without a PyTorch installation |
| Focused CTest | 2/2 passed |
| Flake evaluation and profile dry-runs | Passed |
| Default/provider/runtime/release derivation scan | No PyTorch/CUDA client references |

## Product boundary

D0017 and M0001 remain PTX 9.0/`sm_70`. The baseline records gaps on that path;
the frontier is research until a later milestone owns an `sm_80` capability
descriptor, negotiation, compiler/runtime changes, and complete evidence.

## Cleanup

Task-owned local Nix stores, pip error logs, and Python bytecode were removed.
The host Nix store and shared external dev build tree were left intact; no host
GC ran. Concurrent runtime work was not included in either content revision.

## Handoff

Enter only the needed client with `nix develop .#pytorch-baseline` or
`nix develop .#pytorch-frontier`. Use
`tests/compatibility/pytorch_cuda_probe.py --profile PROFILE` diagnostically;
an owning compatibility test may add `--require-stage STAGE` when provider
injection and that stage's acceptance claim are ready.
