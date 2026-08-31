# Notes

The socket implementation is intentionally host-independent and uses only
POSIX Unix-stream primitives already available to the vfio-user server. It is
not a QEMU process harness and does not infer lifecycle identity from wire
events. The pathname connect regression removes its temporary socket path
before returning; no session-owned temporary artifacts remain.
