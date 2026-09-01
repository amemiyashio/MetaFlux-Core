# Notes

P102 is the exact product resume boundary. Applied SC0009 changes privilege
ownership, not W0112 product evidence: P101 remains factual, and the live cdev
Exit Gate remains open.

P103 records the host-independent worker-side generation guard at content
revision `3a79e8e`: daemon bindings carry `kDeviceGeneration`, lifecycle commit
invalidates old bindings for new work, and pending operations retain their
captured binding while draining. This does not qualify daemon rebind, kernel DMA
import, or a live device.

Use `manage-host-privilege` together with `linux-device-driver-uapi` for module
lifecycle, kernel logs, kmemleak, and live qualification. The privilege skill
owns elevation and authorization only; Kbuild, tests, and W0112 retain their
commands, behavior, and evidence.

P104 records product revision `16707ec`. The cdev paired-ring mapping now
rounds each raw ring to a page boundary, and every worker control fd negotiates
view/generation before a lease-bound payload query. The first live attempt
exposed the unaligned mapping contract; the next exposed the missing control-fd
negotiation; both are fixed in the product revision.

The governed live run reached session open, queue mapping, lease acquisition,
payload query, and payload mapping, then returned exit 77 because the standalone
virtual misc cdev has no DMA mask or parent master. Registered-memory now
returns an explicit unsupported capability before pinning. The module was
unloaded through the bounded helper and no device nodes remain active.

P105 records product revision `75284e1`. Worker rebind now requires an online
committed generation; lifecycle commit clears the old current binding; pending
backend, lease, and memory references drain before the optional owner retire
callback; and rebinding a pending binding back to its current owner does not
retire it twice. Focused cdev worker testing covers replacement, reset stale
generation rejection, asynchronous completion, timeout, and rebind-back.

The repository-wide format target remains red because of existing violations
across unrelated files. No unrelated formatting migration was performed. The
W0112 source boundary is therefore recorded as a worker-side closure only;
daemon/provider-controlled replacement, physical DMA/live Add/Copy, and the
Linux fault matrix remain open.
