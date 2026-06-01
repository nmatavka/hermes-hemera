defmodule LinuxRollout.Workspace do
  alias LinuxRollout.{LocalConfig, Util, Yaml}

  defstruct [
    :root,
    :packaging_dir,
    :manifest_path,
    :targets_path,
    :local_config_path,
    :manifest,
    :local_config,
    :targets,
    :defaults,
    :version,
    :work_dir,
    :rendered_root,
    :bundle_root,
    :log_root,
    :approval_root,
    :checkout_root,
    :state_path,
    :tokens
  ]

  def load!(root, opts \\ []) do
    expanded_root = Path.expand(root)
    packaging_dir = Path.join(expanded_root, "packaging")

    manifest_path =
      Path.expand(Keyword.get(opts, :manifest, Path.join(packaging_dir, "release_manifest.yaml")))

    targets_path =
      Path.expand(Keyword.get(opts, :targets, Path.join(packaging_dir, "targets.yaml")))

    local_config_path =
      Path.expand(Keyword.get(opts, :local_config, Util.local_config_path()))

    local_config = LocalConfig.load(local_config_path)
    manifest = manifest_path |> Yaml.load!() |> LocalConfig.merge_manifest(local_config)
    targets_document = Yaml.load!(targets_path)
    version = fetch!(manifest, ["app", "version"]) |> to_string()
    defaults = Map.get(targets_document, "defaults", %{})

    rendered_root =
      Path.join(expanded_root, Map.get(defaults, "rendered_root", "build/linux-rollout"))

    work_dir = Path.join(rendered_root, version)

    base_workspace = %__MODULE__{
      root: expanded_root,
      packaging_dir: packaging_dir,
      manifest_path: manifest_path,
      targets_path: targets_path,
      local_config_path: local_config_path,
      manifest: manifest,
      local_config: local_config,
      defaults: defaults,
      version: version,
      work_dir: work_dir,
      rendered_root: Path.join(work_dir, "rendered"),
      bundle_root: Path.join(work_dir, "bundles"),
      log_root: Path.join(work_dir, "logs"),
      approval_root: Path.join(work_dir, "approvals"),
      checkout_root: Path.join(work_dir, "checkouts"),
      state_path: Path.join(work_dir, "state.json"),
      tokens: %{},
      targets: %{}
    }

    tokens = build_tokens(base_workspace)
    targets = build_targets(targets_document, manifest, defaults, tokens)
    artifact_tokens = build_artifact_tokens(base_workspace, manifest)
    all_tokens = Map.merge(tokens, artifact_tokens)
    resolved_targets = resolve_target_tokens(targets, all_tokens)

    %{base_workspace | tokens: all_tokens, targets: resolved_targets}
  end

  def enabled_targets(%__MODULE__{targets: targets}) do
    targets
    |> Enum.filter(fn {_name, target} -> Map.get(target, "enabled", false) end)
    |> Enum.into(%{})
  end

  def headless_targets(%__MODULE__{} = workspace) do
    workspace
    |> enabled_targets()
    |> Enum.filter(fn {_name, target} -> headless_capable?(target) end)
    |> Enum.into(%{})
  end

  def target!(%__MODULE__{targets: targets}, name) do
    Map.fetch!(targets, name)
  end

  def target_units(target) do
    case Map.get(target, "units", []) do
      units when is_list(units) and units != [] ->
        Enum.map(units, &normalize_unit(&1, target))

      _ ->
        [normalize_unit(%{}, target)]
    end
  end

  def headless_capable?(target) do
    Map.get(target, "headless_capability") in ["full", "conditional", "gated"]
  end

  def branch_name(%__MODULE__{defaults: defaults, tokens: tokens}, target_name) do
    template = Map.get(defaults, "branch_template", "linux-rollout-@release_version@-@target@")
    Util.render_string(template, Map.put(tokens, "target", target_name))
  end

  def commit_message(%__MODULE__{defaults: defaults, tokens: tokens}, target_name) do
    template =
      Map.get(
        defaults,
        "commit_template",
        "linux: publish @target@ packaging for @release_version@"
      )

    Util.render_string(template, Map.put(tokens, "target", target_name))
  end

  def rendered_packaging_dir(%__MODULE__{rendered_root: rendered_root}) do
    Path.join(rendered_root, "packaging")
  end

  def artifact_path(%__MODULE__{root: root}, "jar", asset_name), do: Path.join(root, asset_name)

  def artifact_path(%__MODULE__{root: root}, _kind, asset_name) do
    Path.join([root, "build", "release-artifacts", asset_name])
  end

  defp build_tokens(%__MODULE__{manifest: manifest}) do
    github_owner_config = present_or_blank(get_in(manifest, ["accounts", "github_owner"]))
    nix_maintainer_github_id_config =
      present_or_blank(get_in(manifest, ["accounts", "nix_maintainer_github_id"]))

    github_user =
      if is_nil(github_owner_config) or is_nil(nix_maintainer_github_id_config) do
        Util.github_current_user()
      end

    github_login = present_or_blank(github_user && github_user["login"])
    github_id = present_or_blank(github_user && github_user["id"])
    github_owner = github_owner_config || github_login

    gitlab_owner =
      present_or_blank(get_in(manifest, ["accounts", "gitlab_owner"])) || github_owner

    repo_slug = fetch!(manifest, ["repository", "slug"])
    repo_name = repo_slug |> String.split("/", parts: 2) |> List.last()

    homebrew_tap_repo =
      present_or_blank(get_in(manifest, ["accounts", "homebrew_tap_repo"])) ||
        if(github_owner == "", do: "", else: "#{github_owner}/homebrew-wireshare")

    obs_owner =
      present_or_blank(get_in(manifest, ["accounts", "obs_owner"])) ||
        Util.obs_username_from_oscrc() || github_owner

    obs_project =
      present_or_blank(get_in(manifest, ["accounts", "obs_project"])) ||
        if(obs_owner == "", do: "", else: "home:#{obs_owner}:wireshare")

    launchpad_project =
      present_or_blank(get_in(manifest, ["accounts", "launchpad_project"])) || repo_name

    submission_name =
      present_or_blank(get_in(manifest, ["accounts", "submission_name"])) ||
        present_or_blank(Util.git_config("user.name"))

    submission_email =
      present_or_blank(get_in(manifest, ["accounts", "submission_email"])) ||
        present_or_blank(Util.git_config("user.email"))

    pkgsrc_wip_user =
      present_or_blank(get_in(manifest, ["accounts", "pkgsrc_wip_user"])) ||
        Util.ssh_config_user("wip.pkgsrc.org")

    nix_maintainer_handle =
      present_or_blank(get_in(manifest, ["accounts", "nix_maintainer_handle"])) ||
        github_owner || github_login || "@nix_maintainer_handle@"

    nix_maintainer_name =
      present_or_blank(get_in(manifest, ["accounts", "nix_maintainer_name"])) ||
        submission_name || "@nix_maintainer_name@"

    nix_maintainer_email =
      present_or_blank(get_in(manifest, ["accounts", "nix_maintainer_email"])) ||
        submission_email || "@nix_maintainer_email@"

    nix_maintainer_github =
      present_or_blank(get_in(manifest, ["accounts", "nix_maintainer_github"])) ||
        github_owner || github_login || nix_maintainer_handle || "@nix_maintainer_github@"

    nix_maintainer_github_id =
      nix_maintainer_github_id_config ||
        github_id || "@nix_maintainer_github_id@"

    nix_host_system = Util.nix_host_system()
    nix_host_os = Util.nix_host_os()

    %{
      "release_app_name" => fetch!(manifest, ["app", "name"]),
      "release_app_id" => fetch!(manifest, ["app", "id"]),
      "legacy_release_app_id" => fetch!(manifest, ["app", "legacy_ids", "primary"]),
      "legacy_older_release_app_id" => fetch!(manifest, ["app", "legacy_ids", "secondary"]),
      "release_vendor" => fetch!(manifest, ["app", "vendor"]),
      "release_version" => fetch!(manifest, ["app", "version"]) |> to_string(),
      "release_tag" => fetch!(manifest, ["release", "tag"]),
      "release_date" => fetch!(manifest, ["release", "date"]),
      "release_notes" => fetch!(manifest, ["release", "notes"]),
      "release_repo_slug" => repo_slug,
      "release_repo_default_branch" =>
        get_in(manifest, ["repository", "default_branch"]) || "main",
      "release_homepage_url" => fetch!(manifest, ["repository", "homepage_url"]),
      "release_issues_url" => fetch!(manifest, ["repository", "issues_url"]),
      "source_tarball_name" => fetch!(manifest, ["assets", "source_tarball", "name"]),
      "source_tarball_url" => fetch!(manifest, ["assets", "source_tarball", "url"]),
      "wire_share_jar_name" => fetch!(manifest, ["assets", "jar", "name"]),
      "wire_share_jar_url" => fetch!(manifest, ["assets", "jar", "url"]),
      "checksums_name" => fetch!(manifest, ["assets", "checksums", "name"]),
      "checksums_url" => fetch!(manifest, ["assets", "checksums", "url"]),
      "dependency_inventory_name" => fetch!(manifest, ["assets", "dependency_inventory", "name"]),
      "dependency_inventory_url" => fetch!(manifest, ["assets", "dependency_inventory", "url"]),
      "linux_primary_arch" => get_in(manifest, ["architectures", "primary"]) || "x86_64",
      "flathub_fork_repo" =>
        present_or_blank(get_in(manifest, ["accounts", "flathub_fork_repo"])) || "",
      "homebrew_tap_repo" => homebrew_tap_repo,
      "homebrew_base_branch" =>
        present_or_blank(get_in(manifest, ["accounts", "homebrew_base_branch"])) || "main",
      "launchpad_owner" =>
        present_or_blank(get_in(manifest, ["accounts", "launchpad_owner"])) || "",
      "launchpad_project" => launchpad_project,
      "submission_name" => submission_name || "WireShare Packaging Bot",
      "submission_email" => submission_email || "",
      "pkgsrc_wip_user" => pkgsrc_wip_user || "",
      "nix_maintainer_handle" => nix_maintainer_handle,
      "nix_maintainer_name" => nix_maintainer_name,
      "nix_maintainer_email" => nix_maintainer_email,
      "nix_maintainer_github" => nix_maintainer_github,
      "nix_maintainer_github_id" => nix_maintainer_github_id,
      "nix_host_system" => nix_host_system,
      "nix_host_os" => nix_host_os,
      "github_owner" => github_owner,
      "gitlab_owner" => gitlab_owner,
      "obs_owner" => obs_owner,
      "obs_project" => obs_project,
      "copr_owner" =>
        present_or_blank(get_in(manifest, ["accounts", "copr_owner"])) || github_owner,
      "release_debian_timestamp" =>
        Util.default_debian_timestamp(fetch!(manifest, ["release", "date"]))
    }
  end

  defp build_artifact_tokens(%__MODULE__{} = workspace, manifest) do
    source_name = fetch!(manifest, ["assets", "source_tarball", "name"])
    jar_name = fetch!(manifest, ["assets", "jar", "name"])
    checksums_name = fetch!(manifest, ["assets", "checksums", "name"])
    dependency_inventory_name = fetch!(manifest, ["assets", "dependency_inventory", "name"])

    source_sha =
      present_or_nil(get_in(manifest, ["assets", "source_tarball", "sha256"])) ||
        checksum_for_file(workspace, checksums_name, source_name)

    jar_sha =
      present_or_nil(get_in(manifest, ["assets", "jar", "sha256"])) ||
        checksum_for_file(workspace, checksums_name, jar_name)

    %{
      "source_tarball_sha256" => source_sha || "",
      "source_tarball_sha512" => hash_for_artifact(workspace, source_name, :sha512) || "",
      "source_tarball_size" => artifact_size(workspace, source_name) || "",
      "wire_share_jar_sha256" => jar_sha || "",
      "source_tarball_nix_base32" =>
        present_or_nil(get_in(manifest, ["assets", "source_tarball", "nix_base32"])) ||
          Util.nix_base32_from_hex(source_sha) || "",
      "dependency_inventory_path" =>
        artifact_path(workspace, "release", dependency_inventory_name)
    }
  end

  defp build_targets(targets_document, manifest, defaults, tokens) do
    targets_document
    |> Map.get("targets", %{})
    |> Enum.map(fn {name, target} ->
      manifest_target = get_in(manifest, ["targets", name]) || %{}
      enabled = Map.get(manifest_target, "enabled")
      target_override = Map.delete(manifest_target, "enabled")

      merged_target = Util.deep_merge(target, target_override)
      branch_template = Map.get(merged_target, "branch_name", Map.get(defaults, "branch_template", ""))

      commit_template =
        Map.get(
          merged_target,
          "commit_message",
          Map.get(
            defaults,
            "commit_template",
            "linux: publish @target@ packaging for @release_version@"
          )
        )

      base_target =
        merged_target
        |> Map.put("name", name)
        |> Map.put("enabled", enabled != false)
        |> Map.put(
          "branch_name",
          Util.render_string(branch_template, Map.put(tokens, "target", name))
        )
        |> Map.put(
          "commit_message",
          Util.render_string(commit_template, Map.put(tokens, "target", name))
        )

      {name, base_target}
    end)
    |> Map.new()
  end

  defp resolve_target_tokens(targets, tokens) do
    targets
    |> Enum.map(fn {name, target} ->
      {name, Util.render_tokens(target, Map.put(tokens, "target", name))}
    end)
    |> Map.new()
  end

  defp checksum_for_file(workspace, checksums_name, target_name) do
    checksums_path = artifact_path(workspace, "release", checksums_name)

    with true <- File.exists?(checksums_path),
         {:ok, contents} <- File.read(checksums_path) do
      contents
      |> String.split("\n", trim: true)
      |> Enum.find_value(fn line ->
        case String.split(line, ~r/\s+/, parts: 2, trim: true) do
          [checksum, filename] ->
            if String.trim(filename) == target_name, do: checksum

          _ ->
            nil
        end
      end)
    else
      _ -> nil
    end
  end

  defp hash_for_artifact(workspace, asset_name, algorithm) do
    path = artifact_path(workspace, "release", asset_name)

    if File.exists?(path) do
      :crypto.hash(algorithm, File.read!(path))
      |> Base.encode16(case: :lower)
    end
  end

  defp artifact_size(workspace, asset_name) do
    path = artifact_path(workspace, "release", asset_name)

    if File.exists?(path) do
      File.stat!(path).size
    end
  end

  defp fetch!(map, path) do
    case get_in(map, path) do
      nil -> raise "missing required manifest field #{Enum.join(path, ".")}"
      value -> value
    end
  end

  defp present_or_nil(value) when value in [nil, ""], do: nil
  defp present_or_nil(value), do: value
  defp present_or_blank(value), do: present_or_nil(value)

  defp normalize_unit(unit, target) do
    unit_destination = Map.get(unit, "destination", %{})

    unit
    |> Map.put_new("name", target["name"])
    |> Map.put_new("driver", target["driver"])
    |> Map.put_new("submission_mode", target["submission_mode"])
    |> Map.put_new("forge", target["forge"])
    |> Map.put_new("service", target["service"])
    |> Map.put_new("headless_capability", target["headless_capability"])
    |> Map.put_new("fallback_reason", target["fallback_reason"])
    |> Map.put_new("submission_paths", Map.get(target, "submission_paths", []))
    |> Map.put_new("required_files", Map.get(target, "required_files", []))
    |> Map.put_new("path_map", Map.get(target, "path_map", %{}))
    |> Map.put_new("checkout_mutations", Map.get(target, "checkout_mutations", []))
    |> Map.put_new("commit_plan", Map.get(target, "commit_plan", []))
    |> Map.put_new("git_commit_signoff", Map.get(target, "git_commit_signoff", false))
    |> Map.put_new("push_command", Map.get(target, "push_command"))
    |> Map.put_new("pre_submit_checks", Map.get(target, "pre_submit_checks", []))
    |> Map.put_new("pull_request_handoff", Map.get(target, "pull_request_handoff"))
    |> Map.put_new("suggested_pr_title", Map.get(target, "suggested_pr_title"))
    |> Map.put_new("validated_system", Map.get(target, "validated_system"))
    |> Map.put(
      "destination",
      Util.deep_merge(Map.get(target, "destination", %{}), unit_destination)
    )
    |> Map.put_new("branch_name", target["branch_name"])
    |> Map.put_new("commit_message", target["commit_message"])
    |> Map.put("target_name", target["name"])
  end
end
