# Notes

- The userspace client treats `ENOENT`/`ENODEV` as the only local cdev fallback
  cases; malformed negotiation, permission failures, and policy errors remain
  visible.
- Ring descriptors use the inherited M0100 sequence protocol. The worker
  consumes with acquire/release ordering, validates the request generation,
  bounds copy offsets inside the mapped payload arena, and emits a completion
  timeline instead of issuing control traffic on the active queue.
- The generated transport header now has a `__KERNEL__` projection using Linux
  fixed-width types and suppresses inherited user-space layout assertions so it
  can be included by Kbuild without importing libc headers.
