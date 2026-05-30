defmodule HemeraHaikuRollout.Doctor do
  alias HemeraHaikuRollout.Executor

  def run(config, opts \\ []) do
    executor = Keyword.get(opts, :executor, HemeraHaikuRollout.SystemExecutor)

    for binary <- ~w(git gh mix elixir) do
      unless System.find_executable(binary) do
        raise ArgumentError, "missing required executable: #{binary}"
      end
    end

    Executor.run!(executor, "gh", ["auth", "status"])

    clean_repo!(executor, HemeraHaikuRollout.repo_root(), "Hemera repo")
    validate_haikuports_checkout!(executor, config)

    IO.puts("doctor: ok")
  end

  defp validate_haikuports_checkout!(executor, config) do
    checkout = config.haikuports_checkout_path

    if File.dir?(Path.join(checkout, ".git")) do
      clean_repo!(executor, checkout, "HaikuPorts checkout")
      remote_matches!(executor, checkout, "origin", config.haikuports_fork_url)
      remote_matches!(executor, checkout, "upstream", config.haikuports_upstream_url)
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
