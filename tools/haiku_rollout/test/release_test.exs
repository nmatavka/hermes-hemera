defmodule HemeraHaikuRollout.ReleaseTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.Release

  test "dry-run plan includes Haiku preflight only on Haiku" do
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

    haiku_plan = Release.plan(config, "1.0.0-rc1", :haiku)
    mac_plan = Release.plan(config, "1.0.0-rc1", :macos)

    assert Enum.any?(haiku_plan, &String.starts_with?(&1, "haiku-preflight:"))
    assert Enum.any?(mac_plan, &(&1 == "skip local Haiku preflight on macos"))
    assert Enum.any?(mac_plan, &String.contains?(&1, "open/update PR"))
  end
end
