defmodule LinuxRollout.Validator do
  alias LinuxRollout.{Renderer, State, Util, Workspace}

  @unresolved_pattern ~r/@[a-z0-9_]+@/
  @stale_path_markers [
    "org.teamhermes.WireShare.desktop",
    "org.teamhermes.WireShare.metainfo.xml",
    "org.teamhermes.WireShare.XferDone",
    "org.teamhermes.WireShare.svg"
  ]

  def run!(workspace) do
    Util.witness("validate: rendering packaging tree for validation")
    rendered_root = Renderer.render!(workspace)
    enabled_targets = Workspace.enabled_targets(workspace)
    Util.witness("validate: checking #{map_size(enabled_targets)} enabled target(s)")

    errors =
      []
      |> validate_required_files(enabled_targets, rendered_root)
      |> validate_unresolved_tokens(rendered_root)
      |> validate_stale_markers(rendered_root)

    if errors == [] do
      State.put_step!(workspace, :validate, %{status: "completed"})
      Util.witness("validate: completed without errors")
      :ok
    else
      State.put_step!(workspace, :validate, %{status: "failed", errors: errors})
      raise Enum.join(errors, "\n")
    end
  end

  defp validate_required_files(errors, enabled_targets, rendered_root) do
    Enum.reduce(enabled_targets, errors, fn {_name, target}, acc ->
      required_files =
        target
        |> Workspace.target_units()
        |> Enum.flat_map(&Map.get(&1, "required_files", []))
        |> Enum.uniq()

      missing =
        required_files
        |> Enum.map(&rendered_path(rendered_root, &1))
        |> Enum.reject(&File.exists?/1)

      if missing == [] do
        acc
      else
        acc ++ ["#{target["name"]}: missing required rendered files: #{Enum.join(missing, ", ")}"]
      end
    end)
  end

  defp validate_unresolved_tokens(errors, rendered_root) do
    unresolved =
      rendered_root
      |> Util.list_files_recursively()
      |> Enum.filter(&Util.text_file?/1)
      |> Enum.filter(fn path ->
        Regex.match?(@unresolved_pattern, File.read!(path))
      end)

    if unresolved == [] do
      errors
    else
      errors ++ ["unresolved template tokens remain in: #{Enum.join(unresolved, ", ")}"]
    end
  end

  defp validate_stale_markers(errors, rendered_root) do
    stale =
      rendered_root
      |> Util.list_files_recursively()
      |> Enum.filter(&Util.text_file?/1)
      |> Enum.filter(fn path ->
        contents = File.read!(path)
        Enum.any?(@stale_path_markers, &String.contains?(contents, &1))
      end)

    if stale == [] do
      errors
    else
      errors ++ ["stale pre-rename Linux app-id paths remain in: #{Enum.join(stale, ", ")}"]
    end
  end

  defp rendered_path(rendered_root, relative_path) do
    normalized = String.replace_prefix(relative_path, "packaging/", "")
    Path.join(rendered_root, normalized)
  end
end
