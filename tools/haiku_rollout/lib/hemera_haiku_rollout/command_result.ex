defmodule HemeraHaikuRollout.CommandResult do
  @enforce_keys [:program, :args, :status, :stdout]
  defstruct [:program, :args, :status, :stdout]
end
