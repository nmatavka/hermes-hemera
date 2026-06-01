# Hemera Haiku Packaging

This directory contains the Haiku resource and package metadata for the `1.0` release.

- `release_manifest.yml`: the repo-owned Haiku rollout manifest consumed by the Elixir orchestrator
- `Hemera.rdef.in`: app signature, version metadata, and embedded app icon resource template
- `Hemera.PackageInfo.in`: HPKG metadata template used by the Haiku package target
- `../haikuports/mail-client/hemera/hemera.recipe.in`: the repo-local HaikuPorts recipe template that the rollout tool materializes for a real release submission

The packaged app is named `Hemera`, while the Haiku app signature remains `application/x-vnd.hermes-hemera`.

For release work, use the rollout tool instead of editing recipes and checksums by hand. The tool
also validates the Hemera recipe against the checked-in HaikuPorter field ordering file before
submission, stops if the managed fork branch has diverged remotely, and only then hands off to a
guided `gh pr create` handoff for the final HaikuPorts PR:

```sh
scripts/release_haiku_rollout.sh doctor
scripts/release_haiku_rollout.sh status
scripts/release_haiku_rollout.sh release 1.0
scripts/release_haiku_rollout.sh resume 1.0
```
