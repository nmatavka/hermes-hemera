defmodule HemeraHaikuRollout.WatchTest do
  use ExUnit.Case, async: false

  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.TestSupport.{FakeExecutor, WorkspaceFactory}
  alias HemeraHaikuRollout.Watch

  test "watch resolves a branch and runs pr checks" do
    workspace = WorkspaceFactory.workspace!()
    context = ReleaseContext.build(workspace, "1.0.0-rc1")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh",
          args: ["pr", "list", "--repo", context.haikuports_repo_slug, "--head", context.haikuports_branch, "--json", "number"],
          stdout: ~s([{"number":42}])},
        %{program: "gh",
          args: ["pr", "checks", "42", "--repo", context.haikuports_repo_slug, "--watch"],
          stdout: "checks ok\n"},
        %{program: "gh",
          args: ["pr", "view", "42", "--repo", context.haikuports_repo_slug, "--json", "url,state"],
          stdout: ~s({"url":"https://example.test/pr/42","state":"OPEN"})}
      ])

    Watch.run(workspace, context.haikuports_branch, executor: {FakeExecutor, agent}, version: "1.0.0-rc1")

    assert length(FakeExecutor.commands(agent)) == 3
  end

  test "watch tolerates prs with no reported checks yet" do
    workspace = WorkspaceFactory.workspace!()
    context = ReleaseContext.build(workspace, "1.0.0-rc1")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh",
          args: ["pr", "list", "--repo", context.haikuports_repo_slug, "--head", context.haikuports_branch, "--json", "number"],
          stdout: ~s([{"number":42}])},
        %{program: "gh",
          args: ["pr", "checks", "42", "--repo", context.haikuports_repo_slug, "--watch"],
          status: 1,
          stdout: "no checks reported on the '#{context.haikuports_branch}' branch\n"},
        %{program: "gh",
          args: ["pr", "view", "42", "--repo", context.haikuports_repo_slug, "--json", "url,state"],
          stdout: ~s({"url":"https://example.test/pr/42","state":"OPEN"})}
      ])

    Watch.run(workspace, context.haikuports_branch, executor: {FakeExecutor, agent}, version: "1.0.0-rc1")

    assert length(FakeExecutor.commands(agent)) == 3
  end
end
