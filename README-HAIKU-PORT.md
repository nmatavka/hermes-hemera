# Hemera Haiku Port

This repository root now contains the new Haiku-port scaffolding and build system.
The original Windows/MFC/Stingray-era source drop has been moved to
`legacy/eudora-drop/` and is intentionally excluded from version control so the new
port can evolve independently.

## Layout

- `src/hermes_port/core/`: portable app-owned interfaces and bootstrap implementations.
- `src/hermes_port/haiku/`: native Haiku shell skeleton and future Haiku adapters.
- `tests/`: lightweight regression coverage for the new portable seams.
- `third_party/`: optional pinned dependency submodules/checkouts for `Hermes-Paige`, `OpenSSL`,
  `Hunspell`, and `krb5`.
- `legacy/eudora-drop/`: ignored archive of the original Eudora source/materials.

## Dependencies

The dependency record stays in Git submodules:

- `third_party/Hermes-Paige`
- `third_party/openssl`
- `third_party/hunspell`
- `third_party/krb5`

The normal build no longer requires those checkout trees to exist under the repo root.
`cmake -S . -B build` will prefer system-installed libraries and then fetch pinned source
trees into `build/_deps/` when it needs a local dependency checkout.
The shared pinned refs for both paths live in
`cmake/HermesDependencyRefs.env`, so the clean-clone configure path and the optional
bootstrap path use the same repositories and revisions.

[`scripts/bootstrap_dependencies.sh`](/Users/nick/hermes-hemera/scripts/bootstrap_dependencies.sh)
is still available as an optional prewarm/offline helper if you want repo-local submodule
checkouts.

## Building

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

On non-Haiku hosts this builds the portable core and tests. On Haiku, enabling
`HERMES_BUILD_HAIKU_SHELL` also builds the native shell scaffold. Deleting `build/`
is fine; rerunning the configure and build commands will recreate the build tree and any
auto-fetched dependency sources it needs.

## Releasing

Use the rollout tool for Haiku package-manager submissions instead of editing the
HaikuPorts recipe manually:

```sh
scripts/release_haiku_rollout.sh doctor
scripts/release_haiku_rollout.sh status
scripts/release_haiku_rollout.sh release 1.0
```

The rollout tool now reads the repo-owned release manifest at
`packaging/haiku/release_manifest.yml` plus user-local overrides in
`~/.local/share/hemera_haiku_rollout/config.yml`.

The rollout tool lives in `tools/haiku_rollout/`, reads its own YAML config file, uploads
the source tarball asset to the GitHub release, renders the recipe from
`packaging/haikuports/mail-client/hemera/hemera.recipe.in`, and opens or updates the
HaikuPorts PR.
