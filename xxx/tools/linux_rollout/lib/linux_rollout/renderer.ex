defmodule LinuxRollout.Renderer do
  import Bitwise

  alias LinuxRollout.{State, Util, Workspace}

  @ignored_files MapSet.new(["release_manifest.yaml", "targets.yaml"])

  def render!(workspace) do
    target_root = Workspace.rendered_packaging_dir(workspace)
    File.rm_rf!(workspace.rendered_root)
    File.mkdir_p!(target_root)

    source_root = workspace.packaging_dir
    source_paths = Util.list_files_recursively(source_root)

    Util.witness(
      "render: materializing #{count_renderable_files(source_root, source_paths)} packaging file(s)"
    )

    for source_path <- source_paths do
      relative_path = Path.relative_to(source_path, source_root)

      unless MapSet.member?(@ignored_files, relative_path) do
        rendered_relative_path = Util.render_string(relative_path, workspace.tokens)
        destination_path = Path.join(target_root, rendered_relative_path)
        copy_rendered_file!(source_path, destination_path, workspace.tokens)
      end
    end

    State.put_step!(workspace, :render, %{status: "completed", rendered_root: target_root})
    Util.witness("render: completed into #{target_root}")
    target_root
  end

  defp copy_rendered_file!(source_path, destination_path, tokens) do
    Util.ensure_directory!(destination_path)

    if Util.text_file?(source_path) do
      contents = File.read!(source_path) |> Util.render_string(tokens)
      File.write!(destination_path, contents)
    else
      File.cp!(source_path, destination_path)
    end

    mode = File.stat!(source_path).mode &&& 0o777
    File.chmod!(destination_path, mode)
  end

  defp count_renderable_files(source_root, source_paths) do
    Enum.count(source_paths, fn source_path ->
      relative_path = Path.relative_to(source_path, source_root)
      not MapSet.member?(@ignored_files, relative_path)
    end)
  end
end
