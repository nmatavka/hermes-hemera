defmodule LinuxRollout.Submitter do
  alias LinuxRollout.{Bundles, Doctor, State, Util, Workspace}

  alias LinuxRollout.Adapters.{
    BugzillaApi,
    ConditionalGitSubmit,
    GitSubmit,
    GuidedHandoff,
    HumanGate,
    LaunchpadApi,
    MailSubmit,
    ManualHandoff,
    ServiceCli
  }

  def submit!(workspace, target_names, opts \\ []) do
    Enum.each(target_names, fn target_name ->
      Util.witness("submit: starting #{target_name}")
      result = submit_target(workspace, target_name, opts)
      Util.witness("submit: #{target_name} -> #{result_status(result)}#{result_suffix(result)}")

      State.put_target!(workspace, target_name, result)
    end)
  end

  def submit_target(workspace, target_name, opts \\ []) do
    try do
      target = Workspace.target!(workspace, target_name)
      bundle = Bundles.create!(workspace, target)
      preflight = Doctor.inspect_target(workspace, target)

      case {target["driver"], preflight.status} do
        {"conditional_git_submit", _status} ->
          ConditionalGitSubmit.submit(workspace, bundle, target, preflight, opts)

        {_driver, "ok"} ->
          case target["driver"] do
            "human_gate" -> HumanGate.submit(workspace, bundle, target, opts)
            "guided_handoff" -> GuidedHandoff.submit(bundle, target, opts)
            "git_submit" -> GitSubmit.submit(workspace, bundle, target, opts)
            "service_cli" -> ServiceCli.submit(workspace, bundle, target, opts)
            "launchpad_api" -> LaunchpadApi.submit(workspace, bundle, target, opts)
            "mail_submit" -> MailSubmit.submit(workspace, bundle, target, opts)
            "bugzilla_api" -> BugzillaApi.submit(workspace, bundle, target, opts)
            "manual_handoff" -> ManualHandoff.submit(bundle, target, opts)
            other -> raise "unsupported driver #{inspect(other)} for target #{target_name}"
          end

        {_driver, blocked_status} ->
          Map.merge(preflight, %{status: blocked_status, bundle_root: bundle.bundle_root})
      end
    rescue
      error ->
        %{
          status: "failed",
          error: Exception.message(error),
          bundle_root: Path.join(workspace.bundle_root, target_name)
        }
    end
  end

  defp result_status(result) do
    result[:status] || result["status"] || "unknown"
  end

  defp result_suffix(result) do
    result = stringify_result(result)

    cond do
      Map.has_key?(result, "error") ->
        " (" <> first_reason_line(result["error"]) <> ")"

      Map.has_key?(result, "messages") and result["messages"] != [] ->
        " (" <> first_reason_line(List.first(result["messages"])) <> ")"

      true ->
        ""
    end
  end

  defp stringify_result(result) when is_map(result) do
    Enum.into(result, %{}, fn {key, value} -> {to_string(key), value} end)
  end

  defp first_reason_line(message) when is_binary(message) do
    message
    |> String.split("\n", trim: true)
    |> List.first()
    |> case do
      nil -> ""
      line -> line
    end
  end
end
