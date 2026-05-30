defmodule HemeraHaikuRollout.Executor do
  alias HemeraHaikuRollout.CommandError
  alias HemeraHaikuRollout.CommandResult

  @callback run(String.t(), [String.t()], keyword()) :: CommandResult.t()

  def run!(executor, program, args, opts \\ []) do
    result = invoke(executor, program, args, opts)

    if result.status == 0 do
      result
    else
      raise CommandError,
        message: "command failed: #{Enum.join([program | args], " ")}",
        program: program,
        args: args,
        status: result.status,
        stdout: result.stdout
    end
  end

  def invoke({module, state}, program, args, opts) do
    module.run(program, args, Keyword.put(opts, :executor_state, state))
  end

  def invoke(module, program, args, opts) when is_atom(module) do
    module.run(program, args, opts)
  end
end
