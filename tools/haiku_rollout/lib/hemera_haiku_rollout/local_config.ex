defmodule HemeraHaikuRollout.LocalConfig do
  alias HemeraHaikuRollout.Util
  alias HemeraHaikuRollout.Workspace

  @allowed_paths [
    ["github", "repo_owner"],
    ["haikuports", "fork_url"],
    ["haikuports", "fork_owner"],
    ["haikuports", "checkout_path"]
  ]

  def path, do: HemeraHaikuRollout.default_config_path()

  def load(path \\ path()) do
    if File.exists?(path) do
      case YamlElixir.read_from_file(path) do
        {:ok, map} when is_map(map) -> map
        map when is_map(map) -> map
        _other -> %{}
      end
    else
      %{}
    end
  rescue
    _error -> %{}
  end

  def init(path \\ path()) do
    destination = Path.expand(path)
    File.mkdir_p!(Path.dirname(destination))

    cond do
      File.exists?(destination) ->
        {:ok, :exists, destination}

      true ->
        File.cp!(HemeraHaikuRollout.example_config_path(), destination)
        {:ok, :created, destination}
    end
  end

  def write!(data, path \\ path()) do
    if Util.blank?(data) do
      File.rm(path)
    else
      File.mkdir_p!(Path.dirname(path))
      File.write!(path, dump_yaml(data))
    end
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

    path
    |> load()
    |> Util.path_delete(segments)
    |> write!(path)
  end

  def merge_manifest(manifest_map, local_config) do
    Util.deep_merge(manifest_map, local_config)
  end

  def status_lines(%Workspace{} = workspace) do
    local_entries =
      workspace.local_config
      |> flatten()
      |> Enum.sort()

    resolved_entries =
      [
        {"github.repo_owner", workspace.manifest.repo_owner},
        {"github.repo_name", workspace.manifest.repo_name},
        {"haikuports.fork_url", workspace.manifest.haikuports_fork_url},
        {"haikuports.fork_owner", workspace.manifest.haikuports_fork_owner},
        {"haikuports.checkout_path", workspace.manifest.haikuports_checkout_path}
      ]
      |> Enum.reject(fn {_key, value} -> Util.blank?(value) end)

    [
      "Local config: #{workspace.local_config_path}#{if(File.exists?(workspace.local_config_path), do: "", else: " (not present)")}",
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

    if segments in @allowed_paths do
      segments
    else
      raise "unsupported local config path #{dotted_path}"
    end
  end

  defp flatten(value), do: flatten(value, [])

  defp flatten(value, prefix) when is_map(value) do
    value
    |> Enum.flat_map(fn {key, nested} ->
      flatten(nested, prefix ++ [to_string(key)])
    end)
  end

  defp flatten(value, prefix) do
    [{Enum.join(prefix, "."), inspect_scalar(value)}]
  end

  defp inspect_scalar(value) when is_binary(value), do: value
  defp inspect_scalar(value), do: inspect(value)

  defp dump_yaml(data) when is_map(data) do
    data
    |> dump_map(0)
    |> IO.iodata_to_binary()
  end

  defp dump_map(map, indent) do
    map
    |> Enum.sort_by(fn {key, _value} -> to_string(key) end)
    |> Enum.flat_map(fn {key, value} ->
      dump_entry(to_string(key), value, indent)
    end)
  end

  defp dump_entry(key, value, indent) when is_map(value) do
    [indentation(indent), key, ":\n", dump_map(value, indent + 2)]
  end

  defp dump_entry(key, value, indent) when is_list(value) do
    [
      indentation(indent),
      key,
      ":\n",
      Enum.map(value, fn item -> dump_list_item(item, indent + 2) end)
    ]
  end

  defp dump_entry(key, value, indent) do
    [indentation(indent), key, ": ", scalar_yaml(value), "\n"]
  end

  defp dump_list_item(item, indent) when is_list(item) do
    [indentation(indent), "- ", flow_list(item), "\n"]
  end

  defp dump_list_item(item, indent) when is_map(item) do
    [indentation(indent), "-\n", dump_map(item, indent + 2)]
  end

  defp dump_list_item(item, indent) do
    [indentation(indent), "- ", scalar_yaml(item), "\n"]
  end

  defp flow_list(items) do
    inner =
      items
      |> Enum.map_join(", ", &scalar_yaml/1)

    "[#{inner}]"
  end

  defp scalar_yaml(value) when is_binary(value) do
    escaped =
      value
      |> String.replace("\\", "\\\\")
      |> String.replace("\"", "\\\"")

    ~s|"#{escaped}"|
  end

  defp scalar_yaml(value), do: to_string(value)

  defp indentation(count), do: String.duplicate(" ", count)
end
