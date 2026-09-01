# BARs, MSI-X, and Ordering

This profile applies to the milestone-0.1.1.0 static guest function, not the initial vroot
fixture.

## Region contract

| Region | Size | Purpose | Key constraints |
| --- | ---: | --- | --- |
| BAR0 | 64 KiB | capability, setup, status/error, extension directory, reserved lifecycle mailbox | typed fields, explicit writable masks, no hidden ABI expansion |
| BAR2 | 4 KiB | ioeventfd doorbell | map only permitted page; typed 32-bit write; no unrelated control exposure |
| BAR4 | 4 KiB | MSI-X table and PBA | two vectors; exact table/PBA bounds and mask semantics |

Vector 0 is admin/fatal. Vector 1 is armed completion. Complete descriptors and
publish the timeline with release ordering before signaling. Interrupt only a
waiter that completed the arm/recheck protocol or a fatal transition; polling
does not receive per-command interrupts.

## Access and ordering

- Define allowed access widths/alignment and byte order for every BAR field.
- Validate BAR sizing probes and mapping boundaries independently from runtime
  accesses.
- Producer stores descriptor/data, performs the project DMA/MMIO publication
  barrier, then writes the typed doorbell. Consumer performs the matching DMA
  read/acquire before dereference.
- Completion stores precede release publication of the timeline; guest polling
  loads acquire, and interrupt readers use the required DMA barrier.
- MSI-X table writes, function/vector mask transitions, pending bits, eventfd
  replacement, and teardown are serialized against signaling.

Trace QEMU/server region accesses and IRQ injection to prove the warm path uses
ioeventfd/shared memory and does not enter the QEMU main loop.
