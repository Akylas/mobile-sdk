# Archive — historical, not published, not maintained

Superseded design plans and one-off investigations, kept because they record **what was tried and
why it was dropped**. They are excluded from the documentation site (`exclude: ['_archive/**']` in
`website/docusaurus.config.js`) and they are **not** kept in sync with the code.

Do not cite these as current behaviour. The current design is:

- render path → [`docs/internals/rendering/`](../internals/rendering/index.mdx)
- measurements → [`docs/internals/performance-log.md`](../internals/performance-log.md)
- procedures → [`docs/maintenance/`](../maintenance/index.md)

| File | Superseded by |
|---|---|
| `TERRAIN_GPU_DRAPING_PLAN.md`, `TERRAIN_PHASE_D_PLAN.md`, `terrain-rtt-draping-plan.md`, `terrain-3d-draping.md` | [`04-terrain.md`](../internals/rendering/04-terrain.md) — the RTT drape these describe is being removed |
| `terrain-lighting-architecture.md`, `sky-sun-shadows-plan.md` | [`08-lighting-sky-fog.md`](../internals/rendering/08-lighting-sky-fog.md) |
| `composite-vector-tile-layer-plan.md` | [`09-composite-layer.md`](../internals/rendering/09-composite-layer.md) + [the style reference](../features/composite-layer-reference.md) |
| `terrain-landcover-holes-investigation.md` | [`05-depth-model.md`](../internals/rendering/05-depth-model.md) |
| `pmtiles-implementation.md` | [`pmtiles-datasource.md`](../internals/pmtiles-datasource.md) + [the feature page](../features/pmtiles.md) |
| `known-issues.md` | GitHub issues |

Adding to this directory is a **move**, not a copy: a plan that is now implemented is either folded
into the page that documents the behaviour, or moved here — never both.
