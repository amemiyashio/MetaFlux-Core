# Validated Experience

Experience records capture reusable engineering practice backed by evidence.
Routine work loads only records related to the current task.

| ID | Status | Topic |
| --- | --- | --- |
| [E0001](E0001-nix-untracked-flake.md) | Validated | Nix path flakes before the first Git commit |
| [E0002](E0002-provider-closure-symbol-gates.md) | Validated | Provider closure and exact ELF gates |
| [E0003](E0003-llvm-mlir-sdk-outputs-runpath.md) | Validated | LLVM/MLIR SDK outputs and RUNPATH |

IDs are monotonic and never reused. `Candidate` records require reproducible
evidence before becoming `Validated`; `Superseded` records remain at their path
and link their replacement. Use the [experience template](../templates/experience.md).

