defmodule HemeraHaikuRollout.WatchTest do
  use ExUnit.Case, async: false

  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.TestSupport.FakeExecutor
  alias HemeraHaikuRollout.Watch

  test "watch resolves a branch and runs pr checks" do
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

    repo_slug = HemeraHaikuRollout.Config.haikuports_repo_slug(config)

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh",
          args: ["pr", "list", "--repo", repo_slug, "--head", "hemera-1.0.0~rc1", "--json", "number"],
          stdout: ~s([{"number":42}])},
        %{program: "gh",
          args: ["pr", "checks", "42", "--repo", repo_slug, "--watch"],
          stdout: "checks ok\n"},
        %{program: "gh",
          args: ["pr", "view", "42", "--repo", repo_slug, "--json", "url,state"],
          stdout: ~s({"url":"https://example.test/pr/42","state":"OPEN"})}
      ])

    Watch.run(config, "hemera-1.0.0~rc1", executor: {FakeExecutor, agent})

    assert length(FakeExecutor.commands(agent)) == 3
  end

  test "watch tolerates prs with no reported checks yet" do
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

    repo_slug = HemeraHaikuRollout.Config.haikuports_repo_slug(config)

    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh",
          args: ["pr", "list", "--repo", repo_slug, "--head", "hemera-1.0.0~rc1", "--json", "number"],
          stdout: ~s([{"number":42}])},
        %{program: "gh",
          args: ["pr", "checks", "42", "--repo", repo_slug, "--watch"],
          status: 1,
          stdout: "no checks reported on the 'hemera-1.0.0~rc1' branch\n"},
        %{program: "gh",
          args: ["pr", "view", "42", "--repo", repo_slug, "--json", "url,state"],
          stdout: ~s({"url":"https://example.test/pr/42","state":"OPEN"})}
      ])

    Watch.run(config, "hemera-1.0.0~rc1", executor: {FakeExecutor, agent})

    assert length(FakeExecutor.commands(agent)) == 3
  end
end
