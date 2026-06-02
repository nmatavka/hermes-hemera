defmodule HemeraHaikuRollout.HaikuPorts do
  alias HemeraHaikuRollout.Executor

  def ensure_checkout!(executor, context) do
    checkout = context.haikuports_checkout_path

    if File.dir?(Path.join(checkout, ".git")) do
      :ok
    else
      File.mkdir_p!(Path.dirname(checkout))
      Executor.run!(executor, "git", ["clone", context.haikuports_fork_url, checkout])
    end
  end

  def refresh_master!(executor, context) do
    checkout = context.haikuports_checkout_path
    ensure_remote!(executor, checkout, "origin", context.haikuports_fork_url)
    ensure_remote!(executor, checkout, "upstream", context.haikuports_upstream_url)
    Executor.run!(executor, "git", ["fetch", "upstream"], cwd: checkout)
    Executor.run!(executor, "git", ["checkout", context.haikuports_target_branch], cwd: checkout)
    Executor.run!(executor, "git", ["reset", "--hard", "upstream/#{context.haikuports_target_branch}"], cwd: checkout)
  end

  def checkout_branch!(executor, context, branch_name) do
    checkout = context.haikuports_checkout_path
    Executor.run!(executor, "git", ["checkout", "-B", branch_name], cwd: checkout)
  end

  def sync_port_tree!(context, recipe_body) do
    checkout = context.haikuports_checkout_path
    destination = Path.join(checkout, context.haikuports_port_path)
    File.rm_rf!(destination)
    File.mkdir_p!(destination)

    for path <- Path.wildcard(Path.join(context.port_template_dir, "**/*")) do
      if File.regular?(path) and Path.extname(path) != ".in" do
        relative = Path.relative_to(path, context.port_template_dir)
        target = Path.join(destination, relative)
        File.mkdir_p!(Path.dirname(target))
        File.cp!(path, target)
      end
    end

    recipe_path = Path.join(destination, context.recipe_output_name)
    File.write!(recipe_path, recipe_body)
    recipe_path
  end

  def commit_changes!(executor, context) do
    checkout = context.haikuports_checkout_path
    relative_port_path = context.haikuports_port_path

    status =
      Executor.run!(
        executor,
        "git",
        ["status", "--porcelain", "--", relative_port_path],
        cwd: checkout
      )

    if String.trim(status.stdout) == "" do
      :noop
    else
      Executor.run!(executor, "git", ["add", relative_port_path], cwd: checkout)
      Executor.run!(
        executor,
        "git",
        ["commit", "-m", context.suggested_pr_title],
        cwd: checkout
      )
      :committed
    end
  end

  def push_branch!(executor, context) do
    checkout = context.haikuports_checkout_path
    branch = context.haikuports_branch
    local_sha = rev_parse(executor, checkout, branch)
    tracked_remote_sha = maybe_rev_parse(executor, checkout, "refs/remotes/origin/#{branch}")

    Executor.run!(executor, "git", ["fetch", "--prune", "origin"], cwd: checkout)

    remote_sha = maybe_rev_parse(executor, checkout, "refs/remotes/origin/#{branch}")

    cond do
      is_nil(remote_sha) ->
        Executor.run!(
          executor,
          "git",
          ["push", "--set-upstream", "origin", branch],
          cwd: checkout
        )

        {:ok,
         %{
           branch: branch,
           push_status: "pushed",
           local_sha: local_sha,
           tracked_remote_sha: tracked_remote_sha,
           remote_sha: local_sha
         }}

      remote_sha == local_sha ->
        {:ok,
         %{
           branch: branch,
           push_status: "already_pushed",
           local_sha: local_sha,
           tracked_remote_sha: tracked_remote_sha,
           remote_sha: remote_sha
         }}

      true ->
        {:diverged,
         %{
           branch: branch,
           push_status: "diverged",
           local_sha: local_sha,
           tracked_remote_sha: tracked_remote_sha,
           remote_sha: remote_sha
         }}
    end
  end

  defp ensure_remote!(executor, checkout, name, url) do
    current = Executor.invoke(executor, "git", ["remote", "get-url", name], cwd: checkout)

    cond do
      current.status == 0 and String.trim(current.stdout) == url ->
        :ok

      current.status == 0 ->
        Executor.run!(executor, "git", ["remote", "set-url", name, url], cwd: checkout)

      true ->
        Executor.run!(executor, "git", ["remote", "add", name, url], cwd: checkout)
    end
  end

  defp rev_parse(executor, checkout, ref) do
    executor
    |> Executor.run!("git", ["rev-parse", ref], cwd: checkout)
    |> Map.fetch!(:stdout)
    |> String.trim()
  end

  defp maybe_rev_parse(executor, checkout, ref) do
    result = Executor.invoke(executor, "git", ["rev-parse", "--verify", ref], cwd: checkout)

    if result.status == 0 do
      String.trim(result.stdout)
    end
  end
end
