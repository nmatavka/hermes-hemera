defmodule HemeraHaikuRollout.ManifestTest do
  use ExUnit.Case, async: true

  alias HemeraHaikuRollout.Manifest

  defp base_manifest_map do
    %{
      "github" => %{
        "repo_owner" => "nick",
        "repo_name" => "hermes-hemera"
      },
      "release" => %{
        "version" => "1.0"
      },
      "haikuports" => %{
        "upstream_url" => "https://github.com/haikuports/haikuports.git"
      }
    }
  end

  test "loads pr notes canonically and accepts the legacy pr body alias" do
    canonical =
      base_manifest_map()
      |> put_in(["haikuports", "pr_notes_template"], "canonical notes")
      |> Manifest.from_map!("memory")

    legacy =
      base_manifest_map()
      |> put_in(["haikuports", "pr_body_template"], "legacy notes")
      |> Manifest.from_map!("memory")

    both =
      base_manifest_map()
      |> put_in(["haikuports", "pr_notes_template"], "canonical notes")
      |> put_in(["haikuports", "pr_body_template"], "legacy notes")
      |> Manifest.from_map!("memory")

    assert canonical.haikuports_pr_notes_template == "canonical notes"
    assert legacy.haikuports_pr_notes_template == "legacy notes"
    assert both.haikuports_pr_notes_template == "canonical notes"
  end
end
