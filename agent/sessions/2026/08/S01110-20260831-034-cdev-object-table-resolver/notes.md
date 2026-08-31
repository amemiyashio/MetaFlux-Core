# Notes

`CdevObjectTableResolver` keeps the daemon lookup as the authority for object
ID, generation, kind, permissions, and byte range. Argument blocks are checked
against the exact protocol length (`sizeof(header) + 3 * sizeof(entry)`), which
is 160 bytes; an aligned C++ fixture may occupy more storage because of tail
padding, so the advertised `total_size` and lookup view intentionally expose
only the protocol bytes. A complete lookup-provided backend reference is used
as-is; otherwise the importer receives only the checked subrange and the
worker owns the retain/release lifetime after resolution.

The resolver releases a destination reference if source resolution fails. It
does not retain or infer object-table ownership, and it does not alter the
stable ring descriptor, backend ABI, or Linux cdev UAPI. The CPU regression is
host-independent evidence of the callback composition; live daemon wiring,
generation replacement, and physical device qualification remain separate.
