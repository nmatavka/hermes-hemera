defmodule LinuxRollout.Yaml do
  def load!(path) do
    Application.ensure_all_started(:yaml_elixir)

    case YamlElixir.read_from_file(path, merge_anchors: true) do
      {:ok, data} ->
        data || %{}

      {:error, error} ->
        raise "failed to parse YAML #{path}: #{Exception.message(error)}"
    end
  end

  def dump!(path, data) do
    File.mkdir_p!(Path.dirname(path))
    File.write!(path, Ymlr.document!(data, sort_maps: true))
    path
  end
end
