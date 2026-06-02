# Hemera Haiku Rollout

This Mix tool automates Hemera release preparation for HaikuPorts.
It follows the same broad operator shape as the other HERMES rollout tooling, but only for
Hemera's single Haiku release path.

It is a developer tool, not part of the Hemera app package. The tool:

- validates local prerequisites with `doctor`
- reports workspace/state progress with `status`
- manages user-local overrides with `config status|set|unset`
- builds and uploads a tagged source tarball release asset
- renders the Hemera HaikuPorts recipe from `packaging/haikuports/mail-client/hemera/hemera.recipe.in`
- updates a local HaikuPorts checkout, pushes a branch, and prints the exact `gh pr create` handoff command in a template-safe `--editor` mode
- can `resume` an interrupted rollout from recorded state in `build/haiku_rollout/<version>/state.json`

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

The rollout now uses two layers of configuration:

- repo-owned release manifest: `packaging/haiku/release_manifest.yml`
- user-local overrides: `~/.local/share/hemera_haiku_rollout/config.yml`

The local override file is optional. If you want a scaffold, bootstrap it with:

```sh
scripts/release_haiku_rollout.sh init
```

That copies `tools/haiku_rollout/config.example.yml` to your user-local config path.
Only user-specific HaikuPorts and optional GitHub override values belong there.

The repo-owned manifest is the source of truth for:

- release version surfaces
- tag/title/asset templates
- release notes path
- HaikuPorts target path, suggested PR title template, and optional PR handoff notes
- Haiku-only preflight defaults

The local override file is only for user-specific values:

- optional GitHub repo owner override
- HaikuPorts fork URL
- HaikuPorts fork owner
- local HaikuPorts checkout path
- optional Haiku-only preflight command overrides

You can override the local config path with:

```sh
HEMERA_HAIKU_ROLLOUT_CONFIG=/abs/path/to/config.yml
```

You can also point the tool at a different checkout or manifest:

```sh
scripts/release_haiku_rollout.sh doctor --cwd /path/to/hermes-hemera
scripts/release_haiku_rollout.sh doctor --manifest /path/to/release_manifest.yml
```

## Usage

From the repository root:

```sh
scripts/release_haiku_rollout.sh init
scripts/release_haiku_rollout.sh doctor
scripts/release_haiku_rollout.sh status
scripts/release_haiku_rollout.sh config status
scripts/release_haiku_rollout.sh config set haikuports.fork_owner your-github-user
scripts/release_haiku_rollout.sh release 1.0
scripts/release_haiku_rollout.sh resume 1.0
scripts/release_haiku_rollout.sh watch 1234
```

For a non-mutating preview of the release plan:

```sh
scripts/release_haiku_rollout.sh release 1.0 --dry-run
```

## End-to-end flow

`release <version>` performs the full submission flow:

1. validates Hemera version surfaces for the requested release
2. reuses or creates the configured git tag
3. builds a source tarball from the tagged tree
4. creates or updates the GitHub release and uploads the tarball asset
5. computes the tarball SHA-256 and validates the rendered HaikuPorts recipe ordering
6. clones or refreshes the local HaikuPorts checkout and resets its target branch from upstream
7. writes the finished port tree into `mail-client/hemera`
8. commits and pushes the HaikuPorts branch
9. prints the exact `gh pr create` handoff command, the suggested PR title, and the follow-up watch command

If the managed HaikuPorts branch already exists on your fork at a different commit, the rollout
stops before push and prints the local SHA, remote SHA, and manual recovery commands instead of
overwriting the remote branch automatically.

`resume <version>` reuses recorded rollout state, skips already-completed early steps when their
recorded outputs are still present, and re-checks whether the HaikuPorts branch already has an open
PR before printing another handoff command.

The rollout does not inject PR body text into `gh pr create`. HaikuPorts' own contributor template
is expected to appear in the editor. If the checklist template does not appear, abort without
submitting and create the PR manually so the template is preserved.

When the tool runs on Haiku, it inserts the local Hemera configure/build/test/HPKG preflight before
the HaikuPorts branch and PR steps. On other hosts, that preflight is skipped and the flow relies on
the HaikuPorts PR checks for build validation.
