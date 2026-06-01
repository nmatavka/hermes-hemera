defmodule HemeraHaikuRollout.SubmissionTest do
  use ExUnit.Case, async: false

  alias HemeraHaikuRollout.{Submission, Workspace}
  alias HemeraHaikuRollout.TestSupport.{FakeExecutor, WorkspaceFactory}

  test "submission readiness aggregates missing local values with actionable guidance" do
    root = Path.join(System.tmp_dir!(), "hemera-haiku-rollout-root-#{System.unique_integer([:positive])}")
    File.mkdir_p!(root)
    local_config_path = Path.join(root, "missing.yml")
    workspace =
      Workspace.load!(
        root,
        manifest: HemeraHaikuRollout.default_manifest_path(),
        local_config: local_config_path
      )

    {:ok, agent} = FakeExecutor.start_link([%{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})}])

    resolution = Submission.resolve(workspace, executor: {FakeExecutor, agent})

    assert Enum.map(resolution.missing, & &1.key) == [
             "haikuports.checkout_path",
             "haikuports.fork_url"
           ]

    assert_raise ArgumentError, fn ->
      Submission.validate_ready!(resolution)
    end
  end

  test "submission resolution reuses existing checkout remotes before requiring local overrides" do
    checkout = Path.join(System.tmp_dir!(), "haikuports-checkout-resolution-#{System.unique_integer([:positive])}")
    File.mkdir_p!(Path.join(checkout, ".git"))

    config_path =
      WorkspaceFactory.local_config_file!(
        """
        haikuports:
          checkout_path: "#{checkout}"
        """
      )

    workspace = WorkspaceFactory.workspace!(local_config: config_path)

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["api", "user"], stdout: ~s({"login":"ignored-login"})},
        %{program: "git", args: ["remote", "get-url", "origin"], stdout: "git@github.com:nick/haikuports.git\n"}
      ])

    resolution = Submission.resolve(workspace, executor: {FakeExecutor, agent})

    assert resolution.workspace.manifest.haikuports_fork_url == "git@github.com:nick/haikuports.git"
    assert resolution.workspace.manifest.haikuports_fork_owner == "nick"
    assert resolution.missing == []
  end
end
