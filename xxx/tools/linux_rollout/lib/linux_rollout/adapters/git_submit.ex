defmodule LinuxRollout.Adapters.GitSubmit do
  alias LinuxRollout.{Util, Workspace}

  def submit(workspace, bundle, target, opts) do
    plan_path = Path.join(bundle.bundle_root, "GIT_SUBMIT_PLAN.md")
    units = Workspace.target_units(target)
    write_plan!(plan_path, target, units)

    if Keyword.get(opts, :dry_run, false) do
      unit_results =
        Enum.map(units, fn unit ->
          Util.witness(
            "git-submit: #{target["name"]}/#{unit["name"]} using #{target["submission_mode"]}"
          )

          dry_run_unit!(workspace, bundle, target, unit)
        end)

      %{status: "dry_run", plan: plan_path, units: unit_results}
    else
      unit_results =
        Enum.map(units, fn unit ->
          Util.witness(
            "git-submit: #{target["name"]}/#{unit["name"]} using #{target["submission_mode"]}"
          )

          submit_unit!(workspace, bundle, target, unit)
        end)

      top_level_result(unit_results, plan_path)
    end
  end

  defp submit_unit!(workspace, bundle, target, unit) do
    case target["submission_mode"] do
      "push_only" -> submit_push_only!(workspace, bundle, unit)
      "github_pr" -> submit_github_pr!(workspace, bundle, target, unit)
      "gitlab_mr" -> submit_gitlab_mr!(workspace, bundle, target, unit)
      other -> raise "unsupported git submission mode #{inspect(other)}"
    end
  end

  defp dry_run_unit!(workspace, bundle, target, unit) do
    case {target["submission_mode"], needs_dry_run_checkout?(unit)} do
      {"push_only", true} ->
        dry_run_push_only!(workspace, bundle, unit)

      {"github_pr", true} ->
        dry_run_github_pr!(workspace, bundle, unit)

      {"gitlab_mr", true} ->
        dry_run_gitlab_mr!(workspace, bundle, unit)

      _ ->
        unit_summary(unit)
    end
  end

  defp needs_dry_run_checkout?(unit) do
    unit
    |> Map.get("pre_submit_checks", [])
    |> case do
      checks when is_list(checks) and checks != [] ->
        true

      _ ->
        Map.get(unit, "git_commit_signoff", false) or
          Map.get(unit, "checkout_mutations", []) != [] or
          Map.get(unit, "commit_plan", []) != []
    end
  end

  defp dry_run_github_pr!(workspace, bundle, unit) do
    upstream_repo = get_in(unit, ["destination", "repo"]) || raise "missing GitHub upstream repo"
    fork_repo = get_in(unit, ["destination", "fork_repo"]) || upstream_repo
    base_branch = get_in(unit, ["destination", "base_branch"]) || "main"
    branch_name = unit["branch_name"]

    checkout_dir =
      Path.join([bundle.bundle_root, "DRY_RUN_CHECKOUTS", unit["target_name"], unit["name"]])

    persistent_checkout =
      Path.join([workspace.checkout_root, unit["target_name"], unit["name"]])

    File.rm_rf!(checkout_dir)

    Util.witness("git-submit: #{unit["name"]} cloning #{fork_repo} (dry run)")
    clone_github_repo_for_dry_run!(fork_repo, persistent_checkout, checkout_dir)
    ensure_git_remote!(checkout_dir, "upstream", "https://github.com/#{upstream_repo}.git")
    Util.witness("git-submit: #{unit["name"]} preparing branch #{branch_name} (dry run)")
    ensure_git_branch!(checkout_dir, base_branch, branch_name, false)
    apply_path_map!(bundle.source_root, unit["path_map"], checkout_dir)
    apply_checkout_mutations!(bundle.source_root, unit, checkout_dir)
    run_pre_submit_checks!(checkout_dir, unit, "format", dry_run: true)
    commit_checkout!(checkout_dir, unit)
    run_pre_submit_checks!(checkout_dir, unit, "check", dry_run: true)

    unit
    |> unit_summary()
    |> Map.merge(%{
      dry_run_checkout: checkout_dir,
      checks: dry_run_checks(unit),
      commit_messages: planned_commit_messages(unit)
    })
  end

  defp dry_run_push_only!(workspace, bundle, unit) do
    repo = get_in(unit, ["destination", "repo"]) || raise "missing push destination repo"
    base_branch = get_in(unit, ["destination", "base_branch"]) || "master"
    branch_name = unit["branch_name"] || base_branch
    git_env = git_env_for_repo(repo)

    checkout_dir =
      Path.join([bundle.bundle_root, "DRY_RUN_CHECKOUTS", unit["target_name"], unit["name"]])

    persistent_checkout =
      Path.join([workspace.checkout_root, unit["target_name"], unit["name"]])

    File.rm_rf!(checkout_dir)

    Util.witness("git-submit: #{unit["name"]} cloning #{repo} (dry run)")
    clone_git_repo_for_dry_run!(repo, persistent_checkout, checkout_dir, git_env)
    Util.witness("git-submit: #{unit["name"]} preparing branch #{branch_name} (dry run)")
    ensure_git_branch!(checkout_dir, base_branch, branch_name, true)
    apply_path_map!(bundle.source_root, unit["path_map"], checkout_dir)
    apply_checkout_mutations!(bundle.source_root, unit, checkout_dir)
    run_pre_submit_checks!(checkout_dir, unit, "format", dry_run: true)
    commit_checkout!(checkout_dir, unit)
    run_pre_submit_checks!(checkout_dir, unit, "check", dry_run: true)

    unit
    |> unit_summary()
    |> Map.merge(%{
      dry_run_checkout: checkout_dir,
      checks: dry_run_checks(unit),
      commit_messages: planned_commit_messages(unit)
    })
  end

  defp dry_run_gitlab_mr!(workspace, bundle, unit) do
    upstream_repo = get_in(unit, ["destination", "repo"]) || raise "missing GitLab upstream repo"
    fork_repo = get_in(unit, ["destination", "fork_repo"]) || upstream_repo
    host = get_in(unit, ["destination", "host"]) || "gitlab.com"
    base_branch = get_in(unit, ["destination", "base_branch"]) || "master"
    branch_name = unit["branch_name"]
    glab_env = gitlab_env(host)

    checkout_dir =
      Path.join([bundle.bundle_root, "DRY_RUN_CHECKOUTS", unit["target_name"], unit["name"]])

    persistent_checkout =
      Path.join([workspace.checkout_root, unit["target_name"], unit["name"]])

    File.rm_rf!(checkout_dir)

    Util.witness("git-submit: #{unit["name"]} cloning #{fork_repo} (dry run)")
    clone_gitlab_repo_for_dry_run!(fork_repo, persistent_checkout, checkout_dir, glab_env)
    ensure_git_remote!(checkout_dir, "upstream", "https://#{host}/#{upstream_repo}.git")
    Util.witness("git-submit: #{unit["name"]} preparing branch #{branch_name} (dry run)")
    ensure_git_branch!(checkout_dir, base_branch, branch_name, false)
    apply_path_map!(bundle.source_root, unit["path_map"], checkout_dir)
    apply_checkout_mutations!(bundle.source_root, unit, checkout_dir)
    run_pre_submit_checks!(checkout_dir, unit, "format", dry_run: true)
    commit_checkout!(checkout_dir, unit)
    run_pre_submit_checks!(checkout_dir, unit, "check", dry_run: true)

    unit
    |> unit_summary()
    |> Map.merge(%{
      dry_run_checkout: checkout_dir,
      checks: dry_run_checks(unit),
      commit_messages: planned_commit_messages(unit)
    })
  end

  defp top_level_result(unit_results, plan_path) do
    base = %{status: overall_status(unit_results), plan: plan_path, units: unit_results}

    case Enum.find(unit_results, &(&1[:status] == "awaiting_pr_creation")) do
      nil ->
        base

      handoff_unit ->
        Map.merge(
          base,
          Map.take(handoff_unit, [
            :handoff,
            :next_step_command,
            :repo,
            :fork_repo,
            :head_ref,
            :base_branch,
            :validated_system,
            :suggested_pr_title,
            :commit_messages
          ])
        )
    end
  end

  defp overall_status(unit_results) do
    if Enum.any?(unit_results, &(&1[:status] == "awaiting_pr_creation")) do
      "awaiting_pr_creation"
    else
      "submitted"
    end
  end

  defp submit_push_only!(workspace, bundle, unit) do
    repo = get_in(unit, ["destination", "repo"]) || raise "missing push destination repo"
    branch = get_in(unit, ["destination", "base_branch"]) || "master"
    local_branch = unit["branch_name"] || branch
    checkout_dir = Path.join([workspace.checkout_root, unit["target_name"], unit["name"]])
    git_env = git_env_for_repo(repo)

    Util.witness("git-submit: #{unit["name"]} cloning #{repo}")
    clone_git_repo_if_missing!(repo, checkout_dir, git_env)
    Util.witness("git-submit: #{unit["name"]} updating branch #{branch}")
    ensure_git_branch!(checkout_dir, branch, local_branch, true)
    apply_path_map!(bundle.source_root, unit["path_map"], checkout_dir)
    apply_checkout_mutations!(bundle.source_root, unit, checkout_dir)
    run_pre_submit_checks!(checkout_dir, unit, "format", dry_run: false)
    commit_checkout!(checkout_dir, unit)
    run_pre_submit_checks!(checkout_dir, unit, "check", dry_run: false)
    Util.witness("git-submit: #{unit["name"]} pushing to #{repo}")
    push_checkout!(checkout_dir, repo, branch, unit)

    %{name: unit["name"], status: "submitted", repo: repo}
    |> with_validated_system(unit)
  end

  defp submit_github_pr!(workspace, bundle, target, unit) do
    upstream_repo = get_in(unit, ["destination", "repo"]) || raise "missing GitHub upstream repo"
    fork_repo = get_in(unit, ["destination", "fork_repo"]) || upstream_repo
    base_branch = get_in(unit, ["destination", "base_branch"]) || "main"
    branch_name = unit["branch_name"]
    checkout_dir = Path.join([workspace.checkout_root, unit["target_name"], unit["name"]])

    head_owner =
      Util.split_repo_owner(fork_repo) || raise "cannot determine fork owner for #{fork_repo}"

    ensure_github_repo!(upstream_repo, fork_repo)
    Util.witness("git-submit: #{unit["name"]} cloning #{fork_repo}")
    clone_github_repo_if_missing!(fork_repo, checkout_dir)
    ensure_git_remote!(checkout_dir, "upstream", "https://github.com/#{upstream_repo}.git")
    Util.witness("git-submit: #{unit["name"]} preparing branch #{branch_name}")
    ensure_git_branch!(checkout_dir, base_branch, branch_name, false)
    apply_path_map!(bundle.source_root, unit["path_map"], checkout_dir)
    apply_checkout_mutations!(bundle.source_root, unit, checkout_dir)
    run_pre_submit_checks!(checkout_dir, unit, "format", dry_run: false)
    commit_checkout!(checkout_dir, unit)
    run_pre_submit_checks!(checkout_dir, unit, "check", dry_run: false)
    Util.witness("git-submit: #{unit["name"]} pushing branch #{branch_name}")
    push_branch!(checkout_dir, branch_name)

    head_ref = "#{head_owner}:#{branch_name}"

    open_or_handoff_github_pr!(
      bundle,
      target,
      unit,
      checkout_dir,
      upstream_repo,
      fork_repo,
      base_branch,
      branch_name,
      head_ref
    )
  end

  defp submit_gitlab_mr!(workspace, bundle, target, unit) do
    upstream_repo = get_in(unit, ["destination", "repo"]) || raise "missing GitLab upstream repo"
    fork_repo = get_in(unit, ["destination", "fork_repo"]) || upstream_repo
    host = get_in(unit, ["destination", "host"]) || "gitlab.com"
    base_branch = get_in(unit, ["destination", "base_branch"]) || "master"
    branch_name = unit["branch_name"]
    checkout_dir = Path.join([workspace.checkout_root, unit["target_name"], unit["name"]])
    glab_env = gitlab_env(host)

    Util.witness("git-submit: #{unit["name"]} ensuring GitLab fork #{fork_repo}")
    ensure_gitlab_fork!(host, upstream_repo, fork_repo, glab_env)
    Util.witness("git-submit: #{unit["name"]} cloning #{fork_repo}")
    clone_gitlab_repo_if_missing!(host, fork_repo, checkout_dir, glab_env)
    ensure_git_remote!(checkout_dir, "upstream", "https://#{host}/#{upstream_repo}.git")
    Util.witness("git-submit: #{unit["name"]} preparing branch #{branch_name}")
    ensure_git_branch!(checkout_dir, base_branch, branch_name, false)
    apply_path_map!(bundle.source_root, unit["path_map"], checkout_dir)
    apply_checkout_mutations!(bundle.source_root, unit, checkout_dir)
    run_pre_submit_checks!(checkout_dir, unit, "format", dry_run: false)
    commit_checkout!(checkout_dir, unit)
    run_pre_submit_checks!(checkout_dir, unit, "check", dry_run: false)
    Util.witness("git-submit: #{unit["name"]} pushing branch #{branch_name}")
    push_branch!(checkout_dir, branch_name)

    Util.witness("git-submit: #{unit["name"]} opening GitLab merge request")

    mr_args = [
      "mr",
      "create",
      "--repo",
      upstream_repo,
      "-H",
      fork_repo,
      "-b",
      base_branch,
      "-s",
      branch_name,
      "-t",
      unit["commit_message"],
      "-d",
      pr_body(target, unit),
      "--draft",
      "--yes"
    ]

    case Util.run("glab", mr_args, cd: checkout_dir, env: glab_env) do
      {:ok, output} ->
        %{
          name: unit["name"],
          status: "submitted",
          repo: upstream_repo,
          fork_repo: fork_repo,
          merge_request: gitlab_mr_url_from_output(output, host, upstream_repo)
        }
        |> with_validated_system(unit)

      {:error, %{output: output}} when is_binary(output) ->
        case detect_existing_gitlab_mr(output, host, upstream_repo) do
          %{merge_request: url} ->
            %{
              name: unit["name"],
              status: "submitted",
              repo: upstream_repo,
              fork_repo: fork_repo,
              merge_request: url,
              existing_merge_request: true
            }
            |> with_validated_system(unit)

          nil ->
            raise "glab #{Enum.join(mr_args, " ")} failed: #{output}"
        end
    end
  end

  defp open_or_handoff_github_pr!(
         bundle,
         target,
         unit,
         checkout_dir,
         upstream_repo,
         fork_repo,
         base_branch,
         branch_name,
         head_ref
       ) do
    case existing_github_pr_url(upstream_repo, [head_ref, branch_name], base_branch) do
      nil ->
        case pull_request_handoff_mode(unit) do
          "manual_template" ->
            manual_github_pr_handoff_result!(
              bundle,
              target,
              unit,
              upstream_repo,
              fork_repo,
              base_branch,
              head_ref
            )

          "interactive_template" ->
            Util.witness(
              "git-submit: #{unit["name"]} deferring automatic template-preserving GitHub PR creation; handing off to your terminal/editor instead"
            )

            manual_github_pr_handoff_result!(
              bundle,
              target,
              unit,
              upstream_repo,
              fork_repo,
              base_branch,
              head_ref
            )

          _ ->
            open_scripted_github_pr!(
              checkout_dir,
              upstream_repo,
              fork_repo,
              base_branch,
              branch_name,
              head_ref,
              target,
              unit
            )
        end

      url ->
        %{
          name: unit["name"],
          status: "submitted",
          repo: upstream_repo,
          fork_repo: fork_repo,
          pull_request: url,
          existing_pull_request: true
        }
        |> with_validated_system(unit)
    end
  end

  defp pull_request_title(unit) do
    case Map.get(unit, "suggested_pr_title") do
      title when is_binary(title) and title != "" ->
        title

      _ ->
        case Map.get(unit, "commit_message") do
          title when is_binary(title) and title != "" ->
            title

          _ ->
            nil
        end
    end
  end

  defp open_scripted_github_pr!(
         checkout_dir,
         upstream_repo,
         fork_repo,
         base_branch,
         branch_name,
         head_ref,
         target,
         unit
       ) do
    Util.witness("git-submit: #{unit["name"]} opening GitHub PR")

    args = [
      "pr",
      "create",
      "--repo",
      upstream_repo,
      "--base",
      base_branch,
      "--head",
      head_ref,
      "--title",
      unit["commit_message"],
      "--body",
      pr_body(target, unit),
      "--draft"
    ]

    case Util.run("gh", args, cd: checkout_dir) do
      {:ok, output} ->
        %{
          name: unit["name"],
          status: "submitted",
          repo: upstream_repo,
          fork_repo: fork_repo,
          pull_request: github_pr_url_from_output(output)
        }
        |> with_validated_system(unit)

      {:error, %{output: output}} ->
        case detect_existing_github_pr(
               output,
               upstream_repo,
               head_ref,
               branch_name,
               base_branch
             ) do
          %{pull_request: url} ->
            %{
              name: unit["name"],
              status: "submitted",
              repo: upstream_repo,
              fork_repo: fork_repo,
              pull_request: url,
              existing_pull_request: true
            }
            |> with_validated_system(unit)

          nil ->
            raise "gh #{Enum.join(args, " ")} failed: #{output}"
        end
    end
  end

  defp write_plan!(plan_path, target, units) do
    contents =
      [
        "# Git submission plan",
        "",
        "- Target: `#{target["name"]}`",
        "- Mode: `#{target["submission_mode"]}`",
        suggested_title_lines(target),
        "- Units:",
        Enum.map(units, fn unit ->
          [
            "  - `#{unit["name"]}`",
            "    - Destination repo: #{get_in(unit, ["destination", "repo"]) || "n/a"}",
            "    - Fork repo: #{get_in(unit, ["destination", "fork_repo"]) || "n/a"}",
            commit_plan_lines(unit)
          ]
        end),
        ""
      ]
      |> List.flatten()
      |> Enum.join("\n")
      |> Kernel.<>("\n")

    File.write!(plan_path, contents)
  end

  defp clone_github_repo_for_dry_run!(repo, persistent_checkout, checkout_dir) do
    File.mkdir_p!(Path.dirname(checkout_dir))

    cond do
      File.dir?(Path.join(persistent_checkout, ".git")) ->
        Util.run!("git", ["clone", "--shared", persistent_checkout, checkout_dir])

      true ->
        clone_github_repo_if_missing!(repo, checkout_dir)
    end
  end

  defp clone_git_repo_for_dry_run!(repo, persistent_checkout, checkout_dir, env) do
    File.mkdir_p!(Path.dirname(checkout_dir))

    cond do
      File.dir?(Path.join(persistent_checkout, ".git")) ->
        Util.run!("git", ["clone", "--shared", persistent_checkout, checkout_dir], env: env)

      true ->
        Util.run!("git", ["clone", repo, checkout_dir], env: env)
    end
  end

  defp clone_gitlab_repo_for_dry_run!(repo, persistent_checkout, checkout_dir, env) do
    File.mkdir_p!(Path.dirname(checkout_dir))

    cond do
      File.dir?(Path.join(persistent_checkout, ".git")) ->
        Util.run!("git", ["clone", "--shared", persistent_checkout, checkout_dir], env: env)

      true ->
        clone_gitlab_repo_if_missing!("gitlab.com", repo, checkout_dir, env)
    end
  end

  defp clone_git_repo_if_missing!(repo, checkout_dir, env) do
    File.mkdir_p!(Path.dirname(checkout_dir))

    unless File.dir?(checkout_dir) do
      Util.run!("git", ["clone", repo, checkout_dir], env: env)
    end
  end

  defp clone_github_repo_if_missing!(repo, checkout_dir) do
    File.mkdir_p!(Path.dirname(checkout_dir))

    unless File.dir?(checkout_dir) do
      Util.run!("gh", ["repo", "clone", repo, checkout_dir])
    end
  end

  defp clone_gitlab_repo_if_missing!(_host, repo, checkout_dir, env) do
    File.mkdir_p!(Path.dirname(checkout_dir))

    unless File.dir?(checkout_dir) do
      Util.run!("glab", ["repo", "clone", repo, checkout_dir], env: env)
    end
  end

  defp suggested_title_lines(target) do
    case Map.get(target, "suggested_pr_title") do
      title when is_binary(title) and title != "" -> ["- Suggested PR title: `#{title}`"]
      _ -> []
    end
  end

  defp commit_plan_lines(unit) do
    case planned_commit_messages(unit) do
      [] ->
        []

      commit_messages ->
        ["    - Commit subjects:"] ++ Enum.map(commit_messages, &"      - `#{&1}`")
    end
  end

  defp ensure_github_repo!(upstream_repo, fork_repo) do
    if github_repo_exists?(fork_repo) do
      :ok
    else
      current_user = github_current_username!()

      repo_owner =
        Util.split_repo_owner(fork_repo) || raise "cannot determine GitHub owner for #{fork_repo}"

      fork_name = repo_name!(fork_repo)
      upstream_name = repo_name!(upstream_repo)

      cond do
        upstream_repo == fork_repo ->
          create_github_repo!(fork_repo, repo_owner, current_user)

        true ->
          fork_github_repo!(upstream_repo, repo_owner, current_user, fork_name, upstream_name)
      end

      wait_for_github_repo!(fork_repo)
    end
  end

  defp github_repo_exists?(repo) do
    case Util.run("gh", ["api", "repos/#{repo}"]) do
      {:ok, _output} ->
        true

      {:error, %{status: 1, output: output}} when is_binary(output) ->
        if String.contains?(output, "Not Found") or String.contains?(output, "HTTP 404") do
          false
        else
          raise "unable to inspect GitHub repository #{repo}: #{output}"
        end

      {:error, %{output: output}} ->
        raise "unable to inspect GitHub repository #{repo}: #{output}"
    end
  end

  defp wait_for_github_repo!(repo) do
    attempts = 15
    interval_ms = 1000

    if wait_for_github_repo(repo, attempts, interval_ms) do
      :ok
    else
      raise "repository #{repo} was requested on GitHub but did not become visible after #{attempts} seconds"
    end
  end

  defp wait_for_github_repo(_repo, 0, _interval_ms), do: false

  defp wait_for_github_repo(repo, attempts_left, interval_ms) do
    if github_repo_exists?(repo) do
      true
    else
      Process.sleep(interval_ms)
      wait_for_github_repo(repo, attempts_left - 1, interval_ms)
    end
  end

  defp create_github_repo!(repo, repo_owner, current_user) do
    if repo_owner != current_user do
      raise """
      configured GitHub repository #{repo} does not exist, and the authenticated GitHub user #{current_user} \
      cannot auto-create it in namespace #{repo_owner}; create the repository manually or set the destination owner to #{current_user}
      """
    end

    Util.run!("gh", ["repo", "create", repo, "--public", "--add-readme"])
  end

  defp fork_github_repo!(upstream_repo, repo_owner, current_user, fork_name, upstream_name) do
    if fork_name != upstream_name do
      raise """
      auto-fork currently supports GitHub forks that keep the upstream repository name; \
      pre-create #{repo_owner}/#{fork_name} manually or set destination.fork_repo to #{repo_owner}/#{upstream_name}
      """
    end

    fork_args =
      ["api", "repos/#{upstream_repo}/forks", "--method", "POST"] ++
        if(repo_owner == current_user, do: [], else: ["-f", "organization=#{repo_owner}"])

    Util.run!("gh", fork_args)
  end

  defp github_current_username! do
    Util.run!("gh", ["api", "user", "--jq", ".login"])
    |> String.trim()
  end

  defp ensure_gitlab_fork!(_host, upstream_repo, fork_repo, _env) when upstream_repo == fork_repo,
    do: :ok

  defp ensure_gitlab_fork!(host, upstream_repo, fork_repo, env) do
    if gitlab_project_exists?(host, fork_repo, env) do
      :ok
    else
      current_user = gitlab_current_username!(host, env)

      fork_owner =
        Util.split_repo_owner(fork_repo) ||
          raise "cannot determine GitLab fork owner for #{fork_repo}"

      if fork_owner != current_user do
        raise """
        configured GitLab fork #{fork_repo} does not exist, and the authenticated #{host} user #{current_user} \
        cannot auto-create a fork in namespace #{fork_owner}; set accounts.gitlab_owner to #{current_user} or create the fork manually
        """
      end

      fork_name = repo_name!(fork_repo)
      upstream_name = repo_name!(upstream_repo)

      fork_args =
        ["repo", "fork", upstream_repo, "--clone=false", "--remote=false"] ++
          if fork_name == upstream_name, do: [], else: ["--path", fork_name]

      Util.run!("glab", fork_args, env: env)
      wait_for_gitlab_project!(host, fork_repo, env)
    end
  end

  defp gitlab_project_exists?(host, repo, env) do
    endpoint = "projects/#{gitlab_project_id(repo)}"

    case Util.run("glab", ["api", endpoint, "--hostname", host], env: env) do
      {:ok, _output} ->
        true

      {:error, %{status: 1, output: output}} when is_binary(output) ->
        if String.contains?(output, "404 Project Not Found") do
          false
        else
          raise "unable to inspect GitLab project #{repo} on #{host}: #{output}"
        end

      {:error, %{output: output}} ->
        raise "unable to inspect GitLab project #{repo} on #{host}: #{output}"
    end
  end

  defp wait_for_gitlab_project!(host, repo, env) do
    attempts = 15
    interval_ms = 1000

    if wait_for_gitlab_project(repo, host, env, attempts, interval_ms) do
      :ok
    else
      raise "fork #{repo} was requested on #{host} but did not become visible after #{attempts} seconds"
    end
  end

  defp wait_for_gitlab_project(_repo, _host, _env, 0, _interval_ms), do: false

  defp wait_for_gitlab_project(repo, host, env, attempts_left, interval_ms) do
    if gitlab_project_exists?(host, repo, env) do
      true
    else
      Process.sleep(interval_ms)
      wait_for_gitlab_project(repo, host, env, attempts_left - 1, interval_ms)
    end
  end

  defp gitlab_current_username!(host, env) do
    host
    |> gitlab_current_user!(env)
    |> Map.fetch!("username")
  end

  defp gitlab_current_user!(host, env) do
    output = Util.run!("glab", ["api", "user", "--hostname", host], env: env)
    :json.decode(output)
  end

  defp gitlab_project_id(repo) do
    URI.encode_www_form(repo)
  end

  defp gitlab_env(host) do
    [{"GITLAB_HOST", host}]
  end

  defp git_env_for_repo(repo) do
    if String.starts_with?(repo, "ssh://aur@aur.archlinux.org/") or
         String.contains?(repo, "@wip.pkgsrc.org:") do
      [{"GIT_SSH_COMMAND", "ssh -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10"}]
    else
      []
    end
  end

  defp push_branch!(checkout_dir, branch_name) do
    Util.run!(
      "git",
      ["push", "--force-with-lease", "--set-upstream", "origin", branch_name],
      cd: checkout_dir
    )
  end

  defp push_checkout!(checkout_dir, repo, branch_name, unit) do
    env = git_env_for_repo(repo)

    case Map.get(unit, "push_command") do
      [program | args] ->
        Util.witness(
          "git-submit: #{unit["name"]} running #{Enum.join([program | args], " ")}"
        )

        Util.run!(program, args, cd: checkout_dir, env: env)

      [] ->
        Util.run!("git", ["push", "origin", "HEAD:#{branch_name}"], cd: checkout_dir, env: env)

      nil ->
        Util.run!("git", ["push", "origin", "HEAD:#{branch_name}"], cd: checkout_dir, env: env)

      other ->
        raise "push_command must be a non-empty argv list, got: #{inspect(other)}"
    end
  end

  @doc false
  def detect_existing_gitlab_mr(output, host, upstream_repo)

  def detect_existing_gitlab_mr(output, host, upstream_repo) when is_binary(output) do
    normalized = normalize_whitespace(output)

    if String.contains?(
         normalized,
         "Another open merge request already exists for this source branch"
       ) do
      iid = gitlab_existing_mr_iid!(normalized)
      %{merge_request: "https://#{host}/#{upstream_repo}/-/merge_requests/#{iid}"}
    else
      nil
    end
  end

  @doc false
  def detect_existing_github_pr(output, repo, head_ref, branch_name, base_branch)

  def detect_existing_github_pr(output, repo, head_ref, branch_name, base_branch)
      when is_binary(output) do
    normalized = normalize_whitespace(output)

    if String.contains?(normalized, "already exists: https://github.com/") or
         String.contains?(normalized, "pull request already exists") or
         String.contains?(normalized, "pull request for branch") or
         String.contains?(normalized, "A pull request already exists for") do
      case github_pr_url_from_output(output) do
        url when is_binary(url) ->
          %{pull_request: url}

        _ ->
          case existing_github_pr_url(repo, [head_ref, branch_name], base_branch) do
            nil -> nil
            url -> %{pull_request: url}
          end
      end
    else
      nil
    end
  end

  defp gitlab_existing_mr_iid!(output) do
    case Regex.run(~r/source branch:\s*!(\d+)/, output, capture: :all_but_first) do
      [iid] -> iid
      _ -> raise "could not determine existing GitLab merge request IID from: #{output}"
    end
  end

  defp normalize_whitespace(output) do
    output
    |> String.replace(~r/\s+/, " ")
    |> String.trim()
  end

  defp existing_github_pr_url(repo, head_refs, base_branch) when is_list(head_refs) do
    head_refs
    |> Enum.uniq()
    |> Enum.find_value(&existing_github_pr_url(repo, &1, base_branch))
  end

  defp existing_github_pr_url(repo, head_ref, base_branch) do
    case Util.run("gh", [
           "pr",
           "list",
           "--repo",
           repo,
           "--head",
           head_ref,
           "--base",
           base_branch,
           "--state",
           "open",
           "--json",
           "url"
         ]) do
      {:ok, output} ->
        case :json.decode(output) do
          [%{"url" => url} | _] when is_binary(url) and url != "" -> url
          _ -> nil
        end

      {:error, %{output: output}} ->
        raise "unable to inspect existing GitHub pull requests for #{head_ref} in #{repo}: #{output}"
    end
  end

  defp github_pr_url_from_output(output) when is_binary(output) do
    case Regex.run(~r|https://github\.com/[^\s]+/pull/\d+|, output) do
      [url] -> url
      _ -> nil
    end
  end

  defp gitlab_mr_url_from_output(output, host, upstream_repo) when is_binary(output) do
    case Regex.run(~r|https://#{Regex.escape(host)}/[^\s]+/-/merge_requests/\d+|, output) do
      [url] -> url
      _ -> "https://#{host}/#{upstream_repo}/-/merge_requests"
    end
  end

  defp repo_name!(repo) do
    repo
    |> String.split("/", parts: 2)
    |> List.last()
    |> case do
      nil -> raise "cannot determine repository name for #{repo}"
      "" -> raise "cannot determine repository name for #{repo}"
      name -> name
    end
  end

  defp ensure_git_remote!(checkout_dir, remote_name, remote_url) do
    case Util.run("git", ["remote", "get-url", remote_name], cd: checkout_dir) do
      {:ok, _url} ->
        :ok

      {:error, _reason} ->
        Util.run!("git", ["remote", "add", remote_name, remote_url], cd: checkout_dir)
    end
  end

  defp ensure_git_branch!(checkout_dir, base_branch, branch_name, allow_orphan) do
    Util.run("git", ["fetch", "origin", "--prune"], cd: checkout_dir)

    case Util.run("git", ["checkout", base_branch], cd: checkout_dir) do
      {:ok, _output} ->
        :ok

      {:error, _reason} when allow_orphan ->
        Util.run!("git", ["checkout", "--orphan", base_branch], cd: checkout_dir)

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

  defp apply_checkout_mutations!(bundle_source_root, unit, checkout_dir) do
    unit
    |> Map.get("checkout_mutations", [])
    |> Enum.each(fn mutation ->
      apply_checkout_mutation!(bundle_source_root, mutation, checkout_dir)
    end)
  end

  defp apply_checkout_mutation!(bundle_source_root, mutation, checkout_dir) do
    case Map.get(mutation, "type") do
      "nix_maintainer_entry" ->
        source_path = Path.join(bundle_source_root, Map.fetch!(mutation, "source"))
        destination_path = Path.join(checkout_dir, Map.fetch!(mutation, "destination"))
        handle = Map.fetch!(mutation, "handle")
        rendered_entry = File.read!(source_path)
        updated = upsert_nix_maintainer_entry(File.read!(destination_path), handle, rendered_entry)
        File.write!(destination_path, updated)

      other ->
        raise "unsupported checkout mutation #{inspect(other)}"
    end
  end

  @doc false
  def upsert_nix_maintainer_entry(contents, handle, rendered_entry)
      when is_binary(contents) and is_binary(handle) and is_binary(rendered_entry) do
    normalized_entry = String.trim_trailing(rendered_entry) <> "\n"
    lines = String.split(contents, "\n", trim: false)

    case nix_maintainer_entry_range(lines, handle) do
      {start_index, end_index} ->
        existing_entry =
          lines
          |> Enum.slice(start_index..end_index)
          |> Enum.join("\n")
          |> Kernel.<>("\n")

        if String.trim(existing_entry) == String.trim(normalized_entry) do
          contents
        else
          raise "maintainers/maintainer-list.nix already defines #{handle} with different public data"
        end

      nil ->
        insert_nix_maintainer_entry(contents, lines, normalized_entry, handle)
    end
  end

  defp insert_nix_maintainer_entry(_contents, lines, rendered_entry, handle) do
    insertion_index =
      case first_sorted_entry_index(lines, handle) do
        nil -> closing_brace_index!(lines)
        index -> index
      end

    entry_lines =
      String.split(String.trim_trailing(rendered_entry) <> "\n\n", "\n", trim: false)

    {before_lines, remaining_lines} = Enum.split(lines, insertion_index)
    Enum.join(before_lines ++ entry_lines ++ remaining_lines, "\n")
  end

  defp nix_maintainer_entry_range(lines, handle) do
    start_pattern = ~r/^  #{Regex.escape(handle)}\s*=\s*\{$/

    case Enum.find_index(lines, &Regex.match?(start_pattern, &1)) do
      nil ->
        nil

      start_index ->
        end_index =
          lines
          |> Enum.drop(start_index + 1)
          |> Enum.find_index(&Regex.match?(~r/^  \};\s*$/, &1))

        if is_nil(end_index) do
          raise "could not determine the end of maintainer entry #{handle}"
        else
          {start_index, start_index + end_index + 1}
        end
    end
  end

  defp first_sorted_entry_index(lines, handle) do
    lines
    |> Enum.with_index()
    |> Enum.find_value(fn {line, index} ->
      case Regex.run(~r/^  ([A-Za-z0-9._+-]+)\s*=\s*\{$/, line, capture: :all_but_first) do
        [existing_handle] ->
          if existing_handle > handle, do: index

        _ ->
          nil
      end
    end)
  end

  defp closing_brace_index!(lines) do
    lines
    |> Enum.with_index()
    |> Enum.reverse()
    |> Enum.find_value(fn {line, index} ->
      if Regex.match?(~r/^\}\s*$/, line), do: index
    end)
    |> case do
      nil -> raise "could not locate the end of maintainers/maintainer-list.nix"
      index -> index
    end
  end

  defp commit_checkout!(checkout_dir, unit) do
    case Map.get(unit, "commit_plan", []) do
      commit_plan when is_list(commit_plan) and commit_plan != [] ->
        commit_planned_steps!(checkout_dir, commit_plan, unit)

      _ ->
        commit_if_needed!(checkout_dir, unit["commit_message"], unit)
    end
  end

  defp commit_if_needed!(checkout_dir, message, unit) do
    Util.run!("git", ["add", "-A"], cd: checkout_dir)

    case System.cmd("git", ["diff", "--cached", "--quiet"], cd: checkout_dir) do
      {_output, 0} ->
        :ok

      {_output, 1} ->
        Util.run!("git", commit_args(unit, message), cd: checkout_dir)
    end
  end

  defp commit_planned_steps!(checkout_dir, commit_plan, unit) do
    Enum.each(commit_plan, fn step ->
      message = Map.fetch!(step, "message")
      paths = Map.get(step, "paths", [])

      if paths == [] do
        raise "commit plan step #{inspect(message)} is missing paths"
      end

      stage_paths!(checkout_dir, paths)

      if cached_changes?(checkout_dir) do
        Util.run!("git", commit_args(unit, message), cd: checkout_dir)
      end
    end)

    case git_status_porcelain(checkout_dir) do
      "" ->
        :ok

      remaining ->
        raise "checkout has uncommitted changes outside the commit plan: #{String.trim(remaining)}"
    end
  end

  defp stage_paths!(checkout_dir, paths) do
    Util.run!("git", ["add", "-A", "--"] ++ paths, cd: checkout_dir)
  end

  defp commit_args(unit, message) do
    ["commit"] ++
      if(Map.get(unit, "git_commit_signoff", false), do: ["-s"], else: []) ++ ["-m", message]
  end

  defp cached_changes?(checkout_dir) do
    case System.cmd("git", ["diff", "--cached", "--quiet"], cd: checkout_dir) do
      {_output, 0} -> false
      {_output, 1} -> true
    end
  end

  defp git_status_porcelain(checkout_dir) do
    Util.run!("git", ["status", "--porcelain"], cd: checkout_dir)
  end

  defp run_pre_submit_checks!(checkout_dir, unit, mode, opts) do
    dry_run = Keyword.get(opts, :dry_run, false)

    unit
    |> Map.get("pre_submit_checks", [])
    |> Enum.filter(&(Map.get(&1, "mode", "check") == mode))
    |> Enum.filter(&(not dry_run or Map.get(&1, "run_on_dry_run", false)))
    |> Enum.each(fn check ->
      run_pre_submit_check!(checkout_dir, check)
    end)
  end

  defp run_pre_submit_check!(checkout_dir, check) do
    name = Map.get(check, "name", "pre-submit check")
    command = Map.fetch!(check, "command")
    cwd = check_cwd!(checkout_dir, Map.get(check, "cwd", "checkout"))
    {program, args} = normalize_check_command(command, cwd)

    Util.witness("git-submit: running #{name}")
    Util.run!(program, args, cd: cwd)
  end

  defp check_cwd!(checkout_dir, nil), do: checkout_dir
  defp check_cwd!(checkout_dir, "checkout"), do: checkout_dir

  defp check_cwd!(_checkout_dir, other) do
    raise "unsupported pre_submit_checks cwd #{inspect(other)}"
  end

  defp normalize_check_command(command, cwd) when is_list(command) do
    case command do
      [program | args] when is_binary(program) -> {resolve_check_program(program, cwd), args}
      _ -> raise "pre_submit_checks command lists must start with a program name"
    end
  end

  defp normalize_check_command(command, _cwd) when is_binary(command) do
    {"sh", ["-c", command]}
  end

  defp resolve_check_program(program, cwd) do
    if String.contains?(program, "/") and Path.type(program) != :absolute do
      Path.expand(program, cwd)
    else
      program
    end
  end

  defp dry_run_checks(unit) do
    unit
    |> Map.get("pre_submit_checks", [])
    |> Enum.filter(&Map.get(&1, "run_on_dry_run", false))
    |> Enum.map(fn check ->
      %{
        name: Map.get(check, "name", "pre-submit check"),
        mode: Map.get(check, "mode", "check")
      }
    end)
  end

  defp planned_commit_messages(unit) do
    case Map.get(unit, "commit_plan", []) do
      commit_plan when is_list(commit_plan) and commit_plan != [] ->
        Enum.map(commit_plan, &Map.fetch!(&1, "message"))

      _ ->
        case Map.get(unit, "commit_message") do
          message when is_binary(message) and message != "" -> [message]
          _ -> []
        end
    end
  end

  defp pull_request_handoff_mode(unit) do
    Map.get(unit, "pull_request_handoff")
  end

  defp manual_github_pr_handoff_result!(
         bundle,
         target,
         unit,
         upstream_repo,
         fork_repo,
         base_branch,
         head_ref
       ) do
    handoff_path =
      write_pr_handoff!(
        bundle,
        target,
        unit,
        upstream_repo,
        base_branch,
        head_ref
      )

    suggested_pr_title = pull_request_title(unit)
    next_step_command = manual_pr_command(upstream_repo, base_branch, head_ref, suggested_pr_title)
    validated_system = validated_system_for(unit)
    print_manual_pr_command!(target, next_step_command, suggested_pr_title, validated_system)

    %{
      name: unit["name"],
      status: "awaiting_pr_creation",
      repo: upstream_repo,
      fork_repo: fork_repo,
      head_ref: head_ref,
      base_branch: base_branch,
      handoff: handoff_path,
      next_step_command: next_step_command,
      validated_system: validated_system,
      suggested_pr_title: suggested_pr_title,
      commit_messages: planned_commit_messages(unit)
    }
  end

  defp write_pr_handoff!(bundle, target, unit, upstream_repo, base_branch, head_ref) do
    handoff_name =
      if unit["name"] == target["name"] do
        "PULL_REQUEST_HANDOFF.md"
      else
        "PULL_REQUEST_HANDOFF_#{unit["name"]}.md"
      end

    handoff_path = Path.join(bundle.bundle_root, handoff_name)
    suggested_pr_title = pull_request_title(unit)
    next_step_command = manual_pr_command(upstream_repo, base_branch, head_ref, suggested_pr_title)
    commit_messages = planned_commit_messages(unit)
    validated_system = validated_system_for(unit)

    File.write!(
      handoff_path,
      """
      # Pull request handoff

      This branch was pushed for `#{target["name"]}` / `#{unit["name"]}`, but the
      rollout tool intentionally did not create the upstream pull request.
      This target expects contributors to go through the upstream pull request
      template directly from their own terminal/editor session instead of
      running rollout-hosted interactive prompts.

      Branch details:

      - Upstream repo: `#{upstream_repo}`
      - Base branch: `#{base_branch}`
      - Head branch: `#{head_ref}`
      #{if is_binary(validated_system) and validated_system != "", do: "- Validated locally on: `#{validated_system}`", else: ""}

      Suggested PR title:

      - `#{suggested_pr_title || "(set this yourself)"}`

      Commit subjects:

      #{Enum.map_join(commit_messages, "\n", &"- `#{&1}`")}

      Run this command in your own terminal/editor session and complete the
      upstream template there:

      ```bash
      #{next_step_command}
      ```

      Optional follow-up after the PR exists:

      - `nixpkgs-review pr <number>`
      - executable testing on `#{validated_system || "a Linux/Nix host"}` if relevant for the package
      """
    )

    handoff_path
  end

  defp manual_pr_command(upstream_repo, base_branch, head_ref, pr_title) do
    args =
      [
        {"--repo", upstream_repo},
        {"--base", base_branch},
        {"--head", head_ref}
      ] ++ if(is_binary(pr_title) and pr_title != "", do: [{"--title", pr_title}], else: [])

    ["gh pr create \\"]
    |> Kernel.++(
      args
      |> Enum.with_index()
      |> Enum.map(fn {{flag, value}, index} ->
        suffix = if index == length(args) - 1, do: "", else: " \\"
        "  #{flag} #{Util.shell_escape(value)}#{suffix}"
      end)
    )
    |> Enum.join("\n")
  end

  defp print_manual_pr_command!(target, pr_command, suggested_pr_title, validated_system) do
    IO.puts("")

    if is_binary(suggested_pr_title) and suggested_pr_title != "" do
      IO.puts("[linux-rollout] #{target["name"]}: suggested PR title: #{suggested_pr_title}")
      IO.puts("")
    end

    if is_binary(validated_system) and validated_system != "" do
      IO.puts("[linux-rollout] #{target["name"]}: validated locally on #{validated_system}")
      IO.puts("")
    end

    IO.puts(
      "[linux-rollout] #{target["name"]}: run this command in your own terminal/editor session and complete the upstream PR template there:"
    )

    IO.puts("")
    IO.puts(pr_command)
    IO.puts("")
  end

  defp unit_summary(unit) do
    %{
      name: unit["name"],
      repo: get_in(unit, ["destination", "repo"]),
      fork_repo: get_in(unit, ["destination", "fork_repo"]),
      validated_system: validated_system_for(unit)
    }
  end

  defp validated_system_for(unit) do
    case Map.get(unit, "validated_system") do
      value when is_binary(value) and value != "" -> value
      _ -> nil
    end
  end

  defp with_validated_system(result, unit) do
    case validated_system_for(unit) do
      nil -> result
      validated_system -> Map.put(result, :validated_system, validated_system)
    end
  end

  defp pr_body(target, unit) do
    [
      "Prepared by the local WireShare Linux rollout orchestrator.",
      "",
      "- Target: `#{target["name"]}`",
      "- Unit: `#{unit["name"]}`",
      "- Submission mode: `#{target["submission_mode"]}`"
    ]
    |> Enum.join("\n")
  end
end
