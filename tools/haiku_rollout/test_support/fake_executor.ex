defmodule HemeraHaikuRollout.TestSupport.FakeExecutor do
  @behaviour HemeraHaikuRollout.Executor

  alias HemeraHaikuRollout.CommandResult

  def start_link(script) do
    Agent.start_link(fn -> {script, []} end)
  end

  def commands(agent) do
    Agent.get(agent, fn {_script, commands} -> Enum.reverse(commands) end)
  end

  @impl true
  def run(program, args, opts) do
    agent = Keyword.fetch!(opts, :executor_state)
    cwd = Keyword.get(opts, :cwd)

    Agent.get_and_update(agent, fn {script, commands} ->
      case script do
        [%{program: ^program, args: ^args} = step | rest] ->
          result =
            %CommandResult{
              program: program,
              args: args,
              status: Map.get(step, :status, 0),
              stdout: Map.get(step, :stdout, "")
            }

          {result, {rest, [%{program: program, args: args, cwd: cwd} | commands]}}

        [step | _rest] ->
          raise "unexpected command #{inspect({program, args})}, expected #{inspect({step.program, step.args})}"

        [] ->
          raise "unexpected command with empty script: #{inspect({program, args})}"
      end
    end)
  end
end
