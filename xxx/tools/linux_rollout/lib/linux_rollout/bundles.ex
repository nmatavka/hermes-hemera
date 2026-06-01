defmodule LinuxRollout.Bundles do
  alias LinuxRollout.{Util, Workspace}

  def create!(workspace, target) do
    bundle_root = Path.join(workspace.bundle_root, target["name"])
    source_root = Path.join(bundle_root, "source")
    Util.witness("bundle: assembling #{target["name"]}")
    File.rm_rf!(bundle_root)
    File.mkdir_p!(source_root)

    rendered_root = Workspace.rendered_packaging_dir(workspace)

    for relative <- Map.get(target, "submission_paths", []) do
      normalized = String.replace_prefix(relative, "packaging/", "")

      copy_submission_path!(
        Path.join(rendered_root, normalized),
        Path.join(source_root, relative)
      )
    end

    summary_path = Path.join(bundle_root, "SUMMARY.md")
    summary = build_summary(workspace, target)
    Util.ensure_directory!(summary_path)
    File.write!(summary_path, summary)

    %{bundle_root: bundle_root, source_root: source_root, summary_path: summary_path}
  end

  defp copy_submission_path!(source, destination) do
    cond do
      File.regular?(source) ->
        Util.ensure_directory!(destination)
        File.cp!(source, destination)

      File.dir?(source) ->
        File.mkdir_p!(destination)

        for child <- File.ls!(source) do
          copy_submission_path!(Path.join(source, child), Path.join(destination, child))
        end

      true ->
        raise "cannot bundle missing submission path #{source}"
    end
  end

  defp build_summary(workspace, target) do
    units =
      target
      |> Workspace.target_units()
      |> Enum.map(&"  - `#{&1["name"]}`")

    [
      "# #{target["name"]} submission bundle",
      "",
      "- Driver: `#{target["driver"]}`",
      "- Submission mode: `#{target["submission_mode"]}`",
      "- Headless capability: `#{target["headless_capability"]}`",
      "- Destination: #{get_in(target, ["destination", "submission_url"]) || get_in(target, ["destination", "url"]) || "n/a"}",
      "- Branch: `#{Workspace.branch_name(workspace, target["name"])}`",
      "- Units:",
      units,
      "- Required files:",
      Enum.map(
        target
        |> Workspace.target_units()
        |> Enum.flat_map(&Map.get(&1, "required_files", []))
        |> Enum.uniq(),
        &"  - `#{&1}`"
      ),
      "",
      "Generated: #{Util.timestamp_utc()}"
    ]
    |> List.flatten()
    |> Enum.join("\n")
    |> Kernel.<>("\n")
  end
end
