defmodule HemeraHaikuRollout.Release do
  alias HemeraHaikuRollout.Executor
  alias HemeraHaikuRollout.GitHub
  alias HemeraHaikuRollout.HaikuPorts
  alias HemeraHaikuRollout.Host
  alias HemeraHaikuRollout.Recipe
  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.RepoVersion

  def plan(config, version, host \\ Host.current()) do
    context = ReleaseContext.build(config, version)

    preflight_steps =
      if Host.haiku?(host) do
        Enum.map(context.haiku_preflight_commands, fn command ->
          "haiku-preflight: #{Enum.join(command, " ")}"
        end)
      else
        ["skip local Haiku preflight on #{host}"]
      end

    preflight_steps ++
      [
        "ensure release tag #{context.tag}",
        "build source tarball #{context.artifact_path}",
        "create or update GitHub release #{context.tag}",
        "upload tarball asset #{context.asset_name}",
        "render #{context.recipe_output_name} from #{Path.basename(context.recipe_template_path)}",
        "clone or refresh HaikuPorts checkout at #{config.haikuports_checkout_path}",
        "write port tree to #{context.haikuports_port_path}",
        "commit, push #{context.haikuports_branch}, and open/update PR",
        "watch HaikuPorts PR checks"
      ]
  end

  def run(config, version, opts \\ []) do
    executor = Keyword.get(opts, :executor, HemeraHaikuRollout.SystemExecutor)
    host = Keyword.get(opts, :host, Host.current())
    dry_run = Keyword.get(opts, :dry_run, false)
    context = ReleaseContext.build(config, version)
    RepoVersion.validate!(context)

    if dry_run do
      Enum.each(plan(config, version, host), &IO.puts/1)
      :ok
    else
      File.mkdir_p!(context.artifact_dir)

      maybe_run_haiku_preflight!(executor, host, context)

      target_sha = ensure_tag!(executor, context)
      build_source_tarball!(executor, context)
      GitHub.ensure_release!(executor, context, target_sha)
      checksum = Recipe.checksum!(context.artifact_path)
      recipe_body = Recipe.render!(context, checksum)

      HaikuPorts.ensure_checkout!(executor, config)
      HaikuPorts.refresh_master!(executor, config)
      HaikuPorts.checkout_branch!(executor, config, context.haikuports_branch)
      HaikuPorts.sync_port_tree!(config, context, recipe_body)
      HaikuPorts.commit_changes!(executor, config, context)
      HaikuPorts.push_branch!(executor, config, context)
      pr_url = GitHub.ensure_pull_request!(executor, config, context)
      IO.puts("opened or updated PR: #{pr_url}")
      HemeraHaikuRollout.Watch.run(config, context.haikuports_branch, executor: executor)
    end
  end

  defp maybe_run_haiku_preflight!(executor, host, context) do
    if Host.haiku?(host) do
      Enum.each(context.haiku_preflight_commands, fn [program | args] ->
        Executor.run!(executor, program, args, cwd: HemeraHaikuRollout.repo_root())
      end)
    end
  end

  defp ensure_tag!(executor, context) do
    root = HemeraHaikuRollout.repo_root()
    tag_ref = "refs/tags/#{context.tag}"
    tag_result = Executor.invoke(executor, "git", ["rev-parse", "--verify", tag_ref], cwd: root)

    if tag_result.status != 0 do
      Executor.run!(
        executor,
        "git",
        ["tag", "-a", context.tag, "-m", context.release_title],
        cwd: root
      )
    end

    result = Executor.run!(executor, "git", ["rev-parse", "#{context.tag}^{commit}"], cwd: root)
    String.trim(result.stdout)
  end

  defp build_source_tarball!(executor, context) do
    root = HemeraHaikuRollout.repo_root()

    Executor.run!(
      executor,
      "git",
      [
        "archive",
        "--format=tar.gz",
        "--prefix=#{context.archive_prefix}/",
        "-o",
        context.artifact_path,
        context.tag
      ],
      cwd: root
    )
  end
end
