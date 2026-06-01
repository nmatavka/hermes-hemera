defmodule HemeraHaikuRollout.StateTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.{Release, ReleaseContext, State}
  alias HemeraHaikuRollout.TestSupport.WorkspaceFactory

  test "tracks completed steps and pending step order" do
    version = "1.0-state-#{System.unique_integer([:positive])}"
    context = WorkspaceFactory.workspace!() |> ReleaseContext.build(version)
    File.rm_rf!(context.work_dir)

    State.put_step!(context, :repo_version_validated, %{status: "completed"})
    State.put_step!(context, :tag_ensured, %{status: "completed"})

    state = State.load(context)

    assert State.completed?(state, :repo_version_validated)
    assert State.last_completed_step(state, Release.step_order()) == {:tag_ensured, "git tag ensured"}
    assert State.next_pending_step(state, Release.step_order()) == {:haiku_preflight, "haiku preflight"}
  end
end
