defmodule HemeraHaikuRollout.StatusTest do
  use ExUnit.Case, async: false

  import ExUnit.CaptureIO

  alias HemeraHaikuRollout.{ReleaseContext, State, Status}
  alias HemeraHaikuRollout.TestSupport.{FakeExecutor, WorkspaceFactory}

  test "status reports missing local keys and the next PR command before a PR exists" do
    root = Path.join(System.tmp_dir!(), "hemera-haiku-rollout-status-root-#{System.unique_integer([:positive])}")
    File.mkdir_p!(root)
    local_config_path =
      Path.join(
        System.tmp_dir!(),
        "hemera-haiku-rollout-status-missing-#{System.unique_integer([:positive])}.yml"
      )

    workspace =
      HemeraHaikuRollout.Workspace.load!(
        root,
        manifest: HemeraHaikuRollout.default_manifest_path(),
        local_config: local_config_path
      )
    version = "1.0"
    context = ReleaseContext.build(workspace, version)
    File.rm_rf!(context.work_dir)
    State.put_step!(context, :pr_command_prepared, %{
      status: "completed",
      pr_create_command: "gh pr create --editor --repo demo",
      watch_command: "scripts/release_haiku_rollout.sh watch hemera-1.0",
      suggested_pr_title: "hemera: add 1.0-1",
      handoff_path: "/tmp/pr_handoff.md",
      template_warning: "Confirm the HaikuPorts contributor checklist template appears in the editor."
    })

    {:ok, agent} = FakeExecutor.start_link([%{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})}])

    output =
      capture_io(fn ->
        Status.run(workspace, version, executor: {FakeExecutor, agent})
      end)

    assert output =~ "Missing local keys:"
    assert output =~ "haikuports.fork_url"
    assert output =~ "Suggested PR title: hemera: add 1.0-1"
    assert output =~ "PR handoff file: /tmp/pr_handoff.md"
    assert output =~ "PR handoff warning: Confirm the HaikuPorts contributor checklist template appears in the editor."
    assert output =~ "Next command:"
    assert output =~ "gh pr create --editor --repo demo"
  end

  test "status reports workspace paths, PR URL, and watch command once a PR exists" do
    workspace = WorkspaceFactory.workspace!()
    version = "1.0-status-#{System.unique_integer([:positive])}"
    context = ReleaseContext.build(workspace, version)
    File.rm_rf!(context.work_dir)

    State.put_step!(context, :repo_version_validated, %{status: "completed"})
    State.put_step!(context, :pr_discovered, %{status: "completed", pr_url: "https://example.test/pr/17", watch_command: "gh pr checks 17 --repo 'haikuports/haikuports' --watch"})

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})},
        %{program: "git", args: ["remote", "get-url", "origin"], stdout: "git@github.com:nick/haikuports.git\n"}
      ])

    output =
      capture_io(fn ->
        Status.run(workspace, version, executor: {FakeExecutor, agent})
      end)

    assert output =~ "Repo root: #{workspace.root}"
    assert output =~ "Version: #{version}"
    assert output =~ "PR: https://example.test/pr/17"
    assert output =~ "Last completed step:"
    assert output =~ "Next pending step:"
    assert output =~ "gh pr checks 17 --repo 'haikuports/haikuports' --watch"
  end

  test "status reports branch divergence details and recovery commands" do
    workspace = WorkspaceFactory.workspace!()
    version = "1.0"
    context = ReleaseContext.build(workspace, version)
    File.rm_rf!(context.work_dir)

    State.put_step!(context, :branch_pushed, %{
      status: "failed",
      branch: "hemera-1.0",
      local_sha: "local123",
      tracked_remote_sha: "tracked456",
      remote_sha: "remote789",
      error: "HaikuPorts branch hemera-1.0 already exists on origin at remote789, but the local branch is local123.",
      recovery_commands: [
        "git -C '/tmp/haikuports' log --oneline origin/hemera-1.0..hemera-1.0",
        "git -C '/tmp/haikuports' push origin :hemera-1.0"
      ]
    })
    State.put_step!(context, :pr_discovered, %{status: "completed", pr_url: "https://example.test/pr/17"})

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})},
        %{program: "git", args: ["remote", "get-url", "origin"], stdout: "git@github.com:nick/haikuports.git\n"}
      ])

    output =
      capture_io(fn ->
        Status.run(workspace, version, executor: {FakeExecutor, agent})
      end)

    assert output =~ "Branch push status: failed"
    assert output =~ "Branch local SHA: local123"
    assert output =~ "Branch tracked origin SHA: tracked456"
    assert output =~ "Branch live origin SHA: remote789"
    assert output =~ "Branch push safety: blocked by remote divergence"
    assert output =~ "Last failed step: branch pushed"
    assert output =~ "git -C '/tmp/haikuports' push origin :hemera-1.0"
    refute output =~ "PR: https://example.test/pr/17"
  end
end
