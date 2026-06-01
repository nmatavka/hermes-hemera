defmodule HemeraHaikuRollout.RepoVersionTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.{ReleaseContext, RepoVersion}
  alias HemeraHaikuRollout.TestSupport.WorkspaceFactory

  test "accepts the checked-in final release notes and manifest version" do
    workspace = WorkspaceFactory.workspace!()
    context = ReleaseContext.build(workspace, "1.0")

    assert RepoVersion.validate!(context) == nil
  end
end
