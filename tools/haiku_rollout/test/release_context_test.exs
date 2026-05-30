defmodule HemeraHaikuRollout.ReleaseContextTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.ReleaseContext

  defp config do
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

    config
  end

  test "builds release naming and URLs" do
    context = ReleaseContext.build(config(), "1.0.0-rc1")

    assert context.tag == "v1.0.0-rc1"
    assert context.asset_name == "hemera-1.0.0-rc1-source.tar.gz"
    assert context.source_uri ==
             "https://github.com/nick/hermes-hemera/releases/download/v1.0.0-rc1/hemera-1.0.0-rc1-source.tar.gz"
    assert context.recipe_output_name == "hemera-1.0.0~rc1.recipe"
    assert context.pr_title == "hemera: add 1.0.0~rc1-1"
    assert context.pr_body =~ "Hemera 1.0.0~rc1-1"
  end
end
