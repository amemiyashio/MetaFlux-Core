# Notes

Direct payload-memory references are intentionally optional for compatibility
with existing fixture bindings. A production object-table adapter must supply a
complete retain/release pair before publishing an imported handle to a bound
worker.
