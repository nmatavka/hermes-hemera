defmodule LinuxRollout.ReleasePrep do
  alias LinuxRollout.{State, Util, Workspace}

  def run!(workspace, opts \\ []) do
    unless opts[:skip_gradle] do
      Util.witness("prepare: running Gradle release inputs")
      run_gradle!(workspace)
      Util.witness("prepare: Gradle release inputs completed")
    else
      Util.witness("prepare: skipping Gradle and reusing existing artifacts")
    end

    Util.witness("prepare: verifying canonical release artifacts")

    ensure_artifact!(
      Workspace.artifact_path(workspace, "jar", workspace.tokens["wire_share_jar_name"])
    )

    ensure_artifact!(
      Workspace.artifact_path(workspace, "release", workspace.tokens["source_tarball_name"])
    )

    ensure_artifact!(
      Workspace.artifact_path(workspace, "release", workspace.tokens["checksums_name"])
    )

    ensure_artifact!(
      Workspace.artifact_path(workspace, "release", workspace.tokens["dependency_inventory_name"])
    )

    Util.witness("prepare: release artifacts verified")

    State.put_step!(workspace, :prepare, %{
      status: "completed",
      dry_run: !!opts[:dry_run],
      skip_gradle: !!opts[:skip_gradle]
    })
  end

  defp run_gradle!(workspace) do
    args = ["packageLinuxReleaseInputs", "--console=plain"]
    shell = System.find_executable("sh") || "/bin/sh"
    gradlew = Path.join(workspace.root, "gradlew")

    case System.cmd(shell, [gradlew | args],
           cd: workspace.root,
           into: IO.stream(:stdio, :line),
           stderr_to_stdout: true
         ) do
      {_output, 0} -> :ok
      {_output, status} -> raise "gradle release prep failed with exit #{status}"
    end
  end

  defp ensure_artifact!(path) do
    if File.exists?(path) do
      :ok
    else
      raise "expected release artifact at #{path}"
    end
  end
end
