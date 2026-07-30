# Writing documentation in mobile-sdk

Four distinct homes, four different bars. Pick the home first — most changes need **none** of them.

| What you learned                                  | Where it goes                                           |
| ------------------------------------------------- | ------------------------------------------------------- |
| Why this code is weird / non-obvious              | a comment next to the code                              |
| What a public API does and how an app calls it    | doc comment in the SWIG `.i` (it becomes Javadoc/Jazzy) |
| How a subsystem works, for the next maintainer    | the relevant section of the root `CLAUDE.md`            |
| A user-facing feature, guide, or config reference | `website/docs/` (Docusaurus site)                       |

## Code comments — WHY only

The default is **no comment**. Write one only when the code cannot carry the information itself:

- A magic constant that came from measurement (`// 12 clip units: chord error of the terrain tesselation at z11`).
- A workaround for platform/driver behaviour (`// glGetUniformLocation, not Shader::getUniformLoc — the latter returns 0 for a dropped uniform and 0 is a valid location`).
- An invariant a future edit would silently break (`// prefer the geometry copy with the same (tileId, localId): re-snapping by list order breaks line fitting`).
- An ordering or threading constraint (which thread runs this, what must not clear the culler grid).

Never write: what the next line does, a restated function name, a changelog entry, a TODO with no owner, or a comment that will drift out of date the moment the code moves.

Match the surrounding density: `all/native/` and `libs-carto/vt/` are sparsely commented. A block of prose in a hot render path is noise; one sharp sentence above the constant is the norm.

## Public API — SWIG `.i` doc comments

`all/modules/*.i` is the public surface, and its comments are what app developers actually read: the generated Javadoc/Jazzy under `website/static/api/{android,ios}` comes from there (`scripts/docs/gen-api-android.sh` / `gen-api-ios.sh`).

- Document every new public method/class you add there: one-line purpose, each parameter's unit and range, the default, and what happens at the edges (0, negative, unset).
- Units matter more than prose in this SDK — meters vs tiles vs NDC vs degrees vs hours. State them.
- Say when a setter takes effect (next frame / next tile fetch / requires a re-render) and whether it is thread-safe to call from the app thread.
- A doc-comment-only change still needs the wrappers regenerated (`swigpp-java.py`) before it reaches the site.

## Subsystem knowledge — root `CLAUDE.md`

The root `CLAUDE.md` is the maintainer-facing map: repository layout, the demo-app loop, the renderer debugging playbook, the label pipeline invariants, terrain/fog/sky wiring. Add there when you learned something **durable** that would otherwise cost the next person a day:

- A new invariant, with the failure it prevents ("`LabelCuller::process` must NOT clear the grid — layers must collide").
- A debugging technique that actually worked, and what it distinguished ("A/B per screen row band separated 'tile never loaded' from 'tile drawn but depth-rejected'").
- A trap with a misleading symptom (camera clearance clamp auto-zooming out looks like a broken renderer).

Keep it additive and terse, in the existing voice. Do not paste a plan, a diff summary, or an experiment log — those go in `docs/*-plan.md` if anywhere.

## User-facing docs — `website/docs/`

Docusaurus site (`cd website && npm start`), deployed on pushes to `master` that touch `website/**`, `docs/**` or `scripts/docs/**`.

- Feature/guide pages live under `website/docs/{getting-started,guides,features}/` with front-matter (`title`, `sidebar_position`, `slug`).
- Show the app-facing call, not the C++ internals: the option, its default, its unit, and a short snippet.
- Screenshots come from the demo app via `scripts/docs/capture-screenshots.sh` into `website/static/img/features/` — reference existing images rather than inventing paths.
- `docs/guides/*.md` are the converted legacy CARTO guides (`scripts/docs/convert-guides.py`); edit the converted output under `website/docs/` for new content, not the legacy source, unless you re-run the converter.

## What NOT to document

- Anything the code, the git history, or a test already states.
- Speculative future work (that is an issue, not a doc).
- A summary of the change you just made — the commit message and PR body carry that.
