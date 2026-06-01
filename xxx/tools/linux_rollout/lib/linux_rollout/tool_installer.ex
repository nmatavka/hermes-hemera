defmodule LinuxRollout.ToolInstaller do
  alias LinuxRollout.{Doctor, Util, Workspace}

  def install!(workspace, target_names) do
    managed =
      target_names
      |> Enum.flat_map(fn target_name ->
        workspace
        |> Workspace.target!(target_name)
        |> Map.get("required_cli", [])
      end)
      |> Enum.filter(&managed_cli?/1)
      |> Enum.uniq()
      |> Enum.sort()

    missing =
      target_names
      |> Enum.flat_map(fn target_name ->
        target = Workspace.target!(workspace, target_name)
        Doctor.inspect_target(workspace, target) |> Map.get(:missing_cli, [])
      end)
      |> Enum.uniq()
      |> Enum.sort()

    case managed do
      [] ->
        IO.puts("No managed CLIs to install for the selected targets.")

      _ ->
        Enum.each(managed, &install_cli!/1)

        case missing do
          [] -> IO.puts("Ensured CLI helpers: #{Enum.join(managed, ", ")}")
          _ -> IO.puts("Installed CLI helpers: #{Enum.join(missing, ", ")}")
        end
    end
  end

  def install_cli!("osc") do
    ensure_pipx!()
    Util.run!("pipx", ["install", "--force", "osc"])
  end

  def install_cli!("copr-cli") do
    ensure_pipx!()
    Util.run!("pipx", ["install", "--force", "copr-cli"])
    Util.run!("pipx", ["inject", "--force", "copr-cli", "rich"])
  end

  def install_cli!("launchpadlib") do
    ensure_launchpad_venv!()

    python = Util.launchpad_python()
    Util.run!(python, ["-m", "pip", "install", "--upgrade", "pip"])
    Util.run!(python, ["-m", "pip", "install", "--upgrade", "launchpadlib[keyring]"])
  end

  def install_cli!("glab") do
    ensure_local_bin!()
    platform = glab_platform!()
    release = latest_glab_release!()
    asset_url = glab_asset_url!(release, platform)

    tmp_dir =
      Path.join(System.tmp_dir!(), "linux_rollout_glab_#{System.unique_integer([:positive])}")

    archive_path = Path.join(tmp_dir, "glab.tar.gz")
    extract_dir = Path.join(tmp_dir, "extract")

    File.rm_rf!(tmp_dir)
    File.mkdir_p!(extract_dir)
    Util.run!("curl", ["-fsSL", "-o", archive_path, asset_url])
    Util.run!("tar", ["-xzf", archive_path, "-C", extract_dir])

    binary_path =
      extract_dir
      |> Util.list_files_recursively()
      |> Enum.find(fn path -> Path.basename(path) == "glab" and File.regular?(path) end) ||
        raise "unable to locate extracted glab binary"

    destination = Path.join(Util.local_bin_dir(), "glab")
    File.cp!(binary_path, destination)
    File.chmod!(destination, 0o755)
  end

  def install_cli!(cli) do
    raise "no installer defined for #{cli}"
  end

  defp managed_cli?(cli), do: cli in ["glab", "osc", "copr-cli", "launchpadlib"]

  defp ensure_pipx! do
    case System.find_executable("pipx") do
      nil -> raise "pipx is required to install Python-backed rollout CLIs"
      _path -> ensure_local_bin!()
    end
  end

  defp ensure_launchpad_venv! do
    venv_dir = Util.launchpad_venv_dir()

    unless File.exists?(Util.launchpad_python()) do
      File.rm_rf!(venv_dir)
      File.mkdir_p!(Path.dirname(venv_dir))
      Util.run!("python3", ["-m", "venv", venv_dir])
    end

    File.mkdir_p!(Util.launchpad_cache_dir())
    File.mkdir_p!(Path.dirname(Util.launchpad_credentials_file()))
  end

  defp ensure_local_bin! do
    File.mkdir_p!(Util.local_bin_dir())
  end

  defp latest_glab_release! do
    output =
      Util.run!(
        "curl",
        ["-fsSL", "https://gitlab.com/api/v4/projects/gitlab-org%2Fcli/releases/permalink/latest"]
      )

    :json.decode(output)
  end

  defp glab_asset_url!(release, platform) do
    release
    |> get_in(["assets", "links"])
    |> Enum.find_value(fn link ->
      url = link["direct_asset_url"] || link["url"] || ""

      if String.contains?(url, platform) and String.ends_with?(url, ".tar.gz") do
        url
      end
    end) || raise "could not find glab asset for #{platform}"
  end

  defp glab_platform! do
    case String.trim(Util.run!("uname", ["-sm"])) do
      "Darwin arm64" -> "darwin_arm64"
      "Darwin x86_64" -> "darwin_amd64"
      other -> raise "unsupported host for glab installer: #{other}"
    end
  end
end
