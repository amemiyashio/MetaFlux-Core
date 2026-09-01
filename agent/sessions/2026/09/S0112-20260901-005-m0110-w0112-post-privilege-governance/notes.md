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
