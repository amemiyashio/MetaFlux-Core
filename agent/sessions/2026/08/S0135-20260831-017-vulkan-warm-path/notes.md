# Notes

The trace validator is intentionally host-independent. It records the warm
path's admission policy and forbidden transitions without pretending to observe
an ICD, allocate a shader module, or execute a queue. Actual Vulkan trace
collection remains a later physical-device gate.
