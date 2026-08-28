# Kernel Compatibility

Compile-time shims for the supported Linux 6.12 and 6.18 lines live here. Shims
are grouped by API capability and selected by target-kernel compile probes; they
contain no device policy, transport protocol, or stable external kABI.

Initial coverage includes page pin/dirty-unpin helpers, VMA and mmap changes, PCI
device publication, eventfd, and external-module build differences.
