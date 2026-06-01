defmodule LinuxRollout.Adapters.MailSubmit do
  alias LinuxRollout.{Mail, Util}

  def submit(workspace, bundle, target, opts) do
    plan_path = Path.join(bundle.bundle_root, "MAIL_SUBMIT_PLAN.md")
    write_plan!(plan_path, target)

    case target["submission_mode"] do
      "email_patch_thread" -> submit_patch_thread!(workspace, bundle, target, plan_path, opts)
      "email_attachment" -> submit_attachment!(workspace, bundle, target, plan_path, opts)
      other -> raise "unsupported mail submission mode #{other}"
    end
  end

  defp submit_patch_thread!(workspace, bundle, target, plan_path, opts) do
    {sender_name, sender_email} = sender_identity!(workspace)
    checkout_dir = Path.join([workspace.checkout_root, target["name"], "patch-series"])
    patch_dir = Path.join(bundle.bundle_root, "patches")
    Util.witness("mail-submit: #{target["name"]} preparing patch series")
    prepare_patch_repo!(bundle, target, checkout_dir, sender_name, sender_email, patch_dir)
    intro_payload_path = Path.join(bundle.bundle_root, "intro_mail.json")
    intro_preview_path = Path.join(bundle.bundle_root, "intro_mail.eml")

    intro_payload = %{
      "from_name" => sender_name,
      "from_email" => sender_email,
      "to" => get_in(target, ["destination", "email"]) || "guix-patches@gnu.org",
      "subject" => "[PATCH] gnu: add wireshare package",
      "body" => intro_body(bundle)
    }

    File.write!(intro_payload_path, IO.iodata_to_binary(:json.encode(intro_payload)))
    render_email!(workspace, intro_payload_path, intro_preview_path)

    thread_address = Keyword.get(opts, :thread_address, "")

    cond do
      thread_address != "" and Keyword.get(opts, :dry_run, false) ->
        Util.witness(
          "mail-submit: #{target["name"]} dry-running patch reply to #{thread_address}"
        )

        send_patch_series!(
          workspace,
          checkout_dir,
          sender_name,
          sender_email,
          thread_address,
          patch_dir,
          true
        )

        %{
          status: "dry_run",
          plan: plan_path,
          preview: intro_preview_path,
          patches: patch_dir
        }

      thread_address != "" ->
        Util.witness("mail-submit: #{target["name"]} sending patch reply to #{thread_address}")

        send_patch_series!(
          workspace,
          checkout_dir,
          sender_name,
          sender_email,
          thread_address,
          patch_dir,
          false
        )

        %{
          status: "submitted",
          plan: plan_path,
          preview: intro_preview_path,
          patches: patch_dir,
          thread_address: thread_address
        }

      Keyword.get(opts, :dry_run, false) ->
        %{
          status: "dry_run",
          plan: plan_path,
          preview: intro_preview_path,
          patches: patch_dir
        }

      true ->
        Util.witness("mail-submit: #{target["name"]} sending intro message")
        send_email!(workspace, intro_payload_path)

        %{
          status: "awaiting_thread_address",
          plan: plan_path,
          preview: intro_preview_path,
          patches: patch_dir
        }
    end
  end

  defp submit_attachment!(workspace, bundle, target, plan_path, opts) do
    {sender_name, sender_email} = sender_identity!(workspace)
    source_dir = Path.join(bundle.source_root, "packaging/openbsd")
    tarball_path = Path.join(bundle.bundle_root, "wireshare-openbsd-port.tar.gz")
    payload_path = Path.join(bundle.bundle_root, "openbsd_mail.json")
    preview_path = Path.join(bundle.bundle_root, "openbsd_mail.eml")

    Util.witness("mail-submit: #{target["name"]} packaging mail attachment")
    Util.run!("tar", ["-czf", tarball_path, "-C", source_dir, "."])

    payload = %{
      "from_name" => sender_name,
      "from_email" => sender_email,
      "to" => get_in(target, ["destination", "email"]) || "ports@openbsd.org",
      "subject" => "[PORT] wireshare #{workspace.version}",
      "body" => openbsd_body(bundle),
      "attachments" => [tarball_path]
    }

    File.write!(payload_path, IO.iodata_to_binary(:json.encode(payload)))
    render_email!(workspace, payload_path, preview_path)

    if Keyword.get(opts, :dry_run, false) do
      %{status: "dry_run", plan: plan_path, preview: preview_path, attachment: tarball_path}
    else
      Util.witness("mail-submit: #{target["name"]} sending message")
      send_email!(workspace, payload_path)
      %{status: "submitted", plan: plan_path, preview: preview_path, attachment: tarball_path}
    end
  end

  defp prepare_patch_repo!(bundle, target, checkout_dir, sender_name, sender_email, patch_dir) do
    File.rm_rf!(checkout_dir)
    File.mkdir_p!(checkout_dir)
    File.rm_rf!(patch_dir)
    File.mkdir_p!(patch_dir)

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

    Util.run!("git", ["format-patch", "--root", "--output-directory", patch_dir, "HEAD"],
      cd: checkout_dir
    )
  end

  defp send_patch_series!(
         workspace,
         checkout_dir,
         sender_name,
         sender_email,
         thread_address,
         patch_dir,
         dry_run
       ) do
    patches =
      patch_dir
      |> File.ls!()
      |> Enum.filter(&String.ends_with?(&1, ".patch"))
      |> Enum.sort()
      |> Enum.map(&Path.join(patch_dir, &1))

    if Util.git_send_email_available?() do
      from_value = "#{sender_name} <#{sender_email}>"
      dry_args = if dry_run, do: ["--dry-run"], else: []

      args =
        ["send-email"]
        |> Kernel.++(dry_args)
        |> Kernel.++(["--confirm=never", "--from", from_value])
        |> Kernel.++(["--to", thread_address])
        |> Kernel.++(patches)

      Util.run!("git", args, cd: checkout_dir)
    else
      Enum.each(patches, fn patch_path ->
        payload = patch_email_payload!(patch_path, thread_address, sender_name, sender_email)
        payload_path = patch_path <> ".mail.json"
        preview_path = patch_path <> ".eml"

        File.write!(payload_path, IO.iodata_to_binary(:json.encode(payload)))
        render_email!(workspace, payload_path, preview_path)

        unless dry_run do
          send_email!(workspace, payload_path)
        end
      end)
    end
  end

  defp render_email!(_workspace, payload_path, output_path) do
    Mail.render_payload!(payload_path, output_path)
  end

  defp send_email!(_workspace, payload_path) do
    Mail.send_payload!(payload_path)
  end

  defp sender_identity!(workspace) do
    configured_name = workspace.tokens["submission_name"] || ""
    configured_email = workspace.tokens["submission_email"] || ""
    git_name = Util.git_config("user.name")
    git_email = Util.git_config("user.email")

    {
      configured_name |> blank_to(git_name) |> blank_to("WireShare Packaging Bot"),
      configured_email |> blank_to(git_email)
    }
  end

  defp blank_to(value, fallback) when value in [nil, ""], do: fallback
  defp blank_to(value, _fallback), do: value

  defp patch_email_payload!(patch_path, to, sender_name, sender_email) do
    {subject, body} = patch_subject_and_body!(patch_path)

    %{
      "from_name" => sender_name,
      "from_email" => sender_email,
      "to" => to,
      "subject" => subject,
      "body" => body
    }
  end

  defp patch_subject_and_body!(patch_path) do
    contents = File.read!(patch_path)

    [header_block, body] =
      case Regex.split(~r/\r?\n\r?\n/, contents, parts: 2) do
        [headers, rest] -> [headers, rest]
        _ -> raise "could not split patch headers from #{patch_path}"
      end

    subject =
      header_block
      |> String.split(~r/\r?\n/)
      |> Enum.find_value(fn line ->
        case String.split(line, ":", parts: 2) do
          ["Subject", value] -> String.trim(value)
          _ -> nil
        end
      end)

    if subject in [nil, ""] do
      raise "could not parse patch subject from #{patch_path}"
    end

    {subject, body}
  end

  defp intro_body(bundle) do
    """
    Please assign a Guix issue number for the pending WireShare packaging update.

    The follow-up patch series is prepared locally and will be sent in reply to the
    assigned Debbugs thread address.

    Bundle summary:
    - #{bundle.summary_path}
    """
  end

  defp openbsd_body(bundle) do
    """
    Hello ports@,

    Please find a proposed WireShare OpenBSD port attached as a gzipped tarball.
    The upstream release inputs referenced by this port come from the current
    WireShare release bundle.

    Bundle summary:
    - #{bundle.summary_path}
    """
  end

  defp write_plan!(plan_path, target) do
    File.write!(
      plan_path,
      """
      # Mail submission plan

      - Target: `#{target["name"]}`
      - Mode: `#{target["submission_mode"]}`
      - Recipient: #{get_in(target, ["destination", "email"]) || "n/a"}
      """
    )
  end
end
