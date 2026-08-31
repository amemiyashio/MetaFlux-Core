# Notes

The fixed values in `kernel/core/metaflux_core_main.c` are a candidate UAPI
fixture, not daemon authority. M0110's locked boundaries place coordinated
replacement generations in M0120, so wiring a replacement ioctl here would
cross milestone ownership.
