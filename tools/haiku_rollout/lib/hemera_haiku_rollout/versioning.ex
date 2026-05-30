defmodule HemeraHaikuRollout.Versioning do
  def to_haikuports(version) when is_binary(version) do
    String.replace(version, "-", "~", global: false)
  end

  def package_version(version) do
    "#{to_haikuports(version)}-1"
  end

  def recipe_filename(version) do
    "hemera-#{to_haikuports(version)}.recipe"
  end
end
