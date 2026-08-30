# Notes

- All package instructions are English so the same repository skills are usable
  across Codex invocations independent of prompt language.
- References are repository-specific checklists plus primary-source links. They
  intentionally do not mirror entire vendor, kernel, QEMU, LLVM, or Vulkan
  specifications.
- The MLIR skill owns dialect/conversion/pass/versioning mechanics across CPU and
  Vulkan. PTX owns source semantics; CPU and Vulkan own target constraints and
  execution evidence.
- vfio-user transport, PCI presentation, Linux UAPI/lifetime, and authoritative
  lifecycle remain separate skills because their failure and compatibility
  contracts change independently.
