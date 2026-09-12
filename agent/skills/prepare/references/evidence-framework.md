# Implementation Readiness Evidence Framework

Use this reference to conduct a full readiness review. It synthesizes primary
architecture, maturity, and launch-readiness sources into a repository-oriented
method; it is not a replacement for the repository's own acceptance criteria.

## Primary sources and applied rules

| Source | Applied rule |
| --- | --- |
| [SEI Architecture Tradeoff Analysis Method](https://www.sei.cmu.edu/library/atam-method-for-architecture-evaluation/) | Evaluate architecture against concrete quality attributes, tradeoffs, risks, and sensitivity points rather than diagram completeness. |
| [Google SRE, Reliable Product Launches at Scale](https://sre.google/sre-book/reliable-product-launches/) | Keep readiness checks concrete, actionable, proportional to risk, adaptable, and supported by reusable automation instead of ceremony. |
| [NASA Technology Readiness Levels](https://www.nasa.gov/directorates/somd/space-communications-navigation-program/technology-readiness-levels/) | Separate concept, proof, integrated prototype, qualified system, and proven operation; do not infer implementation maturity from design maturity. |
| [Thoughtworks, Building Evolutionary Architectures](https://www.thoughtworks.com/content/dam/thoughtworks/documents/books/bk_building_evolutionary_architectures_second_edition_free_chapter.pdf) | Make architectural characteristics explicit and continuously testable with fitness functions while changing the system incrementally. |

## Evidence lanes

### Architecture baseline

Inspect boundary ownership, contract versioning, dependency direction,
language/runtime walls, data and failure flows, and quality-attribute conflicts.
Strong evidence is executable: architecture tests, consumer tests, ABI checks,
closure checks, or a reproducible tradeoff experiment. A complete document with
no enforced characteristic is weaker evidence.

### Workstream activation

Inspect the active work item's dependencies, decisions-to-close, inputs,
deliverables, and acceptance evidence. Separate prerequisite decisions from
decisions whose evidence can only come from implementation. Activation is ready
when an engineer can make the first bounded edit without inventing a contract,
ownership boundary, or success condition.

### Implementation maturity

Classify observed code by behavior, not directory presence:

| State | Required evidence |
| --- | --- |
| Scaffold | Targets configure/build, but behavior is fixture or stubbed. |
| Vertical slice | One real user or system path crosses the intended contracts end to end. |
| Integrated | Normal and failure paths work across participating components with automated tests. |
| Qualified | Compatibility, performance, resilience, and acceptance thresholds pass in the named environment. |

Do not promote a state based on planned files, placeholder APIs, or tests that
only prove process startup.

Inspect the exact fixture/source list and selected execution path before using
a passing test as evidence for a new feature. A manifest self-test proves its
declarations; embedding source text in a successful build does not prove that
the product parser or executor consumed it. Separate a failed delivery gate
from independent checks that can still measure the in-scope implementation.

### Release and operations

Inspect deployment artifacts, supported environments, upgrade/rollback,
security controls, observability, capacity, incident response, compatibility,
performance, and external obligations. Apply only to a release decision; these
gaps do not automatically block an early implementation slice.

## Decision timing

For each relevant open decision, record one timing class and why:

| Timing | Test |
| --- | --- |
| Before implementation | The first edit would otherwise guess a public contract, ownership boundary, irreversible data shape, or dependency rule. |
| Before integration or qualification | A local slice can proceed, but merging, scaling, compatibility, or acceptance depends on the answer. |
| Evidence-generating | Implementing a reversible probe or slice is the named method for obtaining the decision evidence. |

An open-decision count is not a readiness score. A small unresolved contract can
block work while many release-timing decisions may remain safely open.

## Fitness-function test

For every important `-ility`, ask:

1. What observable system behavior represents it?
2. Which command or artifact measures that behavior?
3. What threshold distinguishes pass from fail?
4. At which boundary must it become binding?

Record missing harnesses honestly. A provisional budget can guide design, but
it does not become qualification evidence until the harness and baseline exist.

## Review matrix

```text
Decision scope: <implementation start | integration | qualification | release>

| Lane | Verdict | Strongest evidence | Blocking gap |
| Architecture baseline | ... | ... | ... |
| Workstream activation | ... | ... | ... |
| Implementation maturity | ... | ... | ... |
| Release and operations | ... | ... | ... |

Must close now:
- <decision or evidence gap -> exact reason>

Can close later:
- <decision -> latest safe closure boundary>

Next actions:
1. <action -> expected command/artifact evidence>
```
