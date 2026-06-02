defmodule HemeraHaikuRollout.ReleaseTest do
  use ExUnit.Case, async: false

  import ExUnit.CaptureIO

  alias HemeraHaikuRollout.{BranchPushDivergedError, Release, State, Workspace}
  alias HemeraHaikuRollout.TestSupport.{FakeExecutor, WorkspaceFactory}

  test "dry-run plan includes Haiku preflight only on Haiku" do
    workspace = WorkspaceFactory.workspace!()

    haiku_plan = Release.plan(workspace, "1.0.0-rc1", :haiku)
    mac_plan = Release.plan(workspace, "1.0.0-rc1", :macos)

    assert Enum.any?(haiku_plan, &String.starts_with?(&1, "haiku-preflight:"))
    assert Enum.any?(mac_plan, &(&1 == "skip local Haiku preflight on macos"))
    assert Enum.any?(mac_plan, &String.contains?(&1, "print exact gh pr create handoff command"))
  end

  test "release reports repo version mismatch before local HaikuPorts overrides are required" do
    local_config_path =
      Path.join(
        System.tmp_dir!(),
        "hemera-haiku-rollout-missing-#{System.unique_integer([:positive])}.yml"
      )

    workspace = Workspace.load!(WorkspaceFactory.repo_root(), local_config: local_config_path)

    assert_raise ArgumentError,
                 "rollout manifest version 1.0 does not match requested version 7.0",
                 fn ->
                   Release.run(
                     workspace,
                     "7.0",
                     mode: :release,
                     executor: HemeraHaikuRollout.SystemExecutor,
                     host: :macos
                   )
                 end
  end

  test "release prints a guided PR handoff instead of auto-creating or watching a PR" do
    workspace = WorkspaceFactory.workspace!()
    context = HemeraHaikuRollout.ReleaseContext.build(workspace, "1.0")
    File.rm_rf!(context.work_dir)
    File.mkdir_p!(context.artifact_dir)
    File.write!(context.artifact_path, "tarball")

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["auth", "status"], stdout: "ok\n"},
        %{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})},
        %{program: "git", args: ["remote", "get-url", "origin"], status: 2, stdout: ""},
        %{program: "git", args: ["rev-parse", "--verify", "refs/tags/#{context.tag}"], status: 1, stdout: ""},
        %{program: "git", args: ["tag", "-a", context.tag, "-m", context.release_title], stdout: ""},
        %{program: "git", args: ["rev-parse", "#{context.tag}^{commit}"], stdout: "deadbeef\n"},
        %{program: "git",
          args: [
            "archive",
            "--format=tar.gz",
            "--prefix=#{context.archive_prefix}/",
            "-o",
            context.artifact_path,
            context.tag
          ],
          stdout: ""},
        %{program: "gh", args: ["release", "view", context.tag, "--repo", context.repo_slug], status: 1, stdout: ""},
        %{program: "gh",
          args: [
            "release",
            "create",
            context.tag,
            "--repo",
            context.repo_slug,
            "--target",
            "deadbeef",
            "--title",
            context.release_title,
            "--notes-file",
            context.release_notes_path
          ],
          stdout: ""},
        %{program: "gh",
          args: ["release", "upload", context.tag, context.artifact_path, "--clobber", "--repo", context.repo_slug],
          stdout: ""},
        %{program: "gh",
          args: ["api", "repos/#{context.repo_slug}/releases/tags/#{URI.encode(context.tag, &URI.char_unreserved?/1)}"],
          stdout: ~s({"html_url":"https://example.test/release","assets":[{"name":"#{context.asset_name}","browser_download_url":"https://example.test/asset.tar.gz"}]})},
        %{program: "git", args: ["remote", "get-url", "origin"], status: 2, stdout: ""},
        %{program: "git", args: ["remote", "add", "origin", context.haikuports_fork_url], stdout: ""},
        %{program: "git", args: ["remote", "get-url", "upstream"], status: 2, stdout: ""},
        %{program: "git", args: ["remote", "add", "upstream", context.haikuports_upstream_url], stdout: ""},
        %{program: "git", args: ["fetch", "upstream"], stdout: ""},
        %{program: "git", args: ["checkout", context.haikuports_target_branch], stdout: ""},
        %{program: "git", args: ["reset", "--hard", "upstream/#{context.haikuports_target_branch}"], stdout: ""},
        %{program: "git", args: ["checkout", "-B", context.haikuports_branch], stdout: ""},
        %{program: "git", args: ["status", "--porcelain", "--", context.haikuports_port_path], stdout: ""},
        %{program: "git", args: ["rev-parse", context.haikuports_branch], stdout: "localsha\n"},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], status: 1, stdout: ""},
        %{program: "git", args: ["fetch", "--prune", "origin"], stdout: ""},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], status: 1, stdout: ""},
        %{program: "git", args: ["push", "--set-upstream", "origin", context.haikuports_branch], stdout: ""},
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
          stdout: "[]"}
      ])

    output =
      capture_io(fn ->
        Release.run(workspace, "1.0", mode: :release, executor: {FakeExecutor, agent}, host: :macos)
      end)

    assert output =~ "Run this command to open the PR"
    assert output =~ "gh pr create"
    assert output =~ "--editor"
    assert output =~ "Suggested PR title:"
    assert output =~ context.suggested_pr_title
    assert output =~ "Optional rollout notes:"
    assert output =~ context.pr_notes
    assert output =~ context.pr_handoff_path
    assert output =~ "HaikuPorts contributor checklist template appears in the editor"
    assert output =~ "scripts/release_haiku_rollout.sh watch #{context.haikuports_branch}"
    assert File.exists?(context.pr_handoff_path)
    assert File.read!(context.pr_handoff_path) =~ context.suggested_pr_title
    assert File.read!(context.pr_handoff_path) =~ "gh pr create"
    assert File.read!(context.pr_handoff_path) =~ "--editor"

    commands = FakeExecutor.commands(agent)
    refute Enum.any?(commands, &(&1.args |> Enum.take(2) == ["pr", "create"]))
    refute Enum.any?(commands, &(&1.args |> Enum.take(2) == ["pr", "checks"]))
  end

  test "resume reprints the template-safe PR handoff when the remote already matches and no PR exists yet" do
    workspace = WorkspaceFactory.workspace!()
    context = HemeraHaikuRollout.ReleaseContext.build(workspace, "1.0")
    File.rm_rf!(context.work_dir)
    File.mkdir_p!(context.artifact_dir)
    File.write!(context.artifact_path, "tarball")
    File.write!(context.rendered_recipe_path, "recipe")

    State.put_step!(context, :tag_ensured, %{status: "completed", target_sha: "deadbeef"})
    State.put_step!(context, :source_tarball_built, %{status: "completed", artifact_path: context.artifact_path})
    State.put_step!(context, :checksum_computed, %{status: "completed", sha256: "abc123"})
    State.put_step!(context, :recipe_rendered, %{status: "completed", recipe_path: context.rendered_recipe_path})
    State.put_step!(context, :github_release_ensured, %{status: "completed", url: "https://example.test/release"})

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["auth", "status"], stdout: "ok\n"},
        %{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})},
        %{program: "git", args: ["remote", "get-url", "origin"], status: 2, stdout: ""},
        %{program: "git", args: ["remote", "get-url", "origin"], status: 2, stdout: ""},
        %{program: "git", args: ["remote", "add", "origin", context.haikuports_fork_url], stdout: ""},
        %{program: "git", args: ["remote", "get-url", "upstream"], status: 2, stdout: ""},
        %{program: "git", args: ["remote", "add", "upstream", context.haikuports_upstream_url], stdout: ""},
        %{program: "git", args: ["fetch", "upstream"], stdout: ""},
        %{program: "git", args: ["checkout", context.haikuports_target_branch], stdout: ""},
        %{program: "git", args: ["reset", "--hard", "upstream/#{context.haikuports_target_branch}"], stdout: ""},
        %{program: "git", args: ["checkout", "-B", context.haikuports_branch], stdout: ""},
        %{program: "git", args: ["status", "--porcelain", "--", context.haikuports_port_path], stdout: ""},
        %{program: "git", args: ["rev-parse", context.haikuports_branch], stdout: "same123\n"},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], stdout: "tracked456\n"},
        %{program: "git", args: ["fetch", "--prune", "origin"], stdout: ""},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], stdout: "same123\n"},
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
          stdout: "[]"}
      ])

    output =
      capture_io(fn ->
        Release.run(workspace, "1.0", mode: :resume, executor: {FakeExecutor, agent}, host: :macos)
      end)

    refute output =~ "error:"
    assert output =~ "Run this command to open the PR"
    assert output =~ "--editor"
    assert output =~ context.suggested_pr_title
    assert output =~ context.pr_handoff_path
    assert output =~ "HaikuPorts contributor checklist template appears in the editor"
    assert File.exists?(context.pr_handoff_path)

    commands = FakeExecutor.commands(agent)
    refute Enum.any?(commands, &(&1.args |> Enum.take(2) == ["release", "view"]))
    refute Enum.any?(commands, &(&1.args |> Enum.take(2) == ["release", "create"]))
    refute Enum.any?(commands, &(&1.args |> Enum.take(2) == ["release", "upload"]))
    refute Enum.any?(commands, &(&1.args |> Enum.take(2) == ["push", "--set-upstream"]))
    refute Enum.any?(commands, &(&1.args |> Enum.take(2) == ["pr", "create"]))
    refute Enum.any?(commands, &(&1.args |> Enum.take(2) == ["pr", "checks"]))
  end

  test "release persists branch divergence details instead of only a raw git push failure" do
    workspace = WorkspaceFactory.workspace!()
    context = HemeraHaikuRollout.ReleaseContext.build(workspace, "1.0")
    File.rm_rf!(context.work_dir)
    File.mkdir_p!(context.artifact_dir)
    File.write!(context.artifact_path, "tarball")
    State.put_step!(context, :pr_discovered, %{status: "completed", pr_url: "https://example.test/pr/17"})

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["auth", "status"], stdout: "ok\n"},
        %{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})},
        %{program: "git", args: ["remote", "get-url", "origin"], status: 2, stdout: ""},
        %{program: "git", args: ["rev-parse", "--verify", "refs/tags/#{context.tag}"], status: 1, stdout: ""},
        %{program: "git", args: ["tag", "-a", context.tag, "-m", context.release_title], stdout: ""},
        %{program: "git", args: ["rev-parse", "#{context.tag}^{commit}"], stdout: "deadbeef\n"},
        %{program: "git",
          args: [
            "archive",
            "--format=tar.gz",
            "--prefix=#{context.archive_prefix}/",
            "-o",
            context.artifact_path,
            context.tag
          ],
          stdout: ""},
        %{program: "gh", args: ["release", "view", context.tag, "--repo", context.repo_slug], status: 1, stdout: ""},
        %{program: "gh",
          args: [
            "release",
            "create",
            context.tag,
            "--repo",
            context.repo_slug,
            "--target",
            "deadbeef",
            "--title",
            context.release_title,
            "--notes-file",
            context.release_notes_path
          ],
          stdout: ""},
        %{program: "gh",
          args: ["release", "upload", context.tag, context.artifact_path, "--clobber", "--repo", context.repo_slug],
          stdout: ""},
        %{program: "gh",
          args: ["api", "repos/#{context.repo_slug}/releases/tags/#{URI.encode(context.tag, &URI.char_unreserved?/1)}"],
          stdout: ~s({"html_url":"https://example.test/release","assets":[{"name":"#{context.asset_name}","browser_download_url":"https://example.test/asset.tar.gz"}]})},
        %{program: "git", args: ["remote", "get-url", "origin"], status: 2, stdout: ""},
        %{program: "git", args: ["remote", "add", "origin", context.haikuports_fork_url], stdout: ""},
        %{program: "git", args: ["remote", "get-url", "upstream"], status: 2, stdout: ""},
        %{program: "git", args: ["remote", "add", "upstream", context.haikuports_upstream_url], stdout: ""},
        %{program: "git", args: ["fetch", "upstream"], stdout: ""},
        %{program: "git", args: ["checkout", context.haikuports_target_branch], stdout: ""},
        %{program: "git", args: ["reset", "--hard", "upstream/#{context.haikuports_target_branch}"], stdout: ""},
        %{program: "git", args: ["checkout", "-B", context.haikuports_branch], stdout: ""},
        %{program: "git", args: ["status", "--porcelain", "--", context.haikuports_port_path], stdout: ""},
        %{program: "git", args: ["rev-parse", context.haikuports_branch], stdout: "local123\n"},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], stdout: "tracked456\n"},
        %{program: "git", args: ["fetch", "--prune", "origin"], stdout: ""},
        %{program: "git", args: ["rev-parse", "--verify", "refs/remotes/origin/#{context.haikuports_branch}"], stdout: "remote789\n"},
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
          stdout: "[]"}
      ])

    assert_raise BranchPushDivergedError, fn ->
      Release.run(workspace, "1.0", mode: :release, executor: {FakeExecutor, agent}, host: :macos)
    end

    branch_step = State.step(State.load(context), :branch_pushed)
    assert branch_step["status"] == "failed"
    assert branch_step["local_sha"] == "local123"
    assert branch_step["tracked_remote_sha"] == "tracked456"
    assert branch_step["remote_sha"] == "remote789"
    assert branch_step["error"] =~ "already exists on origin"
    assert Enum.any?(branch_step["recovery_commands"], &String.contains?(&1, "push --force-with-lease origin hemera-1.0"))
    assert State.step(State.load(context), :pr_discovered) == %{}
  end
end
