defmodule HemeraHaikuRollout.CLI do
  alias HemeraHaikuRollout.Doctor
  alias HemeraHaikuRollout.LocalConfig
  alias HemeraHaikuRollout.Release
  alias HemeraHaikuRollout.Status
  alias HemeraHaikuRollout.Watch
  alias HemeraHaikuRollout.Workspace

  def main(argv) do
    {options, args, []} =
      OptionParser.parse(argv,
        strict: [
          cwd: :string,
          manifest: :string,
          local_config: :string,
          dry_run: :boolean
        ]
      )

    repo_root = Keyword.get(options, :cwd, HemeraHaikuRollout.repo_root())
    workspace_opts = [
      manifest: Keyword.get(options, :manifest),
      local_config: Keyword.get(options, :local_config)
    ]
    |> Enum.reject(fn {_key, value} -> is_nil(value) end)

    case args do
      ["init"] ->
        local_config_path = Keyword.get(options, :local_config, HemeraHaikuRollout.default_config_path())

        case LocalConfig.init(local_config_path) do
          {:ok, :created, path} ->
            IO.puts("created rollout config at #{path}; fill in the GitHub and HaikuPorts values before running doctor or release")

          {:ok, :exists, path} ->
            IO.puts("rollout config already exists at #{path}")
        end

      ["doctor"] ->
        repo_root |> Workspace.load!(workspace_opts) |> Doctor.run()

      ["status"] ->
        repo_root |> Workspace.load!(workspace_opts) |> Status.run()

      ["status", version] ->
        repo_root |> Workspace.load!(workspace_opts) |> Status.run(version)

      ["config", "status"] ->
        workspace = Workspace.load!(repo_root, workspace_opts)
        IO.puts(LocalConfig.status_lines(workspace))

      ["config", "set", dotted_path | values] when values != [] ->
        path = Keyword.get(options, :local_config, HemeraHaikuRollout.default_config_path())
        LocalConfig.set!(dotted_path, Enum.join(values, " "), path)
        IO.puts("updated local config: #{path}")
        IO.puts("set #{dotted_path}")

      ["config", "unset", dotted_path] ->
        path = Keyword.get(options, :local_config, HemeraHaikuRollout.default_config_path())
        LocalConfig.unset!(dotted_path, path)
        IO.puts("updated local config: #{path}")
        IO.puts("unset #{dotted_path}")

      ["release", version] ->
        repo_root
        |> Workspace.load!(workspace_opts)
        |> Release.run(version, mode: :release, dry_run: Keyword.get(options, :dry_run, false))

      ["resume", version] ->
        repo_root
        |> Workspace.load!(workspace_opts)
        |> Release.run(version, mode: :resume, dry_run: Keyword.get(options, :dry_run, false))

      ["watch", identifier] ->
        repo_root
        |> Workspace.load!(workspace_opts)
        |> Watch.run(identifier, dry_run: Keyword.get(options, :dry_run, false))

      _ ->
        IO.puts("""
        usage:
          scripts/release_haiku_rollout.sh init
          scripts/release_haiku_rollout.sh doctor
          scripts/release_haiku_rollout.sh status [version]
          scripts/release_haiku_rollout.sh config status
          scripts/release_haiku_rollout.sh config set <dotted.path> <value>
          scripts/release_haiku_rollout.sh config unset <dotted.path>
          scripts/release_haiku_rollout.sh release <version> [--dry-run]
          scripts/release_haiku_rollout.sh resume <version> [--dry-run]
          scripts/release_haiku_rollout.sh watch <pr-or-branch>
        """)

        System.halt(1)
    end
  rescue
    error ->
      IO.puts(:stderr, "error: #{Exception.message(error)}")
      System.halt(1)
  end
end
