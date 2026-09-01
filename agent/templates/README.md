# Knowledge Templates

Required shapes for durable product plans and validated experience. Agent
execution state uses [`agent/goal.json`](../goal.json) directly and has no
per-run template or archive.

| Template | Instantiated at |
| --- | --- |
| [milestone.md](milestone.md) | `agent/plan/milestone-MAJOR.MINOR.PATCH.0-slug/plan.md` |
| [work-item.md](work-item.md) | `agent/plan/milestone-.../work/work-item-MAJOR.MINOR.PATCH.WORK-slug.md` |
| [experience.md](experience.md) | `agent/experience/experience-NNNN-slug.md` |

Breaking changes to these shapes require explicit `govern-epoch` execution.
