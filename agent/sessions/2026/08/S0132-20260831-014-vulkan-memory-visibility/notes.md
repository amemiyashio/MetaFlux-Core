# Notes

Keep Vulkan handles out of this stage. The model should be usable by a future
`vkFlushMappedMemoryRanges`/`vkInvalidateMappedMemoryRanges` adapter, but the
tests only validate range math, access ordering, generation, and timeline
ownership. Direct external-memory import and physical memory-type selection stay
behind the qualified-device boundary.
