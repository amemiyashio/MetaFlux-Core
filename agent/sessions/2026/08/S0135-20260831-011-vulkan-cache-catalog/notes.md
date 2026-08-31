# Notes

The repository layer is an in-process authority over two representations of a
cache entry. The file store remains the durable source; the catalog controls
resident payloads and live-reference pins. A file hit that cannot fit because
all resident entries are pinned is reported as quota exhaustion rather than
silently dropping the persistent entry. Cross-process stampede control and
pipeline-bound invalidation remain outside this stage.
