defmodule HemeraHaikuRollout.Manifest do
  alias HemeraHaikuRollout.Util

  defstruct [
    :path,
    :app_name,
    :repo_owner,
    :repo_name,
    :release_version,
    :package_revision,
    :release_tag_template,
    :asset_name_template,
    :release_title_template,
    :release_notes_path_template,
    :archive_prefix_template,
    :haikuports_branch_template,
    :haikuports_pr_title_template,
    :haikuports_pr_body_template,
    :haikuports_upstream_url,
    :haikuports_fork_url,
    :haikuports_fork_owner,
    :haikuports_checkout_path,
    :haikuports_target_branch,
    :haikuports_port_path,
    :haiku_preflight_commands
  ]

  @default_app_name "Hemera"
  @default_package_revision "1"
  @default_tag_template "v<version>"
  @default_asset_name_template "hemera-<version>-source.tar.gz"
  @default_release_title_template "Hemera <version>"
  @default_release_notes_path "docs/release-notes/<version>.md"
  @default_archive_prefix_template "hemera-<version>"
  @default_haikuports_branch_template "hemera-<haikuports_version>"
  @default_haikuports_pr_title_template "hemera: add <package_version>"
  @default_haikuports_pr_body_template """
  ## Summary

  - add <app_name> <package_version>
  - source asset: <source_uri>
  - generated from <app_name> tag `<tag>`

  ## Validation

  - <app_name> rollout tool prepared the recipe and source tarball
  - local Haiku preflight runs only when the rollout host is Haiku
  - follow-up build/check results are tracked through this PR
  """
  @default_haikuports_target_branch "master"
  @default_haikuports_port_path "mail-client/hemera"
  @default_haiku_preflight_commands [
    ["cmake", "-S", ".", "-B", "build", "-DHERMES_BUILD_HAIKU_SHELL=ON"],
    ["cmake", "--build", "build", "--target", "Hemera"],
    ["ctest", "--test-dir", "build", "--output-on-failure"],
    ["cmake", "--build", "build", "--target", "hemera_hpkg"]
  ]

  def load_map!(path) do
    case YamlElixir.read_from_file(path) do
      {:ok, map} when is_map(map) -> map
      map when is_map(map) -> map
      other -> raise ArgumentError, "unable to parse rollout manifest #{path}: #{inspect(other)}"
    end
  rescue
    error -> raise ArgumentError, "unable to parse rollout manifest #{path}: #{Exception.message(error)}"
  end

  def load!(path) do
    path |> load_map!() |> from_map!(path)
  end

  def from_map!(map, path \\ "memory") when is_map(map) do
    case from_map(map, path) do
      {:ok, manifest} -> manifest
      {:error, message} -> raise ArgumentError, message
    end
  end

  def from_map(map, path \\ "memory") when is_map(map) do
    with {:ok, github} <- required_map(map, "github"),
         {:ok, release} <- required_map(map, "release"),
         {:ok, haikuports} <- required_map(map, "haikuports"),
         {:ok, repo_owner} <- required_string(github, "repo_owner"),
         {:ok, repo_name} <- required_string(github, "repo_name"),
         {:ok, release_version} <- required_string(release, "version"),
         {:ok, upstream_url} <- required_string(haikuports, "upstream_url"),
         {:ok, commands} <- command_list(Map.get(map, "haiku_preflight_commands", @default_haiku_preflight_commands)) do
      {:ok,
       %__MODULE__{
         path: path,
         app_name: string_or_default(map, "app_name", @default_app_name),
         repo_owner: repo_owner,
         repo_name: repo_name,
         release_version: release_version,
         package_revision: string_or_default(release, "package_revision", @default_package_revision),
         release_tag_template: string_or_default(release, "tag_template", @default_tag_template),
         asset_name_template: string_or_default(release, "asset_name_template", @default_asset_name_template),
         release_title_template: string_or_default(release, "title_template", @default_release_title_template),
         release_notes_path_template:
           string_or_default(release, "release_notes_path", @default_release_notes_path),
         archive_prefix_template:
           string_or_default(release, "archive_prefix_template", @default_archive_prefix_template),
         haikuports_branch_template:
           string_or_default(haikuports, "branch_template", @default_haikuports_branch_template),
         haikuports_pr_title_template:
           string_or_default(haikuports, "pr_title_template", @default_haikuports_pr_title_template),
         haikuports_pr_body_template:
           string_or_default(haikuports, "pr_body_template", @default_haikuports_pr_body_template),
         haikuports_upstream_url: upstream_url,
         haikuports_fork_url: optional_string(haikuports, "fork_url"),
         haikuports_fork_owner: optional_string(haikuports, "fork_owner"),
         haikuports_checkout_path: optional_string(haikuports, "checkout_path"),
         haikuports_target_branch:
           string_or_default(haikuports, "target_branch", @default_haikuports_target_branch),
         haikuports_port_path:
           string_or_default(haikuports, "port_path", @default_haikuports_port_path),
         haiku_preflight_commands: commands
       }}
    end
  end

  def with_repo_paths(%__MODULE__{} = manifest, repo_root) do
    %{
      manifest
      | haikuports_checkout_path: expand_repo_path(manifest.haikuports_checkout_path, repo_root)
    }
  end

  def validate_release_ready!(%__MODULE__{} = manifest) do
    required = [
      {"haikuports.fork_url", manifest.haikuports_fork_url},
      {"haikuports.fork_owner", manifest.haikuports_fork_owner},
      {"haikuports.checkout_path", manifest.haikuports_checkout_path}
    ]

    case Enum.find(required, fn {_label, value} -> Util.blank?(value) end) do
      nil ->
        manifest

      {label, _value} ->
        raise ArgumentError,
              "rollout manifest plus local config is missing required value #{label}; run scripts/release_haiku_rollout.sh init and fill in your local override"
    end
  end

  defp required_map(map, key) do
    case Map.get(map, key) do
      value when is_map(value) -> {:ok, value}
      _ -> {:error, "rollout manifest #{inspect(key)} must be a map"}
    end
  end

  defp required_string(map, key) do
    case Map.get(map, key) do
      value when is_binary(value) and value != "" -> {:ok, value}
      _ -> {:error, "rollout manifest is missing required string #{key}"}
    end
  end

  defp string_or_default(map, key, default) do
    case Map.get(map, key) do
      value when is_binary(value) and value != "" -> value
      _ -> default
    end
  end

  defp optional_string(map, key) do
    case Map.get(map, key) do
      value when is_binary(value) and value != "" -> value
      _ -> nil
    end
  end

  defp expand_repo_path(nil, _repo_root), do: nil

  defp expand_repo_path(path, repo_root) do
    if Path.type(path) == :absolute do
      path
    else
      Path.expand(path, repo_root)
    end
  end

  defp command_list(commands) when is_list(commands) do
    normalized =
      Enum.map(commands, fn
        command when is_list(command) and command != [] ->
          if Enum.all?(command, &is_binary/1) do
            command
          else
            {:invalid, command}
          end

        invalid ->
          {:invalid, invalid}
      end)

    case Enum.find(normalized, &match?({:invalid, _}, &1)) do
      nil -> {:ok, normalized}
      {:invalid, invalid} -> {:error, "invalid haiku_preflight_commands entry: #{inspect(invalid)}"}
    end
  end

  defp command_list(_), do: {:error, "haiku_preflight_commands must be a list of argv lists"}
end
