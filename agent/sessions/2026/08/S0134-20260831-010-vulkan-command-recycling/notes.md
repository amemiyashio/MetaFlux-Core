# Notes

The pool deliberately owns logical resources rather than Vulkan handles. Its
completion timeline is global to the pool, so queue adapters must submit values
in strictly increasing order and report observed completion monotonically.
Generation changes reset the completion window only after all slots are free;
resource IDs remain monotonic so a retired handle cannot alias a new one.

The focused test initially exposed a fixture error: an acquired second slot was
left in flight, and an old-handle probe reused an already-published timeline.
The final regression submits both slots and uses valid increasing probe values,
leaving the implementation's `busy` and `not_found` results observable.
