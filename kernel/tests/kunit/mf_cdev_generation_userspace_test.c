/*
 * mf_cdev_generation_userspace_test.c — userspace self-test for the
 * mf_cdev_generation.h integer logic, without linux/kunit.h.
 *
 * This file compiles on any host with a C17 compiler and exercises the
 * same generation-match, lease-exclusivity, and tombstone invariants
 * that the KUnit suite targets.  It is used by the CTest gate
 * metaflux.kernel.cdev-generation-helper, which CAN pass on any host.
 *
 * To build manually:
 *   gcc -std=c17 -Wall -Wextra -o mf_cdev_generation_userspace_test \
 *       -Ikernel/core \
 *       kernel/tests/kunit/mf_cdev_generation_userspace_test.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/*
 * Include the shared kernel header.  The header's ifndef __KERNEL__
 * block provides userspace-compatible type definitions (u64, bool, etc.)
 * and the inline helper implementations, so this test shares the exact
 * same logic as the kernel module and KUnit suite.
 */
#include "mf_cdev_generation.h"

/* ------------------------------------------------------------------ */
/*  Minimal assertion helpers (no kunit.h dependency)                  */
/* ------------------------------------------------------------------ */

static int failures = 0;

#define ASSERT_TRUE(cond, msg)                                          \
	do {                                                            \
		if (!(cond)) {                                          \
			fprintf(stderr, "FAIL: %s\n", msg);             \
			++failures;                                     \
		}                                                       \
	} while (0)

#define ASSERT_FALSE(cond, msg)                                         \
	do {                                                            \
		if ((cond)) {                                           \
			fprintf(stderr, "FAIL: %s\n", msg);             \
			++failures;                                     \
		}                                                       \
	} while (0)

/* ------------------------------------------------------------------ */
/*  Stale generation rejected                                          */
/* ------------------------------------------------------------------ */

static int test_stale_rejected(void)
{
	uint64_t current = 42ULL;

	ASSERT_FALSE(mf_cdev_generation_match(current + 1, current),
		     "stale gen +1 must be rejected");
	ASSERT_FALSE(mf_cdev_generation_match(current - 1, current),
		     "stale gen -1 must be rejected");
	ASSERT_FALSE(mf_cdev_generation_match(current + 1024, current),
		     "stale gen +1024 must be rejected");
	ASSERT_TRUE(mf_cdev_generation_is_stale(current + 1, current),
		    "is_stale must report true for mismatched gen");
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Matching generation accepted                                       */
/* ------------------------------------------------------------------ */

static int test_matching_accepted(void)
{
	uint64_t current = 7ULL;

	ASSERT_TRUE(mf_cdev_generation_match(current, current),
		    "exact match must be accepted");
	ASSERT_FALSE(mf_cdev_generation_is_stale(current, current),
		     "exact match must not be stale");
	ASSERT_TRUE(mf_cdev_generation_match(0ULL, current),
		    "zero sentinel (no binding) must be accepted");
	ASSERT_FALSE(mf_cdev_generation_is_stale(0ULL, current),
		     "zero sentinel must not be stale");
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Exclusive lease occupancy                                          */
/* ------------------------------------------------------------------ */

static int test_exclusive_lease(void)
{
	struct mf_cdev_queue queue;
	struct mf_cdev_file file_a = { 0 };
	struct mf_cdev_file file_b = { 0 };

	queue.generation = 1ULL;
	queue.online = true;
	queue.lease_owner = NULL;
	queue.eventfd_owner = NULL;

	ASSERT_TRUE(mf_cdev_lease_can_take(&queue, &file_a),
		    "first take by file_a must succeed");

	queue.lease_owner = &file_a;
	ASSERT_TRUE(mf_cdev_lease_can_take(&queue, &file_a),
		    "re-take by same file must still succeed");
	ASSERT_FALSE(mf_cdev_lease_can_take(&queue, &file_b),
		     "second take by file_b must fail (exclusive)");
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Exclusive eventfd occupancy                                        */
/* ------------------------------------------------------------------ */

static int test_exclusive_eventfd(void)
{
	struct mf_cdev_queue queue;
	struct mf_cdev_file file_a = { 0 };
	struct mf_cdev_file file_b = { 0 };

	queue.generation = 1ULL;
	queue.online = true;
	queue.lease_owner = NULL;
	queue.eventfd_owner = NULL;

	ASSERT_TRUE(mf_cdev_eventfd_can_take(&queue, &file_a),
		    "first eventfd take by file_a must succeed");
	queue.eventfd_owner = &file_a;
	ASSERT_TRUE(mf_cdev_eventfd_can_take(&queue, &file_a),
		    "re-take eventfd by same file must succeed");
	ASSERT_FALSE(mf_cdev_eventfd_can_take(&queue, &file_b),
		     "second eventfd take by file_b must fail");
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Tombstone                                                          */
/* ------------------------------------------------------------------ */

static int test_tombstone(void)
{
	struct mf_cdev_queue queue;
	struct mf_cdev_file file_a = { 0 };
	u64 current = 1ULL;

	queue.generation = current;
	queue.online = true;
	queue.lease_owner = &file_a;
	queue.eventfd_owner = NULL;

	ASSERT_TRUE(mf_cdev_generation_match(current, current),
		    "match while online must work");
	ASSERT_FALSE(mf_cdev_generation_tombstoned(&queue),
		     "queue must not be tombstoned while online");

	/* Release: simulate owner close draining the lease. */
	queue.lease_owner = NULL;
	queue.online = false;

	ASSERT_TRUE(mf_cdev_generation_tombstoned(&queue),
		    "queue must be tombstoned after release");
	ASSERT_FALSE(mf_cdev_generation_match(current + 1, current),
		     "stale gen must not be re-admitted after tombstone");
	/* Correct generation still matches the value, but the queue is
	 * inert — the caller must re-negotiate to bring it back online. */
	ASSERT_TRUE(mf_cdev_generation_match(current, current),
		    "correct gen still matches its own value");
	ASSERT_TRUE(mf_cdev_generation_tombstoned(&queue),
		    "tombstone persists until brought online again");
	return 0;
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
	printf("Running mf_cdev_generation userspace self-test...\n");

	test_stale_rejected();
	test_matching_accepted();
	test_exclusive_lease();
	test_exclusive_eventfd();
	test_tombstone();

	if (failures == 0) {
		printf("PASS: all 5 tests passed\n");
		return 0;
	}
	fprintf(stderr, "FAIL: %d assertion(s) failed\n", failures);
	return 1;
}
