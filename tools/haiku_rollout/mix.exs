defmodule HemeraHaikuRollout.MixProject do
  use Mix.Project

  def project do
    [
      app: :hemera_haiku_rollout,
      version: "0.1.0",
      elixir: "~> 1.15",
      start_permanent: Mix.env() == :prod,
      elixirc_paths: elixirc_paths(Mix.env()),
      deps: deps()
    ]
  end

  def application do
    [
      extra_applications: [:logger]
    ]
  end

  defp deps do
    [
      {:jason, "~> 1.4"},
      {:yaml_elixir, "~> 2.11"}
    ]
  end

  defp elixirc_paths(:test), do: ["lib", "test_support"]
  defp elixirc_paths(_), do: ["lib"]
end
