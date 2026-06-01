defmodule LinuxRollout.Adapters.GuidedHandoff do
  alias LinuxRollout.Util

  def submit(bundle, target, opts) do
    instructions_path = Path.join(bundle.bundle_root, "GUIDED_HANDOFF.md")

    File.write!(
      instructions_path,
      """
      # Guided handoff

      Target: `#{target["name"]}`

      Open:

      - #{get_in(target, ["destination", "submission_url"]) || get_in(target, ["destination", "url"]) || "n/a"}

      Then use the rendered files from `source/` to complete the hosted-builder
      or Launchpad-managed workflow. The rollout tool keeps this path human-led
      because the downstream flow still includes account-specific review screens,
      one-time setup, or web-only controls.
      """
    )

    Util.open_url(
      get_in(target, ["destination", "submission_url"]) || get_in(target, ["destination", "url"]) ||
        "",
      opts
    )

    %{status: if(opts[:dry_run], do: "dry_run", else: "guided"), instructions: instructions_path}
  end
end
