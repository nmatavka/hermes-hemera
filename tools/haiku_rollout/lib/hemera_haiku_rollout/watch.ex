defmodule HemeraHaikuRollout.Watch do
  alias HemeraHaikuRollout.CommandError
  alias HemeraHaikuRollout.Executor
  alias HemeraHaikuRollout.GitHub
  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.State

  def run(workspace, identifier, opts \\ []) do
    executor = Keyword.get(opts, :executor, HemeraHaikuRollout.SystemExecutor)
    dry_run = Keyword.get(opts, :dry_run, false)
    version = Keyword.get(opts, :version, workspace.version)
    context = ReleaseContext.build(workspace, version)
    repo_slug = context.haikuports_repo_slug

    if dry_run do
      IO.puts("watch: gh pr checks #{identifier} --repo #{repo_slug} --watch")
      :ok
    else
      pr_number = GitHub.resolve_pr!(executor, repo_slug, identifier)
      watch_result = maybe_watch_checks!(executor, repo_slug, pr_number)
      summary =
        Executor.run!(
          executor,
          "gh",
          ["pr", "view", Integer.to_string(pr_number), "--repo", repo_slug, "--json", "url,state"]
        )

      decoded_summary = Jason.decode!(summary.stdout)
      State.put_step!(context, :watch_started, %{
        status: "completed",
        pr_number: pr_number,
        pr_url: decoded_summary["url"],
        pr_state: decoded_summary["state"],
        watch_status: watch_result
      })

      IO.write(summary.stdout)
      decoded_summary
    end
  end

  defp maybe_watch_checks!(executor, repo_slug, pr_number) do
    result =
      Executor.invoke(
        executor,
        "gh",
        ["pr", "checks", Integer.to_string(pr_number), "--repo", repo_slug, "--watch"],
        []
      )

    cond do
      result.status == 0 ->
        "checks_completed"

      String.contains?(result.stdout, "no checks reported") ->
        IO.puts(String.trim(result.stdout))
        "no_checks_reported"

      true ->
        raise CommandError,
          message: "command failed: #{Enum.join([result.program | result.args], " ")}",
          program: result.program,
          args: result.args,
          status: result.status,
          stdout: result.stdout
    end
  end
end
