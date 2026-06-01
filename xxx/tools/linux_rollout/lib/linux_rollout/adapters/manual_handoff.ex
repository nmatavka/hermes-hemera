defmodule LinuxRollout.Adapters.ManualHandoff do
  alias LinuxRollout.Util

  def submit(bundle, target, opts) do
    instructions_path = Path.join(bundle.bundle_root, "MANUAL_HANDOFF.md")

    File.write!(
      instructions_path,
      """
      # Manual handoff

      Submit the files in `source/` to:

      - #{get_in(target, ["destination", "submission_url"]) || get_in(target, ["destination", "url"]) || "n/a"}

      The rollout tool intentionally stops here because this target uses a manual
      submission workflow or a non-standard downstream review path.
      """
    )

    Util.open_url(
      get_in(target, ["destination", "submission_url"]) || get_in(target, ["destination", "url"]) ||
        "",
      opts
    )

    %{status: if(opts[:dry_run], do: "dry_run", else: "manual"), handoff: instructions_path}
  end
end
