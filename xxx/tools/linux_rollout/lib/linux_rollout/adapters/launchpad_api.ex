defmodule LinuxRollout.Adapters.LaunchpadApi do
  alias LinuxRollout.Util

  def submit(workspace, bundle, target, opts) do
    plan_path = Path.join(bundle.bundle_root, "LAUNCHPAD_PLAN.md")
    payload_path = Path.join(bundle.bundle_root, "launchpad_payload.json")
    payload = build_payload(workspace, bundle, target)
    File.write!(payload_path, IO.iodata_to_binary(:json.encode(payload)))
    write_plan!(plan_path, target, payload)

    if Keyword.get(opts, :dry_run, false) do
      %{status: "dry_run", plan: plan_path, payload: payload_path}
    else
      Util.witness("launchpad: #{target["name"]} submitting via #{target["submission_mode"]}")
      result = run_helper!(workspace, target, payload_path)

      %{
        status: Map.fetch!(result, "status"),
        plan: plan_path,
        payload: payload_path,
        details: result
      }
    end
  end

  defp build_payload(_workspace, _bundle, %{"submission_mode" => "launchpad_snap"} = target) do
    destination = Map.fetch!(target, "destination")

    %{
      owner: destination["owner"],
      launchpad_project: destination["launchpad_project"],
      import_repo_name: destination["import_repo_name"],
      import_source_url: destination["import_source_url"],
      import_branch: destination["import_branch"],
      snap_name: destination["snap_name"],
      pocket: destination["pocket"] || "Updates",
      channels: destination["channels"] || %{},
      target_name: target["name"],
      submission_mode: target["submission_mode"]
    }
  end

  defp build_payload(_workspace, bundle, %{"submission_mode" => "launchpad_ppa"} = target) do
    destination = Map.fetch!(target, "destination")
    recipe_path = Path.join(bundle.source_root, "packaging/ppa/wireshare.recipe")

    %{
      owner: destination["owner"],
      launchpad_project: destination["launchpad_project"],
      import_repo_name: destination["import_repo_name"],
      import_source_url: destination["import_source_url"],
      import_branch: destination["import_branch"],
      recipe_name: destination["recipe_name"],
      recipe_description: destination["recipe_description"],
      recipe_text: File.read!(recipe_path),
      ppa_name: destination["ppa_name"],
      ppa_display_name: destination["ppa_display_name"],
      ppa_description: destination["ppa_description"],
      ubuntu_series: destination["ubuntu_series"],
      pocket: destination["pocket"] || "Updates",
      target_name: target["name"],
      submission_mode: target["submission_mode"]
    }
  end

  defp run_helper!(workspace, target, payload_path) do
    helper_path =
      Path.join([workspace.root, "tools", "linux_rollout", "scripts", "launchpad_helper.py"])

    python = Util.launchpad_python()

    unless File.exists?(python) do
      raise "Launchpad helper runtime is missing. Run `scripts/release_linux_rollout.sh install-tools --target snap --target ppa` first."
    end

    command =
      case target["submission_mode"] do
        "launchpad_snap" -> "submit-snap"
        "launchpad_ppa" -> "submit-ppa"
        other -> raise "unsupported Launchpad submission mode #{other}"
      end

    env = [
      {"LINUX_ROLLOUT_LAUNCHPAD_DIR", Util.launchpad_cache_dir()},
      {"LINUX_ROLLOUT_LAUNCHPAD_CREDENTIALS", Util.launchpad_credentials_file()}
    ]

    python
    |> Util.run!([helper_path, command, payload_path], env: env)
    |> :json.decode()
  end

  defp write_plan!(plan_path, target, _payload) do
    destination = Map.fetch!(target, "destination")

    contents =
      [
        "# Launchpad API submission plan",
        "",
        "- Target: `#{target["name"]}`",
        "- Mode: `#{target["submission_mode"]}`",
        "- Owner: `#{destination["owner"]}`",
        "- Launchpad project: `#{destination["launchpad_project"]}`",
        "- Import repo: `#{destination["import_repo_name"]}`",
        "- Import source: #{destination["import_source_url"]}",
        "- Import branch: `#{destination["import_branch"]}`",
        if(target["submission_mode"] == "launchpad_snap",
          do: "- Snap name: `#{destination["snap_name"]}`",
          else: "- Recipe name: `#{destination["recipe_name"]}`"
        ),
        ""
      ]
      |> Enum.reject(&is_nil/1)
      |> Enum.join("\n")
      |> Kernel.<>("\n")

    File.write!(plan_path, contents)
  end
end
