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
        result =
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

        String.trim(result.stdout)

      pr ->
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
          "#{config.haikuports_fork_owner}:#{context.haikuports_branch}",
          "--json",
          "number,url"
        ]
      )

    case Jason.decode!(result.stdout) do
      [pr | _] -> pr
      _ -> nil
    end
  end
end
