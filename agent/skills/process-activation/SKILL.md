---
name: process-activation
description: Implement or review process-scoped MetaFlux provider selection, loader coexistence, managed/passthrough transitions, daemon socket activation and device access. Use for ordinary stock-client launch integration; package formats, tool provisioning and kernel execution have separate owners.
---

# Process Activation

Start with the actual process entry and the first missing boundary: selected
provider DSO, daemon connection, inherited activation descriptor or backend
device access. Follow its current caller into the source below. For a requested
implementation, deliver that launch/connection behavior and clean exit; an
inspection request remains read-only.

| Task | Read |
| --- | --- |
| Stock-client entry, loader environment, sockets or render-node access | [Application entry](references/application-entry.md) |
| Vendor coexistence, managed/auto/passthrough mode or fork invalidation | [Loader coexistence](references/loader-coexistence.md) |

The stock compatibility runners currently arrange their own daemon and process
environment. The release activation launcher is a tests-owned fd-3 fixture;
the vroot launcher emits presentation plans. Neither establishes an installed
general stock-PyTorch entry. Implement the product entry only in its assigned
scope, using the current Goal and work-item acceptance boundary.

Preserve the following behavior:

- Keep the stock PyTorch source, wheel and `torch.cuda` calls unchanged.
  Activation may be process-scoped through a MetaFlux package or launcher.
- Select providers and backend context before successful resources. Explicit
  managed mode must not silently enter the vendor stack. Auto-mode transfer
  requires successful rollback and a pristine managed state.
- Keep loader changes local to the launched process; restore the caller's
  environment and retain the application's arguments, exit result and signals.
  Never overwrite vendor libraries or install compatibility aliases globally.
- Validate the actual service identity, socket permissions and device visibility.
  A process environment variable is not evidence that a physical GPU executed.
- Finish one activation transaction before warm work. Repeated launches should
  reuse the established provider/session instead of spawning a daemon or
  rebuilding loader state per kernel.

Use [$pytorch-cuda-profile](../pytorch-cuda-profile/SKILL.md) skill for the client
profile after activation, [$daemon-execution-runtime](../daemon-execution-runtime/SKILL.md) skill
for session dispatch, and [$vulkan-spirv-compute](../vulkan-spirv-compute/SKILL.md) skill
for physical AMD execution. Provider API semantics remain with CUDA/NVML experts.
Tool inputs use [$manage-toolchain](../manage-toolchain/SKILL.md) skill; every
privileged operation uses [$manage-host-privilege](../manage-host-privilege/SKILL.md) skill.
Package formats and install/upgrade/removal stay in the packaging owner.

Return the implemented entry, selected library/service/device identities,
environment/lifetime behavior and the actual client result to
[$review](../review/SKILL.md) skill. Choose the affected activation/loader checks;
a fixture daemon launch alone does not qualify ordinary application use.
