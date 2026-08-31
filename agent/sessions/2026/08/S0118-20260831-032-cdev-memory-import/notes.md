# Notes

The importer callback deliberately receives the backend instance/context and a
caller-owned range. The resolver remains responsible for object ID/generation,
permission, and range validation; the worker remains responsible for operation
lease and retain/release ordering. No procfs or process-address inference is
introduced.
