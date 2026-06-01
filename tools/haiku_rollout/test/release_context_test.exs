defmodule HemeraHaikuRollout.ReleaseContextTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.TestSupport.WorkspaceFactory

  test "builds release naming, paths, and pr text from the workspace manifest" do
    workspace = WorkspaceFactory.workspace!()
    context = ReleaseContext.build(workspace, "1.0.0-rc1")

    assert context.tag == "v1.0.0-rc1"
    assert context.asset_name == "hemera-1.0.0-rc1-source.tar.gz"
    assert context.source_uri ==
             "https://github.com/nick/hermes-hemera/releases/download/v1.0.0-rc1/hemera-1.0.0-rc1-source.tar.gz"
    assert context.recipe_output_name == "hemera-1.0.0~rc1.recipe"
    assert context.pr_title == "hemera: add 1.0.0~rc1-1"
    assert context.pr_body =~ "Hemera 1.0.0~rc1-1"
    assert context.state_path == Path.join(context.work_dir, "state.json")
  end
end
