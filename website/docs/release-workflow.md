---
title: Release Workflow
sidebar_position: 7
slug: /release-workflow
---

# Release & auto-publishing

The docs (and API reference) publish themselves. A GitHub Actions workflow rebuilds and deploys to
GitHub Pages:

- on every **push to `master`** that touches docs, the SDK API surface or the workflow itself,
- on every **published GitHub Release**,
- and **manually** via *Run workflow* (workflow_dispatch).

## What the workflow does

1. Checks out the repo **with submodules** (`libs-carto`, `libs-external`).
2. Generates the **Android Javadoc** and **iOS Jazzy** reference from the SWIG bindings into
   `website/static/api/{android,ios}`.
3. Builds the **Docusaurus** site (`npm ci && npm run build`).
4. Uploads the result and **deploys to GitHub Pages**.

The workflow file is
[`.github/workflows/docs.yml`](https://github.com/Akylas/mobile-sdk/blob/master/.github/workflows/docs.yml).

## One-time setup

Enable Pages for the repository:

1. **Settings → Pages → Build and deployment → Source: GitHub Actions.**
2. Push to `master` (or run the workflow manually). The site publishes to
   `https://akylas.github.io/mobile-sdk/`.

## Versioned docs (optional)

Docusaurus supports [versioned docs](https://docusaurus.io/docs/versioning). To snapshot the
current docs for a release tag:

```bash
cd website
npm run docusaurus docs:version 5.0.0
```

This freezes `docs/` into `versioned_docs/version-5.0.0/` and adds a version dropdown. Commit the
snapshot; the workflow will publish all versions. Do this per major/minor release you want to keep
browsable.
