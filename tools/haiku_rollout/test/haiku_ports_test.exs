defmodule HemeraHaikuRollout.HaikuPortsTest do
  use ExUnit.Case, async: false

  alias HemeraHaikuRollout.HaikuPorts
  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.TestSupport.{FakeExecutor, WorkspaceFactory}

  test "pushes and sets upstream when the remote rollout branch is missing" do
    context = WorkspaceFactory.workspace!() |> ReleaseContext.build("1.0")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "git", args: ["rev-parse", context.haikuports_branch], stdout: "localsha\n"},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], status: 1, stdout: ""},
        %{program: "git", args: ["fetch", "--prune", "origin"], stdout: ""},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], status: 1, stdout: ""},
        %{program: "git", args: ["push", "--set-upstream", "origin", context.haikuports_branch], stdout: ""}
      ])

    assert {:ok, result} = HaikuPorts.push_branch!({FakeExecutor, agent}, context)
    assert result.push_status == "pushed"
    assert result.local_sha == "localsha"
    assert result.remote_sha == "localsha"
  end

  test "treats a matching remote rollout branch as already pushed" do
    context = WorkspaceFactory.workspace!() |> ReleaseContext.build("1.0")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "git", args: ["rev-parse", context.haikuports_branch], stdout: "same123\n"},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], stdout: "stale456\n"},
        %{program: "git", args: ["fetch", "--prune", "origin"], stdout: ""},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], stdout: "same123\n"}
      ])

    assert {:ok, result} = HaikuPorts.push_branch!({FakeExecutor, agent}, context)
    assert result.push_status == "already_pushed"
    assert result.local_sha == "same123"
    assert result.tracked_remote_sha == "stale456"
    assert result.remote_sha == "same123"
  end

  test "reports divergence when the remote rollout branch points at a different commit" do
    context = WorkspaceFactory.workspace!() |> ReleaseContext.build("1.0")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "git", args: ["rev-parse", context.haikuports_branch], stdout: "local123\n"},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], stdout: "tracked456\n"},
        %{program: "git", args: ["fetch", "--prune", "origin"], stdout: ""},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], stdout: "remote789\n"}
      ])

    assert {:diverged, result} = HaikuPorts.push_branch!({FakeExecutor, agent}, context)
    assert result.push_status == "diverged"
    assert result.local_sha == "local123"
    assert result.tracked_remote_sha == "tracked456"
    assert result.remote_sha == "remote789"
  end
end
