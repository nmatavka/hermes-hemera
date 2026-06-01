defmodule HemeraHaikuRollout.RecipeTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.{Recipe, ReleaseContext}
  alias HemeraHaikuRollout.TestSupport.WorkspaceFactory

  test "renders the recipe with source URL and checksum" do
    context = WorkspaceFactory.workspace!() |> ReleaseContext.build("1.0.0-rc1")
    rendered = Recipe.render!(context, "abc123")

    assert rendered =~ "SOURCE_URI=\"#{context.source_uri}\""
    assert rendered =~ "CHECKSUM_SHA256=\"abc123\""
    assert rendered =~ "REVISION=\"1\""
  end

  test "validates the checked-in recipe template and rendered field order" do
    workspace = WorkspaceFactory.workspace!()
    context = ReleaseContext.build(workspace, "1.0")
    rendered = Recipe.render!(context, String.duplicate("a", 64))

    assert Recipe.validate_template!(workspace) == :ok
    assert Recipe.validate_rendered!(workspace, rendered) == :ok
  end
end
