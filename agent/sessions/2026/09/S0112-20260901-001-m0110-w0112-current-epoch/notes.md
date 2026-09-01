# Notes

This is the first M0110/W0112 owner scaffolded after the SC0007 liquidation.
Its authority is the current `AGENTS.md`, focus projection, W0112 plan, current
source/tests, and durable checkpoints. The settlement tombstone resolves old
identities administratively but contains no task context and is not a resume
surface.

The immediate product boundary is the W0112 Exit Gate: connect the live cdev
lease/object-table path to registered-memory handles, prove unmodified CPU
Add/Copy through the device node across generation replacement, then collect
the specified Linux 6.12/6.18 fault evidence.

The D0030 provider boundary is now implemented in current-branch content
revision `0404481` (tree-equivalent replacement for the capture-time
`047ea94`):
cdev-first initialization requires a matching Unix session/view/generation and
an explicit daemon bind before success; Unix owns object/control operations and
cdev owns COPY plus primary-entry LAUNCH. The daemon-side CPU mapping remains a
host-memory import fixture until live registered-memory/DMA qualification.

Revision `95c960d` adds the live cdev qualification executable. Its current-host
result is an explicit skip because `/dev/metaflux0` is absent, not acceptance
evidence for the W0112 Exit Gate.
