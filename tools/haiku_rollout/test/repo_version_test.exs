defmodule HemeraHaikuRollout.RepoVersionTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.RepoVersion

  test "accepts the checked-in rc release notes display form" do
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

    context = ReleaseContext.build(config, "1.0.0-rc1")

    assert RepoVersion.validate!(context) == nil
  end
end
