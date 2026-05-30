defmodule HemeraHaikuRollout.RepoVersion do
  def validate!(context) do
    root = HemeraHaikuRollout.repo_root()
    cmake = File.read!(Path.join(root, "CMakeLists.txt"))

    expected_rc = ~s|set(HEMERA_RC_VERSION "#{context.version}")|
    expected_package = ~s|set(HEMERA_HAIKU_PACKAGE_VERSION "#{context.package_version}")|

    unless String.contains?(cmake, expected_rc) do
      raise ArgumentError, "CMakeLists.txt does not expose HEMERA_RC_VERSION #{context.version}"
    end

    unless String.contains?(cmake, expected_package) do
      raise ArgumentError,
            "CMakeLists.txt does not expose HEMERA_HAIKU_PACKAGE_VERSION #{context.package_version}"
    end

    unless File.exists?(context.release_notes_path) do
      raise ArgumentError, "missing release notes at #{context.release_notes_path}"
    end

    release_notes = File.read!(context.release_notes_path)

    unless notes_mention_version?(release_notes, context.version) do
      raise ArgumentError,
            "release notes do not mention #{context.version} or #{display_version(context.version)}"
    end

    unless File.exists?(context.recipe_template_path) do
      raise ArgumentError, "missing recipe template at #{context.recipe_template_path}"
    end
  end

  defp notes_mention_version?(release_notes, version) do
    String.contains?(release_notes, version) or String.contains?(release_notes, display_version(version))
  end

  defp display_version(version) do
    Regex.replace(~r/-rc(\d+)/, version, " RC\\1")
  end
end
