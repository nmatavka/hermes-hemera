defmodule HemeraHaikuRollout.Host do
  def current(override \\ nil)

  def current(override) when override in [:haiku, :macos, :windows, :linux] do
    override
  end

  def current(nil) do
    case :os.type() do
      {:win32, _} ->
        :windows

      {:unix, :darwin} ->
        :macos

      {:unix, :linux} ->
        :linux

      {:unix, _} ->
        case System.cmd("uname", ["-s"], stderr_to_stdout: true) do
          {"Haiku\n", 0} -> :haiku
          {"Haiku", 0} -> :haiku
          _ -> :linux
        end
    end
  end

  def haiku?(host \\ current()) do
    host == :haiku
  end
end
