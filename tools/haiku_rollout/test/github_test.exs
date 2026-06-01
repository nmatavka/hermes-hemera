defmodule HemeraHaikuRollout.GitHubTest do
  use ExUnit.Case, async: false

  alias HemeraHaikuRollout.GitHub
  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.TestSupport.{FakeExecutor, WorkspaceFactory}

  defp context(version \\ "1.0.0-rc1") do
    workspace = WorkspaceFactory.workspace!()
    ReleaseContext.build(workspace, version)
  end

  test "updates an existing release instead of recreating it" do
    context = context()

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["release", "view", context.tag, "--repo", context.repo_slug], stdout: "release\n"},
        %{program: "gh",
          args: ["release", "edit", context.tag, "--repo", context.repo_slug, "--title", context.release_title, "--notes-file", context.release_notes_path],
          stdout: ""},
        %{program: "gh",
          args: ["release", "upload", context.tag, context.artifact_path, "--clobber", "--repo", context.repo_slug],
          stdout: ""},
        %{program: "gh",
          args: ["api", "repos/#{context.repo_slug}/releases/tags/#{URI.encode(context.tag, &URI.char_unreserved?/1)}"],
          stdout: ~s({"html_url":"https://example.test/release","assets":[{"name":"#{context.asset_name}","browser_download_url":"https://example.test/asset.tar.gz"}]})}
      ])

    result = GitHub.ensure_release!({FakeExecutor, agent}, context, "deadbeef")

    assert result.url == "https://example.test/release"
    assert result.asset_url == "https://example.test/asset.tar.gz"
  end

  test "resolves the current GitHub login through gh api user" do
    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})}
      ])

    assert GitHub.current_login({FakeExecutor, agent}) == "nick"
  end

  test "renders a guided gh pr create command" do
    context = context("1.0")
    command = GitHub.pull_request_command(context)

    assert command =~ "gh pr create"
    assert command =~ "--repo 'haikuports/haikuports'"
    assert command =~ "--head 'nick:hemera-1.0'"
    assert command =~ "--title 'hemera: add 1.0-1'"
  end

  test "reuses and edits an existing pull request" do
    context = context()

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
          stdout: ~s([{"number":17,"url":"https://example.test/pr/17","headRefName":"#{context.haikuports_branch}","headRepositoryOwner":{"login":"nick"}}])},
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

    result = GitHub.ensure_pull_request!({FakeExecutor, agent}, context)
    assert result.url == "https://example.test/pr/17"
    assert result.number == 17
  end

  test "recovers when gh pr create reports an existing pull request" do
    context = context()

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
            context.haikuports_target_branch,
            "--head",
            "#{context.haikuports_fork_owner}:#{context.haikuports_branch}",
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
          stdout: ~s([{"number":17,"url":"https://example.test/pr/17","headRefName":"#{context.haikuports_branch}","headRepositoryOwner":{"login":"nick"}}])},
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

    result = GitHub.ensure_pull_request!({FakeExecutor, agent}, context)
    assert result.url == "https://example.test/pr/17"
    assert result.number == 17
  end
end
