# Notes

The resolver callback pair is intentionally transport-internal. It supplies
reference ownership for handles already resolved by the daemon/object table;
mapping a kernel registered-memory record into a backend handle remains an
integration concern for a later W0112 stage.
