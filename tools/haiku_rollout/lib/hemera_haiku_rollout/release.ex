defmodule HemeraHaikuRollout.Release do
  alias HemeraHaikuRollout.BranchPushDivergedError
  alias HemeraHaikuRollout.Executor
  alias HemeraHaikuRollout.GitHub
  alias HemeraHaikuRollout.HaikuPorts
  alias HemeraHaikuRollout.Host
  alias HemeraHaikuRollout.Recipe
  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.RepoVersion
  alias HemeraHaikuRollout.State
  alias HemeraHaikuRollout.Submission

  @ordered_steps [
    {:repo_version_validated, "repo version validated"},
    {:haiku_preflight, "haiku preflight"},
    {:tag_ensured, "git tag ensured"},
    {:source_tarball_built, "source tarball built"},
    {:github_release_ensured, "GitHub release ensured"},
    {:checksum_computed, "checksum computed"},
    {:recipe_rendered, "recipe rendered"},
    {:haikuports_checkout_refreshed, "HaikuPorts checkout refreshed"},
    {:branch_checked_out, "HaikuPorts branch checked out"},
    {:port_tree_synced, "port tree synced"},
    {:commit_created, "commit created"},
    {:branch_pushed, "branch pushed"},
    {:pr_command_prepared, "PR command prepared"},
    {:pr_discovered, "PR discovered or pending operator handoff"},
    {:watch_started, "watch started / last result"}
  ]

  def step_order, do: @ordered_steps

  def plan(workspace, version, host \\ Host.current()) do
    context = ReleaseContext.build(workspace, version)

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
        "validate repo version surfaces for #{context.version}",
        "ensure release tag #{context.tag}",
        "build source tarball #{context.artifact_path}",
        "create or update GitHub release #{context.tag}",
        "render #{context.recipe_output_name} from #{Path.basename(context.recipe_template_path)}",
        "clone or refresh HaikuPorts checkout at #{context.haikuports_checkout_path}",
        "write port tree to #{context.haikuports_port_path}",
        "commit and push #{context.haikuports_branch}",
        "print exact gh pr create handoff command",
        "optionally discover an already-open PR by branch"
      ]
  end

  def run(workspace, version, opts \\ []) do
    executor = Keyword.get(opts, :executor, HemeraHaikuRollout.SystemExecutor)
    host = Keyword.get(opts, :host, Host.current())
    dry_run = Keyword.get(opts, :dry_run, false)
    mode = Keyword.get(opts, :mode, :release)
    version_context = ReleaseContext.build(workspace, version)

    if dry_run do
      Enum.each(plan(workspace, version, host), &IO.puts/1)
      :ok
    else
      File.mkdir_p!(version_context.work_dir)
      File.mkdir_p!(version_context.artifact_dir)

      run_step!(version_context, :repo_version_validated, fn ->
        RepoVersion.validate!(version_context)
        %{version: version_context.version, package_version: version_context.package_version}
      end)

      GitHub.ensure_auth!(executor)
      submission = workspace |> Submission.resolve(executor: executor) |> Submission.validate_ready!()
      workspace = submission.workspace
      context = ReleaseContext.build(workspace, version)
      File.mkdir_p!(context.work_dir)
      File.mkdir_p!(context.artifact_dir)
      Recipe.validate_template!(workspace)

      run_resume_step!(context, mode, :haiku_preflight, fn _step ->
        maybe_run_haiku_preflight!(executor, host, context)
        %{host: host, skipped: !Host.haiku?(host)}
      end)

      tag_step =
        run_resume_step!(context, mode, :tag_ensured, fn _step ->
          %{target_sha: ensure_tag!(executor, context)}
        end)

      run_resume_step!(context, mode, :source_tarball_built, fn _step ->
        build_source_tarball!(executor, context)
        %{artifact_path: context.artifact_path}
      end, fn step ->
        File.exists?(step["artifact_path"] || context.artifact_path)
      end)

      run_resume_step!(context, mode, :github_release_ensured, fn _step ->
        GitHub.ensure_release!(executor, context, tag_step["target_sha"])
      end)

      checksum_step =
        run_resume_step!(context, mode, :checksum_computed, fn _step ->
          %{sha256: Recipe.checksum!(context.artifact_path)}
        end)

      recipe_step =
        run_resume_step!(context, mode, :recipe_rendered, fn _step ->
          recipe_body = Recipe.render!(context, checksum_step["sha256"])
          Recipe.validate_rendered!(workspace, recipe_body)
          File.write!(context.rendered_recipe_path, recipe_body)
          %{recipe_path: context.rendered_recipe_path}
        end, fn step ->
          File.exists?(step["recipe_path"] || context.rendered_recipe_path)
        end)

      recipe_body = File.read!(recipe_step["recipe_path"] || context.rendered_recipe_path)

      run_resume_step!(context, mode, :haikuports_checkout_refreshed, fn _step ->
        HaikuPorts.ensure_checkout!(executor, context)
        HaikuPorts.refresh_master!(executor, context)
        %{checkout_path: context.haikuports_checkout_path}
      end)

      run_resume_step!(context, mode, :branch_checked_out, fn _step ->
        HaikuPorts.checkout_branch!(executor, context, context.haikuports_branch)
        %{branch: context.haikuports_branch}
      end)

      run_resume_step!(context, mode, :port_tree_synced, fn _step ->
        recipe_path = HaikuPorts.sync_port_tree!(context, recipe_body)
        %{recipe_path: recipe_path, port_path: context.haikuports_port_path}
      end, fn step ->
        File.exists?(
          step["recipe_path"] ||
            Path.join([
              context.haikuports_checkout_path,
              context.haikuports_port_path,
              context.recipe_output_name
            ])
        )
      end)

      run_resume_step!(context, mode, :commit_created, fn _step ->
        %{commit_status: HaikuPorts.commit_changes!(executor, context)}
      end)

      run_resume_step!(context, mode, :branch_pushed, fn _step ->
        case HaikuPorts.push_branch!(executor, context) do
          {:ok, attrs} ->
            attrs

          {:diverged, attrs} ->
            pr = GitHub.find_pull_request(executor, context)

            raise BranchPushDivergedError,
              message: branch_push_diverged_message(context, attrs, pr),
              branch: context.haikuports_branch,
              checkout_path: context.haikuports_checkout_path,
              local_sha: attrs.local_sha,
              tracked_remote_sha: attrs.tracked_remote_sha,
              remote_sha: attrs.remote_sha,
              pr_number: pr && pr["number"],
              pr_url: pr && pr["url"],
              recovery_commands: branch_push_recovery_commands(context)
        end
      end)

      pr_command_step =
        run_resume_step!(context, mode, :pr_command_prepared, fn _step ->
          %{
            pr_create_command: GitHub.pull_request_command(context),
            watch_command: "scripts/release_haiku_rollout.sh watch #{context.haikuports_branch}"
          }
        end)

      pr_step =
        run_step!(context, :pr_discovered, fn ->
          case GitHub.find_pull_request(executor, context) do
            nil ->
              %{
                status: "pending",
                branch: context.haikuports_branch,
                pr_create_command: pr_command_step["pr_create_command"],
                watch_command: pr_command_step["watch_command"]
              }

            pr ->
              %{
                status: "completed",
                pr_number: pr["number"],
                pr_url: pr["url"],
                watch_command: GitHub.watch_command(context.haikuports_repo_slug, pr["number"])
              }
          end
        end)

      print_pr_handoff(pr_command_step, pr_step)
      :ok
    end
  end

  defp run_resume_step!(context, mode, step, fun, present? \\ fn _step -> true end) do
    state = State.load(context)
    existing = State.step(state, step)

    if mode == :resume and State.completed?(state, step) and present?.(existing) do
      existing
    else
      run_step!(context, step, fn -> fun.(existing) end)
    end
  end

  defp run_step!(context, step, fun) do
    try do
      result =
        fun.()
        |> normalize_result()

      status = Map.get(result, :status, "completed")
      State.put_step!(context, step, Map.put(result, :status, status))
      State.step(State.load(context), step)
    rescue
      error in BranchPushDivergedError ->
        State.put_step!(
          context,
          step,
          %{
            status: "failed",
            error: Exception.message(error),
            branch: error.branch,
            checkout_path: error.checkout_path,
            local_sha: error.local_sha,
            tracked_remote_sha: error.tracked_remote_sha,
            remote_sha: error.remote_sha,
            pr_number: error.pr_number,
            pr_url: error.pr_url,
            recovery_commands: error.recovery_commands
          }
        )

        reraise error, __STACKTRACE__

      error ->
        State.put_step!(context, step, %{status: "failed", error: Exception.message(error)})
        reraise error, __STACKTRACE__
    end
  end

  defp maybe_run_haiku_preflight!(executor, host, context) do
    if Host.haiku?(host) do
      Enum.each(context.haiku_preflight_commands, fn [program | args] ->
        Executor.run!(executor, program, args, cwd: context.workspace.root)
      end)
    end
  end

  defp ensure_tag!(executor, context) do
    root = context.workspace.root
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
    root = context.workspace.root

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

  defp normalize_result(nil), do: %{}
  defp normalize_result(result) when is_map(result), do: result
  defp normalize_result(other), do: %{value: other}

  defp branch_push_diverged_message(context, attrs, pr) do
    pr_line =
      case pr do
        %{"url" => url, "number" => number} ->
          "An open HaikuPorts PR already exists for this branch: ##{number} #{url}\n"

        _ ->
          "No open HaikuPorts PR was found for this branch.\n"
      end

    """
    HaikuPorts branch #{context.haikuports_branch} already exists on origin at #{attrs.remote_sha}, but the local branch is #{attrs.local_sha}.
    Last known local origin/#{context.haikuports_branch} was #{attrs.tracked_remote_sha || "(none)"}.
    #{pr_line}Review the divergence before rerunning the rollout.
    """
    |> String.trim()
  end

  defp branch_push_recovery_commands(context) do
    checkout = context.haikuports_checkout_path
    branch = context.haikuports_branch

    [
      "git -C #{HemeraHaikuRollout.Util.shell_escape(checkout)} log --oneline origin/#{branch}..#{branch}",
      "git -C #{HemeraHaikuRollout.Util.shell_escape(checkout)} log --oneline #{branch}..origin/#{branch}",
      "git -C #{HemeraHaikuRollout.Util.shell_escape(checkout)} push --force-with-lease origin #{branch}",
      "git -C #{HemeraHaikuRollout.Util.shell_escape(checkout)} push origin :#{branch}"
    ]
  end

  defp print_pr_handoff(pr_command_step, pr_step) do
    case pr_step["status"] do
      "completed" ->
        IO.puts("discovered existing PR: #{pr_step["pr_url"]}")
        IO.puts("watch next with:")
        IO.puts(pr_step["watch_command"])

      _ ->
        IO.puts("HaikuPorts branch pushed. Run this command to open the PR:")
        IO.puts("")
        IO.puts(pr_command_step["pr_create_command"])
        IO.puts("")
        IO.puts("After the PR exists, watch it with:")
        IO.puts(pr_command_step["watch_command"])
    end
  end
end
