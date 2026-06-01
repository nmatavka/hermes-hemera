defmodule LinuxRollout.Util do
  import Bitwise

  @token_pattern ~r/@([a-z0-9_]+)@/
  @nix_base32_alphabet "0123456789abcdfghijklmnpqrsvwxyz" |> String.graphemes()

  def render_string(value, tokens) when is_binary(value) do
    Regex.replace(@token_pattern, value, fn _, token ->
      case Map.get(tokens, token, "@#{token}@") do
        replacement when is_binary(replacement) -> replacement
        replacement -> to_string(replacement)
      end
    end)
  end

  def render_tokens(value, tokens) when is_map(value) do
    value
    |> Enum.map(fn {key, nested_value} ->
      rendered_key =
        case key do
          binary when is_binary(binary) -> render_string(binary, tokens)
          other -> other
        end

      {rendered_key, render_tokens(nested_value, tokens)}
    end)
    |> Map.new()
  end

  def render_tokens(value, tokens) when is_list(value) do
    Enum.map(value, &render_tokens(&1, tokens))
  end

  def render_tokens(value, tokens) when is_binary(value), do: render_string(value, tokens)
  def render_tokens(value, _tokens), do: value

  def timestamp_utc do
    DateTime.utc_now() |> DateTime.truncate(:second) |> DateTime.to_iso8601()
  end

  def ensure_directory!(path) do
    path |> Path.dirname() |> File.mkdir_p!()
    path
  end

  def blank?(value) when value in [nil, "", []], do: true
  def blank?(value) when is_map(value), do: map_size(value) == 0
  def blank?(_value), do: false

  def text_file?(path) do
    case File.read(path) do
      {:ok, contents} ->
        String.valid?(contents) and not String.contains?(contents, <<0>>)

      {:error, _reason} ->
        false
    end
  end

  def nix_base32_from_hex(nil), do: nil
  def nix_base32_from_hex(""), do: ""

  def nix_base32_from_hex(hex) do
    normalized_hex = String.downcase(hex)

    case nix_base32_via_nix(normalized_hex) do
      {:ok, nix32} ->
        nix32

      _ ->
        legacy_nix_base32_from_hex(normalized_hex)
    end
  end

  def nix_executable do
    System.find_executable("nix") ||
      standard_nix_executable()
  end

  defp nix_base32_via_nix(hex) do
    case nix_executable() do
      nil ->
        :error

      nix ->
        run(nix, [
          "--extra-experimental-features",
          "nix-command flakes",
          "hash",
          "convert",
          "--hash-algo",
          "sha256",
          "--to",
          "nix32",
          hex
        ])
        |> case do
          {:ok, output} -> {:ok, String.trim(output)}
          {:error, _reason} -> :error
        end
    end
  end

  defp standard_nix_executable do
    path = Path.join(standard_nix_bin_dir(), "nix")

    if path != "" and File.regular?(path) do
      path
    end
  end

  defp legacy_nix_base32_from_hex(hex) do
    binary = Base.decode16!(String.upcase(hex), case: :mixed)
    total_bits = byte_size(binary) * 8
    output_length = div(total_bits + 4, 5)
    value = :binary.decode_unsigned(binary)

    encoded =
      for index <- 0..(output_length - 1) do
        shift = max((output_length - index - 1) * 5, 0)
        chunk = value >>> shift &&& 0x1F
        Enum.at(@nix_base32_alphabet, chunk)
      end

    Enum.join(encoded)
  end

  def shell_escape(argument) do
    "'" <> String.replace(argument, "'", "'\"'\"'") <> "'"
  end

  def witness(message) when is_binary(message) do
    IO.puts("[linux-rollout] " <> message)
  end

  def local_bin_dir do
    Path.join(System.user_home!(), ".local/bin")
  end

  def managed_share_dir do
    System.get_env("LINUX_ROLLOUT_SHARE_DIR") ||
      Path.join(System.user_home!(), ".local/share/linux_rollout")
  end

  def managed_cache_dir do
    System.get_env("LINUX_ROLLOUT_CACHE_DIR") ||
      Path.join(System.user_home!(), ".cache/linux_rollout")
  end

  def local_config_path do
    System.get_env("LINUX_ROLLOUT_CONFIG") || Path.join(managed_share_dir(), "config.yaml")
  end

  def bugzilla_credentials_dir do
    Path.join(managed_share_dir(), "bugzilla")
  end

  def bugzilla_credentials_file(base_url) when is_binary(base_url) do
    Path.join(bugzilla_credentials_dir(), "#{bugzilla_auth_id(base_url)}.json")
  end

  def launchpad_venv_dir do
    Path.join(managed_share_dir(), "launchpad")
  end

  def launchpad_python do
    Path.join(launchpad_venv_dir(), "bin/python")
  end

  def launchpad_credentials_file do
    Path.join(managed_share_dir(), "launchpad/credentials")
  end

  def launchpad_cache_dir do
    Path.join(managed_cache_dir(), "launchpadlib")
  end

  def git_config(key) when is_binary(key) do
    case run("git", ["config", "--get", key]) do
      {:ok, output} -> String.trim(output)
      {:error, _reason} -> ""
    end
  end

  def github_current_user do
    case run("gh", ["api", "user"]) do
      {:ok, output} ->
        case :json.decode(output) do
          %{} = user -> user
          _ -> nil
        end

      {:error, _reason} ->
        nil
    end
  end

  def github_current_login do
    case github_current_user() do
      %{"login" => login} when is_binary(login) and login != "" -> login
      _ -> nil
    end
  end

  def github_current_id do
    case github_current_user() do
      %{"id" => id} when is_integer(id) -> id
      %{"id" => id} when is_binary(id) and id != "" -> id
      _ -> nil
    end
  end

  def obs_username_from_oscrc(path \\ "~/.oscrc") when is_binary(path) do
    path
    |> expand_home()
    |> File.read()
    |> case do
      {:ok, contents} ->
        case Regex.run(~r/^\s*user\s*=\s*(\S+)\s*$/m, contents, capture: :all_but_first) do
          [username] when username != "" -> username
          _ -> nil
        end

      {:error, _reason} ->
        nil
    end
  end

  def ssh_config_user(host) when is_binary(host) do
    case run("ssh", ["-G", host]) do
      {:ok, output} ->
        output
        |> String.split("\n")
        |> Enum.find_value(fn line ->
          case String.split(String.trim(line), ~r/\s+/, parts: 2, trim: true) do
            ["user", user] when user != "" -> user
            _ -> nil
          end
        end)

      {:error, _reason} ->
        nil
    end
  end

  def git_send_email_available? do
    match?({:ok, _output}, run("git", ["send-email", "-h"], success_codes: [0, 129]))
  end

  def expand_home(path) when is_binary(path) do
    home = System.get_env("HOME") || System.user_home!()

    case path do
      "~" ->
        home

      "~/" <> rest ->
        Path.join(home, rest)

      _ ->
        Path.expand(path)
    end
  end

  def write_json!(path, data) do
    ensure_directory!(path)
    File.write!(path, IO.iodata_to_binary(:json.encode(data)))
  end

  def read_json(path) do
    case File.read(path) do
      {:ok, contents} -> {:ok, :json.decode(contents)}
      {:error, _reason} = error -> error
    end
  end

  def default_debian_timestamp(date_string) when is_binary(date_string) do
    with {:ok, date} <- Date.from_iso8601(date_string),
         {:ok, datetime} <- DateTime.new(date, ~T[00:00:00], "Etc/UTC") do
      Calendar.strftime(datetime, "%a, %d %b %Y %H:%M:%S %z")
    else
      _ -> "Thu, 01 Jan 1970 00:00:00 +0000"
    end
  end

  def open_url(url, opts \\ []) do
    dry_run = Keyword.get(opts, :dry_run, false)
    allow_open = Keyword.get(opts, :open, false)
    no_open = Keyword.get(opts, :no_open, false)

    cond do
      url in [nil, ""] ->
        :ok

      dry_run or no_open or not allow_open ->
        :ok

      System.find_executable("open") ->
        System.cmd("open", [url])
        :ok

      true ->
        :ok
    end
  end

  def run(command, args, opts \\ []) do
    command_path = System.find_executable(command) || command
    success_codes = Keyword.get(opts, :success_codes, [0]) |> List.wrap()

    exec_opts =
      [stderr_to_stdout: true]
      |> maybe_put(:cd, Keyword.get(opts, :cd))
      |> maybe_put(:env, Keyword.get(opts, :env))
      |> maybe_put(:into, Keyword.get(opts, :into))

    case System.cmd(command_path, args, exec_opts) do
      {output, status} ->
        if status in success_codes do
          {:ok, output}
        else
          {:error, %{command: command, args: args, output: output, status: status}}
        end
    end
  end

  def run!(command, args, opts \\ []) do
    case run(command, args, opts) do
      {:ok, output} ->
        output

      {:error, %{status: status, output: output}} ->
        raise "#{command} #{Enum.join(args, " ")} failed with exit #{status}: #{output}"
    end
  end

  def run_with_input(command, args, input, opts \\ []) do
    command_path = System.find_executable(command) || command
    success_codes = Keyword.get(opts, :success_codes, [0]) |> List.wrap()

    tmp_path =
      Path.join(
        System.tmp_dir!(),
        "linux_rollout_stdin_#{System.unique_integer([:positive])}.txt"
      )

    File.write!(tmp_path, input)

    command_line =
      [command_path | args]
      |> Enum.map(&shell_escape/1)
      |> Enum.join(" ")

    shell = System.find_executable("sh") || "/bin/sh"
    script = "#{command_line} < #{shell_escape(tmp_path)}"

    exec_opts =
      [stderr_to_stdout: true]
      |> maybe_put(:cd, Keyword.get(opts, :cd))
      |> maybe_put(:env, Keyword.get(opts, :env))

    try do
      case System.cmd(shell, ["-lc", script], exec_opts) do
        {output, status} ->
          if status in success_codes do
            {:ok, output}
          else
            {:error, %{command: command, args: args, output: output, status: status}}
          end
      end
    after
      File.rm(tmp_path)
    end
  end

  def run_with_input!(command, args, input, opts \\ []) do
    case run_with_input(command, args, input, opts) do
      {:ok, output} ->
        output

      {:error, %{status: status, output: output}} ->
        raise "#{command} #{Enum.join(args, " ")} failed with exit #{status}: #{output}"
    end
  end

  def interactive_run(command, args, opts \\ []) do
    command_path = System.find_executable(command) || command
    success_codes = Keyword.get(opts, :success_codes, [0]) |> List.wrap()

    exec_opts =
      [stderr_to_stdout: true, use_stdio: true, into: IO.stream(:stdio, :line)]
      |> maybe_put(:cd, Keyword.get(opts, :cd))
      |> maybe_put(:env, Keyword.get(opts, :env))

    case System.cmd(command_path, args, exec_opts) do
      {_output, status} ->
        if status in success_codes do
          :ok
        else
          {:error, %{command: command, args: args, status: status}}
      end
    end
  end

  def interactive_run!(command, args, opts \\ []) do
    case interactive_run(command, args, opts) do
      :ok ->
        :ok

      {:error, %{status: status}} ->
        raise "#{command} #{Enum.join(args, " ")} failed with exit #{status}"
    end
  end

  def maybe_put(keyword, _key, nil), do: keyword
  def maybe_put(keyword, key, value), do: Keyword.put(keyword, key, value)

  def deep_merge(left, right) when is_map(left) and is_map(right) do
    Map.merge(left, right, fn _key, left_value, right_value ->
      cond do
        blank?(right_value) ->
          left_value

        is_map(left_value) and is_map(right_value) ->
          deep_merge(left_value, right_value)

        true ->
          right_value
      end
    end)
  end

  def deep_merge(_left, right), do: right

  def path_get(data, path) when is_binary(path) do
    path
    |> String.split(".", trim: true)
    |> Enum.reduce_while(data, fn key, current ->
      if is_map(current) do
        case Map.fetch(current, key) do
          {:ok, value} -> {:cont, value}
          :error -> {:halt, nil}
        end
      else
        {:halt, nil}
      end
    end)
  end

  def path_get(data, path) when is_list(path), do: get_in(data, path)

  def path_put(data, path, value) when is_binary(path) do
    path_put(data, String.split(path, ".", trim: true), value)
  end

  def path_put(data, [key], value) when is_map(data), do: Map.put(data, key, value)

  def path_put(data, [key | rest], value) when is_map(data) do
    nested =
      case Map.get(data, key) do
        value when is_map(value) -> value
        _ -> %{}
      end

    Map.put(data, key, path_put(nested, rest, value))
  end

  def path_put(_data, path, value) when is_list(path), do: path_put(%{}, path, value)

  def path_delete(data, path) when is_binary(path) do
    path_delete(data, String.split(path, ".", trim: true))
  end

  def path_delete(data, [key]) when is_map(data), do: Map.delete(data, key)

  def path_delete(data, [key | rest]) when is_map(data) do
    case Map.fetch(data, key) do
      {:ok, nested} when is_map(nested) ->
        updated = path_delete(nested, rest)

        if blank?(updated) do
          Map.delete(data, key)
        else
          Map.put(data, key, updated)
        end

      _ ->
        data
    end
  end

  def path_delete(data, _path), do: data

  def split_repo_owner(repo) when is_binary(repo) do
    case String.split(repo, "/", parts: 2) do
      [owner, _name] when owner != "" -> owner
      _ -> nil
    end
  end

  def bugzilla_auth_id(base_url) when is_binary(base_url) do
    uri = URI.parse(base_url)
    host = uri.host || "unknown-host"
    path = uri.path || ""

    [host, path]
    |> Enum.join("_")
    |> String.replace(~r/[^A-Za-z0-9]+/, "_")
    |> String.trim("_")
  end

  def copy_file!(source_path, destination_path) do
    ensure_directory!(destination_path)
    File.cp!(source_path, destination_path)
  end

  def standard_nix_profile_script do
    System.get_env(
      "LINUX_ROLLOUT_NIX_PROFILE_SCRIPT",
      "/nix/var/nix/profiles/default/etc/profile.d/nix-daemon.sh"
    )
  end

  def standard_nix_bin_dir do
    System.get_env("LINUX_ROLLOUT_NIX_BIN_DIR", "/nix/var/nix/profiles/default/bin")
  end

  def standard_nix_installed? do
    File.regular?(standard_nix_profile_script()) or File.dir?(standard_nix_bin_dir())
  end

  def nix_host_system do
    System.get_env("LINUX_ROLLOUT_NIX_HOST_SYSTEM") || detect_nix_host_system!()
  end

  def nix_host_os do
    case String.split(nix_host_system(), "-", parts: 2) do
      [_arch, os] -> os
      _ -> raise "unable to derive Nix host OS from #{inspect(nix_host_system())}"
    end
  end

  def cli_install_hint("glab"),
    do: "run `scripts/release_linux_rollout.sh install-tools --target alpine`"

  def cli_install_hint("osc"),
    do: "run `scripts/release_linux_rollout.sh install-tools --target obs --target opensuse`"

  def cli_install_hint("copr-cli"),
    do: "run `scripts/release_linux_rollout.sh install-tools --target copr`"

  def cli_install_hint("launchpadlib"),
    do: "run `scripts/release_linux_rollout.sh install-tools --target snap --target ppa`"

  def cli_install_hint("nix"),
    do: nix_cli_install_hint("nix")

  def cli_install_hint("nix-build"),
    do: nix_cli_install_hint("nix-build")

  def cli_install_hint("nix-shell"),
    do: nix_cli_install_hint("nix-shell")

  def cli_install_hint(_cli), do: nil

  defp nix_cli_install_hint(cli) do
    cond do
      File.regular?(standard_nix_profile_script()) ->
        "Nix appears installed but is off-PATH; source `#{standard_nix_profile_script()}` or ensure `#{cli}` is on PATH"

      File.dir?(standard_nix_bin_dir()) ->
        "Nix appears installed but is off-PATH; add `#{standard_nix_bin_dir()}` to PATH so `#{cli}` is available"

      true ->
        "install Nix and ensure `#{cli}` is on PATH"
    end
  end

  defp detect_nix_host_system! do
    system_architecture = :erlang.system_info(:system_architecture) |> to_string() |> String.downcase()

    arch =
      cond do
        String.starts_with?(system_architecture, "aarch64") or
            String.starts_with?(system_architecture, "arm64") ->
          "aarch64"

        String.starts_with?(system_architecture, "x86_64") or
            String.starts_with?(system_architecture, "amd64") ->
          "x86_64"

        true ->
          raise "unsupported architecture for Nix rollout: #{system_architecture}"
      end

    case :os.type() do
      {:unix, :darwin} -> "#{arch}-darwin"
      {:unix, :linux} -> "#{arch}-linux"
      other -> raise "unsupported host OS for Nix rollout: #{inspect(other)}"
    end
  end

  def list_files_recursively(root) do
    root
    |> Path.expand()
    |> do_list_files([])
    |> Enum.sort()
  end

  defp do_list_files(path, acc) do
    cond do
      File.regular?(path) ->
        [path | acc]

      File.dir?(path) ->
        path
        |> File.ls!()
        |> Enum.reduce(acc, fn child, nested_acc ->
          do_list_files(Path.join(path, child), nested_acc)
        end)

      true ->
        acc
    end
  end
end
