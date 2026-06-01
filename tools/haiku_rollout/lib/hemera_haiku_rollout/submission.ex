defmodule HemeraHaikuRollout.Submission do
  alias HemeraHaikuRollout.{Executor, GitHub, Util}

  def resolve(workspace, opts \\ []) do
    executor = Keyword.get(opts, :executor, HemeraHaikuRollout.SystemExecutor)
    gh_login =
      case Keyword.fetch(opts, :gh_login) do
        {:ok, login} -> login
        :error -> GitHub.current_login(executor)
      end
    checkout_path = resolve_checkout_path(workspace)
    checkout_info = inspect_checkout(executor, checkout_path)

    resolved_manifest = %{
      workspace.manifest
      | haikuports_checkout_path: checkout_path,
        haikuports_fork_url:
          first_present([workspace.manifest.haikuports_fork_url, checkout_info.origin_url]),
        haikuports_fork_owner:
          first_present([
            workspace.manifest.haikuports_fork_owner,
            checkout_info.origin_owner,
            gh_login
          ])
    }

    missing =
      [
        missing_checkout_path(resolved_manifest.haikuports_checkout_path),
        missing_fork_url(
          resolved_manifest.haikuports_fork_url,
          resolved_manifest.haikuports_fork_owner || gh_login
        ),
        missing_fork_owner(resolved_manifest.haikuports_fork_owner, gh_login)
      ]
      |> Enum.reject(&is_nil/1)

    %{
      workspace: %{workspace | manifest: resolved_manifest, version: resolved_manifest.release_version},
      gh_login: gh_login,
      checkout_info: checkout_info,
      missing: missing,
      local_config_exists: File.exists?(workspace.local_config_path)
    }
  end

  def validate_ready!(resolution) do
    case resolution.missing do
      [] ->
        resolution

      missing ->
        header =
          "missing required HaikuPorts rollout values:\n" <>
            Enum.map_join(missing, "\n", fn %{key: key, message: message} ->
              "- #{key}: #{message}"
            end)

        suffix =
          if resolution.local_config_exists do
            ""
          else
            "\n\nNo local override file exists yet at #{resolution.workspace.local_config_path}. " <>
              "Run scripts/release_haiku_rollout.sh init if you want a scaffold first."
          end

        raise ArgumentError, header <> suffix
    end
  end

  def missing_lines(resolution) do
    Enum.map(resolution.missing, fn %{key: key, message: message} ->
      "#{key}: #{message}"
    end)
  end

  defp resolve_checkout_path(workspace) do
    first_present([
      workspace.manifest.haikuports_checkout_path,
      obvious_sibling_checkout(workspace.root)
    ])
  end

  defp obvious_sibling_checkout(repo_root) do
    sibling = Path.expand("../haikuports", repo_root)

    if File.dir?(sibling) do
      sibling
    end
  end

  defp inspect_checkout(_executor, nil) do
    %{exists?: false, origin_url: nil, origin_owner: nil}
  end

  defp inspect_checkout(executor, checkout_path) do
    git_dir = Path.join(checkout_path, ".git")

    if File.dir?(git_dir) do
      origin_url = remote_url(executor, checkout_path, "origin")

      %{
        exists?: true,
        origin_url: origin_url,
        origin_owner: GitHub.github_owner_from_repo_url(origin_url)
      }
    else
      %{exists?: false, origin_url: nil, origin_owner: nil}
    end
  end

  defp remote_url(executor, checkout_path, remote) do
    result = Executor.invoke(executor, "git", ["remote", "get-url", remote], cwd: checkout_path)

    if result.status == 0 do
      String.trim(result.stdout)
    end
  rescue
    _error -> nil
  end

  defp missing_checkout_path(value) do
    if Util.blank?(value) do
      %{
        key: "haikuports.checkout_path",
        message:
          "set it with `scripts/release_haiku_rollout.sh config set haikuports.checkout_path /abs/path/to/haikuports`"
      }
    end
  end

  defp missing_fork_url(value, fork_owner_hint) do
    if Util.blank?(value) do
      suggested_owner = fork_owner_hint || "YOUR_GITHUB_OWNER"

      %{
        key: "haikuports.fork_url",
        message:
          "set it with `scripts/release_haiku_rollout.sh config set haikuports.fork_url git@github.com:#{suggested_owner}/haikuports.git`"
      }
    end
  end

  defp missing_fork_owner(value, gh_login) do
    if Util.blank?(value) do
      suggested_owner = gh_login || "YOUR_GITHUB_OWNER"

      %{
        key: "haikuports.fork_owner",
        message:
          "set it with `scripts/release_haiku_rollout.sh config set haikuports.fork_owner #{suggested_owner}`"
      }
    end
  end

  defp first_present(values) do
    Enum.find(values, fn value -> not Util.blank?(value) end)
  end
end
