defmodule HemeraHaikuRollout.CLI do
  alias HemeraHaikuRollout.Config
  alias HemeraHaikuRollout.Doctor
  alias HemeraHaikuRollout.Release
  alias HemeraHaikuRollout.Watch

  def main(argv) do
    {options, args, []} =
      OptionParser.parse(argv,
        strict: [
          config: :string,
          dry_run: :boolean
        ]
      )

    config_path = Keyword.get(options, :config, HemeraHaikuRollout.default_config_path())

    case args do
      ["doctor"] ->
        config_path |> Config.load!() |> Doctor.run()

      ["release", version] ->
        config_path
        |> Config.load!()
        |> Release.run(version, dry_run: Keyword.get(options, :dry_run, false))

      ["watch", identifier] ->
        config_path
        |> Config.load!()
        |> Watch.run(identifier, dry_run: Keyword.get(options, :dry_run, false))

      _ ->
        IO.puts("""
        usage:
          scripts/release_haiku_rollout.sh doctor
          scripts/release_haiku_rollout.sh release <version> [--dry-run]
          scripts/release_haiku_rollout.sh watch <pr-or-branch>
        """)

        System.halt(1)
    end
  rescue
    error ->
      IO.puts(:stderr, "error: #{Exception.message(error)}")
      System.halt(1)
  end
end
