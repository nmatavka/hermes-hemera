defmodule HemeraHaikuRollout.TestSupport.WorkspaceFactory do
  alias HemeraHaikuRollout.Workspace

  def repo_root do
    HemeraHaikuRollout.repo_root()
  end

  def local_config_file!(contents \\ nil) do
    dir = Path.join(System.tmp_dir!(), "hemera-haiku-rollout-test-#{System.unique_integer([:positive])}")
    File.mkdir_p!(dir)
    path = Path.join(dir, "config.yml")
    checkout_path = Path.join(System.tmp_dir!(), "haikuports-checkout-#{System.unique_integer([:positive])}")
    File.mkdir_p!(Path.join(checkout_path, ".git"))

    File.write!(
      path,
      contents ||
        """
        github:
          repo_owner: "nick"
        haikuports:
          fork_url: "git@github.com:nick/haikuports.git"
          fork_owner: "nick"
          checkout_path: "#{checkout_path}"
        """
    )

    path
  end

  def workspace!(opts \\ []) do
    root = Keyword.get(opts, :root, repo_root())
    local_config = Keyword.get(opts, :local_config, local_config_file!())
    manifest = Keyword.get(opts, :manifest)

    load_opts =
      [
        local_config: local_config
      ]
      |> maybe_put(:manifest, manifest)

    Workspace.load!(root, load_opts)
  end

  defp maybe_put(opts, _key, nil), do: opts
  defp maybe_put(opts, key, value), do: Keyword.put(opts, key, value)
end
