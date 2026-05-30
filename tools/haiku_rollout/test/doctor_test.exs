defmodule HemeraHaikuRollout.DoctorTest do
  use ExUnit.Case, async: false

  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.Doctor
  alias HemeraHaikuRollout.TestSupport.FakeExecutor

  defp config do
    checkout = Path.join(System.tmp_dir!(), "haikuports-checkout")
    File.mkdir_p!(Path.join(checkout, ".git"))

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
            "checkout_path" => checkout
          }
        },
        "memory"
      )

    config
  end

  test "doctor validates git and gh state with a fake executor" do
    {:ok, agent} =
      FakeExecutor.start_link([
        %{program: "gh", args: ["auth", "status"], stdout: "ok\n"},
        %{program: "git", args: ["status", "--porcelain"], stdout: ""},
        %{program: "git", args: ["status", "--porcelain"], stdout: ""},
        %{program: "git", args: ["remote", "get-url", "origin"], stdout: "git@github.com:nick/haikuports.git\n"},
        %{program: "git", args: ["remote", "get-url", "upstream"], stdout: "https://github.com/haikuports/haikuports.git\n"}
      ])

    Doctor.run(config(), executor: {FakeExecutor, agent})

    assert length(FakeExecutor.commands(agent)) == 5
  end
end
