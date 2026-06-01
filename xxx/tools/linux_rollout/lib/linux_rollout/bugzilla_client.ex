defmodule LinuxRollout.BugzillaClient do
  alias LinuxRollout.Util

  @http_status_marker "\n__LINUX_ROLLOUT_HTTP_STATUS__="
  @content_type_marker "\n__LINUX_ROLLOUT_CONTENT_TYPE__="

  defmodule RequestError do
    defexception [
      :stage,
      :http_status,
      :content_type,
      :response_excerpt,
      :bug_id,
      :message
    ]

    def exception(opts) do
      stage = opts[:stage] || "request"
      http_status = opts[:http_status]
      content_type = opts[:content_type]
      response_excerpt = opts[:response_excerpt]
      bug_id = opts[:bug_id]

      summary =
        [
          "Bugzilla #{stage} failed",
          http_status && "HTTP #{http_status}",
          content_type && "content-type #{content_type}",
          bug_id && "bug #{bug_id}"
        ]
        |> Enum.reject(&is_nil/1)
        |> Enum.join(" ")

      message =
        case response_excerpt do
          excerpt when is_binary(excerpt) and excerpt != "" -> "#{summary}: #{excerpt}"
          _ -> summary
        end

      %__MODULE__{
        stage: stage,
        http_status: http_status,
        content_type: content_type,
        response_excerpt: response_excerpt,
        bug_id: bug_id,
        message: message
      }
    end
  end

  def auth_login!(base_url, login, api_key) do
    login = require_value(login, "Bugzilla login/email")
    api_key = require_value(api_key, "Bugzilla API key")

    IO.puts("Validating credentials against #{base_url} (this may take up to 60 seconds)...")
    user = validate_auth!(base_url, login, api_key)
    File.mkdir_p!(Util.bugzilla_credentials_dir())

    Util.write_json!(Util.bugzilla_credentials_file(base_url), %{
      "login" => login,
      "api_key" => api_key
    })

    display_name = user["real_name"] || user["name"] || login
    "Authenticated to #{base_url} as #{display_name}\n"
  end

  def auth_logout!(base_url) do
    path = Util.bugzilla_credentials_file(base_url)

    if File.exists?(path) do
      File.rm!(path)
      "Removed Bugzilla auth state: #{path}\n"
    else
      "No Bugzilla auth state was present.\n"
    end
  end

  def submit_request!(payload) do
    base_url = Map.fetch!(payload, "bugzilla_url")
    auth = load_auth!(base_url)
    bug_id = create_bug!(base_url, auth, payload)

    attachment_ids =
      payload
      |> Map.get("attachments", [])
      |> Enum.flat_map(fn attachment ->
        response =
          request!(
            "attach_file",
            base_url,
            "POST",
            "/bug/#{bug_id}/attachment",
            %{
              "api_key" => auth["api_key"],
              "data" => encoded_file!(Map.fetch!(attachment, "path")),
              "file_name" => Map.fetch!(attachment, "file_name"),
              "summary" => Map.fetch!(attachment, "summary"),
              "content_type" => Map.get(attachment, "content_type", "text/plain"),
              "is_patch" => false
            },
            bug_id: bug_id
          )

        response.json["ids"] || []
      end)

    %{
      "status" => "awaiting_remote_review",
      "bug_id" => bug_id,
      "bug_url" => bug_url(base_url, bug_id),
      "attachment_ids" => attachment_ids
    }
  end

  def submit_patch!(payload) do
    base_url = Map.fetch!(payload, "bugzilla_url")
    auth = load_auth!(base_url)
    bug_id = create_bug!(base_url, auth, payload)

    response =
      request!(
        "attach_patch",
        base_url,
        "POST",
        "/bug/#{bug_id}/attachment",
        %{
          "api_key" => auth["api_key"],
          "data" => encoded_file!(Map.fetch!(payload, "patch_path")),
          "file_name" => Map.fetch!(payload, "patch_file_name"),
          "summary" => Map.fetch!(payload, "patch_summary"),
          "content_type" => "text/x-patch",
          "is_patch" => true,
          "comment" => "Patch prepared by the WireShare Linux rollout orchestrator."
        },
        bug_id: bug_id
      )

    %{
      "status" => "awaiting_remote_review",
      "bug_id" => bug_id,
      "bug_url" => bug_url(base_url, bug_id),
      "attachment_ids" => response.json["ids"] || []
    }
  end

  def ping!(base_url), do: request!("ping", base_url, "GET", "/version").json

  defp validate_auth!(base_url, login, api_key) do
    query = URI.encode_query(%{"api_key" => api_key})

    if login != "" do
      encoded_login = URI.encode_www_form(login)

      try do
        response = request!("validate_auth", base_url, "GET", "/user/#{encoded_login}?#{query}")
        users = response.json["users"] || []

        if users != [] do
          List.first(users)
        else
          fallback_validate_auth!(base_url, login, query)
        end
      rescue
        error in RequestError ->
          if error.http_status == 404 do
            fallback_validate_auth!(base_url, login, query)
          else
            reraise error, __STACKTRACE__
          end
      end
    else
      fallback_validate_auth!(base_url, login, query)
    end
  end

  defp fallback_validate_auth!(base_url, login, query) do
    response = request!("validate_auth", base_url, "GET", "/user/1?#{query}")

    case response.json["users"] || [] do
      [] -> %{"name" => login}
      [user | _] -> user
    end
  end

  defp load_auth!(base_url) do
    path = Util.bugzilla_credentials_file(base_url)

    case Util.read_json(path) do
      {:ok, auth} ->
        auth

      {:error, _reason} ->
        raise "Missing Bugzilla credentials for #{base_url}. Run `scripts/release_linux_rollout.sh auth login --target <target>` first."
    end
  end

  defp create_bug!(base_url, auth, payload) do
    response =
      request!("create_bug", base_url, "POST", "/bug", %{
        "api_key" => Map.fetch!(auth, "api_key"),
        "product" => Map.fetch!(payload, "product"),
        "component" => Map.fetch!(payload, "component"),
        "summary" => Map.fetch!(payload, "summary"),
        "version" => Map.fetch!(payload, "version"),
        "description" => Map.fetch!(payload, "description"),
        "platform" => Map.fetch!(payload, "platform"),
        "severity" => Map.fetch!(payload, "severity"),
        "op_sys" => Map.get(payload, "op_sys", "All")
      })

    case response.json["id"] do
      bug_id when is_integer(bug_id) -> bug_id

      _ ->
        raise RequestError,
          stage: "create_bug",
          http_status: response.http_status,
          content_type: response.content_type,
          response_excerpt: excerpt(response.body) || inspect(response.json)
    end
  end

  def bug_url(base_url, bug_id), do: "#{normalize_base_url(base_url)}/show_bug.cgi?id=#{bug_id}"

  defp request!(stage, base_url, method, path, payload \\ nil, opts \\ []) do
    body = if payload, do: IO.iodata_to_binary(:json.encode(payload)), else: nil

    args =
      [
        "-sS",
        "-L",
        "--connect-timeout",
        "20",
        "--max-time",
        "60",
        "-X",
        method,
        "-H",
        "Accept: application/json",
        "--write-out",
        @http_status_marker <> "%{http_code}" <> @content_type_marker <> "%{content_type}"
      ]
      |> maybe_append_body_args(payload)
      |> Kernel.++([rest_base(base_url) <> path])

    runner =
      if body do
        &Util.run_with_input/4
      else
        fn command, runner_args, _input, runner_opts -> Util.run(command, runner_args, runner_opts) end
      end

    case runner.("curl", args, body, []) do
      {:ok, output} ->
        response = parse_response!(output, stage, opts)

        if response.http_status < 200 or response.http_status >= 300 do
          raise RequestError,
            stage: stage,
            http_status: response.http_status,
            content_type: response.content_type,
            response_excerpt: excerpt(response.body),
            bug_id: opts[:bug_id]
        end

        try do
          Map.put(response, :json, :json.decode(response.body))
        rescue
          error in [ArgumentError, ErlangError] ->
            raise RequestError,
              stage: stage,
              http_status: response.http_status,
              content_type: response.content_type,
              response_excerpt: excerpt(response.body),
              bug_id: opts[:bug_id],
              message: Exception.message(error)
        end

      {:error, %{output: output}} ->
        raise RequestError,
          stage: stage,
          response_excerpt: excerpt(output),
          bug_id: opts[:bug_id]
    end
  end

  defp parse_response!(output, stage, opts) do
    status_marker_index =
      output
      |> :binary.matches(@http_status_marker)
      |> List.last()

    case status_marker_index do
      {offset, _length} ->
        body = binary_part(output, 0, offset)

        metadata =
          binary_part(
            output,
            offset + byte_size(@http_status_marker),
            byte_size(output) - offset - byte_size(@http_status_marker)
          )

        case String.split(metadata, @content_type_marker, parts: 2) do
          [status_text, content_type] ->
            %{
              body: body,
              http_status: parse_http_status(status_text, stage, body, opts),
              content_type: blank_to_nil(String.trim(content_type))
            }

          _ ->
            raise RequestError,
              stage: stage,
              response_excerpt: excerpt(output),
              bug_id: opts[:bug_id]
        end

      nil ->
        raise RequestError,
          stage: stage,
          response_excerpt: excerpt(output),
          bug_id: opts[:bug_id]
    end
  end

  defp parse_http_status(status_text, stage, body, opts) do
    case Integer.parse(String.trim(status_text)) do
      {status, ""} ->
        status

      _ ->
        raise RequestError,
          stage: stage,
          response_excerpt: excerpt(body),
          bug_id: opts[:bug_id]
    end
  end

  defp maybe_append_body_args(args, nil), do: args

  defp maybe_append_body_args(args, _payload) do
    args ++ ["-H", "Content-Type: application/json", "--data-binary", "@-"]
  end

  defp encoded_file!(path), do: path |> File.read!() |> Base.encode64()

  defp excerpt(value) when is_binary(value) do
    value
    |> String.trim()
    |> case do
      "" ->
        nil

      trimmed ->
        if String.length(trimmed) > 400 do
          String.slice(trimmed, 0, 400) <> "..."
        else
          trimmed
        end
    end
  end

  defp blank_to_nil(""), do: nil
  defp blank_to_nil(value), do: value

  defp require_value(value, label) when value in [nil, ""], do: raise("#{label} is required")
  defp require_value(value, _label), do: value

  defp rest_base(base_url) do
    base = normalize_base_url(base_url)

    cond do
      String.ends_with?(base, "/rest") -> base
      String.ends_with?(base, "/rest.cgi") -> base
      true -> base <> "/rest"
    end
  end

  defp normalize_base_url(base_url), do: String.trim_trailing(base_url, "/")
end
