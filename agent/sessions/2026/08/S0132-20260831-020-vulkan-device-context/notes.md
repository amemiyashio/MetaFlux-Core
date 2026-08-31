# Notes

The existing capability probe owns no handles after returning. This session
adds a separate internal owner so later allocation and pipeline work can use an
enabled logical device without widening `mf_vulkan_capability_profile_v1` or the
stable backend C ABI.

The current host is AMD/RADV. A successful runtime-shell smoke is useful for
binding and timeline behavior, but it cannot satisfy the two-driver-family or
physical NVIDIA gates assigned to M1000/v1.0.0.
