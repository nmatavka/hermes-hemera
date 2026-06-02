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

  test "renders a template-safe guided gh pr create command" do
    context = context("1.0")
    command = GitHub.pull_request_command(context)

    assert command =~ "gh pr create"
    assert command =~ "--editor"
    assert command =~ "--repo 'haikuports/haikuports'"
    assert command =~ "--head 'nick:hemera-1.0'"
    refute command =~ "--title"
    refute command =~ "--body"
    refute command =~ "--body-file"
    refute command =~ "--template"
    refute command =~ "--fill"
  end

  test "renders PR handoff markdown with suggested title, notes, and template warning" do
    context = context("1.0")
    command = GitHub.pull_request_command(context)
    watch_command = "scripts/release_haiku_rollout.sh watch hemera-1.0"
    handoff = GitHub.pull_request_handoff_markdown(context, command, watch_command)

    assert handoff =~ "Suggested PR title:"
    assert handoff =~ context.suggested_pr_title
    assert handoff =~ context.pr_notes
    assert handoff =~ GitHub.pull_request_template_warning()
    assert handoff =~ watch_command
  end

  test "finds an existing pull request by branch and fork owner" do
    context = context("1.0")

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
          stdout:
            ~s([{"number":11,"url":"https://example.test/pr/11","headRefName":"#{context.haikuports_branch}","headRepositoryOwner":{"login":"someone-else"}},{"number":17,"url":"https://example.test/pr/17","headRefName":"#{context.haikuports_branch}","headRepositoryOwner":{"login":"nick"}}])}
      ])

    pr = GitHub.find_pull_request({FakeExecutor, agent}, context)

    assert pr["number"] == 17
    assert pr["url"] == "https://example.test/pr/17"
  end
end
