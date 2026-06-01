defmodule HemeraHaikuRollout.ConfigTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.{LocalConfig, Manifest, Workspace}
  alias HemeraHaikuRollout.TestSupport.WorkspaceFactory

  test "manifest loads the checked-in release defaults" do
    manifest = Manifest.load!(HemeraHaikuRollout.default_manifest_path())

    assert manifest.repo_owner == "nmatavka"
    assert manifest.repo_name == "hermes-hemera"
    assert manifest.release_version == "1.0"
    assert manifest.haikuports_target_branch == "master"
    assert manifest.haikuports_port_path == "mail-client/hemera"
  end

  test "local config init copies the example file and preserves existing content" do
    tmp_dir = Path.join(System.tmp_dir!(), "hemera-haiku-rollout-config-#{System.unique_integer([:positive])}")
    config_path = Path.join(tmp_dir, "config.yml")
    File.rm_rf!(tmp_dir)

    assert {:ok, :created, ^config_path} = LocalConfig.init(config_path)
    assert File.exists?(config_path)
    assert File.read!(config_path) == File.read!(HemeraHaikuRollout.example_config_path())

    File.write!(config_path, "github:\n  repo_owner: custom\n")
    assert {:ok, :exists, ^config_path} = LocalConfig.init(config_path)
    assert File.read!(config_path) == "github:\n  repo_owner: custom\n"
  end

  test "local config set and unset round-trips scalar overrides" do
    config_path = WorkspaceFactory.local_config_file!("{}\n")

    LocalConfig.set!("haikuports.fork_owner", "nick", config_path)
    LocalConfig.set!("github.repo_owner", "fork-owner", config_path)
    loaded = LocalConfig.load(config_path)

    assert get_in(loaded, ["haikuports", "fork_owner"]) == "nick"
    assert get_in(loaded, ["github", "repo_owner"]) == "fork-owner"

    LocalConfig.unset!("github.repo_owner", config_path)
    loaded = LocalConfig.load(config_path)
    refute get_in(loaded, ["github", "repo_owner"])
    assert get_in(loaded, ["haikuports", "fork_owner"]) == "nick"
  end

  test "workspace merges manifest defaults with local overrides and resolves checkout paths" do
    config_path =
      WorkspaceFactory.local_config_file!(
        """
        github:
          repo_owner: "nick"
        haikuports:
          fork_url: "git@github.com:nick/haikuports.git"
          fork_owner: "nick"
          checkout_path: "../haikuports"
        """
      )

    workspace = Workspace.load!(WorkspaceFactory.repo_root(), local_config: config_path)

    assert workspace.version == "1.0"
    assert workspace.manifest.repo_owner == "nick"
    assert Path.type(workspace.manifest.haikuports_checkout_path) == :absolute
  end
end
