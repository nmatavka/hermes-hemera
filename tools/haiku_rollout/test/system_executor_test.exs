defmodule HemeraHaikuRollout.SystemExecutorTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.SystemExecutor

  test "runs commands without requiring cwd" do
    result = SystemExecutor.run("elixir", ["-e", ~s|IO.write("ok")|], [])

    assert result.status == 0
    assert result.stdout == "ok"
  end
end
