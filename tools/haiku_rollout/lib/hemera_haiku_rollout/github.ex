defmodule HemeraHaikuRollout.GitHub do
  alias HemeraHaikuRollout.Executor

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
  end

  def ensure_pull_request!(executor, config, context) do
    case find_pull_request(executor, config, context) do
      nil ->
        create_pull_request!(executor, config, context)

      pr ->
        edit_pull_request!(executor, context, pr)
    end
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

  defp find_pull_request(executor, config, context) do
    result =
      Executor.run!(
        executor,
        "gh",
        [
          "pr",
          "list",
          "--repo",
          context.haikuports_repo_slug,
          "--head",
          context.haikuports_branch,
          "--json",
          "number,url,headRefName,headRepositoryOwner"
        ]
      )

    result.stdout
    |> Jason.decode!()
    |> Enum.find(fn pr ->
      pr["headRefName"] == context.haikuports_branch and
        get_in(pr, ["headRepositoryOwner", "login"]) == config.haikuports_fork_owner
    end)
  end

  defp create_pull_request!(executor, config, context) do
    result =
      try do
        Executor.run!(
          executor,
          "gh",
          [
            "pr",
            "create",
            "--repo",
            context.haikuports_repo_slug,
            "--base",
            config.haikuports_target_branch,
            "--head",
            "#{config.haikuports_fork_owner}:#{context.haikuports_branch}",
            "--title",
            context.pr_title,
            "--body",
            context.pr_body
          ]
        )
      rescue
        error in HemeraHaikuRollout.CommandError ->
          if String.contains?(error.stdout, "already exists") do
            case find_pull_request(executor, config, context) do
              nil -> reraise error, __STACKTRACE__
              pr -> {:existing, pr}
            end
          else
            reraise error, __STACKTRACE__
          end
      end

    case result do
      {:existing, pr} -> edit_pull_request!(executor, context, pr)
      command_result -> String.trim(command_result.stdout)
    end
  end

  defp edit_pull_request!(executor, context, pr) do
    Executor.run!(
      executor,
      "gh",
      [
        "pr",
        "edit",
        Integer.to_string(pr["number"]),
        "--repo",
        context.haikuports_repo_slug,
        "--title",
        context.pr_title,
        "--body",
        context.pr_body
      ]
    )

    pr["url"]
  end
end
