# Notes

The lock file is a stable coordination object derived from the same entry token
as the cache file. It is intentionally retained in the cache directory so
processes can reopen it without a registry or daemon. The repository mutex is
not held while waiting for the OS lock or running the producer; each holder
rechecks both catalog and disk before doing work.
