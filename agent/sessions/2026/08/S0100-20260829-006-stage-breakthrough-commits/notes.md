# Notes

The checkpoint trigger is semantic rather than temporal. It fires only when a
durable outcome can stand alone, its focused gates pass, its diff is coherent,
and the next work crosses into a new risk or scope phase. The active agent makes
that judgment; `.githooks/pre-commit` remains a validator and never invokes
`git commit`.

Checkpoint mode leaves the session at `status: in_progress`, with
`ended_at: null` and `final_revision: null`, after the content revision and its
verification are appended and committed as records. Close mode performs final
cleanup, explicit `$roast` promotion classification, and independent
`session-only` disposition, then records the last content revision before the
separate closing record commit.
