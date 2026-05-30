defmodule HemeraHaikuRollout.SystemExecutor do
  @behaviour HemeraHaikuRollout.Executor

  alias HemeraHaikuRollout.CommandResult

  @impl true
  def run(program, args, opts) do
    cwd = Keyword.get(opts, :cwd)
    env = Keyword.get(opts, :env, [])
    cmd_opts =
      [env: env, stderr_to_stdout: true]
      |> maybe_put(:cd, cwd)

    {stdout, status} =
      System.cmd(program, args, cmd_opts)

    %CommandResult{
      program: program,
      args: args,
      status: status,
      stdout: stdout
    }
  end

  defp maybe_put(opts, _key, nil), do: opts
  defp maybe_put(opts, key, value), do: Keyword.put(opts, key, value)
end
