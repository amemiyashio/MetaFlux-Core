# memfd client half

The M0100 memfd client implementation is the C17 fast path in
`runtime/client/fastpath`. This target registers that existing client half as
the memfd transport boundary without copying its ring, registry, or generation
validation code. The worker-side lifecycle adapter lives in `worker/`.
