defmodule LinuxRollout.Adapters.ConditionalGitSubmit do
  alias LinuxRollout.Adapters.GitSubmit

  def submit(workspace, bundle, target, preflight, opts) do
    case preflight.status do
      "ok" ->
        GitSubmit.submit(workspace, bundle, target, opts)

      status when status in ["blocked_missing_config", "blocked_missing_auth"] ->
        fallback_path = Path.join(bundle.bundle_root, "CONDITIONAL_FALLBACK.md")

        File.write!(
          fallback_path,
          """
          # Conditional submission fallback

          Target: `#{target["name"]}`
          Readiness: `#{preflight.status}`

          The preferred headless path for this target requires maintainer-specific
          downstream access. Since that access is not configured here, the rollout
          tool stopped after producing the rendered payload in `source/`.

          Suggested next steps:

          - Request or configure the gated downstream access for the preferred Git path
          - Or use the rendered packet in `source/` for the target's manual submission flow
          - Destination: #{get_in(target, ["destination", "submission_url"]) || get_in(target, ["destination", "url"]) || "n/a"}
          """
        )

        %{
          status: if(Keyword.get(opts, :dry_run, false), do: "dry_run", else: "manual"),
          handoff: fallback_path
        }

      _other ->
        Map.merge(preflight, %{bundle_root: bundle.bundle_root})
    end
  end
end
