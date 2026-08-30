# Expert Skill Trigger Evaluations

The machine-readable source is [trigger-evals.json](trigger-evals.json). It
contains 81 cases for 13 routed skills: 11 domain skills and two workflow
skills. Every routed skill has two English positives, one English near-miss, one
Chinese positive, and one Chinese near-miss; 16 additional cases exercise
cross-boundary composition. Each workflow skill appears in at least one English
and one Chinese composition.

## Static validation

Run from the repository root:

```sh
python3 -B tools/check-skill-routing.py .
python3 -B tools/test-check-skill-routing.py
```

The first command validates schema, IDs, skill references, locale coverage,
positive/near-miss coverage, and composition count. The self-test mutates the
corpus and captured observations to prove that missing, forbidden, unexpected,
duplicate, and incomplete results fail.

`tools/check-agent-records.py` separately validates the exact skill catalog,
domain section order, and repository-canonical `agents/openai.yaml` subset. These
checks establish package and corpus integrity; they do not execute model routing.

## Behavioral run

Generate a three-run observation template:

```sh
python3 -B tools/check-skill-routing.py . --emit-template --repetitions 3 \
  > /tmp/metaflux-skill-routing.json
```

Run each prompt from the repository root with implicit skill discovery enabled.
The template binds every run both to the canonical corpus SHA-256 and to a routing
input SHA-256 covering the catalog plus every routed `SKILL.md` and
`agents/openai.yaml`. Record `Codex` as the product and the exact model,
host/version, date, iteration, and selected skill slugs. Then score it:

```sh
python3 -B tools/check-skill-routing.py . \
  --observed /tmp/metaflux-skill-routing.json
```

A run passes only when every expected routed skill loads, no forbidden skill
loads, and no unlisted routed skill loads unless that case explicitly permits
it. Both digests must still match, runner metadata cannot retain template
placeholders or name another product, and every case must have exactly the
recorded repetition count.
The checked workflow roster is intentionally limited to
`$govern-semantic-change` and `$distill-project-knowledge`. Other workflow
skills such as `$start-work`, `$session-guidance`, and `$record-session` remain
outside this scorer and may load without changing a case result.

Captured observations are evidence only for their named model and host. Archive
the filled JSON with the qualifying session or benchmark output; do not turn a
static corpus pass into a claim that implicit routing succeeded.

## Coverage intent

The corpus includes:

- ecosystem-neutral registry/client/backend contracts and M0110 schema ownership;
- CUDA/NVML view behavior including `CUDA_VISIBLE_DEVICES`, replacement
  generations, and provider revisions captured at different initialization times;
- PTX semantics, MLIR conversion mechanics, and CPU/Vulkan target ownership;
- Linux UAPI, vfio-user wire/DMA, PCI presentation, and lifecycle composition;
- approved semantic replacements, protected-history migration, evidence-aware
  project-knowledge promotion, and their domain compositions;
- English and Chinese near-miss prompts that mention neighboring terminology.

Add a case whenever a description boundary, ownership route, or supported prompt
language changes. Keep expected routes about ownership, not the wording of the
eventual answer.
