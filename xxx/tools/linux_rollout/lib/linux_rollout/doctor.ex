defmodule LinuxRollout.Doctor do
  alias LinuxRollout.{State, Util, Workspace}

  def run!(workspace, target_names) do
    report =
      target_names
      |> Enum.map(fn target_name ->
        target = Workspace.target!(workspace, target_name)
        {target_name, inspect_target(workspace, target)}
      end)
      |> Map.new()

    State.put_step!(workspace, :doctor, %{status: "completed", report: report})

    Enum.each(target_names, fn target_name ->
      result = Map.fetch!(report, target_name)
      IO.puts("#{target_name}: #{result.status} (#{target_name_display(workspace, target_name)})")

      Enum.each(Map.get(result, :messages, []), fn message ->
        IO.puts("  - #{message}")
      end)
    end)

    report
  end

  def inspect_target(workspace, target) do
    missing_config = missing_config(target)
    missing_cli = missing_cli(workspace, target)
    missing_auth = missing_auth(workspace, target, missing_cli)

    status =
      cond do
        missing_config != [] -> "blocked_missing_config"
        missing_cli != [] -> "blocked_missing_cli"
        missing_auth != [] -> "blocked_missing_auth"
        true -> "ok"
      end

    %{
      status: status,
      missing_config: missing_config,
      missing_cli: missing_cli,
      missing_auth: missing_auth,
      messages: messages_for(status, missing_config, missing_cli, missing_auth, target)
    }
  end

  defp missing_config(target) do
    target
    |> Map.get("required_config", [])
    |> Enum.filter(fn path ->
      target
      |> Util.path_get(path)
      |> Util.blank?()
    end)
  end

  defp missing_cli(workspace, target) do
    target
    |> Map.get("required_cli", [])
    |> Enum.filter(&(not cli_available?(workspace, &1)))
  end

  defp cli_available?(_workspace, "launchpadlib") do
    python = Util.launchpad_python()

    File.exists?(python) and
      match?(
        {:ok, _},
        Util.run(python, ["-c", "import launchpadlib"], success_codes: [0])
      )
  end

  defp cli_available?(_workspace, cli) do
    System.find_executable(cli) != nil
  end

  defp missing_auth(workspace, target, missing_cli) do
    if missing_cli == [] do
      target
      |> auth_probes()
      |> Enum.filter(&probe_failed?(workspace, &1))
      |> Enum.map(&probe_message/1)
    else
      []
    end
  end

  defp auth_probes(target) do
    case Map.get(target, "auth_probe", []) do
      probe when is_map(probe) -> [probe]
      probes when is_list(probes) -> probes
      _ -> []
    end
  end

  defp probe_failed?(_workspace, %{"type" => "command", "command" => [command | args]} = probe) do
    success_codes = Map.get(probe, "success_codes", [0])

    case Util.run(command, args, success_codes: success_codes) do
      {:ok, _output} -> false
      {:error, _reason} -> true
    end
  end

  defp probe_failed?(_workspace, %{"type" => "file", "path" => path}) do
    path
    |> Util.expand_home()
    |> File.exists?()
    |> Kernel.not()
  end

  defp probe_failed?(_workspace, %{"type" => "git_config", "key" => key}) do
    case Util.run("git", ["config", "--get", key]) do
      {:ok, output} -> String.trim(output) == ""
      {:error, _reason} -> true
    end
  end

  defp probe_failed?(_workspace, %{"type" => "launchpad_auth"}) do
    Util.launchpad_credentials_file()
    |> File.exists?()
    |> Kernel.not()
  end

  defp probe_failed?(_workspace, %{"type" => "bugzilla_auth", "url" => url}) do
    url
    |> Util.bugzilla_credentials_file()
    |> File.exists?()
    |> Kernel.not()
  end

  defp probe_failed?(_workspace, %{"type" => "bugzilla_endpoint", "url" => url}) do
    case Util.run("curl", [
           "-fsSL",
           "--connect-timeout",
           "20",
           "--max-time",
           "60",
           bugzilla_version_url(url)
         ]) do
      {:ok, _output} -> false
      {:error, _reason} -> true
    end
  end

  defp probe_failed?(_workspace, _probe), do: false

  defp probe_message(%{"help" => help}) when is_binary(help) and help != "", do: help
  defp probe_message(%{"type" => "file", "path" => path}), do: "missing auth file #{path}"
  defp probe_message(%{"type" => "git_config", "key" => key}), do: "missing git config #{key}"

  defp probe_message(%{"type" => "launchpad_auth"}),
    do:
      "run `scripts/release_linux_rollout.sh auth login --target snap` to create Launchpad OAuth credentials"

  defp probe_message(%{"type" => "bugzilla_auth", "url" => url}),
    do: "missing Bugzilla API credentials for #{url}"

  defp probe_message(%{"type" => "bugzilla_endpoint", "url" => url}),
    do: "Bugzilla endpoint is not reachable at #{bugzilla_version_url(url)}"

  defp probe_message(%{"type" => "command", "command" => [command | args]}) do
    "authentication probe failed: #{Enum.join([command | args], " ")}"
  end

  defp probe_message(_probe), do: "authentication probe failed"

  defp messages_for("blocked_missing_config", missing_config, _missing_cli, _missing_auth, target) do
    Enum.map(missing_config, &missing_config_message(target, &1))
  end

  defp messages_for("blocked_missing_cli", _missing_config, missing_cli, _missing_auth, _target) do
    Enum.map(missing_cli, fn cli ->
      case Util.cli_install_hint(cli) do
        nil -> "install required CLI: #{cli}"
        hint -> "install required CLI: #{cli} (#{hint})"
      end
    end)
  end

  defp messages_for("blocked_missing_auth", _missing_config, _missing_cli, missing_auth, _target) do
    missing_auth
  end

  defp messages_for(_status, _missing_config, _missing_cli, _missing_auth, _target), do: []

  defp missing_config_message(
         %{"driver" => "launchpad_api", "name" => target_name},
         "destination.owner"
       ) do
    "run `scripts/release_linux_rollout.sh auth login --target #{target_name}` or `scripts/release_linux_rollout.sh config set accounts.launchpad_owner <owner>`"
  end

  defp missing_config_message(%{"service" => "obs"}, "destination.project") do
    "run `scripts/release_linux_rollout.sh auth login --target obs` or `scripts/release_linux_rollout.sh config set accounts.obs_project <project>`"
  end

  defp missing_config_message(%{"forge" => "pkgsrc_wip"}, "destination.wip_user") do
    "run `scripts/release_linux_rollout.sh config set accounts.pkgsrc_wip_user <user>`"
  end

  defp missing_config_message(%{"name" => target_name}, "destination." <> rest) do
    "fill required config: destination.#{rest} (set with `scripts/release_linux_rollout.sh config set targets.#{target_name}.destination.#{rest} <value>`)"
  end

  defp missing_config_message(_target, path), do: "fill required config: #{path}"

  defp bugzilla_version_url(url) do
    trimmed = String.trim_trailing(url, "/")

    base =
      cond do
        String.ends_with?(trimmed, "/rest") -> trimmed
        String.ends_with?(trimmed, "/rest.cgi") -> trimmed
        true -> trimmed <> "/rest"
      end

    base <> "/version"
  end

  defp target_name_display(workspace, target_name) do
    target = Workspace.target!(workspace, target_name)
    "#{target["driver"]}/#{target["submission_mode"]}"
  end
end
