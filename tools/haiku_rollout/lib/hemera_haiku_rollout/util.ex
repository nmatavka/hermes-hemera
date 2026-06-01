defmodule HemeraHaikuRollout.Util do
  @template_pattern ~r/<([a-z0-9_]+)>/

  def render_template(template, replacements) when is_binary(template) and is_map(replacements) do
    Regex.replace(@template_pattern, template, fn _, key ->
      case Map.get(replacements, key) do
        nil -> "<#{key}>"
        value when is_binary(value) -> value
        value -> to_string(value)
      end
    end)
  end

  def timestamp_utc do
    DateTime.utc_now() |> DateTime.truncate(:second) |> DateTime.to_iso8601()
  end

  def read_json(path) do
    with {:ok, contents} <- File.read(path),
         {:ok, decoded} <- Jason.decode(contents) do
      {:ok, decoded}
    end
  end

  def write_json!(path, data) do
    path |> Path.dirname() |> File.mkdir_p!()
    File.write!(path, Jason.encode_to_iodata!(data, pretty: true))
  end

  def blank?(value) when value in [nil, "", []], do: true
  def blank?(value) when is_map(value), do: map_size(value) == 0
  def blank?(_value), do: false

  def deep_merge(left, right) when is_map(left) and is_map(right) do
    Map.merge(left, right, fn _key, left_value, right_value ->
      deep_merge(left_value, right_value)
    end)
  end

  def deep_merge(_left, right), do: right

  def path_get(data, segments) when is_list(segments) do
    Enum.reduce_while(segments, data, fn segment, current ->
      case current do
        %{} ->
          if Map.has_key?(current, segment) do
            {:cont, Map.get(current, segment)}
          else
            {:halt, nil}
          end

        _ ->
          {:halt, nil}
      end
    end)
  end

  def path_put(data, [segment], value) do
    Map.put(data, segment, value)
  end

  def path_put(data, [segment | rest], value) do
    nested =
      data
      |> Map.get(segment, %{})
      |> case do
        map when is_map(map) -> map
        _ -> %{}
      end

    Map.put(data, segment, path_put(nested, rest, value))
  end

  def path_delete(data, [segment]) do
    Map.delete(data, segment)
  end

  def path_delete(data, [segment | rest]) do
    case Map.get(data, segment) do
      nested when is_map(nested) ->
        updated_nested = path_delete(nested, rest)

        if blank?(updated_nested) do
          Map.delete(data, segment)
        else
          Map.put(data, segment, updated_nested)
        end

      _ ->
        data
    end
  end

  def stringify_keys(map) when is_map(map) do
    Enum.into(map, %{}, fn {key, value} -> {to_string(key), stringify_value(value)} end)
  end

  def stringify_keys(other), do: other

  def shell_escape(argument) when is_binary(argument) do
    "'" <> String.replace(argument, "'", "'\"'\"'") <> "'"
  end

  defp stringify_value(value) when is_map(value), do: stringify_keys(value)
  defp stringify_value(value) when is_list(value), do: Enum.map(value, &stringify_value/1)
  defp stringify_value(value), do: value
end
