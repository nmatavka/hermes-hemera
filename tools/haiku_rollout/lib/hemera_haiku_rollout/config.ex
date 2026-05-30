defmodule HemeraHaikuRollout.Config do
  defstruct [
    :config_path,
    :repo_owner,
    :repo_name,
    :release_tag_template,
    :asset_name_template,
    :release_title_template,
    :release_notes_path_template,
    :haikuports_upstream_url,
    :haikuports_fork_url,
    :haikuports_fork_owner,
    :haikuports_checkout_path,
    :haikuports_target_branch,
    :haikuports_port_path,
    :haiku_preflight_commands
  ]

  @default_tag_template "v<version>"
  @default_asset_name_template "hemera-<version>-source.tar.gz"
  @default_release_title_template "Hemera <version>"
  @default_release_notes_path "docs/release-notes/<version>.md"
  @default_haikuports_target_branch "master"
  @default_haikuports_port_path "mail-client/hemera"
  @default_haiku_preflight_commands [
    ["cmake", "-S", ".", "-B", "build", "-DHERMES_BUILD_HAIKU_SHELL=ON"],
    ["cmake", "--build", "build", "--target", "Hemera"],
    ["ctest", "--test-dir", "build", "--output-on-failure"],
    ["cmake", "--build", "build", "--target", "hemera_hpkg"]
  ]

  def load!(path \\ HemeraHaikuRollout.default_config_path()) do
    case load(path) do
      {:ok, config} -> config
      {:error, message} -> raise ArgumentError, message
    end
  end

  def init(path \\ HemeraHaikuRollout.default_config_path()) do
    destination = Path.expand(path)
    File.mkdir_p!(Path.dirname(destination))

    cond do
      File.exists?(destination) ->
        {:ok, :exists, destination}

      true ->
        File.cp!(HemeraHaikuRollout.example_config_path(), destination)
        {:ok, :created, destination}
    end
  end

  def load(path) do
    if File.exists?(path) do
      with {:ok, yaml} <- read_yaml(path),
           {:ok, config} <- from_map(yaml, path) do
        {:ok, config}
      end
    else
      {:error,
       "missing rollout config at #{path}; copy #{HemeraHaikuRollout.example_config_path()} to config.yml and fill in real values"}
    end
  end

  def from_map(map, path \\ "memory") when is_map(map) do
    with {:ok, github} <- required_map(map, "github"),
         {:ok, haikuports} <- required_map(map, "haikuports"),
         {:ok, repo_owner} <- required_string(github, "repo_owner"),
         {:ok, repo_name} <- required_string(github, "repo_name"),
         {:ok, upstream_url} <- required_string(haikuports, "upstream_url"),
         {:ok, fork_url} <- required_string(haikuports, "fork_url"),
         {:ok, fork_owner} <- required_string(haikuports, "fork_owner"),
         {:ok, checkout_path} <- required_string(haikuports, "checkout_path"),
         {:ok, commands} <- command_list(Map.get(map, "haiku_preflight_commands", @default_haiku_preflight_commands)) do
      {:ok,
       %__MODULE__{
         config_path: path,
         repo_owner: repo_owner,
         repo_name: repo_name,
         release_tag_template: string_or_default(github, "release_tag_template", @default_tag_template),
         asset_name_template: string_or_default(github, "asset_name_template", @default_asset_name_template),
         release_title_template:
           string_or_default(github, "release_title_template", @default_release_title_template),
         release_notes_path_template:
           string_or_default(github, "release_notes_path", @default_release_notes_path),
         haikuports_upstream_url: upstream_url,
         haikuports_fork_url: fork_url,
         haikuports_fork_owner: fork_owner,
         haikuports_checkout_path: expand_repo_path(checkout_path),
         haikuports_target_branch:
           string_or_default(haikuports, "target_branch", @default_haikuports_target_branch),
         haikuports_port_path:
           string_or_default(haikuports, "port_path", @default_haikuports_port_path),
         haiku_preflight_commands: commands
       }}
    end
  end

  def repo_slug(config) do
    "#{config.repo_owner}/#{config.repo_name}"
  end

  def haikuports_repo_slug(config) do
    parse_github_slug(config.haikuports_upstream_url)
  end

  def render_template(template, version) do
    String.replace(template, "<version>", version)
  end

  defp read_yaml(path) do
    case YamlElixir.read_from_file(path) do
      {:ok, map} when is_map(map) -> {:ok, map}
      map when is_map(map) -> {:ok, map}
      other -> {:error, "unable to parse YAML config #{path}: #{inspect(other)}"}
    end
  rescue
    error -> {:error, "unable to parse YAML config #{path}: #{Exception.message(error)}"}
  end

  defp required_map(map, key) do
    case Map.get(map, key) do
      value when is_map(value) -> {:ok, value}
      _ -> {:error, "config is missing required map #{key}"}
    end
  end

  defp required_string(map, key) do
    case Map.get(map, key) do
      value when is_binary(value) and value != "" -> {:ok, value}
      _ -> {:error, "config is missing required string #{key}"}
    end
  end

  defp string_or_default(map, key, default) do
    case Map.get(map, key) do
      value when is_binary(value) and value != "" -> value
      _ -> default
    end
  end

  defp expand_repo_path(path) do
    if Path.type(path) == :absolute do
      path
    else
      Path.expand(path, HemeraHaikuRollout.repo_root())
    end
  end

  defp parse_github_slug(url) do
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
