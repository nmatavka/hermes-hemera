defmodule HemeraHaikuRollout.SystemExecutor do
  @behaviour HemeraHaikuRollout.Executor

  alias HemeraHaikuRollout.CommandResult

  @impl true
  def run(program, args, opts) do
    cwd = Keyword.get(opts, :cwd)
    env = Keyword.get(opts, :env, [])

    {stdout, status} =
      System.cmd(program, args,
        cwd: cwd,
        env: env,
        stderr_to_stdout: true
      )

    %CommandResult{
      program: program,
      args: args,
      status: status,
      stdout: stdout
    }
  end
end
