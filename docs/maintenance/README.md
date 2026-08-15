# Maintenance docs

Procedures for keeping the vendored dependencies and the generated artefacts of this fork up to
date. Same rule as [`docs/rendering/`](../rendering/README.md): a procedure that had to be
rediscovered gets written down here in the SAME commit as the work — exact commands, the versions
they were run with, what breaks when a step is skipped, and the dead ends.

| Page | What it covers |
|------|----------------|
| [`valhalla-upgrade.md`](valhalla-upgrade.md) | Merging a new upstream Valhalla release into `mbtiles-support`, regenerating protos / locales / the tz database, and the fork patches that must survive |
| [`mac-catalyst.md`](mac-catalyst.md) | Why the Catalyst slices are a macOS project in disguise, what that breaks at link time, and what the build gives up to work around it |
