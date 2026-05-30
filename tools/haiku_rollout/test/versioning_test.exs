defmodule HemeraHaikuRollout.VersioningTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.Versioning

  test "normalizes rc versions for HaikuPorts" do
    assert Versioning.to_haikuports("1.0.0-rc1") == "1.0.0~rc1"
    assert Versioning.package_version("1.0.0-rc1") == "1.0.0~rc1-1"
    assert Versioning.recipe_filename("1.0.0-rc1") == "hemera-1.0.0~rc1.recipe"
  end
end
