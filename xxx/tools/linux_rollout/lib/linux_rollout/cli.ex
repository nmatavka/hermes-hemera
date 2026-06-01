defmodule LinuxRollout.CLI do
  alias LinuxRollout.{
    Auth,
    Doctor,
    GitHubRelease,
    LocalConfig,
    ReleasePrep,
    Renderer,
    State,
    Submitter,
    ToolInstaller,
    Util,
    Validator,
    Workspace
  }

  def main(argv) do
    {options, positional, _invalid} =
      OptionParser.parse(argv,
        strict: [
          cwd: :string,
          manifest: :string,
          targets: :string,
          target: :keep,
          all: :boolean,
          headless_only: :boolean,
          open: :boolean,
          dry_run: :boolean,
          skip_gradle: :boolean,
          no_open: :boolean,
          approve: :boolean,
          acknowledge: :boolean,
          thread_address: :string,
          local_config: :string
        ]
      )

    case positional do
      [command | rest] ->
        workspace = Workspace.load!(Keyword.get(options, :cwd, File.cwd!()), options)
        run_command!(command, rest, workspace, options)

      _ ->
        print_usage()
    end
  end

  defp run_command!("prepare", _rest, workspace, options) do
    Util.witness("prepare: starting release prep")
    ReleasePrep.run!(workspace, options)
    IO.puts("Prepared Linux release inputs in #{workspace.work_dir}")
  end

  defp run_command!("validate", _rest, workspace, _options) do
    Util.witness("validate: starting rendered bundle validation")
    Validator.run!(workspace)
    IO.puts("Validated rendered rollout bundle in #{workspace.work_dir}")
  end

  defp run_command!("doctor", _rest, workspace, options) do
    targets = selected_targets(workspace, options)
    Util.witness("doctor: inspecting #{length(targets)} target(s)")
    report = Doctor.run!(workspace, targets)
    IO.puts("Doctor inspected #{map_size(report)} target(s)")
  end

  defp run_command!("install-tools", _rest, workspace, options) do
    targets = selected_targets(workspace, options)
    Util.witness("install-tools: preparing helpers for #{length(targets)} target(s)")
    ToolInstaller.install!(workspace, targets)
  end

  defp run_command!("render", _rest, workspace, _options) do
    Util.witness("render: rendering packaging tree")
    Renderer.render!(workspace)
    State.put_step!(workspace, :render, %{status: "completed"})
    IO.puts("Rendered packaging tree into #{workspace.rendered_root}")
  end

  defp run_command!("submit", _rest, workspace, options) do
    workspace = prepare_submission_workspace!(workspace, options)
    targets = selected_targets(workspace, options)
    Util.witness("submit: validating and preparing #{length(targets)} target(s)")
    Validator.run!(workspace)
    maybe_publish_release_assets!(workspace, options)
    Submitter.submit!(workspace, targets, options)
    IO.puts("Prepared submission bundles in #{workspace.bundle_root}")
  end

  defp run_command!("resume", _rest, workspace, options) do
    workspace = prepare_submission_workspace!(workspace, options)
    targets = selected_targets(workspace, options)
    Util.witness("resume: continuing #{length(targets)} target(s)")
    Validator.run!(workspace)
    maybe_publish_release_assets!(workspace, options)
    Submitter.submit!(workspace, targets, options)
    IO.puts("Resumed rollout work for #{Enum.join(targets, ", ")}")
  end

  defp run_command!("auth", rest, workspace, options) do
    subcommand = List.first(rest) || "status"

    case subcommand do
      "status" -> Auth.status!(workspace, selected_targets(workspace, options))
      "login" -> Auth.login!(workspace, selected_targets(workspace, options))
      "logout" -> Auth.logout!(workspace, selected_targets(workspace, options))
      other -> raise "unknown auth subcommand #{other}"
    end
  end

  defp run_command!("config", rest, workspace, _options) do
    subcommand = List.first(rest) || "status"

    case subcommand do
      "status" ->
        IO.puts(LocalConfig.status_lines(workspace))

      "set" ->
        case rest do
          ["set", dotted_path | values] when values != [] ->
            LocalConfig.set!(
              dotted_path,
              Enum.join(values, " "),
              workspace.local_config_path
            )

            IO.puts("Updated local config: #{workspace.local_config_path}")
            IO.puts("Set #{dotted_path}")

          _ ->
            raise "usage: scripts/release_linux_rollout.sh config set <dotted.path> <value>"
        end

      "unset" ->
        case rest do
          ["unset", dotted_path] ->
            LocalConfig.unset!(dotted_path, workspace.local_config_path)
            IO.puts("Updated local config: #{workspace.local_config_path}")
            IO.puts("Unset #{dotted_path}")

          _ ->
            raise "usage: scripts/release_linux_rollout.sh config unset <dotted.path>"
        end

      other ->
        raise "unknown config subcommand #{other}"
    end
  end

  defp run_command!("status", _rest, workspace, options) do
    Util.witness("status: collecting rollout state")
    state = State.load(workspace)
    targets = selected_targets(workspace, options)
    IO.puts("Work dir: #{workspace.work_dir}")
    IO.puts("Version: #{workspace.version}")
    IO.puts("")

    Enum.each(targets, fn target_name ->
      target_state = get_in(state, ["targets", target_name]) || %{"status" => "pending"}
      target_config = Workspace.target!(workspace, target_name)
      driver = target_config["driver"]
      submission_mode = target_config["submission_mode"]
      readiness = Doctor.inspect_target(workspace, target_config)
      last_status = target_state["status"] || "pending"
      readiness_status = readiness.status

      IO.puts(
        "#{target_name}: last=#{last_status} readiness=#{readiness_status} (#{driver}/#{submission_mode})"
      )

      Enum.each(Map.get(readiness, :messages, []), fn message ->
        IO.puts("  - #{message}")
      end)

      target_state
      |> remote_links()
      |> Enum.each(fn {label, url} ->
        IO.puts("  - #{label}: #{url}")
      end)

      target_state
      |> status_details()
      |> Enum.each(fn {label, value} ->
        IO.puts("  - #{label}: #{value}")
      end)
    end)
  end

  defp run_command!(command, _rest, _workspace, _options) do
    raise "unknown command #{command}"
  end

  defp selected_targets(workspace, options) do
    requested = Keyword.get_values(options, :target)

    cond do
      requested != [] ->
        filter_headless(workspace, requested, options)

      options[:all] ->
        workspace |> select_default_targets(options) |> Map.keys() |> Enum.sort()

      true ->
        workspace |> select_default_targets(options) |> Map.keys() |> Enum.sort()
    end
  end

  defp select_default_targets(workspace, options) do
    if options[:headless_only] do
      Workspace.headless_targets(workspace)
    else
      Workspace.enabled_targets(workspace)
    end
  end

  defp filter_headless(workspace, targets, options) do
    if options[:headless_only] do
      Enum.filter(targets, fn target_name ->
        workspace
        |> Workspace.target!(target_name)
        |> Workspace.headless_capable?()
      end)
    else
      targets
    end
  end

  defp print_usage do
    IO.puts("""
    usage: scripts/release_linux_rollout.sh <prepare|validate|doctor|install-tools|render|submit|resume|status|auth|config> [options]

      auth status|login|logout
      config status|set|unset

      --target NAME           Limit work to one or more targets
      --all                   Operate on every enabled target
      --headless-only         Limit work to targets with full, conditional, or gated terminal/API submission support
      --dry-run               Generate plans and bundles without outward submission
      --skip-gradle           Reuse existing release artifacts instead of running Gradle
      --approve               Approve a human-gated target during resume/submit
      --acknowledge           Record explicit human acknowledgment for human-gated targets
      --thread-address ADDR   Resume a threaded mail target such as Guix
      --open                  Open browser URLs for guided/manual targets
      --no-open               Explicitly suppress browser opening even if --open is present
      --cwd PATH              Override the workspace root (useful for tests)
      --local-config PATH     Override the local rollout config path (defaults to ~/.local/share/linux_rollout/config.yaml)
    """)
  end

  defp maybe_publish_release_assets!(workspace, options) do
    if options[:dry_run] do
      :ok
    else
      Util.witness("submit: ensuring canonical GitHub release assets are published")
      GitHubRelease.ensure_assets!(workspace)
    end
  end

  defp submit_prep_options(options) do
    if options[:dry_run] do
      Keyword.put_new(options, :skip_gradle, true)
    else
      options
    end
  end

  defp prepare_submission_workspace!(workspace, options) do
    ReleasePrep.run!(workspace, submit_prep_options(options))

    Workspace.load!(workspace.root,
      manifest: workspace.manifest_path,
      targets: workspace.targets_path
    )
  end

  defp remote_links(value) when is_map(value) do
    value
    |> Enum.flat_map(fn {key, nested} ->
      collect_remote_links(to_string(key), nested)
    end)
    |> Enum.uniq()
  end

  defp remote_links(_value), do: []

  defp collect_remote_links(_key, value) when is_map(value) do
    value
    |> Enum.flat_map(fn {nested_key, nested_value} ->
      collect_remote_links(to_string(nested_key), nested_value)
    end)
  end

  defp collect_remote_links(key, value) when is_list(value) do
    Enum.flat_map(value, &collect_remote_links(key, &1))
  end

  defp collect_remote_links(key, value) when is_binary(value) do
    if remote_link_key?(key) and String.starts_with?(value, ["https://", "http://"]) do
      [{String.replace(key, "_", " "), value}]
    else
      []
    end
  end

  defp collect_remote_links(_key, _value), do: []

  defp remote_link_key?(key) do
    key in ["pull_request", "merge_request", "bug_url"] or
      String.ends_with?(key, "_url") or
      String.ends_with?(key, "_web_link")
  end

  defp status_details(value) when is_map(value) do
    value
    |> Enum.flat_map(fn {key, nested} ->
      collect_status_details(to_string(key), nested)
    end)
    |> Enum.uniq()
  end

  defp status_details(_value), do: []

  defp collect_status_details(_key, value) when is_map(value) do
    value
    |> Enum.flat_map(fn {nested_key, nested_value} ->
      collect_status_details(to_string(nested_key), nested_value)
    end)
  end

  defp collect_status_details(key, value) when is_list(value) do
    Enum.flat_map(value, &collect_status_details(key, &1))
  end

  defp collect_status_details("handoff", value) when is_binary(value), do: [{"handoff", value}]

  defp collect_status_details("next_step_command", value) when is_binary(value) do
    [{"next step", value}]
  end

  defp collect_status_details("suggested_pr_title", value) when is_binary(value) do
    [{"suggested PR title", value}]
  end

  defp collect_status_details("validated_system", value) when is_binary(value) do
    [{"validated system", value}]
  end

  defp collect_status_details(_key, _value), do: []
end
