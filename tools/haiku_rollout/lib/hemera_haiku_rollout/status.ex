defmodule HemeraHaikuRollout.Status do
  alias HemeraHaikuRollout.{Host, Release, ReleaseContext, State, Submission}

  def run(workspace, version \\ nil, opts \\ []) do
    executor = Keyword.get(opts, :executor, HemeraHaikuRollout.SystemExecutor)
    submission = Submission.resolve(workspace, executor: executor)
    workspace = submission.workspace
    context = ReleaseContext.build(workspace, version || workspace.version)
    state = State.load(context)
    host = Host.current()
    last_completed = State.last_completed_step(state, Release.step_order())
    last_failed = State.last_failed_step(state, Release.step_order())
    next_pending = State.next_pending_step(state, Release.step_order())
    branch_step = State.step(state, :branch_pushed)

    IO.puts("Repo root: #{workspace.root}")
    IO.puts("Manifest: #{workspace.manifest_path}")
    IO.puts("Local config: #{workspace.local_config_path}")
    IO.puts("Version: #{context.version}")
    IO.puts("Work dir: #{context.work_dir}")
    IO.puts("Host: #{host}")
    IO.puts("Haiku preflight: #{if Host.haiku?(host), do: "enabled", else: "skipped on this host"}")
    IO.puts("Branch: #{context.haikuports_branch}")
    IO.puts("HaikuPorts checkout: #{workspace.manifest.haikuports_checkout_path || "(unresolved)"}")

    case Submission.missing_lines(submission) do
      [] ->
        IO.puts("Missing local keys: (none)")

      missing ->
        IO.puts("Missing local keys:")
        Enum.each(missing, fn line -> IO.puts("  - #{line}") end)
    end

    if pr_step = discovered_pr_step(state) do
      if pr_url = pr_step["pr_url"] do
        IO.puts("PR: #{pr_url}")
      end
    end

    print_branch_state(branch_step)
    IO.puts("Last completed step: #{format_step(last_completed)}")
    IO.puts("Last failed step: #{format_step(last_failed)}")
    IO.puts("Next pending step: #{format_step(next_pending)}")

    case next_command(state) do
      nil -> :ok
      command ->
        IO.puts("Next command:")
        IO.puts(command)
    end
  end

  defp discovered_pr_step(state) do
    step = State.step(state, :pr_discovered)

    if step == %{} do
      nil
    else
      step
    end
  end

  defp next_command(state) do
    branch_step = State.step(state, :branch_pushed)
    pr_step = State.step(state, :pr_discovered)
    pr_command_step = State.step(state, :pr_command_prepared)
    watch_step = State.step(state, :watch_started)

    cond do
      branch_step["status"] == "failed" and branch_step["remote_sha"] && branch_step["local_sha"] ->
        Enum.join(branch_step["recovery_commands"] || [], "\n")

      pr_step["status"] == "completed" and watch_step["status"] != "completed" ->
        pr_step["watch_command"]

      pr_command_step["status"] == "completed" and pr_step["status"] != "completed" ->
        pr_command_step["pr_create_command"]

      true ->
        nil
    end
  end

  defp print_branch_state(%{} = branch_step) when map_size(branch_step) == 0, do: :ok

  defp print_branch_state(branch_step) when is_map(branch_step) do
    branch = branch_step["branch"] || "(unknown)"
    push_status = branch_step["push_status"] || branch_step["status"] || "unknown"

    IO.puts("Branch push status: #{push_status}")
    IO.puts("Branch push branch: #{branch}")

    if local_sha = branch_step["local_sha"] do
      IO.puts("Branch local SHA: #{local_sha}")
    end

    if tracked_remote_sha = branch_step["tracked_remote_sha"] do
      IO.puts("Branch tracked origin SHA: #{tracked_remote_sha}")
    end

    if remote_sha = branch_step["remote_sha"] do
      IO.puts("Branch live origin SHA: #{remote_sha}")
    end

    if pr_url = branch_step["pr_url"] do
      IO.puts("Branch PR: #{pr_url}")
    end

    cond do
      branch_step["status"] == "failed" and branch_step["remote_sha"] && branch_step["local_sha"] ->
        IO.puts("Branch push safety: blocked by remote divergence")

      push_status in ["pushed", "already_pushed"] ->
        IO.puts("Branch push safety: safe")

      true ->
        :ok
    end
  end

  defp print_branch_state(_other), do: :ok

  defp format_step(nil), do: "(none)"
  defp format_step({_step, label}), do: label
end
