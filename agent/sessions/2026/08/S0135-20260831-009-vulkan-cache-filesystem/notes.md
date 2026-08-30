# Notes

The file name is only a deterministic short token; the complete canonical key
is still stored and verified in the envelope so a token collision cannot produce
an accidental hit. File and directory sync happen before a successful publish
is reported. The store lock serializes callers within one process; a future
pipeline boundary must add cross-process coordination and catalog residency
ownership.
