# Notes

The current ABI already supplies `MF_BACKEND_CAP_EVENTS` and `query_event`, so
the slice required no contract or generated-schema change. A worker has one
pending request at a time; it retains the operation lease until the event is
complete and keeps both pending state and the lease when the completion ring is
full. Backend replacement and lifecycle-loss cancellation are deliberately
left to the next W0112 boundary because this worker API does not yet own a
generation-aware cancellation operation.
