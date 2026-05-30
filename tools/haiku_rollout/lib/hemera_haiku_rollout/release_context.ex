defmodule HemeraHaikuRollout.ReleaseContext do
  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.Versioning

  defstruct [
    :version,
    :haikuports_version,
    :package_version,
    :tag,
    :asset_name,
    :release_title,
    :release_notes_path,
    :repo_slug,
    :haikuports_repo_slug,
    :source_uri,
    :artifact_dir,
    :artifact_path,
    :archive_prefix,
    :recipe_template_path,
    :recipe_output_name,
    :port_template_dir,
    :haikuports_branch,
    :pr_title,
    :pr_body,
    :haikuports_port_path,
    :haiku_preflight_commands
  ]

  def build(config, version) do
    haikuports_version = Versioning.to_haikuports(version)
    package_version = Versioning.package_version(version)
    tag = Config.render_template(config.release_tag_template, version)
    asset_name = Config.render_template(config.asset_name_template, version)
    release_title = Config.render_template(config.release_title_template, version)
    release_notes_path =
      Path.expand(Config.render_template(config.release_notes_path_template, version), HemeraHaikuRollout.repo_root())

    repo_slug = Config.repo_slug(config)
    source_uri = "https://github.com/#{repo_slug}/releases/download/#{tag}/#{asset_name}"
    artifact_dir = Path.join(HemeraHaikuRollout.repo_root(), "build/haiku_rollout/#{version}")
    haikuports_branch = "hemera-#{haikuports_version}"

    context = %__MODULE__{
      version: version,
      haikuports_version: haikuports_version,
      package_version: package_version,
      tag: tag,
      asset_name: asset_name,
      release_title: release_title,
      release_notes_path: release_notes_path,
      repo_slug: repo_slug,
      haikuports_repo_slug: Config.haikuports_repo_slug(config),
      source_uri: source_uri,
      artifact_dir: artifact_dir,
      artifact_path: Path.join(artifact_dir, asset_name),
      archive_prefix: "hemera-#{version}",
      recipe_template_path: HemeraHaikuRollout.recipe_template_path(),
      recipe_output_name: Versioning.recipe_filename(version),
      port_template_dir: HemeraHaikuRollout.recipe_template_dir(),
      haikuports_branch: haikuports_branch,
      pr_title: "hemera: add #{package_version}",
      pr_body: "",
      haikuports_port_path: config.haikuports_port_path,
      haiku_preflight_commands: config.haiku_preflight_commands
    }

    %{context | pr_body: pr_body(context)}
  end

  def pr_body(context) do
    """
    ## Summary

    - add Hemera #{context.package_version}
    - source asset: #{context.source_uri}
    - generated from Hemera tag `#{context.tag}`

    ## Validation

    - Hemera rollout tool prepared the recipe and source tarball
    - local Haiku preflight runs only when the rollout host is Haiku
    - follow-up build/check results are tracked through this PR
    """
  end
end
