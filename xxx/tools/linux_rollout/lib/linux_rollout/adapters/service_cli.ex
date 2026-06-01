defmodule LinuxRollout.Adapters.ServiceCli do
  alias LinuxRollout.{Util, Workspace}

  def submit(workspace, bundle, target, opts) do
    plan_path = Path.join(bundle.bundle_root, "SERVICE_CLI_PLAN.md")
    units = Workspace.target_units(target)
    write_plan!(plan_path, target, units)

    if Keyword.get(opts, :dry_run, false) do
      %{status: "dry_run", plan: plan_path, units: Enum.map(units, & &1["name"])}
    else
      results =
        Enum.map(units, fn unit ->
          Util.witness(
            "service-cli: #{target["name"]}/#{unit["name"]} using #{target["service"]}"
          )

          submit_unit!(workspace, bundle, target, unit)
        end)

      %{status: "submitted", plan: plan_path, units: results}
    end
  end

  defp submit_unit!(workspace, bundle, target, unit) do
    case {target["service"], target["submission_mode"]} do
      {"obs", "obs_home_project"} -> submit_obs_home_project!(workspace, bundle, unit)
      {"obs", "obs_submit_request"} -> submit_obs_submit_request!(workspace, bundle, unit)
      {"copr", "copr_scm"} -> submit_copr_scm!(workspace, bundle, unit)
      other -> raise "unsupported service CLI combination #{inspect(other)}"
    end
  end

  defp submit_obs_home_project!(workspace, bundle, unit) do
    Util.witness("service-cli: #{unit["name"]} preparing OBS package checkout")
    package_dir = ensure_obs_package_dir!(workspace, unit)
    apply_path_map!(bundle.source_root, unit["path_map"], package_dir)
    Util.run!("osc", ["addremove"], cd: package_dir)
    Util.witness("service-cli: #{unit["name"]} committing to OBS project")
    Util.run!("osc", ["ci", "-m", unit["commit_message"]], cd: package_dir)

    %{name: unit["name"], status: "submitted", project: get_in(unit, ["destination", "project"])}
  end

  defp submit_obs_submit_request!(workspace, bundle, unit) do
    submit_obs_home_project!(workspace, bundle, unit)

    Util.witness("service-cli: #{unit["name"]} creating OBS submit request")

    Util.run!(
      "osc",
      [
        "submitreq",
        get_in(unit, ["destination", "project"]) || "",
        get_in(unit, ["destination", "package"]) || "",
        get_in(unit, ["destination", "target_project"]) || "",
        get_in(unit, ["destination", "target_package"]) ||
          get_in(unit, ["destination", "package"]) || "",
        "-m",
        unit["commit_message"]
      ],
      cd: obs_package_dir(workspace, unit)
    )

    %{
      name: unit["name"],
      status: "submitted",
      project: get_in(unit, ["destination", "project"]),
      target_project: get_in(unit, ["destination", "target_project"])
    }
  end

  defp submit_copr_scm!(workspace, bundle, unit) do
    source_repo = get_in(unit, ["destination", "source_repo"]) || raise "missing Copr source repo"
    source_base_branch = get_in(unit, ["destination", "source_base_branch"]) || "main"

    source_checkout_dir =
      Path.join([workspace.checkout_root, unit["target_name"], "#{unit["name"]}-source"])

    source_branch = unit["branch_name"]

    Util.witness("service-cli: #{unit["name"]} cloning #{source_repo}")
    clone_github_repo_if_missing!(source_repo, source_checkout_dir)
    Util.witness("service-cli: #{unit["name"]} preparing branch #{source_branch}")
    ensure_git_branch!(source_checkout_dir, source_base_branch, source_branch)
    apply_path_map!(bundle.source_root, unit["path_map"], source_checkout_dir)
    commit_if_needed!(source_checkout_dir, unit["commit_message"])
    Util.witness("service-cli: #{unit["name"]} pushing Copr source branch")

    Util.run!(
      "git",
      ["push", "--force-with-lease", "--set-upstream", "origin", source_branch],
      cd: source_checkout_dir
    )

    project_name = get_in(unit, ["destination", "project"]) || raise "missing Copr project name"
    owner = get_in(unit, ["destination", "owner"]) || raise "missing Copr owner"
    package_name = get_in(unit, ["destination", "package_name"]) || "wireshare"
    project_ref = "#{owner}/#{project_name}"
    clone_url = github_clone_url(source_repo)
    subdir = get_in(unit, ["destination", "subdir"]) || "packaging/copr"
    spec = get_in(unit, ["destination", "spec"]) || "wireshare.spec"
    chroots = get_in(unit, ["destination", "chroots"]) || []

    Util.witness("service-cli: #{unit["name"]} syncing Copr SCM package")
    maybe_create_copr_project!(project_name, chroots)
    sync_copr_scm_package!(project_ref, package_name, clone_url, source_branch, subdir, spec)
    Util.witness("service-cli: #{unit["name"]} requesting Copr build")
    build_request = request_copr_build!(project_ref, package_name)

    %{
      name: unit["name"],
      status: "submitted",
      project: project_ref,
      source_repo: source_repo,
      copr_build: build_request
    }
  end

  defp ensure_obs_package_dir!(workspace, unit) do
    project = get_in(unit, ["destination", "project"]) || raise "missing OBS project"
    package = get_in(unit, ["destination", "package"]) || raise "missing OBS package"
    parent_dir = Path.join(workspace.checkout_root, unit["target_name"])
    project_dir = Path.join(parent_dir, project)
    package_dir = obs_package_dir(workspace, unit)

    File.mkdir_p!(parent_dir)

    unless File.dir?(project_dir) do
      Util.run!("osc", ["co", project], cd: parent_dir)
    end

    Util.run("osc", ["up"], cd: project_dir)

    unless File.dir?(package_dir) do
      Util.run!("osc", ["mkpac", package], cd: project_dir)
    end

    package_dir
  end

  defp obs_package_dir(workspace, unit) do
    project = get_in(unit, ["destination", "project"]) || ""
    package = get_in(unit, ["destination", "package"]) || ""
    Path.join([workspace.checkout_root, unit["target_name"], project, package])
  end

  defp clone_github_repo_if_missing!(repo, checkout_dir) do
    File.mkdir_p!(Path.dirname(checkout_dir))

    unless File.dir?(checkout_dir) do
      Util.run!("gh", ["repo", "clone", repo, checkout_dir])
    end
  end

  defp ensure_git_branch!(checkout_dir, base_branch, branch_name) do
    Util.run("git", ["fetch", "--all", "--prune"], cd: checkout_dir)

    case Util.run("git", ["checkout", base_branch], cd: checkout_dir) do
      {:ok, _output} ->
        :ok

      {:error, _reason} ->
        Util.run!("git", ["checkout", "-B", base_branch, "origin/#{base_branch}"],
          cd: checkout_dir
        )
    end

    Util.run("git", ["pull", "--ff-only", "origin", base_branch], cd: checkout_dir)
    Util.run!("git", ["checkout", "-B", branch_name], cd: checkout_dir)
  end

  defp apply_path_map!(bundle_source_root, path_map, checkout_dir) do
    Enum.each(path_map, fn {source_relative, destination_relative} ->
      source_path = Path.join(bundle_source_root, source_relative)
      destination_path = Path.join(checkout_dir, destination_relative)

      if File.exists?(source_path) do
        Util.copy_file!(source_path, destination_path)
      else
        raise "missing bundled source file #{source_path}"
      end
    end)
  end

  defp commit_if_needed!(checkout_dir, message) do
    Util.run!("git", ["add", "-A"], cd: checkout_dir)

    case System.cmd("git", ["diff", "--cached", "--quiet"], cd: checkout_dir) do
      {_output, 0} ->
        :ok

      {_output, 1} ->
        Util.run!("git", ["commit", "-m", message], cd: checkout_dir)
    end
  end

  defp maybe_create_copr_project!(project_name, chroots) do
    args = ["create", project_name] ++ Enum.flat_map(chroots, &["--chroot", &1])
    Util.run("copr-cli", args)
    :ok
  end

  defp sync_copr_scm_package!(project_ref, package_name, clone_url, branch_name, subdir, spec) do
    args = [
      project_ref,
      "--name",
      package_name,
      "--clone-url",
      clone_url,
      "--commit",
      branch_name,
      "--subdir",
      subdir,
      "--spec",
      spec
    ]

    case Util.run("copr-cli", ["add-package-scm" | args]) do
      {:ok, _output} ->
        :ok

      {:error, _reason} ->
        Util.run!("copr-cli", ["edit-package-scm" | args])
    end
  end

  defp request_copr_build!(project_ref, package_name) do
    args = ["build-package", project_ref, "--name", package_name, "--nowait"]

    case Util.run("copr-cli", args) do
      {:ok, output} ->
        interpret_copr_build_submission(output)

      {:error, %{output: output}} when is_binary(output) ->
        case interpret_copr_build_submission(output) do
          %{accepted: true} = submission -> submission
          _ -> raise "copr-cli #{Enum.join(args, " ")} failed: #{output}"
        end
    end
  end

  @doc false
  def interpret_copr_build_submission(output) when is_binary(output) do
    build_url =
      case Regex.run(~r|https://copr\.fedorainfracloud\.org/\S+|, output) do
        [url] -> url
        _ -> nil
      end

    build_ids =
      case Regex.run(~r/Created builds:\s*([0-9 ][0-9 ]*)/, output, capture: :all_but_first) do
        [ids] -> String.split(ids, ~r/\s+/, trim: true)
        _ -> []
      end

    %{
      accepted: String.contains?(output, "Build was added to"),
      build_url: build_url,
      build_ids: build_ids
    }
  end

  defp github_clone_url(repo) do
    cond do
      String.starts_with?(repo, "http://") or String.starts_with?(repo, "https://") ->
        repo

      true ->
        "https://github.com/#{repo}.git"
    end
  end

  defp write_plan!(plan_path, target, units) do
    contents =
      [
        "# Service CLI submission plan",
        "",
        "- Target: `#{target["name"]}`",
        "- Service: `#{target["service"]}`",
        "- Mode: `#{target["submission_mode"]}`",
        "- Units:",
        Enum.map(units, &"  - `#{&1["name"]}`"),
        ""
      ]
      |> List.flatten()
      |> Enum.join("\n")
      |> Kernel.<>("\n")

    File.write!(plan_path, contents)
  end
end
