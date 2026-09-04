// SPDX-License-Identifier: GPL-2.0
//
// mf_cdev_generation_test.c — KUnit tests for mf_cdev_generation.h helpers.
//
// These tests exercise the generation-match, lease-exclusivity, and
// tombstone invariants extracted from kernel/core/metaflux_core_main.c.
// They require linux/kunit.h and thus only compile when CONFIG_KUNIT=y.
// On hosts without KUnit the CTest gate metaflux.kernel.kunit-generation
// exits 77 (skipped) via probe-debug-kernel.py --require-config CONFIG_KUNIT.
//
// A userspace companion (mf_cdev_generation_userspace_test.c) tests the same
// integer logic without linux/kunit.h and CAN pass on any host.

#include <kunit/test.h>
#include <linux/types.h>

/*
 * The kernel header only forward-declares these structs under __KERNEL__.
 * Define them here so the test cases can instantiate them on the stack,
 * then include the header so its inline helpers resolve against our
 * definitions.
 */
struct mf_cdev_file {
	int dummy;
};

struct mf_cdev_queue {
	u64 generation;
	bool online;
	struct mf_cdev_file *lease_owner;
	struct mf_cdev_file *eventfd_owner;
};

#include "mf_cdev_generation.h"

/* ------------------------------------------------------------------ */
/*  Stale generation rejected                                          */
/* ------------------------------------------------------------------ */

static void mf_cdev_generation_test_stale_rejected(struct kunit *test)
{
	struct mf_cdev_queue queue;
	struct mf_cdev_file file_a;
	u64 current_gen = 42ULL;

	queue.generation = current_gen;
	queue.online = true;
	queue.lease_owner = NULL;
	queue.eventfd_owner = NULL;

	/* A request carrying a mismatched generation is stale. */
	KUNIT_EXPECT_FALSE(test,
			   mf_cdev_generation_match(current_gen + 1, current_gen));
	KUNIT_EXPECT_FALSE(test,
			   mf_cdev_generation_match(current_gen - 1, current_gen));
	KUNIT_EXPECT_FALSE(test,
			   mf_cdev_generation_match(current_gen + 1024, current_gen));
	KUNIT_EXPECT_TRUE(test,
			  mf_cdev_generation_is_stale(current_gen + 1, current_gen));
}

/* ------------------------------------------------------------------ */
/*  Matching generation accepted                                       */
/* ------------------------------------------------------------------ */

static void mf_cdev_generation_test_matching_accepted(struct kunit *test)
{
	struct mf_cdev_queue queue;
	u64 current_gen = 7ULL;

	queue.generation = current_gen;
	queue.online = true;
	queue.lease_owner = NULL;
	queue.eventfd_owner = NULL;

	/* Exact match is accepted. */
	KUNIT_EXPECT_TRUE(test, mf_cdev_generation_match(current_gen, current_gen));
	KUNIT_EXPECT_FALSE(test, mf_cdev_generation_is_stale(current_gen, current_gen));

	/* Zero sentinel (no prior binding) is always accepted. */
	KUNIT_EXPECT_TRUE(test, mf_cdev_generation_match(0ULL, current_gen));
	KUNIT_EXPECT_FALSE(test, mf_cdev_generation_is_stale(0ULL, current_gen));
}

/* ------------------------------------------------------------------ */
/*  Exclusive lease occupancy — one owner; second take fails           */
/* ------------------------------------------------------------------ */

static void mf_cdev_generation_test_exclusive_lease(struct kunit *test)
{
	struct mf_cdev_queue queue;
	struct mf_cdev_file file_a;
	struct mf_cdev_file file_b;

	queue.generation = 1ULL;
	queue.online = true;
	queue.lease_owner = NULL;
	queue.eventfd_owner = NULL;

	/* First take by file_a succeeds. */
	KUNIT_EXPECT_TRUE(test, mf_cdev_lease_can_take(&queue, &file_a));

	/* Claim the lease. */
	queue.lease_owner = &file_a;

	/* file_a re-take is still fine (idempotent). */
	KUNIT_EXPECT_TRUE(test, mf_cdev_lease_can_take(&queue, &file_a));

	/* file_b cannot take the lease — exclusive. */
	KUNIT_EXPECT_FALSE(test, mf_cdev_lease_can_take(&queue, &file_b));
}

/* ------------------------------------------------------------------ */
/*  Exclusive eventfd occupancy — mirrors lease rule                   */
/* ------------------------------------------------------------------ */

static void mf_cdev_generation_test_exclusive_eventfd(struct kunit *test)
{
	struct mf_cdev_queue queue;
	struct mf_cdev_file file_a;
	struct mf_cdev_file file_b;

	queue.generation = 1ULL;
	queue.online = true;
	queue.lease_owner = NULL;
	queue.eventfd_owner = NULL;

	KUNIT_EXPECT_TRUE(test, mf_cdev_eventfd_can_take(&queue, &file_a));
	queue.eventfd_owner = &file_a;
	KUNIT_EXPECT_TRUE(test, mf_cdev_eventfd_can_take(&queue, &file_a));
	KUNIT_EXPECT_FALSE(test, mf_cdev_eventfd_can_take(&queue, &file_b));
}

/* ------------------------------------------------------------------ */
/*  Tombstone — after release, old generation cannot be reused         */
/* ------------------------------------------------------------------ */

static void mf_cdev_generation_test_tombstone(struct kunit *test)
{
	struct mf_cdev_queue queue;
	struct mf_cdev_file file_a;
	u64 current_gen = 1ULL;

	queue.generation = current_gen;
	queue.online = true;
	queue.lease_owner = &file_a;
	queue.eventfd_owner = NULL;

	/* While online, generation match still works for the current gen. */
	KUNIT_EXPECT_TRUE(test, mf_cdev_generation_match(current_gen, current_gen));
	KUNIT_EXPECT_FALSE(test, mf_cdev_generation_tombstoned(&queue));

	/* Release: simulate owner close draining the lease. */
	queue.lease_owner = NULL;
	queue.online = false;

	/* Queue is now a tombstone. */
	KUNIT_EXPECT_TRUE(test, mf_cdev_generation_tombstoned(&queue));

	/* A stale generation handle cannot be re-admitted against a tombstone. */
	KUNIT_EXPECT_FALSE(test, mf_cdev_generation_match(current_gen + 1, current_gen));

	/* Even the correct generation is inert while tombstoned — the
	 * caller must re-negotiate (i.e., get a new generation from a
	 * fresh mf_cdev_init) before the binding is live again. */
	KUNIT_EXPECT_TRUE(test, mf_cdev_generation_match(current_gen, current_gen));
	KUNIT_EXPECT_TRUE(test, mf_cdev_generation_tombstoned(&queue));
}

/* ------------------------------------------------------------------ */
/*  Test suite                                                         */
/* ------------------------------------------------------------------ */

static struct kunit_case mf_cdev_generation_test_cases[] = {
	KUNIT_CASE(mf_cdev_generation_test_stale_rejected),
	KUNIT_CASE(mf_cdev_generation_test_matching_accepted),
	KUNIT_CASE(mf_cdev_generation_test_exclusive_lease),
	KUNIT_CASE(mf_cdev_generation_test_exclusive_eventfd),
	KUNIT_CASE(mf_cdev_generation_test_tombstone),
	{}
};

static struct kunit_suite mf_cdev_generation_test_suite = {
	.name = "mf_cdev_generation",
	.test_cases = mf_cdev_generation_test_cases,
};

kunit_test_suite(mf_cdev_generation_test_suite);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("MetaFlux kernel core");
MODULE_DESCRIPTION("KUnit tests for mf_cdev_generation helpers");
