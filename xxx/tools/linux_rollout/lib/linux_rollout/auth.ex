defmodule LinuxRollout.Auth do
  alias LinuxRollout.{BugzillaClient, Doctor, LocalConfig, Util, Workspace}

  def status!(workspace, target_names) do
    Enum.each(target_names, fn target_name ->
      target = Workspace.target!(workspace, target_name)
      result = Doctor.inspect_target(workspace, target)
      IO.puts("#{target_name}: #{result.status}")

      Enum.each(Map.get(result, :messages, []), fn message ->
        IO.puts("  - #{message}")
      end)
    end)
  end

  def login!(workspace, target_names) do
    Enum.reduce(target_names, MapSet.new(), fn target_name, seen ->
      target = Workspace.target!(workspace, target_name)
      key = managed_auth_key(target_name, target)

      if MapSet.member?(seen, key) do
        seen
      else
        case target["driver"] do
          "launchpad_api" ->
            launchpad_auth_login!(workspace)

          "bugzilla_api" ->
            bugzilla_auth!(workspace, target, "auth-login")

          _ ->
            if target["service"] == "obs" do
              obs_auth_login!(workspace)
            else
              print_auth_hint(target_name, target)
            end
        end

        MapSet.put(seen, key)
      end
    end)
  end

  def logout!(workspace, target_names) do
    Enum.reduce(target_names, MapSet.new(), fn target_name, seen ->
      target = Workspace.target!(workspace, target_name)
      key = managed_auth_key(target_name, target)

      if MapSet.member?(seen, key) do
        seen
      else
        case target["driver"] do
          "launchpad_api" ->
            output = launchpad_auth!(workspace, ["auth-logout"])
            IO.write(output)

          "bugzilla_api" ->
            bugzilla_auth!(workspace, target, "auth-logout")

          _ ->
            IO.puts("#{target_name}: no orchestrator-managed logout flow")
        end

        MapSet.put(seen, key)
      end
    end)
  end

  defp launchpad_auth_login!(workspace) do
    output = launchpad_auth!(workspace, ["auth-login"])
    IO.write(output)

    identity =
      workspace
      |> launchpad_auth!(["whoami"])
      |> :json.decode()

    case LocalConfig.put_if_blank!(
           "accounts.launchpad_owner",
           identity["name"] || "",
           workspace.local_config_path
         ) do
      :stored ->
        IO.puts("Stored local override: accounts.launchpad_owner = #{identity["name"]}")

      :unchanged ->
        :ok
    end
  end

  defp launchpad_auth!(workspace, helper_args) do
    helper =
      Path.join([workspace.root, "tools", "linux_rollout", "scripts", "launchpad_helper.py"])

    python = Util.launchpad_python()

    unless File.exists?(python) do
      raise "Launchpad helper runtime is missing. Run `scripts/release_linux_rollout.sh install-tools --target snap --target ppa` first."
    end

    env = [
      {"LINUX_ROLLOUT_LAUNCHPAD_DIR", Util.launchpad_cache_dir()},
      {"LINUX_ROLLOUT_LAUNCHPAD_CREDENTIALS", Util.launchpad_credentials_file()}
    ]

    Util.run!(python, [helper | helper_args], env: env)
  end

  defp bugzilla_auth!(_workspace, target, command) do
    bugzilla_url =
      get_in(target, ["destination", "bugzilla_url"]) || raise "missing destination.bugzilla_url"

    case command do
      "auth-login" ->
        login = prompt_line!("Bugzilla login/email:")
        api_key = prompt_line!("Bugzilla API key (input visible in this terminal):")
        IO.write(BugzillaClient.auth_login!(bugzilla_url, login, api_key))

      "auth-logout" ->
        IO.write(BugzillaClient.auth_logout!(bugzilla_url))
    end
  end

  defp prompt_line!(prompt) do
    IO.puts(prompt)

    case IO.gets("") do
      nil ->
        raise "expected input for #{prompt}"

      value ->
        value = String.trim(value)

        if value == "" do
          raise "#{prompt} is required"
        else
          value
        end
    end
  end

  defp obs_auth_login!(workspace) do
    Util.interactive_run!("osc", ["whois"])

    owner =
      Util.obs_username_from_oscrc() || raise "could not determine OBS username from ~/.oscrc"

    case LocalConfig.put_if_blank!("accounts.obs_owner", owner, workspace.local_config_path) do
      :stored ->
        IO.puts("Stored local override: accounts.obs_owner = #{owner}")

      :unchanged ->
        :ok
    end

    case LocalConfig.put_if_blank!(
           "accounts.obs_project",
           "home:#{owner}:wireshare",
           workspace.local_config_path
         ) do
      :stored ->
        IO.puts("Stored local override: accounts.obs_project = home:#{owner}:wireshare")

      :unchanged ->
        :ok
    end
  end

  defp managed_auth_key(_target_name, %{"driver" => "launchpad_api"}), do: {:launchpad, :shared}

  defp managed_auth_key(_target_name, %{"driver" => "bugzilla_api", "destination" => destination}),
       do: {:bugzilla, destination["bugzilla_url"]}

  defp managed_auth_key(_target_name, %{"service" => "obs"}), do: {:obs, :shared}
  defp managed_auth_key(target_name, _target), do: {:target, target_name}

  defp print_auth_hint("alpine", _target) do
    IO.puts("alpine: run `glab auth login --hostname gitlab.alpinelinux.org`")
  end

  defp print_auth_hint(target_name, %{"service" => "obs"}) do
    IO.puts("#{target_name}: run `osc` once and complete login so ~/.oscrc exists")
  end

  defp print_auth_hint("copr", _target) do
    IO.puts(
      "copr: ensure `gh auth login` is complete and place your Copr token in ~/.config/copr"
    )
  end

  defp print_auth_hint(target_name, %{"forge" => "github"}) do
    IO.puts("#{target_name}: run `gh auth login`")
  end

  defp print_auth_hint("aur", _target) do
    IO.puts(
      "aur: ensure your SSH key is authorized for the AUR, for example with `ssh -T aur@aur.archlinux.org`"
    )
  end

  defp print_auth_hint(target_name, target) do
    help =
      case Map.get(target, "auth_probe", []) do
        %{"help" => value} -> value
        [%{"help" => value} | _] -> value
        _ -> "no automated auth bootstrap is available for this target"
      end

    IO.puts("#{target_name}: #{help}")
  end
end
