defmodule HemeraHaikuRollout.RecipeTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.Recipe
  alias HemeraHaikuRollout.ReleaseContext

  test "renders the recipe with source URL and checksum" do
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
    rendered = Recipe.render!(context, "abc123")

    assert rendered =~ "SOURCE_URI=\"#{context.source_uri}\""
    assert rendered =~ "CHECKSUM_SHA256=\"abc123\""
    assert rendered =~ "REVISION=\"1\""
  end
end
