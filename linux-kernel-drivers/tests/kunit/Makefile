# SPDX-License-Identifier: GPL-2.0
#
# Kbuild Makefile for KUnit tests in linux-kernel-drivers/tests/kunit/
#
# Usage:
#   make -C /lib/modules/$(uname -r)/build M=$PWD/linux-kernel-drivers/tests/kunit
#
# Note: This target only builds when CONFIG_KUNIT=y is set in the
# running kernel configuration. On hosts without KUnit support,
# the build will silently skip these targets.
#
# The KUnit test module is built as an out-of-tree kernel module.
# The userspace companion test (mf_cdev_generation_userspace_test.c)
# is compiled separately via CMake as metaflux_cdev_generation_helper_test.

obj-m += mf_cdev_generation_kunit.o
mf_cdev_generation_kunit-y := mf_cdev_generation_test.o

# Include path to linux-kernel-drivers/core so the test can include mf_cdev_generation.h
ccflags-y += -I$(src)/../../core

all:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
