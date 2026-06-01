defmodule HemeraHaikuRollout.Doctor do
  alias HemeraHaikuRollout.Executor
  alias HemeraHaikuRollout.GitHub
  alias HemeraHaikuRollout.Recipe
  alias HemeraHaikuRollout.ReleaseContext
  alias HemeraHaikuRollout.RepoVersion
  alias HemeraHaikuRollout.Submission

  def run(workspace, opts \\ []) do
    executor = Keyword.get(opts, :executor, HemeraHaikuRollout.SystemExecutor)

    for binary <- ~w(git gh mix elixir) do
      unless System.find_executable(binary) do
        raise ArgumentError, "missing required executable: #{binary}"
      end
    end

    unless File.exists?(workspace.manifest_path) do
      raise ArgumentError, "missing rollout manifest at #{workspace.manifest_path}"
    end

    context = ReleaseContext.build(workspace, workspace.version)
    RepoVersion.validate!(context)
    GitHub.ensure_auth!(executor)
    github_login = GitHub.current_login!(executor)
    submission =
      workspace
      |> Submission.resolve(executor: executor, gh_login: github_login)
      |> Submission.validate_ready!()
    workspace = submission.workspace

    Recipe.validate_template!(workspace)
    rendered = Recipe.render!(ReleaseContext.build(workspace, workspace.version), String.duplicate("a", 64))
    Recipe.validate_rendered!(workspace, rendered)

    clean_repo!(executor, workspace.root, "Hemera repo")
    validate_haikuports_checkout!(executor, workspace)

    IO.puts("doctor: ok (GitHub login: #{github_login})")
  end

  defp validate_haikuports_checkout!(executor, workspace) do
    checkout = workspace.manifest.haikuports_checkout_path

    if File.dir?(Path.join(checkout, ".git")) do
      clean_repo!(executor, checkout, "HaikuPorts checkout")
      remote_matches!(executor, checkout, "origin", workspace.manifest.haikuports_fork_url)
      remote_matches!(executor, checkout, "upstream", workspace.manifest.haikuports_upstream_url)
    else
      parent = Path.dirname(checkout)

      unless File.dir?(parent) do
        raise ArgumentError, "HaikuPorts checkout parent directory does not exist: #{parent}"
      end
    end
  end

  defp clean_repo!(executor, path, label) do
    result = Executor.run!(executor, "git", ["status", "--porcelain"], cwd: path)

    unless String.trim(result.stdout) == "" do
      raise ArgumentError, "#{label} is dirty: #{path}"
    end
  end

  defp remote_matches!(executor, checkout, remote, expected_url) do
    result = Executor.run!(executor, "git", ["remote", "get-url", remote], cwd: checkout)

    unless String.trim(result.stdout) == expected_url do
      raise ArgumentError, "remote #{remote} for #{checkout} is #{String.trim(result.stdout)}, expected #{expected_url}"
    end
  end
end
