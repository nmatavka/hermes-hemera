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

  test "rejects placeholder config values before any remote action" do
    map = %{
      "github" => %{
        "repo_owner" => "YOUR_GITHUB_OWNER",
        "repo_name" => "hermes-hemera"
      },
      "haikuports" => %{
        "upstream_url" => "https://github.com/haikuports/haikuports.git",
        "fork_url" => "git@github.com:YOUR_GITHUB_OWNER/haikuports.git",
        "fork_owner" => "YOUR_GITHUB_OWNER",
        "checkout_path" => "vendor/haikuports"
      }
    }

    assert {:error, message} = Config.from_map(map, "memory")
    assert message =~ "placeholder value"
    assert message =~ "github.repo_owner"
  end

  test "init copies the example config and does not overwrite existing config" do
    tmp_dir = Path.join(System.tmp_dir!(), "hemera-haiku-rollout-config-#{System.unique_integer([:positive])}")
    config_path = Path.join(tmp_dir, "config.yml")
    File.rm_rf!(tmp_dir)

    assert {:ok, :created, ^config_path} = Config.init(config_path)
    assert File.exists?(config_path)
    assert File.read!(config_path) == File.read!(HemeraHaikuRollout.example_config_path())

    File.write!(config_path, "custom: true\n")
    assert {:ok, :exists, ^config_path} = Config.init(config_path)
    assert File.read!(config_path) == "custom: true\n"
  end
end
