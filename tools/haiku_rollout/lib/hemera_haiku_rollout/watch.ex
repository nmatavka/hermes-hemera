defmodule HemeraHaikuRollout.Watch do
  alias HemeraHaikuRollout.Executor
  alias HemeraHaikuRollout.GitHub

  def run(config, identifier, opts \\ []) do
    executor = Keyword.get(opts, :executor, HemeraHaikuRollout.SystemExecutor)
    dry_run = Keyword.get(opts, :dry_run, false)
    repo_slug = HemeraHaikuRollout.Config.haikuports_repo_slug(config)

    if dry_run do
      IO.puts("watch: gh pr checks #{identifier} --repo #{repo_slug} --watch")
      :ok
    else
      pr_number = GitHub.resolve_pr!(executor, repo_slug, identifier)
      Executor.run!(executor, "gh", ["pr", "checks", Integer.to_string(pr_number), "--repo", repo_slug, "--watch"])
      summary =
        Executor.run!(
          executor,
          "gh",
          ["pr", "view", Integer.to_string(pr_number), "--repo", repo_slug, "--json", "url,state"]
        )

      IO.write(summary.stdout)
    end
  end
end
