# website/ — the documentation site

This directory is the [Docusaurus](https://docusaurus.io/) **shell only**: config, theme, React
pages, static assets. **All documentation content lives in [`../docs/`](../docs)** and is read
through `path: '../docs'` in `docusaurus.config.js`. Editing a page means editing a file under
`../docs`, not here.

Full guide, including how the guides are generated and how the API reference is built:
[`docs/contributing/docs-site.md`](../docs/contributing/docs-site.md).

## Test it locally

Node ≥ 18 (developed on 26).

```bash
cd website
npm install
```

### 1. Dev server — writing and reviewing content

```bash
npm start
```

Serves <http://localhost:3000/MassifMaps/> with hot reload. **Hot reload watches `../docs`** even
though it sits outside this directory — save a `.md` there and the page updates in about a second.

Pick another port when 3000 is taken:

```bash
npm start -- --port 3333
```

### 2. Production build — the real gate

```bash
npm run build      # -> website/build
npm run serve      # serve that build at http://localhost:3000/MassifMaps/
```

Run this before opening a docs PR. It is what CI runs, and several things **only** work here.

## What works where

| | `npm start` | `npm run build` |
|---|---|---|
| Hot reload of `../docs` | ✅ | — |
| Mermaid diagrams (`.mdx` only) | ✅ | ✅ |
| Relative markdown links (`04-terrain.md`) | ⚠️ warns in the terminal | ⚠️ warns |
| Route links (`/docs/…`) | ❌ **not checked** | ⚠️ warns, listed at the end |
| Local search | ❌ *"The search index is only available when you run docusaurus build!"* | ✅ |
| `/roadmap` GitHub issue fetch | at server start | at build time |

So: **write in `npm start`, verify in `npm run build`.** A page that looks right in the dev server
can still ship a dead `/docs/...` link and a search index that never saw it.

## Gotchas

- **Mermaid renders only in `.mdx`.** `markdown.format` is `'detect'`, so a `.md` page is parsed as
  CommonMark and a ` ```mermaid ` block is dropped **without any warning** — the diagram simply is
  not there. Rename the page to `.mdx` and update the links pointing at it.
- **Quote front-matter values containing a colon**, or the YAML parse aborts the whole build with
  `Error while parsing Markdown front matter`.
- **`onBrokenLinks` is `'warn'`**, so a broken route link does not fail the build. Read the
  "Exhaustive list of all broken links found" block at the end of the output; do not just look for
  `[SUCCESS]`.
- **Stale output after a move or rename**: `npm run clear`, then build again.
- `../docs/_archive/**` is excluded from the site on purpose. A page there will 404 if you link it.

## Check a change end to end

```bash
cd website
npm run build 2>&1 | tail -30      # must end [SUCCESS] with no broken-link list
npm run serve                       # then search for a phrase from the page you changed
```
