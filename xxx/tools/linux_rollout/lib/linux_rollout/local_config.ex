defmodule LinuxRollout.LocalConfig do
  alias LinuxRollout.{Util, Workspace, Yaml}

  @resolved_account_tokens [
    {"accounts.launchpad_owner", "launchpad_owner"},
    {"accounts.nix_maintainer_email", "nix_maintainer_email"},
    {"accounts.nix_maintainer_github", "nix_maintainer_github"},
    {"accounts.nix_maintainer_github_id", "nix_maintainer_github_id"},
    {"accounts.nix_maintainer_handle", "nix_maintainer_handle"},
    {"accounts.nix_maintainer_name", "nix_maintainer_name"},
    {"accounts.obs_owner", "obs_owner"},
    {"accounts.obs_project", "obs_project"},
    {"accounts.pkgsrc_wip_user", "pkgsrc_wip_user"},
    {"accounts.submission_email", "submission_email"}
  ]

  def path, do: Util.local_config_path()

  def load(path \\ path()) do
    if File.exists?(path) do
      Yaml.load!(path)
    else
      %{}
    end
  end

  def write!(data, path \\ path()) do
    Yaml.dump!(path, data)
  end

  def set!(dotted_path, value, path \\ path()) when is_binary(value) do
    segments = validate_path!(dotted_path)

    path
    |> load()
    |> Util.path_put(segments, value)
    |> write!(path)
  end

  def unset!(dotted_path, path \\ path()) do
    segments = validate_path!(dotted_path)

    updated =
      path
      |> load()
      |> Util.path_delete(segments)

    if Util.blank?(updated) do
      File.rm(path)
    else
      write!(updated, path)
    end
  end

  def put_if_blank!(dotted_path, value, path \\ path())

  def put_if_blank!(dotted_path, value, path) when is_binary(value) and value != "" do
    segments = validate_path!(dotted_path)
    current = load(path)

    if Util.blank?(Util.path_get(current, segments)) do
      current
      |> Util.path_put(segments, value)
      |> write!(path)

      :stored
    else
      :unchanged
    end
  end

  def put_if_blank!(_dotted_path, _value, _path), do: :unchanged

  def merge_manifest(manifest, config) do
    merged_accounts =
      Util.deep_merge(Map.get(manifest, "accounts", %{}), Map.get(config, "accounts", %{}))

    merged_targets =
      Map.get(config, "targets", %{})
      |> Enum.reduce(Map.get(manifest, "targets", %{}), fn {target_name, target_override}, acc ->
        destination_override = Map.get(target_override, "destination", %{})
        current_target = Map.get(acc, target_name, %{})
        current_destination = Map.get(current_target, "destination", %{})

        Map.put(
          acc,
          target_name,
          Map.put(
            current_target,
            "destination",
            Util.deep_merge(current_destination, destination_override)
          )
        )
      end)

    manifest
    |> Map.put("accounts", merged_accounts)
    |> Map.put("targets", merged_targets)
  end

  def status_lines(%Workspace{} = workspace) do
    local_entries =
      workspace.local_config
      |> flatten()
      |> Enum.sort()

    resolved_entries =
      @resolved_account_tokens
      |> Enum.map(fn {dotted_path, token_key} ->
        {dotted_path, Map.get(workspace.tokens, token_key)}
      end)
      |> Enum.reject(fn {_path, value} -> Util.blank?(value) end)

    [
      "Local config: #{workspace.local_config_path}",
      if(local_entries == [], do: "Local overrides: (none)", else: "Local overrides:"),
      Enum.map(local_entries, fn {key, value} -> "  - #{key}: #{value}" end),
      if(resolved_entries == [], do: "Resolved local keys: (none)", else: "Resolved local keys:"),
      Enum.map(resolved_entries, fn {key, value} -> "  - #{key}: #{value}" end)
    ]
    |> List.flatten()
    |> Enum.join("\n")
  end

  defp validate_path!(dotted_path) when is_binary(dotted_path) do
    segments = String.split(dotted_path, ".", trim: true)

    if allowed_path?(segments) do
      segments
    else
      raise "unsupported local config path #{dotted_path}"
    end
  end

  defp allowed_path?(["accounts" | rest]), do: rest != []
  defp allowed_path?(["targets", _target_name, "destination" | rest]), do: rest != []
  defp allowed_path?(_segments), do: false

  defp flatten(value), do: flatten(value, [])

  defp flatten(value, prefix) when is_map(value) do
    value
    |> Enum.flat_map(fn {key, nested} ->
      flatten(nested, prefix ++ [to_string(key)])
    end)
  end

  defp flatten(value, prefix) do
    [{Enum.join(prefix, "."), value}]
  end
end
