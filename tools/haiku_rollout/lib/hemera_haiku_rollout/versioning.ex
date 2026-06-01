defmodule HemeraHaikuRollout.Versioning do
  def to_haikuports(version) when is_binary(version) do
    String.replace(version, "-", "~", global: false)
  end

  def package_version(version, revision \\ "1") do
    "#{to_haikuports(version)}-#{revision}"
  end

  def recipe_filename(version) do
    "hemera-#{to_haikuports(version)}.recipe"
  end
end
