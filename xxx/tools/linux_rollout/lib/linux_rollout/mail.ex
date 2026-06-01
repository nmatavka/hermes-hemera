defmodule LinuxRollout.Mail do
  alias LinuxRollout.Util

  @text_extensions ~w(.patch .scm .txt .md .diff .desktop .metainfo .xml .yaml .yml .recipe .spec .info .SlackBuild)

  def render_payload!(payload_path, output_path) do
    payload = read_payload!(payload_path)
    raw = render_message(payload)
    Util.ensure_directory!(output_path)
    File.write!(output_path, raw)
    output_path
  end

  def send_payload!(payload_path) do
    payload = read_payload!(payload_path)
    raw = render_message(payload)

    cond do
      sendmail = System.find_executable("sendmail") ->
        Util.run_with_input!(sendmail, ["-t", "-i"], raw)
        "sent via sendmail"

      mailx = System.find_executable("mailx") || System.find_executable("mail") ->
        send_with_mailx!(mailx, payload)
        "sent via mailx"

      true ->
        raise "no sendmail or mailx-compatible mail transport is available"
    end
  end

  def render_message(payload) when is_map(payload) do
    headers = [
      {"From", format_address(payload["from_name"], Map.fetch!(payload, "from_email"))},
      {"To", Map.fetch!(payload, "to")},
      maybe_header("Cc", payload["cc"]),
      {"Subject", Map.fetch!(payload, "subject")},
      {"MIME-Version", "1.0"}
    ]

    attachments = Map.get(payload, "attachments", []) || []

    if attachments == [] do
      encode_headers(headers ++ [{"Content-Type", "text/plain; charset=utf-8"}]) <>
        "\n\n" <> normalize_body(Map.fetch!(payload, "body"))
    else
      boundary = "WireShare-#{System.unique_integer([:positive])}"

      encode_headers(headers ++ [{"Content-Type", "multipart/mixed; boundary=\"#{boundary}\""}]) <>
        "\n\n" <>
        "--#{boundary}\n" <>
        "Content-Type: text/plain; charset=utf-8\n\n" <>
        normalize_body(Map.fetch!(payload, "body")) <>
        "\n" <>
        Enum.map_join(attachments, "", &attachment_part(&1, boundary)) <>
        "--#{boundary}--\n"
    end
  end

  defp read_payload!(payload_path) do
    payload_path
    |> File.read!()
    |> :json.decode()
  end

  defp send_with_mailx!(mailx, payload) do
    args =
      ["-s", Map.fetch!(payload, "subject")]
      |> maybe_append_cc(payload["cc"])
      |> append_attachments(Map.get(payload, "attachments", []) || [])
      |> Kernel.++([Map.fetch!(payload, "to")])

    Util.run_with_input!(mailx, args, Map.fetch!(payload, "body"))
  end

  defp maybe_append_cc(args, cc) when cc in [nil, ""], do: args
  defp maybe_append_cc(args, cc), do: args ++ ["-c", cc]

  defp append_attachments(args, attachments) do
    Enum.reduce(attachments, args, fn attachment, acc -> acc ++ ["-a", attachment] end)
  end

  defp attachment_part(path, boundary) do
    filename = Path.basename(path)
    content_type = content_type(path)

    "\n--#{boundary}\n" <>
      "Content-Type: #{content_type}; name=\"#{escape_quoted(filename)}\"\n" <>
      "Content-Transfer-Encoding: base64\n" <>
      "Content-Disposition: attachment; filename=\"#{escape_quoted(filename)}\"\n\n" <>
      base64_wrapped(File.read!(path)) <>
      "\n"
  end

  defp content_type(path) do
    extension = path |> Path.extname() |> String.downcase()

    cond do
      extension in @text_extensions -> "text/plain"
      extension in [".gz", ".tgz"] -> "application/gzip"
      extension in [".tar"] -> "application/x-tar"
      true -> "application/octet-stream"
    end
  end

  defp base64_wrapped(bytes) do
    bytes
    |> Base.encode64()
    |> String.graphemes()
    |> Enum.chunk_every(76)
    |> Enum.map_join("\n", &Enum.join/1)
    |> Kernel.<>("\n")
  end

  defp encode_headers(headers) do
    headers
    |> Enum.reject(&is_nil/1)
    |> Enum.map_join("\n", fn {name, value} -> "#{name}: #{sanitize_header(value)}" end)
  end

  defp maybe_header(_name, value) when value in [nil, ""], do: nil
  defp maybe_header(name, value), do: {name, value}

  defp format_address(name, email) when name in [nil, ""], do: email
  defp format_address(name, email), do: "#{quote_phrase(name)} <#{email}>"

  defp quote_phrase(value) do
    escaped =
      value |> sanitize_header() |> String.replace("\\", "\\\\") |> String.replace("\"", "\\\"")

    "\"#{escaped}\""
  end

  defp escape_quoted(value), do: String.replace(value, "\"", "\\\"")

  defp sanitize_header(value) do
    value
    |> to_string()
    |> String.replace(~r/[\r\n]+/, " ")
    |> String.trim()
  end

  defp normalize_body(body) do
    body = to_string(body)

    if String.ends_with?(body, "\n") do
      body
    else
      body <> "\n"
    end
  end
end
