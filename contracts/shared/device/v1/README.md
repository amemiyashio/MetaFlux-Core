# Shared Device Layout v1

Planned home of mmap-visible registry, state, and metrics layouts. Contracts use
fixed-width fields, offsets, explicit cache-line placement, and documented atomic
accessors; they do not contain raw pointers or process-owned synchronization
objects.
