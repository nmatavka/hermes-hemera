defmodule HemeraHaikuRollout.GitHubTest do
  use ExUnit.Case, async: false

  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.GitHub
  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.TestSupport.FakeExecutor

  defp config do
    {:ok, config} =
      Config.from_map(
        %{
          "github" => %{
            "repo_owner" => "nick",
            "repo_name" => "hermes-hemera"
          },
          "haikuports" => %{
            "upstream_url" => "https://github.com/haikuports/haikuports.git",
            "fork_url" => "git@github.com:nick/haikuports.git",
            "fork_owner" => "nick",
            "checkout_path" => "vendor/haikuports"
          }
        },
        "memory"
      )

    config
  end

  test "updates an existing release instead of recreating it" do
    context = ReleaseContext.build(config(), "1.0.0-rc1")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["release", "view", context.tag, "--repo", context.repo_slug], stdout: "release\n"},
        %{program: "gh",
          args: ["release", "edit", context.tag, "--repo", context.repo_slug, "--title", context.release_title, "--notes-file", context.release_notes_path],
          stdout: ""},
        %{program: "gh",
          args: ["release", "upload", context.tag, context.artifact_path, "--clobber", "--repo", context.repo_slug],
          stdout: ""}
      ])

    GitHub.ensure_release!({FakeExecutor, agent}, context, "deadbeef")

    assert Enum.map(FakeExecutor.commands(agent), & &1.program) == ["gh", "gh", "gh"]
  end

  test "reuses and edits an existing pull request" do
    config = config()
    context = ReleaseContext.build(config, "1.0.0-rc1")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh",
          args: [
            "pr",
            "list",
            "--repo",
            context.haikuports_repo_slug,
            "--head",
            context.haikuports_branch,
            "--json",
            "number,url,headRefName,headRepositoryOwner"
          ],
          stdout: ~s([{"number":17,"url":"https://example.test/pr/17","headRefName":"hemera-1.0.0~rc1","headRepositoryOwner":{"login":"nick"}}])},
        %{program: "gh",
          args: [
            "pr",
            "edit",
            "17",
            "--repo",
            context.haikuports_repo_slug,
            "--title",
            context.pr_title,
            "--body",
            context.pr_body
          ],
          stdout: ""}
      ])

    assert GitHub.ensure_pull_request!({FakeExecutor, agent}, config, context) == "https://example.test/pr/17"
  end

  test "recovers when gh pr create reports an existing pull request" do
    config = config()
    context = ReleaseContext.build(config, "1.0.0-rc1")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh",
          args: [
            "pr",
            "list",
            "--repo",
            context.haikuports_repo_slug,
            "--head",
            context.haikuports_branch,
            "--json",
            "number,url,headRefName,headRepositoryOwner"
          ],
          stdout: "[]"},
        %{program: "gh",
          args: [
            "pr",
            "create",
            "--repo",
            context.haikuports_repo_slug,
            "--base",
            config.haikuports_target_branch,
            "--head",
            "#{config.haikuports_fork_owner}:#{context.haikuports_branch}",
            "--title",
            context.pr_title,
            "--body",
            context.pr_body
          ],
          status: 1,
          stdout: "a pull request for branch \"#{context.haikuports_branch}\" already exists\n"},
        %{program: "gh",
          args: [
            "pr",
            "list",
            "--repo",
            context.haikuports_repo_slug,
            "--head",
            context.haikuports_branch,
            "--json",
            "number,url,headRefName,headRepositoryOwner"
          ],
          stdout: ~s([{"number":17,"url":"https://example.test/pr/17","headRefName":"hemera-1.0.0~rc1","headRepositoryOwner":{"login":"nick"}}])},
        %{program: "gh",
          args: [
            "pr",
            "edit",
            "17",
            "--repo",
            context.haikuports_repo_slug,
            "--title",
            context.pr_title,
            "--body",
            context.pr_body
          ],
          stdout: ""}
      ])

    assert GitHub.ensure_pull_request!({FakeExecutor, agent}, config, context) == "https://example.test/pr/17"
  end
end
