defmodule HemeraHaikuRollout.Recipe do
  alias HemeraHaikuRollout.Util

  def render!(context, sha256) do
    context.recipe_template_path
    |> File.read!()
    |> String.replace("@HEMERA_REPO_SLUG@", context.repo_slug)
    |> String.replace("@HEMERA_SOURCE_URI@", context.source_uri)
    |> String.replace("@HEMERA_SOURCE_SHA256@", sha256)
    |> String.replace("@HEMERA_HAIKU_PACKAGE_VERSION@", context.package_version)
  end

  def validate_template!(workspace) do
    workspace
    |> then(&HemeraHaikuRollout.recipe_template_path(&1.root))
    |> File.read!()
    |> validate_text!(workspace.root)
  end

  def validate_rendered!(workspace, rendered_recipe) when is_binary(rendered_recipe) do
    validate_text!(rendered_recipe, workspace.root)
  end

  def checksum!(path) do
    path
    |> File.read!()
    |> then(&:crypto.hash(:sha256, &1))
    |> Base.encode16(case: :lower)
  end

  defp validate_text!(recipe_text, repo_root) do
    order = load_field_order!(repo_root)

    positions =
      Enum.map(order, fn field ->
        {field, field_position(recipe_text, field)}
      end)

    case Enum.find(positions, fn {_field, position} -> position == nil end) do
      {field, nil} ->
        raise ArgumentError, "Haiku recipe is missing required field or section #{field}"

      nil ->
        ensure_order!(positions)
        :ok
    end
  end

  defp ensure_order!(positions) do
    positions
    |> Enum.chunk_every(2, 1, :discard)
    |> Enum.each(fn [{left_field, left_pos}, {right_field, right_pos}] ->
      if left_pos > right_pos do
        raise ArgumentError,
              "Haiku recipe field order is invalid: #{left_field} must appear before #{right_field}"
      end
    end)
  end

  defp field_position(recipe_text, field) do
    pattern =
      case field do
        "BUILD" -> ~r/^BUILD\(\)$/m
        "INSTALL" -> ~r/^INSTALL\(\)$/m
        _ -> ~r/^#{Regex.escape(field)}=/m
      end

    case Regex.run(pattern, recipe_text, return: :index) do
      [{position, _length}] -> position
      _ -> nil
    end
  end

  defp load_field_order!(repo_root) do
    repo_root
    |> Path.join("packaging/haiku/haikuporter_field_order.txt")
    |> File.read!()
    |> String.split("\n")
    |> Enum.map(&String.trim/1)
    |> Enum.reject(&(&1 == "" || String.starts_with?(&1, "#")))
    |> then(fn fields ->
      if Util.blank?(fields) do
        raise ArgumentError, "Haiku recipe field order file is empty"
      else
        fields
      end
    end)
  end
end
