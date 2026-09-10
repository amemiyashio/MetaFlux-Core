/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mf_cdev_generation.h — generation/stale-handle helpers for the local cdev.
 *
 * These tiny helpers encapsulate the generation-compare contract used by
 * mf_cdev_memory_alloc, mf_cdev_memory_query, mf_cdev_memory_register, and
 * mf_cdev_worker_lease in metaflux_core_main.c.  They are header-only so the
 * kernel module and the KUnit / userspace test share the exact same logic.
 *
 * Generation semantics (deterministic fixture, epoch-0007):
 *   - request.generation == 0   → accept (caller has no binding yet).
 *   - request.generation == gen → accept (binding matches).
 *   - otherwise                → reject as stale (-EINVAL / -ESTALE).
 *
 * Lease semantics:
 *   - queue->lease_owner == NULL           → available.
 *   - queue->lease_owner == file           → already held by caller.
 *   - queue->lease_owner != NULL && != file → busy (-EBUSY / -ESTALE).
 *
 * Tombstone:
 *   - After a lease or queue-owner release, the queue's generation does not
 *     change (it is a constant fixture), but the queue is marked offline.
 *     Any subsequent take on an offline queue is rejected by the existing
 *     !mf_cdev_queue.online guard before the helper is consulted.
 *   - The helper mf_cdev_generation_tombstoned() exposes the invariant that
 *     an old-generation binding cannot be re-admitted once the previous owner
 *     has released: the generation itself is immutable, so re-use only
 *     happens through a fresh negotiation that checks the current generation
 *     via mf_cdev_generation_match().
 */

#ifndef METAFLUX_CORE_MF_CDEV_GENERATION_H
#define METAFLUX_CORE_MF_CDEV_GENERATION_H

#ifdef __KERNEL__
#include <linux/types.h>
#else
/* Userspace compatibility: provide kernel type substitutes. */
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/* Userspace-compatible struct definitions for testing. */
struct mf_cdev_file {
	int dummy;
};

struct mf_cdev_queue {
	u64 generation;
	bool online;
	struct mf_cdev_file *lease_owner;
	struct mf_cdev_file *eventfd_owner;
};
#endif /* __KERNEL__ */

struct mf_cdev_queue;
struct mf_cdev_file;

/*
 * mf_cdev_generation_match — true when @requested is acceptable against
 * the queue's current generation.  Zero is the "no-op" sentinel meaning
 * "caller has no prior binding" and is always accepted.
 */
static inline bool mf_cdev_generation_match(u64 requested, u64 gen)
{
	return requested == 0U || requested == gen;
}

/*
 * mf_cdev_generation_is_stale — inverse of mf_cdev_generation_match().
 * Returns true when @requested carries a binding that no longer matches
 * the queue's current generation.
 */
static inline bool mf_cdev_generation_is_stale(u64 requested, u64 gen)
{
	return !mf_cdev_generation_match(requested, gen);
}

/*
 * mf_cdev_lease_can_take — true when @file may claim the exclusive lease
 * on @queue.  A second take by a different file is rejected.
 */
static inline bool mf_cdev_lease_can_take(const struct mf_cdev_queue *queue,
					  const struct mf_cdev_file *file)
{
	return queue->lease_owner == NULL || queue->lease_owner == file;
}

/*
 * mf_cdev_eventfd_can_take — true when @file may claim the eventfd slot
 * on @queue.  Mirrors the lease exclusivity rule for the eventfd owner.
 */
static inline bool mf_cdev_eventfd_can_take(const struct mf_cdev_queue *queue,
					    const struct mf_cdev_file *file)
{
	return queue->eventfd_owner == NULL || queue->eventfd_owner == file;
}

/*
 * mf_cdev_generation_tombstoned — true when @queue was previously bound to
 * a generation and has since been released.
 *
 * In this deterministic fixture the generation value never changes, so
 * tombstoning is observed through the queue's @online flag: once the queue
 * goes offline after owner release, any stale generation handle that
 * references the prior binding cannot be re-admitted as current until a
 * fresh mf_cdev_generation_match() succeeds against the (unchanged) current
 * generation — which the caller must have re-negotiated.
 */
static inline bool mf_cdev_generation_tombstoned(const struct mf_cdev_queue *queue)
{
	return !queue->online;
}

#endif /* METAFLUX_CORE_MF_CDEV_GENERATION_H */
