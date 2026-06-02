defmodule HemeraHaikuRollout.GitHub do
  alias HemeraHaikuRollout.Executor
  alias HemeraHaikuRollout.Util

  def ensure_auth!(executor) do
    Executor.run!(executor, "gh", ["auth", "status"])
  end

  def current_login(executor) do
    result = Executor.invoke(executor, "gh", ["api", "user"], [])

    if result.status == 0 do
      case Jason.decode(result.stdout) do
        {:ok, %{"login" => login}} when is_binary(login) and login != "" -> login
        _ -> nil
      end
    end
  rescue
    _error -> nil
  end

  def current_login!(executor) do
    case current_login(executor) do
      login when is_binary(login) and login != "" ->
        login

      _ ->
        raise ArgumentError, "unable to resolve GitHub login from gh; run gh auth login"
    end
  end

  def ensure_release!(executor, context, target_sha) do
    if release_exists?(executor, context) do
      Executor.run!(
        executor,
        "gh",
        [
          "release",
          "edit",
          context.tag,
          "--repo",
          context.repo_slug,
          "--title",
          context.release_title,
          "--notes-file",
          context.release_notes_path
        ]
      )
    else
      Executor.run!(
        executor,
        "gh",
        [
          "release",
          "create",
          context.tag,
          "--repo",
          context.repo_slug,
          "--target",
          target_sha,
          "--title",
          context.release_title,
          "--notes-file",
          context.release_notes_path
        ]
      )
    end

    Executor.run!(
      executor,
      "gh",
      [
        "release",
        "upload",
        context.tag,
        context.artifact_path,
        "--clobber",
        "--repo",
        context.repo_slug
      ]
    )

    release = fetch_release!(executor, context.repo_slug, context.tag)
    asset_url =
      release
      |> Map.get("assets", [])
      |> Enum.find_value(fn asset ->
        if asset["name"] == context.asset_name, do: asset["browser_download_url"]
      end)

    %{
      url: release["html_url"],
      asset_url: asset_url
    }
  end

  def pull_request_command(context) do
    args = [
      "--editor",
      "--repo #{Util.shell_escape(context.haikuports_repo_slug)}",
      "--base #{Util.shell_escape(context.haikuports_target_branch)}",
      "--head #{Util.shell_escape("#{context.haikuports_fork_owner}:#{context.haikuports_branch}")}"
    ]

    ["gh pr create \\"]
    |> Kernel.++(
      args
      |> Enum.with_index()
      |> Enum.map(fn {argument, index} ->
        suffix = if index == length(args) - 1, do: "", else: " \\"
        "  #{argument}#{suffix}"
      end)
    )
    |> Enum.join("\n")
  end

  def pull_request_template_warning do
    "Confirm the HaikuPorts contributor checklist template appears in the editor. If it does not, abort without submitting and open the PR manually so the template is preserved."
  end

  def pull_request_handoff_markdown(context, pr_create_command, watch_command) do
    """
    # HaikuPorts PR Handoff

    Run this command to open the PR in your editor:

    ```sh
    #{pr_create_command}
    ```

    Suggested PR title:

    `#{context.suggested_pr_title}`

    Optional rollout notes:

    #{context.pr_notes}

    Warning:

    #{pull_request_template_warning()}

    After the PR exists, watch it with:

    ```sh
    #{watch_command}
    ```
    """
    |> String.trim()
    |> Kernel.<>("\n")
  end

  def watch_command(repo_slug, pr_number) do
    "gh pr checks #{pr_number} --repo #{Util.shell_escape(repo_slug)} --watch"
  end

  def resolve_pr!(executor, repo_slug, identifier) do
    if Regex.match?(~r/^\d+$/, identifier) do
      String.to_integer(identifier)
    else
      result =
        Executor.run!(
          executor,
          "gh",
          [
            "pr",
            "list",
            "--repo",
            repo_slug,
            "--head",
            identifier,
            "--json",
            "number"
          ]
        )

      case Jason.decode!(result.stdout) do
        [%{"number" => number} | _] -> number
        _ -> raise ArgumentError, "no PR found for #{identifier}"
      end
    end
  end

  def find_pull_request(executor, context) do
    list_pull_requests(executor, context.haikuports_repo_slug, context.haikuports_branch)
    |> Enum.find(fn pr ->
      pr["headRefName"] == context.haikuports_branch and
        get_in(pr, ["headRepositoryOwner", "login"]) == context.haikuports_fork_owner
    end)
  end

  def github_owner_from_repo_url(nil), do: nil

  def github_owner_from_repo_url(url) when is_binary(url) do
    normalized = String.trim_trailing(url, ".git")

    cond do
      Regex.match?(~r|^git@github\.com:[^/]+/[^/]+$|, normalized) ->
        normalized
        |> String.replace_prefix("git@github.com:", "")
        |> String.split("/", parts: 2)
        |> List.first()

      Regex.match?(~r|^https?://github\.com/[^/]+/[^/]+$|, normalized) ->
        normalized
        |> String.replace_prefix("https://github.com/", "")
        |> String.replace_prefix("http://github.com/", "")
        |> String.split("/", parts: 2)
        |> List.first()

      true ->
        nil
    end
  end

  defp release_exists?(executor, context) do
    result =
      Executor.invoke(
        executor,
        "gh",
        ["release", "view", context.tag, "--repo", context.repo_slug],
        []
      )

    result.status == 0
  end

  defp list_pull_requests(executor, repo_slug, branch) do
    result =
      Executor.run!(
        executor,
        "gh",
        [
          "pr",
          "list",
          "--repo",
          repo_slug,
          "--head",
          branch,
          "--json",
          "number,url,headRefName,headRepositoryOwner"
        ]
      )

    Jason.decode!(result.stdout)
  end

  defp fetch_release!(executor, repo_slug, tag) do
    endpoint = "repos/#{repo_slug}/releases/tags/#{URI.encode(tag, &URI.char_unreserved?/1)}"

    executor
    |> Executor.run!("gh", ["api", endpoint])
    |> Map.fetch!(:stdout)
    |> Jason.decode!()
  end
end
