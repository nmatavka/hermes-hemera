defmodule HemeraHaikuRollout.ReleaseContext do
  alias HemeraHaikuRollout.{Util, Versioning}

  defstruct [
    :workspace,
    :version,
    :haikuports_version,
    :package_version,
    :package_revision,
    :tag,
    :asset_name,
    :release_title,
    :release_notes_path,
    :work_dir,
    :repo_slug,
    :haikuports_repo_slug,
    :source_uri,
    :artifact_dir,
    :artifact_path,
    :archive_prefix,
    :recipe_template_path,
    :recipe_output_name,
    :rendered_recipe_path,
    :port_template_dir,
    :haikuports_branch,
    :pr_title,
    :pr_body,
    :haikuports_port_path,
    :haikuports_checkout_path,
    :haikuports_upstream_url,
    :haikuports_fork_url,
    :haikuports_fork_owner,
    :haikuports_target_branch,
    :haiku_preflight_commands,
    :state_path,
    :app_name
  ]

  def build(workspace, version) do
    manifest = workspace.manifest
    haikuports_version = Versioning.to_haikuports(version)
    package_version = Versioning.package_version(version, manifest.package_revision)

    replacements = %{
      "app_name" => manifest.app_name,
      "version" => version,
      "haikuports_version" => haikuports_version,
      "package_version" => package_version
    }

    tag = Util.render_template(manifest.release_tag_template, replacements)
    asset_name = Util.render_template(manifest.asset_name_template, replacements)
    release_title = Util.render_template(manifest.release_title_template, replacements)
    release_notes_path =
      Path.expand(Util.render_template(manifest.release_notes_path_template, replacements), workspace.root)

    repo_slug = "#{manifest.repo_owner}/#{manifest.repo_name}"
    source_uri = "https://github.com/#{repo_slug}/releases/download/#{tag}/#{asset_name}"
    work_dir = HemeraHaikuRollout.Workspace.work_dir(workspace, version)
    artifact_dir = Path.join(work_dir, "artifacts")
    haikuports_branch = Util.render_template(manifest.haikuports_branch_template, Map.put(replacements, "tag", tag))

    replacements =
      replacements
      |> Map.put("tag", tag)
      |> Map.put("repo_slug", repo_slug)
      |> Map.put("source_uri", source_uri)

    context = %__MODULE__{
      workspace: workspace,
      version: version,
      haikuports_version: haikuports_version,
      package_version: package_version,
      package_revision: manifest.package_revision,
      tag: tag,
      asset_name: asset_name,
      release_title: release_title,
      release_notes_path: release_notes_path,
      work_dir: work_dir,
      repo_slug: repo_slug,
      haikuports_repo_slug: github_slug(manifest.haikuports_upstream_url),
      source_uri: source_uri,
      artifact_dir: artifact_dir,
      artifact_path: Path.join(artifact_dir, asset_name),
      archive_prefix: Util.render_template(manifest.archive_prefix_template, replacements),
      recipe_template_path: HemeraHaikuRollout.Workspace.recipe_template_path(workspace),
      recipe_output_name: Versioning.recipe_filename(version),
      rendered_recipe_path: Path.join(work_dir, Versioning.recipe_filename(version)),
      port_template_dir: HemeraHaikuRollout.Workspace.recipe_template_dir(workspace),
      haikuports_branch: haikuports_branch,
      pr_title: Util.render_template(manifest.haikuports_pr_title_template, replacements),
      pr_body: "",
      haikuports_port_path: manifest.haikuports_port_path,
      haikuports_checkout_path: manifest.haikuports_checkout_path,
      haikuports_upstream_url: manifest.haikuports_upstream_url,
      haikuports_fork_url: manifest.haikuports_fork_url,
      haikuports_fork_owner: manifest.haikuports_fork_owner,
      haikuports_target_branch: manifest.haikuports_target_branch,
      haiku_preflight_commands: manifest.haiku_preflight_commands,
      state_path: Path.join(work_dir, "state.json"),
      app_name: manifest.app_name
    }

    %{context | pr_body: pr_body(context)}
  end

  def pr_body(context) do
    Util.render_template(context.workspace.manifest.haikuports_pr_body_template, %{
      "app_name" => context.app_name,
      "version" => context.version,
      "haikuports_version" => context.haikuports_version,
      "package_version" => context.package_version,
      "tag" => context.tag,
      "source_uri" => context.source_uri,
      "repo_slug" => context.repo_slug
    })
  end

  defp github_slug(url) do
    normalized = String.trim_trailing(url, ".git")

    cond do
      String.starts_with?(normalized, "git@github.com:") ->
        String.replace_prefix(normalized, "git@github.com:", "")

      String.starts_with?(normalized, "https://github.com/") ->
        String.replace_prefix(normalized, "https://github.com/", "")

      String.starts_with?(normalized, "http://github.com/") ->
        String.replace_prefix(normalized, "http://github.com/", "")

      true ->
        normalized
    end
  end
end
