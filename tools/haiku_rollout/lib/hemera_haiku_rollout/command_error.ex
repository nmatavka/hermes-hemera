defmodule HemeraHaikuRollout.CommandError do
  defexception [:message, :program, :args, :status, :stdout]
end
