defmodule HemeraHaikuRollout.HaikuPorts do
  alias HemeraHaikuRollout.Executor

  def ensure_checkout!(executor, config) do
    checkout = config.haikuports_checkout_path

    if File.dir?(Path.join(checkout, ".git")) do
      :ok
    else
      File.mkdir_p!(Path.dirname(checkout))
      Executor.run!(executor, "git", ["clone", config.haikuports_fork_url, checkout])
    end
  end

  def refresh_master!(executor, config) do
    checkout = config.haikuports_checkout_path
    ensure_remote!(executor, checkout, "origin", config.haikuports_fork_url)
    ensure_remote!(executor, checkout, "upstream", config.haikuports_upstream_url)
    Executor.run!(executor, "git", ["fetch", "upstream"], cwd: checkout)
    Executor.run!(executor, "git", ["checkout", config.haikuports_target_branch], cwd: checkout)
    Executor.run!(executor, "git", ["reset", "--hard", "upstream/#{config.haikuports_target_branch}"], cwd: checkout)
  end

  def checkout_branch!(executor, config, branch_name) do
    checkout = config.haikuports_checkout_path
    Executor.run!(executor, "git", ["checkout", "-B", branch_name], cwd: checkout)
  end

  def sync_port_tree!(config, context, recipe_body) do
    checkout = config.haikuports_checkout_path
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

  def commit_changes!(executor, config, context) do
    checkout = config.haikuports_checkout_path
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
        ["commit", "-m", context.pr_title],
        cwd: checkout
      )
    end
  end

  def push_branch!(executor, config, context) do
    Executor.run!(
      executor,
      "git",
      ["push", "--force-with-lease", "origin", context.haikuports_branch],
      cwd: config.haikuports_checkout_path
    )
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
end
