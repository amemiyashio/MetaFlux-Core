# UAPI Compatibility

## Canonical ownership

The root milestone-0.1.1.0 transport-envelope manifest lives under
`contracts/protocol/transport/v1/schema/manifest.json`. Linux ioctl, mmap, and
broker records remain authored in their zone under `contracts/uapi/linux/v1/`.
Each definition on the frozen milestone-0.1.1.0 base allowlist is referenced exactly once by
that manifest. Later extension definitions stay outside the base closure and are
referenced by their owning extension manifest. Generated kernel/userspace headers
and byte/offset fixtures are projections, never competing sources of truth.
Kernel, transport, and service directories must not copy a public layout into a
private normative struct.

## Record shape

- Use fixed-width Linux UAPI types, explicit size/version, flags, reserved fields,
  and naturally stable alignment. Avoid pointers, `long`, enums with compiler
  size, bitfields, implicit padding, timespec variants, and internal kernel types.
- Validate minimum known size before access; copy only known bytes; require known
  reserved bytes to be zero; ignore or reject unknown trailing extensions per
  the frozen contract.
- Check addition and multiplication overflow before range tests or allocation.
- Define native and compat behavior from one wire shape. A compat ioctl should
  not invent a second ABI when fixed-width fields suffice.
- Assign one ioctl command per semantic operation with correct direction and
  unique type/number. Do not overload behavior based on accidental user-buffer
  size.
- Define interruption, timeout, retry, duplicate request, partial output, and
  errno behavior. Return `-ENOTTY` for unknown commands.

## mmap and cdev

Specify allowable offsets/regions, page alignment, exact length, protections,
cache attributes, generation, revocation/tombstone behavior, fork policy, and
VMA open/close ownership. Validate `vm_pgoff` conversion and overflow. Mapping a
doorbell must not expose adjacent control pages.

The base data-plane envelope is frozen by
[work-item-0.1.1.4](../../../plan/milestone-0.1.1.0-kernel-guest-transport/work/work-item-0.1.1.4-fault-abi-freeze.md).
Preserve that accepted base and its hash. Later extension changes qualify their
own manifest closure; they do not reopen the base or imply stability of an
unqualified extension.

Primary source: [Linux ioctl design](https://docs.kernel.org/driver-api/ioctl.html).
