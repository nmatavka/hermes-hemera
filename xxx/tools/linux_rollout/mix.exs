defmodule LinuxRollout.MixProject do
  use Mix.Project

  def project do
    [
      app: :linux_rollout,
      version: "0.1.0",
      elixir: "~> 1.19",
      start_permanent: Mix.env() == :prod,
      deps: deps()
    ]
  end

  defp deps do
    [
      {:yaml_elixir, "~> 2.12"},
      {:ymlr, "~> 5.1"}
    ]
  end
end
