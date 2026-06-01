defmodule LinuxRollout.GitHubRelease do
  alias LinuxRollout.{State, Util, Workspace}

  @asset_visibility_attempts 12
  @asset_visibility_sleep_ms 5_000

  def ensure_assets!(workspace) do
    repo = workspace.tokens["release_repo_slug"]
    tag = workspace.tokens["release_tag"]
    title = "#{workspace.tokens["release_app_name"]} #{workspace.tokens["release_version"]}"
    notes_file = Path.join(workspace.work_dir, "github-release-notes.md")
    assets = asset_specs(workspace)

    Enum.each(assets, &ensure_asset!(Map.fetch!(&1, :path)))
    Util.ensure_directory!(notes_file)
    File.write!(notes_file, workspace.tokens["release_notes"] || "")

    if release_exists?(repo, tag) do
      Util.witness("release: uploading canonical assets to #{repo}@#{tag}")

      Util.run!(
        "gh",
        ["release", "upload", tag] ++
          Enum.map(assets, &Map.fetch!(&1, :path)) ++ ["--repo", repo, "--clobber"]
      )
    else
      Util.witness("release: creating GitHub release #{tag} in #{repo}")

      Util.run!(
        "gh",
        ["release", "create", tag] ++
          Enum.map(assets, &Map.fetch!(&1, :path)) ++
          [
            "--repo",
            repo,
            "--title",
            title,
            "--notes-file",
            notes_file,
            "--target",
            workspace.tokens["release_repo_default_branch"]
          ]
      )
    end

    release = ensure_release_assets_visible!(repo, tag, assets)
    ensure_public_asset_urls_live!(release, assets)

    State.put_step!(workspace, :publish_release_assets, %{
      status: "completed",
      repo: repo,
      tag: tag,
      asset_urls: release_asset_urls(release, Enum.map(assets, &Map.fetch!(&1, :name)))
    })
  end

  def api_tag_path(tag) when is_binary(tag) do
    URI.encode(tag, &URI.char_unreserved?/1)
  end

  def missing_release_asset_names(release, expected_asset_names) when is_map(release) do
    actual_asset_names =
      release
      |> Map.get("assets", [])
      |> Enum.map(&Map.get(&1, "name"))
      |> MapSet.new()

    expected_asset_names
    |> MapSet.new()
    |> MapSet.difference(actual_asset_names)
    |> MapSet.to_list()
    |> Enum.sort()
  end

  def release_asset_urls(release, expected_asset_names) when is_map(release) do
    expected_asset_names = MapSet.new(expected_asset_names)

    release
    |> Map.get("assets", [])
    |> Enum.reduce(%{}, fn asset, acc ->
      name = Map.get(asset, "name")

      if MapSet.member?(expected_asset_names, name) do
        Map.put(acc, name, Map.get(asset, "browser_download_url"))
      else
        acc
      end
    end)
  end

  defp asset_specs(workspace) do
    [
      %{
        name: workspace.tokens["source_tarball_name"],
        path:
          Workspace.artifact_path(workspace, "release", workspace.tokens["source_tarball_name"])
      },
      %{
        name: workspace.tokens["wire_share_jar_name"],
        path: Workspace.artifact_path(workspace, "jar", workspace.tokens["wire_share_jar_name"])
      },
      %{
        name: workspace.tokens["checksums_name"],
        path: Workspace.artifact_path(workspace, "release", workspace.tokens["checksums_name"])
      },
      %{
        name: workspace.tokens["dependency_inventory_name"],
        path:
          Workspace.artifact_path(
            workspace,
            "release",
            workspace.tokens["dependency_inventory_name"]
          )
      }
    ]
  end

  defp ensure_release_assets_visible!(repo, tag, assets) do
    expected_asset_names = Enum.map(assets, &Map.fetch!(&1, :name))
    initial_release = fetch_release!(repo, tag)
    missing_assets = missing_release_asset_names(initial_release, expected_asset_names)

    if missing_assets != [] do
      Util.witness("release: re-uploading missing assets #{Enum.join(missing_assets, ", ")}")

      Util.run!(
        "gh",
        ["release", "upload", tag] ++
          missing_asset_paths(assets, missing_assets) ++
          ["--repo", repo, "--clobber"]
      )
    end

    wait_for_release_assets!(repo, tag, expected_asset_names, @asset_visibility_attempts)
  end

  defp ensure_public_asset_urls_live!(release, assets) do
    expected_asset_names = Enum.map(assets, &Map.fetch!(&1, :name))
    urls = release_asset_urls(release, expected_asset_names)

    expected_asset_names
    |> Enum.each(fn asset_name ->
      url = Map.fetch!(urls, asset_name)
      wait_for_public_url!(asset_name, url, @asset_visibility_attempts)
    end)
  end

  defp wait_for_release_assets!(repo, tag, expected_asset_names, attempts_remaining) do
    release = fetch_release!(repo, tag)
    missing_assets = missing_release_asset_names(release, expected_asset_names)

    cond do
      missing_assets == [] ->
        release

      attempts_remaining > 1 ->
        Util.witness(
          "release: waiting for GitHub asset inventory for #{Enum.join(missing_assets, ", ")}"
        )

        Process.sleep(@asset_visibility_sleep_ms)
        wait_for_release_assets!(repo, tag, expected_asset_names, attempts_remaining - 1)

      true ->
        raise "GitHub release #{tag} in #{repo} is still missing assets: #{Enum.join(missing_assets, ", ")}"
    end
  end

  defp wait_for_public_url!(asset_name, url, attempts_remaining) do
    case public_url_status(url) do
      200 ->
        :ok

      status when attempts_remaining > 1 ->
        Util.witness("release: waiting for public asset URL #{asset_name} (HTTP #{status})")
        Process.sleep(@asset_visibility_sleep_ms)
        wait_for_public_url!(asset_name, url, attempts_remaining - 1)

      status ->
        raise "public release asset URL for #{asset_name} is still unavailable (HTTP #{status}): #{url}"
    end
  end

  defp public_url_status(url) do
    Util.run!("curl", ["-I", "-L", "-s", "-o", "/dev/null", "-w", "%{http_code}", url])
    |> String.trim()
    |> String.to_integer()
  rescue
    _error -> 0
  end

  defp fetch_release!(repo, tag) do
    endpoint = "repos/#{repo}/releases/tags/#{api_tag_path(tag)}"

    Util.run!("gh", ["api", endpoint])
    |> :json.decode()
  end

  defp missing_asset_paths(assets, missing_asset_names) do
    missing_asset_names = MapSet.new(missing_asset_names)

    assets
    |> Enum.filter(fn asset -> MapSet.member?(missing_asset_names, Map.fetch!(asset, :name)) end)
    |> Enum.map(&Map.fetch!(&1, :path))
  end

  defp ensure_asset!(path) do
    if File.exists?(path) do
      :ok
    else
      raise "expected release asset at #{path}"
    end
  end

  defp release_exists?(repo, tag) do
    case Util.run("gh", ["release", "view", tag, "--repo", repo]) do
      {:ok, _output} ->
        true

      {:error, %{status: 1, output: output}} when is_binary(output) ->
        if String.contains?(output, "release not found") do
          false
        else
          raise "unable to inspect GitHub release #{tag} in #{repo}: #{output}"
        end

      {:error, %{output: output}} ->
        raise "unable to inspect GitHub release #{tag} in #{repo}: #{output}"
    end
  end
end
