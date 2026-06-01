defmodule HemeraHaikuRollout.Workspace do
  alias HemeraHaikuRollout.{LocalConfig, Manifest}

  defstruct [
    :root,
    :tool_root,
    :manifest_path,
    :local_config_path,
    :manifest_source,
    :local_config,
    :manifest,
    :version
  ]

  def load!(root, opts \\ []) do
    expanded_root = Path.expand(root)
    manifest_path =
      expand_repo_path(Keyword.get(opts, :manifest, HemeraHaikuRollout.default_manifest_path(expanded_root)), expanded_root)

    local_config_path =
      Keyword.get(opts, :local_config, HemeraHaikuRollout.default_config_path())
      |> Path.expand()

    manifest_source = Manifest.load_map!(manifest_path)
    local_config = LocalConfig.load(local_config_path)

    merged_manifest =
      manifest_source
      |> LocalConfig.merge_manifest(local_config)
      |> Manifest.from_map!(manifest_path)
      |> Manifest.with_repo_paths(expanded_root)

    %__MODULE__{
      root: expanded_root,
      tool_root: HemeraHaikuRollout.tool_root(),
      manifest_path: manifest_path,
      local_config_path: local_config_path,
      manifest_source: manifest_source,
      local_config: local_config,
      manifest: merged_manifest,
      version: merged_manifest.release_version
    }
  end

  def work_dir(%__MODULE__{root: root}, version) do
    Path.join(root, "build/haiku_rollout/#{version}")
  end

  def recipe_template_path(%__MODULE__{root: root}) do
    HemeraHaikuRollout.recipe_template_path(root)
  end

  def recipe_template_dir(%__MODULE__{root: root}) do
    HemeraHaikuRollout.recipe_template_dir(root)
  end

  defp expand_repo_path(path, repo_root) when is_binary(path) do
    if Path.type(path) == :absolute do
      path
    else
      Path.expand(path, repo_root)
    end
  end
end
