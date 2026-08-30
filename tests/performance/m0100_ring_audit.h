#ifndef METAFLUX_TESTS_PERFORMANCE_M0100_RING_AUDIT_H
#define METAFLUX_TESTS_PERFORMANCE_M0100_RING_AUDIT_H

#include <stdint.h>

int mf_ring_audit_begin(void);
int mf_ring_audit_end(void);
uint64_t mf_ring_audit_heap_allocation_attempts(void);
uint64_t mf_ring_audit_global_lock_acquisitions(void);

#endif
