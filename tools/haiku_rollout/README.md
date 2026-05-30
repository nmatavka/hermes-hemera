# Hemera Haiku Rollout

This Mix tool automates Hemera release preparation for HaikuPorts.

It is a developer tool, not part of the Hemera app package. The tool:

- validates local prerequisites with `doctor`
- builds and uploads a tagged source tarball release asset
- renders the Hemera HaikuPorts recipe from `packaging/haikuports/mail-client/hemera/hemera.recipe.in`
- updates a local HaikuPorts checkout, pushes a branch, opens a PR, and watches it

## Prerequisites

- `git`
- `gh`
- `mix` / `elixir`
- a writable local HaikuPorts checkout path or parent directory
- `gh auth login` completed for the GitHub account that owns the HaikuPorts fork

On Haiku hosts, the optional local preflight also expects:

- `cmake`
- `ctest`
- Haiku packaging tools such as `package`

## Configuration

Bootstrap a local config first:

```sh
scripts/release_haiku_rollout.sh init
```

That creates `config.yml` beside the tool's `config.example.yml`. Then fill in your real GitHub and HaikuPorts values.

Required config includes:

- Hemera GitHub repo owner and name
- release tag and asset naming templates
- HaikuPorts upstream URL, fork URL, and fork owner
- local HaikuPorts checkout path
- target branch and target port path
- optional Haiku-only local preflight command overrides

You can override the config path with:

```sh
HEMERA_HAIKU_ROLLOUT_CONFIG=/abs/path/to/config.yml
```

## Usage

From the repository root:

```sh
scripts/release_haiku_rollout.sh init
scripts/release_haiku_rollout.sh doctor
scripts/release_haiku_rollout.sh release 1.0.0-rc1
scripts/release_haiku_rollout.sh watch 1234
```

For a non-mutating preview of the release plan:

```sh
scripts/release_haiku_rollout.sh release 1.0.0-rc1 --dry-run
```

## End-to-end flow

`release <version>` performs the full submission flow:

1. validates Hemera version surfaces for the requested release
2. reuses or creates the configured git tag
3. builds a source tarball from the tagged tree
4. creates or updates the GitHub release and uploads the tarball asset
5. computes the tarball SHA-256 and renders the HaikuPorts recipe
6. clones or refreshes the local HaikuPorts checkout and resets its target branch from upstream
7. writes the finished port tree into `mail-client/hemera`
8. commits, pushes, and opens or updates the HaikuPorts PR
9. watches the server-side PR checks

When the tool runs on Haiku, it inserts the local Hemera configure/build/test/HPKG preflight before
the HaikuPorts branch and PR steps. On other hosts, that preflight is skipped and the flow relies on
the HaikuPorts PR checks for build validation.
