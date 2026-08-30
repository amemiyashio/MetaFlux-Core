# Notes

## Codex package authority

The [official OpenAI Build skills documentation](https://learn.chatgpt.com/docs/build-skills)
defines a skill as a directory whose required entry is uppercase `SKILL.md`
with `name` and `description`. `scripts/`, `references/`, `assets/`, and
`agents/openai.yaml` are optional. Codex scans repository `.agents/skills`
locations and follows symlinked skill folders.

The previous repository form required a nonstandard `status` key inside every
`SKILL.md` and did not expose the packages at Codex's repository discovery
path. The final form keeps lifecycle state in the repository index, preserves
the physical `agent/skills` path for durable links, and checks the relative
`.agents/skills -> ../agent/skills` bridge in both the stdlib validator and the
isolated Nix entry-point fixture.

The validator accepts the same standard frontmatter set as the bundled skill
creator (`name`, `description`, `license`, `allowed-tools`, and `metadata`) and
does not require optional package directories. It verifies required scalar
metadata, name/slug agreement, UTF-8 optional OpenAI metadata, index
completeness, and discovery-link integrity.

## Readiness method

The implementation-readiness guidance distills four primary sources:

- [SEI ATAM](https://www.sei.cmu.edu/library/atam-method-for-architecture-evaluation/):
  evaluate architecture through quality attributes, tradeoffs, risks, and
  sensitivity points.
- [Google SRE reliable launches](https://sre.google/sre-book/reliable-product-launches/):
  make readiness checks concrete, actionable, proportional to risk, and
  increasingly automated.
- [NASA technology readiness levels](https://www.nasa.gov/directorates/somd/space-communications-navigation-program/technology-readiness-levels/):
  distinguish concept, integrated prototype, qualification, and proven use.
- [Thoughtworks evolutionary architecture](https://www.thoughtworks.com/content/dam/thoughtworks/documents/books/bk_building_evolutionary_architectures_second_edition_free_chapter.pdf):
  guide incremental change with explicit, executable architecture fitness
  functions.

The resulting method deliberately has no aggregate readiness score. It answers
the exact decision boundary, grades four lanes independently, classifies open
decisions by latest safe closure point, and selects the smallest real vertical
slice that retires the highest uncertainty.

## Verification detail

All five actual packages passed the bundled `quick_validate.py`. The synthetic
validator suite passed 29 cases, including minimal packages, optional resources,
extended standard frontmatter, missing/misdirected discovery links, mismatched
names, and rejection of the retired repository-only `status` field.

The dev preset passed 16/16 tests. Both targeted Nix checks passed from a copied
source, and the full flake check ended with all checks passed. The binary cache
timed out briefly while fetching metadata and succeeded on retry; no local gate
was skipped.
