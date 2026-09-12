# Implement A Presentation Change

Read this before choosing the profile and callback. Paths are
repository-relative; locate current symbols with `rg`.

| Profile or boundary | Implementation to inspect |
| --- | --- |
| Default-off software root | `linux-kernel-drivers/vroot/metaflux_vroot_main.c` |
| Config image model and accesses | `linux-kernel-drivers/vroot/src/config_model.c` and `tests/config_model_test.c` |
| Static guest function driver | `linux-kernel-drivers/pci/metaflux_pci_main.c` |
| Static live QEMU function | `transports/vfio-user/live/src/metaflux_vfu_live_server.c` |
| Guest base schema | `contracts/protocol/transport/v1/schema/manifest.json` |
| vroot extension/import closure | `contracts/protocol/transport/v1/schema/extensions/vroot/v1/manifest.json` |

The current vroot uses its local `metaflux_vroot` PCI driver and
`mf_vroot_init`/`mf_vroot_probe`; inspect them before assuming the required
exclusive `metaflux_pci` binding sequence is wired. The
[binding guide](host-bridge-binding.md) distinguishes design from current code.

Name the requested guest or vroot observation: a specific config access,
successful exclusive bind, BAR/IRQ operation or removal. Follow its callback to
the consumer, reuse the generated profile, and implement that behavior plus
its absent/stale/error boundary. Source directory presence is not qualification.

For config changes, enumerate the affected reset bytes, masks, offsets, widths
and side effects, then regenerate the owning image/tables and byte fixtures.
Preserve import hashes and de-duplicate the validated transitive closure.
For binding changes, trace scan visibility, override, matching enable, probe
and registry commit separately; a visible unbound function remains quarantined.

## Select Qualification By Profile

Read [config](config-and-enumeration.md), [BARs](bars-msix-ordering.md),
[binding](host-bridge-binding.md) or [hotplug](reset-hotplug.md) according to the
changed boundary. The active work item owns the exact kernel/QEMU matrix and
promotion status; a finished milestone is not reopened by this skill.

- Config: widths, every affected byte/boundary/mask, absent functions and
  concurrent remove, generated images and imported-definition hashes.
- Guest BAR/MSI-X: size/protection, typed doorbell, table/PBA masks, vector
  routing and armed notification with no per-command interrupt.
- Identity/binding: domain-local BDF and common live `(UUID, generation)`
  across CUDA/NVML/PCI/sysfs/cdev; no vendor driver or resource displacement.
- vroot promotion: Linux 6.12/6.18 `lspci -Dnn`, sysfs/uevent/binding and the
  required 1,000 add/remove plus load/unload concurrent-use cycles, with exact
  scan/override/probe/quarantine/removal traces.

During implementation run only the check that resolves the named uncertainty;
after parent review run the required covering profile once. Config enumeration
is evidence for presentation, not proof of a working tensor data plane.
