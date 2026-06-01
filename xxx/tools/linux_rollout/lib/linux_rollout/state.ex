defmodule LinuxRollout.State do
  alias LinuxRollout.Util

  def load(workspace) do
    case Util.read_json(workspace.state_path) do
      {:ok, data} -> data
      {:error, _reason} -> base_state(workspace)
    end
  end

  def put_step!(workspace, step, attrs) do
    state = load(workspace)

    updated =
      put_in(
        state,
        ["steps", to_string(step)],
        Map.merge(%{"updated_at" => Util.timestamp_utc()}, stringify_keys(attrs))
      )

    write!(workspace, updated)
    updated
  end

  def put_target!(workspace, target, attrs) do
    state = load(workspace)

    updated =
      put_in(
        state,
        ["targets", target],
        Map.merge(%{"updated_at" => Util.timestamp_utc()}, stringify_keys(attrs))
      )

    write!(workspace, updated)
    updated
  end

  def write!(workspace, data) do
    Util.write_json!(workspace.state_path, data)
  end

  defp base_state(workspace) do
    %{
      "version" => workspace.version,
      "updated_at" => Util.timestamp_utc(),
      "steps" => %{},
      "targets" => %{}
    }
  end

  defp stringify_keys(map) do
    Enum.into(map, %{}, fn {key, value} -> {to_string(key), stringify_value(value)} end)
  end

  defp stringify_value(value) when is_map(value), do: stringify_keys(value)
  defp stringify_value(value) when is_list(value), do: Enum.map(value, &stringify_value/1)
  defp stringify_value(value), do: value
end
