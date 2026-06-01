defmodule LinuxRollout.Adapters.HumanGate do
  alias LinuxRollout.Util

  def submit(workspace, bundle, target, opts) do
    pending_path = Path.join(workspace.approval_root, "#{target["name"]}.pending")
    approved_path = Path.join(workspace.approval_root, "#{target["name"]}.approved")
    File.mkdir_p!(workspace.approval_root)

    review_packet = Path.join(bundle.bundle_root, "REVIEW_PACKET.md")
    approved_steps = Path.join(bundle.bundle_root, "APPROVED_NEXT_STEPS.md")
    pr_body_path = Path.join(bundle.bundle_root, "PR_BODY.md")
    repo_root = Path.join(bundle.bundle_root, "HUMAN_SUBMISSION_ROOT")

    prepare_human_submission_root!(bundle, repo_root)
    File.write!(pr_body_path, pr_body_contents(repo_root))
    pr_command = pr_command(workspace, target, repo_root, pr_body_path)

    File.write!(
      review_packet,
      """
      # Review packet

      Target: `#{target["name"]}`

      Review the rendered files under `source/` and sign off before any outward
      submission step. This gate exists specifically for policies like Flathub's
      that require human review and human-authored final submission.

      This tool will not run the final Flathub submission PR command for you.
      After review, acknowledge responsibility for the final human-run PR step with:

      `scripts/release_linux_rollout.sh resume --target #{target["name"]} --acknowledge`
      """
    )

    cond do
      approved?(opts) ->
        File.rm_rf!(pending_path)

        File.write!(
          approved_path,
          "approved_at=#{Util.timestamp_utc()}\nacknowledgement=#{acknowledgement_value(opts)}\n"
        )

        File.write!(approved_steps, approved_steps_contents(target, repo_root))

        print_manual_pr_command!(target, pr_command)

        %{
          status: "approved",
          review_packet: review_packet,
          next_steps: approved_steps,
          pr_body: pr_body_path,
          submission_root: repo_root,
          manual_pr_command: pr_command
        }

      File.exists?(approved_path) ->
        print_manual_pr_command!(target, pr_command)

        %{
          status: if(opts[:dry_run], do: "dry_run", else: "approved"),
          review_packet: review_packet,
          next_steps: approved_steps,
          pr_body: pr_body_path,
          submission_root: repo_root,
          manual_pr_command: pr_command
        }

      true ->
        File.write!(pending_path, "pending_at=#{Util.timestamp_utc()}\n")
        %{status: "awaiting_approval", review_packet: review_packet}
    end
  end

  defp prepare_human_submission_root!(bundle, repo_root) do
    File.rm_rf!(repo_root)
    File.mkdir_p!(repo_root)

    bundle.source_root
    |> Path.join("packaging/flathub")
    |> File.ls!()
    |> Enum.reject(&(&1 == "README.md"))
    |> Enum.each(fn entry ->
      source = Path.join([bundle.source_root, "packaging/flathub", entry])
      destination = Path.join(repo_root, entry)
      copy_submission_entry!(source, destination)
    end)
  end

  defp copy_submission_entry!(source, destination) do
    cond do
      File.regular?(source) ->
        Util.copy_file!(source, destination)

      File.dir?(source) ->
        File.mkdir_p!(destination)

        for child <- File.ls!(source) do
          copy_submission_entry!(Path.join(source, child), Path.join(destination, child))
        end

      true ->
        raise "missing Flathub submission file #{source}"
    end
  end

  defp approved?(opts) do
    Keyword.get(opts, :approve, false) or Keyword.get(opts, :acknowledge, false)
  end

  defp acknowledgement_value(opts) do
    cond do
      Keyword.get(opts, :acknowledge, false) -> "--acknowledge"
      Keyword.get(opts, :approve, false) -> "--approve"
      true -> ""
    end
  end

  defp approved_steps_contents(target, repo_root) do
    manifest_name =
      repo_root
      |> File.ls!()
      |> Enum.find(&String.ends_with?(&1, [".yml", ".yaml", ".json"])) ||
        "cx.hermes.WireShare.yaml"

    app_id = Path.rootname(manifest_name)

    """
    # Approved next steps

    Final outward submission for `#{target["name"]}` remains human-authored.
    The rollout tool printed the exact final `gh pr create` command for your
    current fork owner. Copy and run that command yourself after you verify the
    branch contents and PR body.

    Bundle-relative payload:

    - `HUMAN_SUBMISSION_ROOT/`
    - `PR_BODY.md`

    Suggested terminal steps from the bundle directory:

    ```bash
    gh repo fork --clone flathub/flathub
    cd flathub
    git checkout --track origin/new-pr
    git checkout -b #{submission_branch_name(app_id)}
    cp ../HUMAN_SUBMISSION_ROOT/#{Util.shell_escape(manifest_name)} .
    git add #{Util.shell_escape(manifest_name)}
    git commit -m #{Util.shell_escape("Add #{app_id}")}
    git push origin HEAD
    ```

    The orchestrator printed the exact final `gh pr create` command separately.
    """
  end

  defp pr_body_contents(repo_root) do
    manifest_name =
      repo_root
      |> File.ls!()
      |> Enum.find(&String.ends_with?(&1, [".yml", ".yaml", ".json"])) ||
        "cx.hermes.WireShare.yaml"

    app_id = Path.rootname(manifest_name)

    """
    Submitting #{app_id} to Flathub.
    """
  end

  defp pr_command(workspace, target, repo_root, pr_body_path) do
    repo =
      get_in(target, ["destination", "repo"]) ||
        "flathub/flathub"

    manifest_name =
      repo_root
      |> File.ls!()
      |> Enum.find(&String.ends_with?(&1, [".yml", ".yaml", ".json"])) ||
        "cx.hermes.WireShare.yaml"

    app_id = Path.rootname(manifest_name)
    branch_name = submission_branch_name(app_id)
    head_owner = head_owner(workspace, target)

    """
    gh pr create \\
      --repo #{repo} \\
      --base new-pr \\
      --head #{head_owner}:#{branch_name} \\
      --title #{Util.shell_escape("Add #{app_id}")} \\
      --body-file #{Util.shell_escape(pr_body_path)}
    """
    |> String.trim_trailing()
  end

  defp submission_branch_name(app_id) do
    app_id
    |> String.replace(~r/[^A-Za-z0-9]+/, "-")
    |> String.trim("-")
    |> Kernel.<>("-submission")
  end

  defp head_owner(workspace, target) do
    target
    |> effective_fork_repo(workspace)
    |> Util.split_repo_owner()
    |> blank_to(gh_current_username())
    |> blank_to("your_github_username")
  end

  defp effective_fork_repo(target, workspace) do
    case get_in(target, ["destination", "fork_repo"]) do
      value when value not in [nil, ""] ->
        value

      _ ->
        case workspace.tokens["github_owner"] do
          value when value not in [nil, ""] -> "#{value}/flathub"
          _ -> ""
        end
    end
  end

  defp gh_current_username do
    case Util.run("gh", ["api", "user"]) do
      {:ok, output} ->
        output
        |> :json.decode()
        |> Map.get("login", "")

      {:error, _reason} ->
        ""
    end
  end

  defp blank_to(value, fallback) when value in [nil, ""], do: fallback
  defp blank_to(value, _fallback), do: value

  defp print_manual_pr_command!(target, pr_command) do
    IO.puts("")
    IO.puts("[linux-rollout] #{target["name"]}: run this command yourself after review:")
    IO.puts("")
    IO.puts(pr_command)
    IO.puts("")
  end
end
