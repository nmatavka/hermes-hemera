defmodule HemeraHaikuRollout.DoctorTest do
  use ExUnit.Case, async: false

  alias HemeraHaikuRollout.Doctor
  alias HemeraHaikuRollout.TestSupport.{FakeExecutor, WorkspaceFactory}

  defp workspace do
    checkout = Path.join(System.tmp_dir!(), "haikuports-checkout-doctor")
    File.mkdir_p!(Path.join(checkout, ".git"))

    config_path =
      WorkspaceFactory.local_config_file!(
        """
        github:
          repo_owner: "nick"
        haikuports:
          fork_url: "git@github.com:nick/haikuports.git"
          fork_owner: "nick"
          checkout_path: "#{checkout}"
        """
      )

    WorkspaceFactory.workspace!(local_config: config_path)
  end

  test "doctor validates gh auth, manifest, and checkout remotes with a fake executor" do
    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["auth", "status"], stdout: "ok\n"},
        %{program: "gh", args: ["api", "user"], stdout: ~s({"login":"nick"})},
        %{program: "git", args: ["remote", "get-url", "origin"], stdout: "git@github.com:nick/haikuports.git\n"},
        %{program: "git", args: ["status", "--porcelain"], stdout: ""},
        %{program: "git", args: ["status", "--porcelain"], stdout: ""},
        %{program: "git", args: ["remote", "get-url", "origin"], stdout: "git@github.com:nick/haikuports.git\n"},
        %{program: "git", args: ["remote", "get-url", "upstream"], stdout: "https://github.com/haikuports/haikuports.git\n"}
      ])

    Doctor.run(workspace(), executor: {FakeExecutor, agent})

    assert length(FakeExecutor.commands(agent)) == 7
  end
end
