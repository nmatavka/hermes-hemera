defmodule HemeraHaikuRollout.State do
  alias HemeraHaikuRollout.Util

  def load(context) do
    case Util.read_json(context.state_path) do
      {:ok, data} -> data
      {:error, _reason} -> base_state(context)
    end
  end

  def put_step!(context, step, attrs) do
    state = load(context)

    updated =
      put_in(
        state,
        ["steps", to_string(step)],
        Map.merge(%{"updated_at" => Util.timestamp_utc()}, Util.stringify_keys(attrs))
      )

    write!(context, updated)
    updated
  end

  def step(state, step) when is_atom(step), do: step(state, Atom.to_string(step))

  def step(state, step) when is_binary(step) do
    get_in(state, ["steps", step]) || %{}
  end

  def completed?(state, step) do
    step(state, step)["status"] == "completed"
  end

  def write!(context, data) do
    Util.write_json!(context.state_path, data)
  end

  def last_completed_step(state, ordered_steps) do
    ordered_steps
    |> Enum.filter(fn {step, _label} -> completed?(state, step) end)
    |> List.last()
  end

  def next_pending_step(state, ordered_steps) do
    Enum.find(ordered_steps, fn {step, _label} ->
      not completed?(state, step)
    end)
  end

  def last_failed_step(state, ordered_steps) do
    ordered_steps
    |> Enum.filter(fn {step, _label} -> step(state, step)["status"] == "failed" end)
    |> List.last()
  end

  defp base_state(context) do
    %{
      "version" => context.version,
      "updated_at" => Util.timestamp_utc(),
      "steps" => %{}
    }
  end
end
