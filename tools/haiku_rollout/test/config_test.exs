defmodule HemeraHaikuRollout.ConfigTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.Config

  test "loads required config and applies defaults" do
    map = %{
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
    }

    {:ok, config} = Config.from_map(map, "memory")

    assert config.release_tag_template == "v<version>"
    assert config.asset_name_template == "hemera-<version>-source.tar.gz"
    assert config.haikuports_target_branch == "master"
    assert config.haikuports_port_path == "mail-client/hemera"
    assert Path.type(config.haikuports_checkout_path) == :absolute
  end

  test "rejects missing required config fields" do
    assert {:error, message} = Config.from_map(%{}, "memory")
    assert message =~ "github"
  end
end
