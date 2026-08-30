# Notes

The owner reaffirmed Ubuntu 20.04 as the minimum supported distribution. In
repository terms this means Ubuntu 20.04 LTS and its glibc 2.31 userspace ABI
floor. D0009, durable constraints, the M0100 plan, current progress, and the
test comment already agreed; W0101 alone still described glibc 2.31 as a
candidate pending the broader distribution matrix.

The corrected W0101 separates two concerns:

- the compatibility floor is fixed and generic provider artifacts must not
  reference symbols newer than `GLIBC_2.31`;
- the remaining distribution matrix may add tested targets without changing
  that minimum.
