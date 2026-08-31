# Notes

The ledger is a transaction/admission model. It intentionally stops before
Vulkan object creation: a successful ledger submission is a validated graph,
resource, and completion tuple that a later queue adapter may translate to
`vkQueueSubmit2`.
