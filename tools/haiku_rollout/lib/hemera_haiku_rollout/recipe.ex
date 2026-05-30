defmodule HemeraHaikuRollout.Recipe do
  def render!(context, sha256) do
    context.recipe_template_path
    |> File.read!()
    |> String.replace("@HEMERA_REPO_SLUG@", context.repo_slug)
    |> String.replace("@HEMERA_SOURCE_URI@", context.source_uri)
    |> String.replace("@HEMERA_SOURCE_SHA256@", sha256)
    |> String.replace("@HEMERA_HAIKU_PACKAGE_VERSION@", context.package_version)
  end

  def checksum!(path) do
    path
    |> File.read!()
    |> then(&:crypto.hash(:sha256, &1))
    |> Base.encode16(case: :lower)
  end
end
