---
name: branch-check
description: Ensure you're on a correct working branch off up-to-date master — in the main repo AND in every submodule you will touch — before planning or editing.
disable-model-invocation: true
---

Run before anything else, before planning or editing.

This repo is a **fork with two submodules that carry their own history and their own PRs**. Every repo you edit needs its own branch — never edit on `master` (main repo) or `develop` (submodules).

| Repo                                                                         | Path             | Base branch | GitHub                           |
| ---------------------------------------------------------------------------- | ---------------- | ----------- | -------------------------------- |
| main SDK                                                                     | `.`              | `master`    | `Akylas/mobile-sdk`              |
| carto libs (`vt`, `mapnikvt`, `cartocss`, `sgre`/`osrm`, `geocoding`, `nml`) | `libs-carto/`    | `develop`   | `farfromrefug/mobile-carto-libs` |
| external libs (cglib, freetype, harfbuzz, …)                                 | `libs-external/` | `develop`   | `Akylas/mobile-external-libs`    |

## Steps

1. Check the current branch in the main repo (`git branch --show-current`). On `master` → you must branch.
2. Bring the base up to date: `git pull origin master`. `upstream` is the **archived** `CartoDB/mobile-sdk` — never pull from or push to it.
3. If a GitHub issue is in play, fetch it and derive the branch from it:
   - `gh issue view <number> --repo Akylas/mobile-sdk --json number,title,labels`
   - Pick the `<type>` from the labels/intent (`fix` for a bug, `feat` for an enhancement, `docs`, `perf`, …)
   - Name it `<type>/<number>-<slug>`, `<slug>` = title lowercased, non-alphanumerics → `-`, trimmed
     (issue #1234 "Labels flicker while panning" → `fix/1234-labels-flicker-while-panning`)
4. No issue → `<type>/<short-description>` from the change itself, matching the naming already in this repo: `fix/label-panning-flicker`, `feat/terrain-tile-edge-matching`, `fix/terrain-drape-zoom-out-flash`.
5. Resolve the branch: check it out if it exists, else create it from the up-to-date base.
6. **Decide up front whether the work reaches into a submodule.** Anything under `vt` / `mapnikvt` / `cartocss` / `sgre` / `osrm` / `geocoding` / `nml` is `libs-carto`; third-party deps are `libs-external`. A renderer change very often spans both `all/native/` and `libs-carto/vt/`. When it does, branch there too, with the **same branch name** as the main repo so the pair is obvious:
   ```sh
   git -C libs-carto fetch origin
   git -C libs-carto checkout -b <same-branch-name> origin/develop
   ```
7. **Submodule detached-HEAD trap.** `libs-carto` / `libs-external` are routinely left on a detached HEAD by a superproject checkout — a commit there lands off-branch and a later push reports "Everything up-to-date" while the work stays invisible. Verify with `git -C libs-carto status -sb`: it must print `## <branch>...origin/<branch>`, not `## HEAD (no branch)`. Check **before** editing and **after** committing.
8. `libs-external` carries unrelated dirty nested-submodule pointers (`brotli/brotli`, `date/date`). Never stage those — commit only paths you actually edited.
9. Check out the branches BEFORE planning. Interactive: confirm the branch name(s) with the user first. **`--auto`** (caller runs autonomously): skip confirmation, just check out.
