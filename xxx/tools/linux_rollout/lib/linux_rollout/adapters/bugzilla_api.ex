defmodule LinuxRollout.Adapters.BugzillaApi do
  alias LinuxRollout.{BugzillaClient, Util}
  alias LinuxRollout.BugzillaClient.RequestError

  def submit(workspace, bundle, target, opts) do
    plan_path = Path.join(bundle.bundle_root, "BUGZILLA_PLAN.md")
    payload_path = Path.join(bundle.bundle_root, "bugzilla_payload.json")
    payload = build_payload(workspace, bundle, target)
    File.write!(payload_path, IO.iodata_to_binary(:json.encode(payload)))
    write_plan!(plan_path, target, payload)

    if Keyword.get(opts, :dry_run, false) do
      %{status: "dry_run", plan: plan_path, payload: payload_path}
    else
      Util.witness("bugzilla: #{target["name"]} submitting via #{target["submission_mode"]}")

      try do
        result = run_helper!(workspace, target, payload_path)

        %{
          status: Map.fetch!(result, "status"),
          plan: plan_path,
          payload: payload_path,
          bug_id: result["bug_id"],
          bug_url: result["bug_url"],
          details: result
        }
      rescue
        error in RequestError ->
          manual_bugzilla_handoff_result!(
            bundle,
            target,
            payload,
            error,
            opts,
            plan_path,
            payload_path
          )
      end
    end
  end

  defp build_payload(workspace, bundle, %{"submission_mode" => "bugzilla_request"} = target) do
    destination = Map.fetch!(target, "destination")

    %{
      bugzilla_url: destination["bugzilla_url"],
      product: destination["product"],
      component: destination["component"],
      version: destination["version"],
      platform: destination["platform"],
      severity: destination["severity"],
      op_sys: destination["op_sys"] || "All",
      summary: destination["summary"] || default_summary(workspace, target),
      description: destination["description"] || request_description(workspace, bundle, target),
      attachments: request_attachments(bundle, target),
      target_name: target["name"],
      submission_mode: target["submission_mode"]
    }
  end

  defp build_payload(workspace, bundle, %{"submission_mode" => "bugzilla_patch"} = target) do
    destination = Map.fetch!(target, "destination")
    patch_path = Path.join(bundle.bundle_root, "#{target["name"]}.patch")
    prepare_patch!(workspace, bundle, target, patch_path)

    %{
      bugzilla_url: destination["bugzilla_url"],
      product: destination["product"],
      component: destination["component"],
      version: destination["version"],
      platform: destination["platform"],
      severity: destination["severity"],
      op_sys: destination["op_sys"] || "All",
      summary: destination["summary"] || default_summary(workspace, target),
      description: destination["description"] || patch_description(workspace, bundle, target),
      patch_path: patch_path,
      patch_file_name: Path.basename(patch_path),
      patch_summary: destination["patch_summary"] || "Proposed #{target["name"]} packaging patch",
      target_name: target["name"],
      submission_mode: target["submission_mode"]
    }
  end

  defp request_attachments(bundle, target) do
    target
    |> Map.get("path_map", %{})
    |> Enum.map(fn {source_relative, destination_relative} ->
      source_path = Path.join(bundle.source_root, source_relative)

      %{
        "path" => source_path,
        "file_name" => Path.basename(destination_relative),
        "summary" => "Rendered packaging file: #{Path.basename(destination_relative)}",
        "content_type" => "text/plain"
      }
    end)
  end

  defp prepare_patch!(workspace, bundle, target, patch_path) do
    checkout_dir = Path.join([workspace.checkout_root, target["name"], "bugzilla-patch"])
    {sender_name, sender_email} = sender_identity!(workspace)

    File.rm_rf!(checkout_dir)
    File.mkdir_p!(checkout_dir)
    Util.run!("git", ["init"], cd: checkout_dir)
    Util.run!("git", ["config", "user.name", sender_name], cd: checkout_dir)
    Util.run!("git", ["config", "user.email", sender_email], cd: checkout_dir)

    Enum.each(target["path_map"], fn {source_relative, destination_relative} ->
      source_path = Path.join(bundle.source_root, source_relative)
      destination_path = Path.join(checkout_dir, destination_relative)

      if File.exists?(source_path) do
        Util.copy_file!(source_path, destination_path)
      else
        raise "missing bundled source file #{source_path}"
      end
    end)

    Util.run!("git", ["add", "-A"], cd: checkout_dir)
    Util.run!("git", ["commit", "-m", target["commit_message"]], cd: checkout_dir)

    patch =
      Util.run!("git", ["format-patch", "--stdout", "--root", "HEAD"], cd: checkout_dir)

    File.write!(patch_path, patch)
  end

  defp sender_identity!(workspace) do
    configured_name = workspace.tokens["submission_name"] || ""
    configured_email = workspace.tokens["submission_email"] || ""
    git_name = git_config("user.name")
    git_email = git_config("user.email")

    {
      blank_to(configured_name, git_name) |> blank_to("WireShare Packaging Bot"),
      blank_to(configured_email, git_email) |> blank_to("packaging@example.invalid")
    }
  end

  defp git_config(key) do
    case Util.run("git", ["config", "--get", key]) do
      {:ok, output} -> String.trim(output)
      {:error, _reason} -> ""
    end
  end

  defp blank_to(value, fallback) when value in [nil, ""], do: fallback
  defp blank_to(value, _fallback), do: value

  defp default_summary(workspace, target) do
    "#{workspace.tokens["release_app_name"]} packaging for #{target["name"]}"
  end

  defp request_description(workspace, bundle, target) do
    """
    Please review the attached packaging payload for #{workspace.tokens["release_app_name"]} #{workspace.version}.

    Upstream homepage:
    - #{workspace.tokens["release_homepage_url"]}

    Canonical source tarball:
    - #{workspace.tokens["source_tarball_url"]}

    Release bundle summary:
    - #{bundle.summary_path}

    This request was prepared by the local WireShare Linux rollout orchestrator for the `#{target["name"]}` target.
    """
  end

  defp patch_description(workspace, bundle, target) do
    """
    Please review the attached patch that adds #{workspace.tokens["release_app_name"]} #{workspace.version} packaging for FreeBSD Ports.

    Upstream homepage:
    - #{workspace.tokens["release_homepage_url"]}

    Canonical source tarball:
    - #{workspace.tokens["source_tarball_url"]}

    Release bundle summary:
    - #{bundle.summary_path}

    This patch was prepared by the local WireShare Linux rollout orchestrator for the `#{target["name"]}` target.
    """
  end

  defp write_plan!(plan_path, target, payload) do
    contents =
      [
        "# Bugzilla API submission plan",
        "",
        "- Target: `#{target["name"]}`",
        "- Mode: `#{target["submission_mode"]}`",
        "- Bugzilla: #{payload[:bugzilla_url] || payload["bugzilla_url"]}",
        "- Product: `#{payload[:product] || payload["product"]}`",
        "- Component: `#{payload[:component] || payload["component"]}`",
        "- Version: `#{payload[:version] || payload["version"]}`",
        "- Platform: `#{payload[:platform] || payload["platform"]}`",
        "- Severity: `#{payload[:severity] || payload["severity"]}`",
        ""
      ]
      |> Enum.join("\n")
      |> Kernel.<>("\n")

    File.write!(plan_path, contents)
  end

  defp manual_bugzilla_handoff_result!(
         bundle,
         target,
         payload,
         %RequestError{} = error,
         opts,
         plan_path,
         payload_path
       ) do
    bug_url =
      case error.bug_id do
        bug_id when is_integer(bug_id) -> BugzillaClient.bug_url(payload[:bugzilla_url], bug_id)
        _ -> nil
      end

    handoff_url = bug_url || get_in(target, ["destination", "submission_url"]) || payload[:bugzilla_url]
    handoff_path = Path.join(bundle.bundle_root, "BUGZILLA_HANDOFF.md")

    File.write!(
      handoff_path,
      handoff_contents(target, payload, error, handoff_url, bug_url, plan_path, payload_path)
    )

    Util.open_url(handoff_url, opts)

    details =
      %{
        "status" => "manual",
        "failure_stage" => error.stage,
        "http_status" => error.http_status,
        "content_type" => error.content_type,
        "response_excerpt" => error.response_excerpt
      }
      |> maybe_put_detail("bug_id", error.bug_id)
      |> maybe_put_detail("bug_url", bug_url)

    %{
      status: "manual",
      plan: plan_path,
      payload: payload_path,
      handoff: handoff_path,
      bug_id: error.bug_id,
      bug_url: bug_url,
      details: details
    }
  end

  defp handoff_contents(_target, payload, error, handoff_url, bug_url, plan_path, payload_path) do
    [
      "# Bugzilla handoff",
      "",
      stage_overview(error, bug_url),
      "",
      "Open this URL in your browser:",
      "",
      "- #{handoff_url}",
      "",
      "Use these exact bug fields:",
      "",
      "- Product: `#{payload[:product]}`",
      "- Component: `#{payload[:component]}`",
      "- Version: `#{payload[:version]}`",
      "- Platform: `#{payload[:platform]}`",
      "- Severity: `#{payload[:severity]}`",
      "- OS: `#{payload[:op_sys]}`",
      "",
      "Summary:",
      "",
      "```text",
      payload[:summary],
      "```",
      "",
      "Description:",
      "",
      "```text",
      payload[:description],
      "```",
      "",
      attachment_section(payload),
      "",
      "Automation stopped here:",
      "",
      "- Stage: `#{error.stage}`",
      "- HTTP status: `#{error.http_status || "n/a"}`",
      "- Content type: `#{error.content_type || "n/a"}`",
      "",
      "Response excerpt:",
      "",
      "```text",
      error.response_excerpt || "(none)",
      "```",
      "",
      "Generated bundle files:",
      "",
      "- Plan: `#{plan_path}`",
      "- Payload: `#{payload_path}`"
    ]
    |> Enum.join("\n")
    |> Kernel.<>("\n")
  end

  defp stage_overview(%RequestError{bug_id: bug_id}, bug_url) when is_integer(bug_id) do
    """
    The bug was created successfully, but the automated attachment step failed.
    Open the existing bug below and upload the generated file there instead of
    creating a duplicate bug.

    Existing bug: #{bug_url}
    """
    |> String.trim_trailing()
  end

  defp stage_overview(_error, _bug_url) do
    """
    The automated Bugzilla API request could not finish in a machine-usable way.
    Create the bug in your own browser session, then upload the generated file(s)
    listed below.
    """
    |> String.trim_trailing()
  end

  defp attachment_section(%{attachments: attachments}) do
    lines =
      attachments
      |> Enum.with_index(1)
      |> Enum.flat_map(fn {attachment, index} ->
        [
          "#{index}. Upload `#{attachment["path"]}` as `#{attachment["file_name"]}`",
          "   Summary: `#{attachment["summary"]}`",
          "   Content type: `#{attachment["content_type"]}`"
        ]
      end)

    ["Attachments:", "" | lines] |> Enum.join("\n")
  end

  defp attachment_section(payload) do
    [
      "Attachment:",
      "",
      "- Upload `#{payload[:patch_path]}` as `#{payload[:patch_file_name]}`",
      "- Summary: `#{payload[:patch_summary]}`",
      "- Content type: `text/x-patch`",
      "- Mark as patch: `yes`",
      "",
      "Attachment comment:",
      "",
      "```text",
      "Patch prepared by the WireShare Linux rollout orchestrator.",
      "```"
    ]
    |> Enum.join("\n")
  end

  defp maybe_put_detail(map, _key, nil), do: map
  defp maybe_put_detail(map, key, value), do: Map.put(map, key, value)

  defp run_helper!(_workspace, target, payload_path) do
    payload = payload_path |> File.read!() |> :json.decode()

    case target["submission_mode"] do
      "bugzilla_request" ->
        BugzillaClient.submit_request!(payload)

      "bugzilla_patch" ->
        BugzillaClient.submit_patch!(payload)

      other ->
        raise "unsupported Bugzilla submission mode #{other}"
    end
  end
end
