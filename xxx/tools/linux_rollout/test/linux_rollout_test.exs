defmodule LinuxRolloutTest do
  use ExUnit.Case, async: false

  import ExUnit.CaptureIO

  alias LinuxRollout.{
    Auth,
    CLI,
    Doctor,
    GitHubRelease,
    LocalConfig,
    Mail,
    ReleasePrep,
    Renderer,
    State,
    Submitter,
    Validator,
    Workspace
  }

  alias LinuxRollout.Adapters.{
    BugzillaApi,
    ConditionalGitSubmit,
    GitSubmit,
    LaunchpadApi,
    MailSubmit,
    ServiceCli
  }

  alias LinuxRollout.Util

  setup do
    root =
      Path.join(System.tmp_dir!(), "linux_rollout_test_#{System.unique_integer([:positive])}")

    File.rm_rf!(root)
    File.mkdir_p!(root)
    File.mkdir_p!(Path.join(root, "home"))

    previous_env =
          for key <- [
            "HOME",
            "LINUX_ROLLOUT_SHARE_DIR",
            "LINUX_ROLLOUT_CACHE_DIR",
            "LINUX_ROLLOUT_CONFIG",
            "LINUX_ROLLOUT_NIX_PROFILE_SCRIPT",
            "LINUX_ROLLOUT_NIX_BIN_DIR",
            "LINUX_ROLLOUT_NIX_HOST_SYSTEM",
            "PATH",
            "TERM"
          ],
          into: %{} do
        {key, System.get_env(key)}
      end

    System.put_env("HOME", Path.join(root, "home"))
    System.put_env("LINUX_ROLLOUT_SHARE_DIR", Path.join(root, ".local-share"))
    System.put_env("LINUX_ROLLOUT_CACHE_DIR", Path.join(root, ".local-cache"))
    System.put_env("LINUX_ROLLOUT_CONFIG", Path.join(root, ".local-share/config.yaml"))
    write_fixture_workspace(root)

    on_exit(fn ->
      Enum.each(previous_env, fn
        {key, nil} -> System.delete_env(key)
        {key, value} -> System.put_env(key, value)
      end)

      File.rm_rf!(root)
    end)

    {:ok, root: root}
  end

  test "workspace loading derives artifact hashes and renders templates", %{root: root} do
    workspace = Workspace.load!(root)
    assert workspace.tokens["source_tarball_sha256"] == String.duplicate("a", 64)
    assert workspace.tokens["wire_share_jar_sha256"] == String.duplicate("b", 64)
    assert String.length(workspace.tokens["source_tarball_nix_base32"]) > 10
    assert String.length(workspace.tokens["source_tarball_sha512"]) == 128

    rendered_root = Renderer.render!(workspace)
    rendered_manifest = Path.join(rendered_root, "flathub/cx.hermes.WireShare.yaml")
    rendered_formula = Path.join(rendered_root, "homebrew/wireshare.rb")
    rendered_openbsd = Path.join(rendered_root, "openbsd/distinfo")
    rendered_alpine = Path.join(rendered_root, "alpine/APKBUILD")

    assert File.read!(rendered_manifest) =~ "id: cx.hermes.WireShare"
    assert File.read!(rendered_manifest) =~ String.duplicate("a", 64)
    assert File.read!(rendered_formula) =~ "sha256 \"#{String.duplicate("a", 64)}\""

    assert File.read!(rendered_openbsd) =~
             "SHA256 (WireShare-9.9-source.tar.gz) = #{String.duplicate("a", 64)}"

    assert File.read!(rendered_alpine) =~
             "source=\"$pkgname-$pkgver.tar.gz::https://github.com/example/wireshare/releases/download/release/$pkgver/WireShare-$pkgver-source.tar.gz\""

    assert File.read!(rendered_alpine) =~
             "sha256sums=\"#{String.duplicate("a", 64)}  $pkgname-$pkgver.tar.gz\""

    refute File.read!(rendered_alpine) =~ "/release/9.9/WireShare-9.9-source.tar.gz"
  end

  test "workspace loading applies local config overrides for accounts and destinations", %{
    root: root
  } do
    LocalConfig.set!("accounts.launchpad_owner", "local-launchpad")
    LocalConfig.set!("targets.homebrew.destination.fork_repo", "local/homebrew-wireshare")

    workspace = Workspace.load!(root)

    assert workspace.tokens["launchpad_owner"] == "local-launchpad"

    assert get_in(workspace.targets, ["homebrew", "destination", "fork_repo"]) ==
             "local/homebrew-wireshare"
  end

  test "workspace exposes host-aware Nix tokens", %{root: root} do
    System.put_env("LINUX_ROLLOUT_NIX_HOST_SYSTEM", "aarch64-darwin")
    workspace = Workspace.load!(root)

    assert workspace.tokens["nix_host_system"] == "aarch64-darwin"
    assert workspace.tokens["nix_host_os"] == "darwin"
    assert get_in(workspace.targets, ["nix", "validated_system"]) == "aarch64-darwin"
  end

  test "workspace resolves upstream-compatible interactive PR titles", %{root: root} do
    workspace = Workspace.load!(root)

    assert Workspace.target!(workspace, "gentoo")["suggested_pr_title"] ==
             "net-p2p/wireshare: add 9.9"

    assert Workspace.target!(workspace, "solus")["suggested_pr_title"] ==
             "wireshare: Add at v9.9"

    assert Workspace.target!(workspace, "homebrew")["suggested_pr_title"] ==
             "wireshare 9.9"

    assert Workspace.target!(workspace, "haiku")["suggested_pr_title"] ==
             "net-p2p/wireshare: Add 9.9"
  end

  test "workspace keeps gentoo main-tree metadata and adds guru direct-push metadata", %{
    root: root
  } do
    workspace = Workspace.load!(root)

    assert get_in(workspace.targets, ["gentoo", "destination", "repo"]) == "gentoo/gentoo"

    assert get_in(workspace.targets, ["guru", "destination", "repo"]) ==
             "git@git.gentoo.org:repo/proj/guru.git"

    assert Workspace.target!(workspace, "gentoo")["commit_message"] ==
             "net-p2p/wireshare: add 9.9"

    assert Workspace.target!(workspace, "guru")["commit_message"] ==
             "net-p2p/wireshare: add 9.9"

    assert Workspace.target!(workspace, "guru")["push_command"] == ["pkgdev", "push", "--pull"]
  end

  test "workspace auto-detects obs and pkgsrc identities from local auth state", %{root: root} do
    write_file(root, "home/.oscrc", "[general]\nuser = obs-auto\n")

    fake_bin = Path.join(root, "fake-bin")

    write_file(root, "fake-bin/ssh", """
    #!/bin/sh
    if [ "$1" = "-G" ]; then
      echo "user pkgsrc-auto"
      exit 0
    fi
    exit 1
    """)

    File.chmod!(Path.join(fake_bin, "ssh"), 0o755)
    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    workspace = Workspace.load!(root)

    assert workspace.tokens["obs_owner"] == "obs-auto"
    assert workspace.tokens["obs_project"] == "home:obs-auto:wireshare"
    assert workspace.tokens["pkgsrc_wip_user"] == "pkgsrc-auto"

    assert get_in(workspace.targets, ["pkgsrc", "destination", "repo"]) ==
             "pkgsrc-auto@wip.pkgsrc.org:/pkgsrc-wip.git"

    assert Doctor.inspect_target(workspace, Workspace.target!(workspace, "pkgsrc")).status == "ok"
  end

  test "human gate requires approval before resume can advance", %{root: root} do
    workspace = Workspace.load!(root)
    ReleasePrep.run!(workspace, skip_gradle: true, dry_run: true)
    Validator.run!(workspace)

    Submitter.submit!(workspace, ["flathub"], dry_run: true, no_open: true)
    assert get_in(State.load(workspace), ["targets", "flathub", "status"]) == "awaiting_approval"

    output =
      capture_io(fn ->
        Submitter.submit!(
          workspace,
          ["flathub"],
          dry_run: true,
          no_open: true,
          acknowledge: true
        )
      end)

    assert get_in(State.load(workspace), ["targets", "flathub", "status"]) == "approved"

    next_steps =
      Path.join(workspace.bundle_root, "flathub/APPROVED_NEXT_STEPS.md")
      |> File.read!()

    pr_body =
      Path.join(workspace.bundle_root, "flathub/PR_BODY.md")
      |> File.read!()

    assert output =~ "gh pr create"
    assert output =~ "--base new-pr"
    assert output =~ "--head tester:cx-hermes-WireShare-submission"
    refute next_steps =~ ~r/^gh pr create/m
    refute next_steps =~ "/Users/"
    refute next_steps =~ "tester:"
    assert pr_body =~ "Submitting cx.hermes.WireShare to Flathub."
  end

  test "cli smoke commands include auth and status output", %{root: root} do
    output =
      capture_io(fn ->
        CLI.main(["prepare", "--cwd", root, "--skip-gradle", "--dry-run"])
        CLI.main(["validate", "--cwd", root])
        CLI.main(["doctor", "--cwd", root, "--target", "homebrew"])
        CLI.main(["auth", "status", "--cwd", root, "--target", "guix"])
        CLI.main(["render", "--cwd", root])
        CLI.main(["status", "--cwd", root, "--target", "flathub"])
      end)

    assert output =~ "[linux-rollout] prepare: starting release prep"
    assert output =~ "Prepared Linux release inputs"
    assert output =~ "Validated rendered rollout bundle"
    assert output =~ "Doctor inspected"
    assert output =~ "guix:"
    assert output =~ "last=pending"
  end

  test "config set, status, and unset manage local overrides through the CLI", %{root: root} do
    output =
      capture_io(fn ->
        CLI.main(["config", "set", "--cwd", root, "accounts.launchpad_owner", "local-owner"])
        CLI.main(["config", "status", "--cwd", root])
        CLI.main(["config", "unset", "--cwd", root, "accounts.launchpad_owner"])
      end)

    assert output =~ "Updated local config:"
    assert output =~ "Local config:"
    assert output =~ "accounts.launchpad_owner: local-owner"
    assert output =~ "Unset accounts.launchpad_owner"
    assert LocalConfig.load() == %{}
  end

  test "launchpad auth login stores a local owner override", %{root: root} do
    write_file(root, ".local-share/launchpad/bin/python", "#!/bin/sh\nexec python3 \"$@\"\n")
    File.chmod!(Path.join(root, ".local-share/launchpad/bin/python"), 0o755)

    workspace = Workspace.load!(root)

    Auth.login!(workspace, ["snap"])

    assert get_in(LocalConfig.load(), ["accounts", "launchpad_owner"]) == "launchpad-tester"
  end

  test "obs auth login stores local owner and derived project overrides", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")

    write_file(root, "fake-bin/osc", """
    #!/bin/sh
    cat > "$HOME/.oscrc" <<'EOF'
    [general]
    user = obs-tester
    EOF
    echo "obs auth ok"
    """)

    File.chmod!(Path.join(fake_bin, "osc"), 0o755)
    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    workspace = Workspace.load!(root)
    Auth.login!(workspace, ["obs"])

    assert get_in(LocalConfig.load(), ["accounts", "obs_owner"]) == "obs-tester"
    assert get_in(LocalConfig.load(), ["accounts", "obs_project"]) == "home:obs-tester:wireshare"
  end

  test "bugzilla auth login runs interactively for Mageia", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")

    write_file(root, "fake-bin/curl", """
    #!/bin/sh
    printf '%s' '{"users":[{"name":"mageia-tester","real_name":"Mageia Tester"}]}'
    printf '\\n__LINUX_ROLLOUT_HTTP_STATUS__=200\\n__LINUX_ROLLOUT_CONTENT_TYPE__=application/json'
    """)

    File.chmod!(Path.join(fake_bin, "curl"), 0o755)
    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    workspace = Workspace.load!(root)

    output =
      capture_io("mageia-tester\nmageia-api-key\n", fn ->
        Auth.login!(workspace, ["mageia"])
      end)

    assert output =~ "Bugzilla login/email:"
    assert output =~ "Bugzilla API key (input visible in this terminal):"
    assert output =~ "Authenticated to https://bugs.mageia.org as Mageia Tester"
    assert File.exists?(Util.bugzilla_credentials_file("https://bugs.mageia.org"))
  end

  test "mail send streams payload through sendmail stdin", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")
    capture_path = Path.join(root, "sendmail-capture.txt")

    write_file(root, "fake-bin/sendmail", """
    #!/bin/sh
    cat > "$SENDMAIL_CAPTURE_PATH"
    """)

    File.chmod!(Path.join(fake_bin, "sendmail"), 0o755)
    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")
    System.put_env("SENDMAIL_CAPTURE_PATH", capture_path)

    payload_path = Path.join(root, "mail.json")

    File.write!(
      payload_path,
      IO.iodata_to_binary(
        :json.encode(%{
          "from_name" => "Tester",
          "from_email" => "tester@example.com",
          "to" => "dest@example.com",
          "subject" => "Hello",
          "body" => "WireShare mail body"
        })
      )
    )

    assert Mail.send_payload!(payload_path) == "sent via sendmail"
    assert File.read!(capture_path) =~ "WireShare mail body"
    assert File.read!(capture_path) =~ "Subject: Hello"
  end

  test "git submit dry run supports push-only, GitHub PR, and GitLab MR targets", %{root: root} do
    %{abuild_log: abuild_log} = prepare_fake_gitlab_repo_env!(root)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    aur_bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "aur"))

    homebrew_bundle =
      LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "homebrew"))

    alpine_bundle =
      LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "alpine"))

    aur_result =
      GitSubmit.submit(workspace, aur_bundle, Workspace.target!(workspace, "aur"), dry_run: true)

    homebrew_result =
      GitSubmit.submit(workspace, homebrew_bundle, Workspace.target!(workspace, "homebrew"),
        dry_run: true
      )

    alpine_result =
      GitSubmit.submit(workspace, alpine_bundle, Workspace.target!(workspace, "alpine"),
        dry_run: true
      )

    assert aur_result.status == "dry_run"
    assert length(aur_result.units) == 2
    assert homebrew_result.status == "dry_run"
    assert alpine_result.status == "dry_run"
    assert File.read!(abuild_log) =~ "checksum"

    alpine_checkout = hd(alpine_result.units).dry_run_checkout

    assert File.read!(Path.join(alpine_checkout, "community/wireshare/APKBUILD")) =~
             "sha256sums=\"#{String.duplicate("c", 64)}  $pkgname-$pkgver.tar.gz\""
  end

  test "gentoo dry run signs commits off when requested", %{root: root} do
    prepare_fake_github_repo_env!(root)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "gentoo"))

    result =
      GitSubmit.submit(workspace, bundle, Workspace.target!(workspace, "gentoo"), dry_run: true)

    assert result.status == "dry_run"

    dry_run_checkout = hd(result.units).dry_run_checkout

    {commit_body, 0} =
      System.cmd("git", ["log", "--format=%B", "-n", "1"], cd: dry_run_checkout)

    assert commit_body =~ "net-p2p/wireshare: add 9.9"
    assert commit_body =~ "Signed-off-by: Fixture Submitter <fixture-submit@example.invalid>"
  end

  test "guru dry run prepares a signed dev-branch commit and runs pkgcheck", %{root: root} do
    %{guru_repo: guru_repo, pkgcheck_log: pkgcheck_log} = prepare_fake_guru_repo_env!(root)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    target =
      Workspace.target!(workspace, "guru")
      |> put_in(["destination", "repo"], guru_repo)

    bundle = LinuxRollout.Bundles.create!(workspace, target)
    result = GitSubmit.submit(workspace, bundle, target, dry_run: true)

    assert result.status == "dry_run"

    dry_run_checkout = hd(result.units).dry_run_checkout

    {branch_name, 0} = System.cmd("git", ["branch", "--show-current"], cd: dry_run_checkout)
    assert String.trim(branch_name) == "dev"

    assert File.exists?(Path.join(dry_run_checkout, "net-p2p/wireshare/metadata.xml"))
    assert File.exists?(Path.join(dry_run_checkout, "net-p2p/wireshare/wireshare-9.9.ebuild"))

    {commit_subject, 0} =
      System.cmd("git", ["log", "--format=%s", "-n", "1"], cd: dry_run_checkout)

    assert String.trim(commit_subject) == "net-p2p/wireshare: add 9.9"

    {commit_body, 0} =
      System.cmd("git", ["log", "--format=%B", "-n", "1"], cd: dry_run_checkout)

    assert commit_body =~ "Signed-off-by: Fixture Submitter <fixture-submit@example.invalid>"
    assert File.read!(pkgcheck_log) =~ "scan --net"
  end

  test "homebrew dry run leaves commit signoff disabled by default", %{root: root} do
    prepare_fake_github_repo_env!(root)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    target =
      Workspace.target!(workspace, "homebrew")
      |> Map.put("pre_submit_checks", [
        %{
          "name" => "No-op dry-run checkout",
          "mode" => "check",
          "cwd" => "checkout",
          "run_on_dry_run" => true,
          "command" => ["sh", "-c", "true"]
        }
      ])

    bundle = LinuxRollout.Bundles.create!(workspace, target)

    result =
      GitSubmit.submit(workspace, bundle, target, dry_run: true)

    assert result.status == "dry_run"

    dry_run_checkout = hd(result.units).dry_run_checkout

    {commit_body, 0} =
      System.cmd("git", ["log", "--format=%B", "-n", "1"], cd: dry_run_checkout)

    assert commit_body =~ "linux: publish homebrew packaging for 9.9"
    refute commit_body =~ "Signed-off-by:"
  end

  test "guru submit uses pkgdev push --pull", %{root: root} do
    %{guru_repo: guru_repo, pkgdev_log: pkgdev_log} = prepare_fake_guru_repo_env!(root)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    target =
      Workspace.target!(workspace, "guru")
      |> put_in(["destination", "repo"], guru_repo)

    bundle = LinuxRollout.Bundles.create!(workspace, target)
    result = GitSubmit.submit(workspace, bundle, target, [])

    assert result.status == "submitted"
    assert File.read!(pkgdev_log) =~ "push --pull"

    verify_checkout = Path.join(root, "verify-guru")
    run_git!(["clone", guru_repo, verify_checkout], cd: root)
    run_git!(["checkout", "dev"], cd: verify_checkout)

    {commit_subject, 0} =
      System.cmd("git", ["log", "--format=%s", "-n", "1"], cd: verify_checkout)

    assert String.trim(commit_subject) == "net-p2p/wireshare: add 9.9"
  end

  test "push_only targets without push_command still use raw git push", %{root: root} do
    %{pkgdev_log: pkgdev_log} = prepare_fake_guru_repo_env!(root)
    raw_repo = seed_github_repo!(root, "raw-push", "master", %{})
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    target =
      Workspace.target!(workspace, "guru")
      |> Map.put("name", "raw-push")
      |> Map.put("target_name", "raw-push")
      |> Map.put("push_command", nil)
      |> Map.put("pre_submit_checks", [])
      |> put_in(["destination", "repo"], raw_repo)
      |> put_in(["destination", "base_branch"], "master")
      |> Map.put("branch_name", "master")

    bundle = LinuxRollout.Bundles.create!(workspace, target)
    result = GitSubmit.submit(workspace, bundle, target, [])

    assert result.status == "submitted"
    refute File.exists?(pkgdev_log) and File.read!(pkgdev_log) =~ "push"

    verify_checkout = Path.join(root, "verify-raw-push")
    run_git!(["clone", raw_repo, verify_checkout], cd: root)

    {commit_subject, 0} =
      System.cmd("git", ["log", "--format=%s", "-n", "1"], cd: verify_checkout)

    assert String.trim(commit_subject) == "net-p2p/wireshare: add 9.9"
  end

  test "service CLI dry run supports OBS and Copr targets", %{root: root} do
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    obs_bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "obs"))
    copr_bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "copr"))

    obs_result =
      ServiceCli.submit(workspace, obs_bundle, Workspace.target!(workspace, "obs"), dry_run: true)

    copr_result =
      ServiceCli.submit(workspace, copr_bundle, Workspace.target!(workspace, "copr"),
        dry_run: true
      )

    assert obs_result.status == "dry_run"
    assert copr_result.status == "dry_run"
  end

  test "gitlab duplicate MR detection tolerates wrapped CLI output" do
    output = """
    Creating draft merge request for linux-rollout-7.0-alpine into master in alpine/aports

       ERROR

      Post https://gitlab.alpinelinux.org/api/v4/projects/nmatavka%2Faports/merge_requests: 409 {message: [Another open
      merge request already exists for this source branch: !102732]}.
    """

    assert GitSubmit.detect_existing_gitlab_mr(output, "gitlab.alpinelinux.org", "alpine/aports") ==
             %{
               merge_request:
                 "https://gitlab.alpinelinux.org/alpine/aports/-/merge_requests/102732"
             }
  end

  test "copr build submission parser treats accepted build handoff as success" do
    output = """
    Build was added to wireshare:
      https://copr.fedorainfracloud.org/coprs/build/10490349
    Created builds: 10490349
    Watching build(s): (this may be safely interrupted)
      18:32:26 Build 10490349: pending
    """

    assert ServiceCli.interpret_copr_build_submission(output) == %{
             accepted: true,
             build_url: "https://copr.fedorainfracloud.org/coprs/build/10490349",
             build_ids: ["10490349"]
           }
  end

  test "github release helper encodes slash tags and finds missing assets" do
    release = %{
      "assets" => [
        %{
          "name" => "WireShare-9.9-source.tar.gz",
          "browser_download_url" =>
            "https://github.com/example/project/releases/download/release/9.9/WireShare-9.9-source.tar.gz"
        },
        %{
          "name" => "SHA256SUMS",
          "browser_download_url" =>
            "https://github.com/example/project/releases/download/release/9.9/SHA256SUMS"
        }
      ]
    }

    expected = ["WireShare-9.9-source.tar.gz", "WireShare.jar", "SHA256SUMS"]

    assert GitHubRelease.api_tag_path("release/9.9") == "release%2F9.9"
    assert GitHubRelease.missing_release_asset_names(release, expected) == ["WireShare.jar"]

    assert GitHubRelease.release_asset_urls(release, expected) == %{
             "WireShare-9.9-source.tar.gz" =>
               "https://github.com/example/project/releases/download/release/9.9/WireShare-9.9-source.tar.gz",
             "SHA256SUMS" =>
               "https://github.com/example/project/releases/download/release/9.9/SHA256SUMS"
           }
  end

  test "launchpad API dry run supports snap and ppa payload generation", %{root: root} do
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    snap_bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "snap"))
    ppa_bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "ppa"))

    snap_result =
      LaunchpadApi.submit(workspace, snap_bundle, Workspace.target!(workspace, "snap"),
        dry_run: true
      )

    ppa_result =
      LaunchpadApi.submit(workspace, ppa_bundle, Workspace.target!(workspace, "ppa"),
        dry_run: true
      )

    assert snap_result.status == "dry_run"
    assert ppa_result.status == "dry_run"
    assert File.exists?(snap_result.payload)
    assert File.exists?(ppa_result.payload)
  end

  test "bugzilla API dry run supports Mageia requests and FreeBSD patches", %{root: root} do
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    mageia_bundle =
      LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "mageia"))

    freebsd_bundle =
      LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "freebsd"))

    mageia_result =
      BugzillaApi.submit(workspace, mageia_bundle, Workspace.target!(workspace, "mageia"),
        dry_run: true
      )

    freebsd_result =
      BugzillaApi.submit(workspace, freebsd_bundle, Workspace.target!(workspace, "freebsd"),
        dry_run: true
      )

    assert mageia_result.status == "dry_run"
    assert freebsd_result.status == "dry_run"
    assert File.exists?(mageia_result.payload)
    assert File.exists?(freebsd_result.payload)
  end

  test "bugzilla API live submit supports Mageia requests and FreeBSD patches", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")
    curl_log = Path.join(root, "bugzilla-curl.log")
    curl_bodies_dir = Path.join(root, "bugzilla-curl-bodies")

    File.mkdir_p!(curl_bodies_dir)

    write_executable(
      root,
      "fake-bin/curl",
      """
      #!/bin/sh
      set -eu

      body_dir=#{Util.shell_escape(curl_bodies_dir)}
      body_path="$body_dir/body-$$.json"
      cat > "$body_path"
      printf '%s\\n' "$*" >> #{Util.shell_escape(curl_log)}
      printf '%s\\n' "$body_path" >> #{Util.shell_escape(curl_log)}

      case "$*" in
        *"/rest/bug/"*"/attachment"*)
          printf '%s' '{"ids":[9001]}'
          printf '\\n__LINUX_ROLLOUT_HTTP_STATUS__=200\\n__LINUX_ROLLOUT_CONTENT_TYPE__=application/json'
          ;;
        *"/rest/bug")
          printf '%s' '{"id":4242}'
          printf '\\n__LINUX_ROLLOUT_HTTP_STATUS__=200\\n__LINUX_ROLLOUT_CONTENT_TYPE__=application/json'
          ;;
        *)
          echo "unsupported fake curl invocation: $*" >&2
          exit 1
          ;;
      esac
      """
    )

    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    Util.write_json!(Util.bugzilla_credentials_file("https://bugs.mageia.org"), %{
      "login" => "mageia-tester",
      "api_key" => "mageia-api-key"
    })

    Util.write_json!(Util.bugzilla_credentials_file("https://bugs.freebsd.org/bugzilla"), %{
      "login" => "freebsd-tester",
      "api_key" => "freebsd-api-key"
    })

    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    mageia_bundle =
      LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "mageia"))

    freebsd_bundle =
      LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "freebsd"))

    mageia_result =
      BugzillaApi.submit(workspace, mageia_bundle, Workspace.target!(workspace, "mageia"), [])

    freebsd_result =
      BugzillaApi.submit(workspace, freebsd_bundle, Workspace.target!(workspace, "freebsd"), [])

    assert mageia_result.status == "awaiting_remote_review"
    assert mageia_result.bug_id == 4242
    assert mageia_result.bug_url == "https://bugs.mageia.org/show_bug.cgi?id=4242"

    assert freebsd_result.status == "awaiting_remote_review"
    assert freebsd_result.bug_id == 4242
    assert freebsd_result.bug_url == "https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=4242"

    curl_entries =
      File.read!(curl_log)
      |> String.split("\n", trim: true)
      |> Enum.chunk_every(2)

    mageia_bug_entry =
      Enum.find(curl_entries, fn [args, body_path] ->
        String.contains?(args, "/rest/bug") and
          not String.contains?(args, "/attachment") and
          File.read!(body_path) =~ "\"product\":\"Mageia\""
      end)

    mageia_attach_entry =
      Enum.find(curl_entries, fn [args, body_path] ->
        String.contains?(args, "/rest/bug/4242/attachment") and
          File.read!(body_path) =~ "\"is_patch\":false"
      end)

    freebsd_bug_entry =
      Enum.find(curl_entries, fn [args, body_path] ->
        String.contains?(args, "/rest/bug") and
          not String.contains?(args, "/attachment") and
          File.read!(body_path) =~ "\"product\":\"Ports & Packages\""
      end)

    freebsd_attach_entry =
      Enum.find(curl_entries, fn [args, body_path] ->
        String.contains?(args, "/rest/bug/4242/attachment") and
          File.read!(body_path) =~ "\"is_patch\":true"
      end)

    assert mageia_bug_entry
    assert mageia_attach_entry
    assert freebsd_bug_entry
    assert freebsd_attach_entry

    [mageia_bug_args, mageia_bug_body] = mageia_bug_entry
    [mageia_attach_args, mageia_attach_body] = mageia_attach_entry
    [freebsd_bug_args, freebsd_bug_body] = freebsd_bug_entry
    [freebsd_attach_args, freebsd_attach_body] = freebsd_attach_entry

    assert mageia_bug_args =~ "/rest/bug"
    assert File.read!(mageia_bug_body) =~ "\"product\":\"Mageia\""
    assert File.read!(mageia_bug_body) =~ "\"api_key\":\"mageia-api-key\""

    assert mageia_attach_args =~ "/rest/bug/4242/attachment"
    assert File.read!(mageia_attach_body) =~ "\"is_patch\":false"

    assert freebsd_bug_args =~ "/rest/bug"
    assert File.read!(freebsd_bug_body) =~ "\"product\":\"Ports & Packages\""
    assert File.read!(freebsd_bug_body) =~ "\"api_key\":\"freebsd-api-key\""

    assert freebsd_attach_args =~ "/rest/bug/4242/attachment"
    freebsd_attachment = File.read!(freebsd_attach_body)
    assert freebsd_attachment =~ "\"is_patch\":true"
    assert freebsd_attachment =~ "\"file_name\":\"freebsd.patch\""
    assert freebsd_attachment =~ "\"data\":\""
  end

  test "bugzilla API falls back to manual handoff when Mageia returns HTML", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")
    curl_log = Path.join(root, "bugzilla-html.log")
    curl_bodies_dir = Path.join(root, "bugzilla-html-bodies")

    File.mkdir_p!(curl_bodies_dir)

    write_executable(
      root,
      "fake-bin/curl",
      """
      #!/bin/sh
      set -eu

      body_dir=#{Util.shell_escape(curl_bodies_dir)}
      body_path="$body_dir/body-$$.json"
      cat > "$body_path"
      printf '%s\\n' "$*" >> #{Util.shell_escape(curl_log)}
      printf '%s\\n' "$body_path" >> #{Util.shell_escape(curl_log)}

      case "$*" in
        *"/rest/bug")
          printf '%s' '<!DOCTYPE html><html><title>Checking your browser</title></html>'
          printf '\\n__LINUX_ROLLOUT_HTTP_STATUS__=200\\n__LINUX_ROLLOUT_CONTENT_TYPE__=text/html'
          ;;
        *)
          echo "unsupported fake curl invocation: $*" >&2
          exit 1
          ;;
      esac
      """
    )

    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    Util.write_json!(Util.bugzilla_credentials_file("https://bugs.mageia.org"), %{
      "login" => "mageia-tester",
      "api_key" => "mageia-api-key"
    })

    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "mageia"))
    result = BugzillaApi.submit(workspace, bundle, Workspace.target!(workspace, "mageia"), [])

    assert result.status == "manual"
    assert result.bug_id == nil
    assert result.bug_url == nil
    assert File.exists?(result.handoff)
    assert result.details["failure_stage"] == "create_bug"
    assert result.details["http_status"] == 200
    assert result.details["content_type"] == "text/html"
    assert result.details["response_excerpt"] =~ "Checking your browser"

    handoff = File.read!(result.handoff)

    assert handoff =~ "https://bugs.mageia.org/enter_bug.cgi?product=Mageia"
    assert handoff =~ "wireshare request"
    assert handoff =~ Path.join(bundle.source_root, "packaging/mageia/wireshare.spec")
    assert handoff =~ "Checking your browser"
  end

  test "bugzilla API falls back to manual handoff when FreeBSD attachment upload fails", %{
    root: root
  } do
    fake_bin = Path.join(root, "fake-bin")
    curl_log = Path.join(root, "bugzilla-freebsd-partial.log")
    curl_bodies_dir = Path.join(root, "bugzilla-freebsd-bodies")

    File.mkdir_p!(curl_bodies_dir)

    write_executable(
      root,
      "fake-bin/curl",
      """
      #!/bin/sh
      set -eu

      body_dir=#{Util.shell_escape(curl_bodies_dir)}
      body_path="$body_dir/body-$$.json"
      cat > "$body_path"
      printf '%s\\n' "$*" >> #{Util.shell_escape(curl_log)}
      printf '%s\\n' "$body_path" >> #{Util.shell_escape(curl_log)}

      case "$*" in
        *"/rest/bug/"*"/attachment"*)
          printf '%s' '{"message":"Not Found"}'
          printf '\\n__LINUX_ROLLOUT_HTTP_STATUS__=404\\n__LINUX_ROLLOUT_CONTENT_TYPE__=application/json'
          ;;
        *"/rest/bug")
          printf '%s' '{"id":4242}'
          printf '\\n__LINUX_ROLLOUT_HTTP_STATUS__=200\\n__LINUX_ROLLOUT_CONTENT_TYPE__=application/json'
          ;;
        *)
          echo "unsupported fake curl invocation: $*" >&2
          exit 1
          ;;
      esac
      """
    )

    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    Util.write_json!(Util.bugzilla_credentials_file("https://bugs.freebsd.org/bugzilla"), %{
      "login" => "freebsd-tester",
      "api_key" => "freebsd-api-key"
    })

    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "freebsd"))
    result = BugzillaApi.submit(workspace, bundle, Workspace.target!(workspace, "freebsd"), [])

    assert result.status == "manual"
    assert result.bug_id == 4242
    assert result.bug_url == "https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=4242"
    assert File.exists?(result.handoff)
    assert result.details["failure_stage"] == "attach_patch"
    assert result.details["http_status"] == 404
    assert result.details["content_type"] == "application/json"
    assert result.details["response_excerpt"] =~ "Not Found"

    handoff = File.read!(result.handoff)

    assert handoff =~ "Existing bug: https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=4242"
    assert handoff =~ "creating a duplicate bug"
    assert handoff =~ Path.join(bundle.bundle_root, "freebsd.patch")
    assert handoff =~ "HTTP status: `404`"

    curl_entries =
      File.read!(curl_log)
      |> String.split("\n", trim: true)
      |> Enum.chunk_every(2)

    assert Enum.any?(curl_entries, fn [args, _body_path] -> String.contains?(args, "/rest/bug") end)

    assert Enum.any?(curl_entries, fn [args, _body_path] ->
             String.contains?(args, "/rest/bug/4242/attachment")
           end)
  end

  test "git submit dry run supports HaikuPorts GitHub PR payloads", %{root: root} do
    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "haiku"))

    result =
      GitSubmit.submit(workspace, bundle, Workspace.target!(workspace, "haiku"), dry_run: true)

    assert result.status == "dry_run"
    recipe_path = Path.join(bundle.source_root, "packaging/haiku/wireshare.recipe")
    recipe = File.read!(recipe_path)

    assert File.exists?(recipe_path)
    assert String.contains?(recipe, "DESCRIPTION=\"")
    assert String.contains?(recipe, "desktop application.")
    assert String.contains?(recipe, "system Java runtime.")
    assert elem(:binary.match(recipe, "CHECKSUM_SHA256"), 0) <
             elem(:binary.match(recipe, "SOURCE_FILENAME"), 0)

    assert elem(:binary.match(recipe, "ARCHITECTURES"), 0) <
             elem(:binary.match(recipe, "DISABLE_SOURCE_PACKAGE"), 0)
  end

  test "git submit dry run includes the nix maintainer payload and planned commit subjects",
       %{root: root} do
    System.put_env("LINUX_ROLLOUT_NIX_HOST_SYSTEM", "aarch64-darwin")
    prepare_fake_github_repo_env!(root)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "nix"))

    result =
      GitSubmit.submit(workspace, bundle, Workspace.target!(workspace, "nix"), dry_run: true)

    assert result.status == "dry_run"
    assert File.exists?(Path.join(bundle.source_root, "packaging/nix/maintainer-entry.nix"))
    assert File.exists?(Path.join(bundle.source_root, "packaging/nix/package.nix"))
    assert File.exists?(Path.join(bundle.source_root, "packaging/nix/deps.json"))
    assert File.exists?(Path.join(bundle.bundle_root, "DRY_RUN_CHECKOUTS/nix/nix/.git"))
    assert hd(result.units).commit_messages == ["maintainers: add tester", "wireshare: init at 9.9"]
    assert hd(result.units).validated_system == "aarch64-darwin"

    dry_run_checkout = result.units |> hd() |> Map.fetch!(:dry_run_checkout)

    assert File.read!(Path.join(dry_run_checkout, "pkgs/by-name/wi/wireshare/package.nix")) =~
             "maintainers = with lib.maintainers; [ tester ];"

    assert File.read!(Path.join(dry_run_checkout, "pkgs/by-name/wi/wireshare/package.nix")) =~
             "platforms = lib.platforms.linux ++ lib.platforms.darwin;"

    assert File.read!(Path.join(dry_run_checkout, "pkgs/by-name/wi/wireshare/package.nix")) =~
             "wrapperRoot = \"$out/share/wireshare\";"

    assert File.read!(Path.join(dry_run_checkout, "pkgs/by-name/wi/wireshare/package.nix")) =~
             "# formatted by fake treefmt"

    assert File.read!(Path.join(dry_run_checkout, "maintainers/maintainer-list.nix")) =~
             "tester = {"

    assert File.read!(Path.join(dry_run_checkout, "maintainers/maintainer-list.nix")) =~
             "# formatted by fake treefmt"

    nix_log = File.read!(Path.join(root, "fake-nix.log"))
    assert nix_log =~ "eval .#legacyPackages.aarch64-darwin.wireshare.drvPath"
    assert nix_log =~ "build .#legacyPackages.aarch64-darwin.wireshare"

    refute File.exists?(Path.join([workspace.checkout_root, "nix", "nix"]))
  end

  test "nix git submit hands off GitHub PR creation with a seeded title", %{root: root} do
    System.put_env("LINUX_ROLLOUT_NIX_HOST_SYSTEM", "aarch64-darwin")
    %{pr_log: pr_log} = prepare_fake_github_repo_env!(root)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "nix"))

    output =
      capture_io(fn ->
        send(
          self(),
          {:git_submit_result,
           GitSubmit.submit(workspace, bundle, Workspace.target!(workspace, "nix"), [])}
        )
      end)

    assert_receive {:git_submit_result, result}
    State.put_target!(workspace, "nix", result)

    assert result.status == "awaiting_pr_creation"
    assert result.handoff == Path.join(bundle.bundle_root, "PULL_REQUEST_HANDOFF.md")
    assert result.next_step_command =~ "gh pr create"
    assert result.next_step_command =~ "--repo 'NixOS/nixpkgs'"
    assert result.next_step_command =~ "--base 'master'"
    assert result.next_step_command =~ "--head 'tester:linux-rollout-9.9-nix'"
    assert result.next_step_command =~ "--title 'wireshare: init at 9.9'"
    refute result.next_step_command =~ "--body"
    refute result.next_step_command =~ "--body-file"
    assert result.suggested_pr_title == "wireshare: init at 9.9"
    assert hd(result.units).validated_system == "aarch64-darwin"
    assert File.read!(result.handoff) =~ "their own terminal/editor session"
    assert File.read!(result.handoff) =~ "wireshare: init at 9.9"
    assert output =~ "run this command in your own terminal/editor session"
    refute File.exists?(pr_log)

    {log_output, 0} =
      System.cmd("git", ["log", "--format=%s", "-n", "2"],
        cd: Path.join([workspace.checkout_root, "nix", "nix"])
      )

    assert String.split(String.trim(log_output), "\n") == [
             "wireshare: init at 9.9",
             "maintainers: add tester"
           ]

    status_output =
      capture_io(fn ->
        CLI.main(["status", "--cwd", root, "--target", "nix"])
      end)

    assert status_output =~ "nix: last=awaiting_pr_creation"
    assert status_output =~ "validated system: aarch64-darwin"
    assert status_output =~ "suggested PR title: wireshare: init at 9.9"
    assert status_output =~ "next step: gh pr create"
    assert status_output =~ "handoff: "
  end

  test "nix maintainer entry insertion is sorted and idempotent" do
    contents = """
    {
      naelstrof = {
        github = "naelstrof";
        githubId = 1;
        name = "Nael Strof";
      };

      ocharles = {
        github = "ocharles";
        githubId = 2;
        name = "Oliver Charles";
      };
    }
    """

    entry = """
      nmatavka = {
        email = "teamhermes@gmx.com";
        github = "nmatavka";
        githubId = 50931263;
        name = "Nick Matavka";
      };
    """

    updated = GitSubmit.upsert_nix_maintainer_entry(contents, "nmatavka", entry)
    naelstrof_index = elem(:binary.match(updated, "naelstrof = {"), 0)
    nmatavka_index = elem(:binary.match(updated, "nmatavka = {"), 0)
    ocharles_index = elem(:binary.match(updated, "ocharles = {"), 0)

    assert String.contains?(updated, "nmatavka = {")
    assert naelstrof_index < nmatavka_index
    assert nmatavka_index < ocharles_index
    assert GitSubmit.upsert_nix_maintainer_entry(updated, "nmatavka", entry) == updated
  end

  test "nix maintainer entry conflicts are rejected" do
    contents = """
    {
      nmatavka = {
        email = "old@example.com";
        github = "nmatavka";
        githubId = 50931263;
        name = "Nick Matavka";
      };
    }
    """

    entry = """
      nmatavka = {
        email = "teamhermes@gmx.com";
        github = "nmatavka";
        githubId = 50931263;
        name = "Nick Matavka";
      };
    """

    assert_raise RuntimeError,
                 "maintainers/maintainer-list.nix already defines nmatavka with different public data",
                 fn ->
                   GitSubmit.upsert_nix_maintainer_entry(contents, "nmatavka", entry)
                 end
  end

  test "template-sensitive GitHub targets hand off with seeded titles", %{root: root} do
    %{pr_log: pr_log} = prepare_fake_github_repo_env!(root)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "homebrew"))

    output =
      capture_io(fn ->
        send(
          self(),
          {:git_submit_result,
           GitSubmit.submit(workspace, bundle, Workspace.target!(workspace, "homebrew"), [])}
        )
      end)

    assert_receive {:git_submit_result, result}

    assert result.status == "awaiting_pr_creation"
    assert result.next_step_command =~ "gh pr create"
    assert result.next_step_command =~ "--repo 'tester/homebrew-wireshare'"
    assert result.next_step_command =~ "--base 'main'"
    assert result.next_step_command =~ "--head 'tester:linux-rollout-9.9-homebrew'"
    assert result.next_step_command =~ "--title 'wireshare 9.9'"
    refute result.next_step_command =~ "--body"
    refute result.next_step_command =~ "--body-file"
    assert result.suggested_pr_title == "wireshare 9.9"
    assert File.read!(result.handoff) =~ "their own terminal/editor session"
    assert File.read!(result.handoff) =~ "wireshare 9.9"
    assert output =~ "run this command in your own terminal/editor session"
    refute File.exists?(pr_log)
  end

  test "interactive-template compatibility mode now hands off GitHub PR creation", %{root: root} do
    %{pr_log: pr_log} = prepare_fake_github_repo_env!(root, fail_pr_create: true)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    target =
      Workspace.target!(workspace, "homebrew")
      |> Map.put("pull_request_handoff", "interactive_template")

    bundle = LinuxRollout.Bundles.create!(workspace, target)

    output =
      capture_io(fn ->
        send(self(), {:git_submit_result, GitSubmit.submit(workspace, bundle, target, [])})
      end)

    assert_receive {:git_submit_result, result}

    assert result.status == "awaiting_pr_creation"
    assert result.next_step_command =~ "gh pr create"
    assert result.next_step_command =~ "--repo 'tester/homebrew-wireshare'"
    assert result.next_step_command =~ "--title 'wireshare 9.9'"
    assert output =~ "deferring automatic template-preserving GitHub PR creation"
    refute File.exists?(pr_log)
  end

  test "manual template mode still hands off GitHub PR creation", %{root: root} do
    %{pr_log: pr_log} = prepare_fake_github_repo_env!(root, fail_pr_create: true)
    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    target =
      Workspace.target!(workspace, "homebrew")
      |> Map.put("pull_request_handoff", "manual_template")
      |> Map.put("suggested_pr_title", "linux: publish homebrew packaging for 9.9")

    bundle = LinuxRollout.Bundles.create!(workspace, target)

    output =
      capture_io(fn ->
        send(self(), {:git_submit_result, GitSubmit.submit(workspace, bundle, target, [])})
      end)

    assert_receive {:git_submit_result, result}

    assert result.status == "awaiting_pr_creation"
    assert result.next_step_command =~ "gh pr create"
    assert result.next_step_command =~ "--repo 'tester/homebrew-wireshare'"
    assert result.next_step_command =~ "--base 'main'"
    assert result.next_step_command =~ "--head 'tester:linux-rollout-9.9-homebrew'"
    assert result.next_step_command =~ "--title 'linux: publish homebrew packaging for 9.9'"
    assert result.suggested_pr_title == "linux: publish homebrew packaging for 9.9"
    refute result.next_step_command =~ "--body"
    refute result.next_step_command =~ "--body-file"
    assert File.read!(result.handoff) =~ "their own terminal/editor session"
    assert File.read!(result.handoff) =~ "linux: publish homebrew packaging for 9.9"
    assert output =~ "run this command in your own terminal/editor session"
    refute File.exists?(pr_log)
  end

  test "doctor reports off-PATH Nix tooling with standard daemon guidance", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")
    nix_bin = Path.join(root, "fake-nix/bin")
    nix_profile = Path.join(root, "fake-nix/etc/profile.d/nix-daemon.sh")
    write_executable(root, "fake-bin/gh", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-bin/git", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-bin/ssh", "#!/bin/sh\nexit 1\n")
    write_executable(root, "fake-nix/bin/nix", "#!/bin/sh\nexit 0\n")
    write_file(root, "fake-nix/etc/profile.d/nix-daemon.sh", "#!/bin/sh\n")
    System.put_env("PATH", fake_bin)
    System.put_env("LINUX_ROLLOUT_NIX_PROFILE_SCRIPT", nix_profile)
    System.put_env("LINUX_ROLLOUT_NIX_BIN_DIR", nix_bin)

    workspace = Workspace.load!(root)
    result = Doctor.inspect_target(workspace, Workspace.target!(workspace, "nix"))

    assert result.status == "blocked_missing_cli"
    assert Enum.any?(result.messages, &String.contains?(&1, "off-PATH"))
    assert Enum.any?(result.messages, &String.contains?(&1, nix_profile))
  end

  test "doctor reports generic Nix install guidance when no standard install is detectable", %{
    root: root
  } do
    fake_bin = Path.join(root, "fake-bin")
    write_executable(root, "fake-bin/gh", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-bin/git", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-bin/ssh", "#!/bin/sh\nexit 1\n")
    System.put_env("PATH", fake_bin)
    System.put_env("LINUX_ROLLOUT_NIX_PROFILE_SCRIPT", Path.join(root, "missing/profile.sh"))
    System.put_env("LINUX_ROLLOUT_NIX_BIN_DIR", Path.join(root, "missing/bin"))

    workspace = Workspace.load!(root)
    result = Doctor.inspect_target(workspace, Workspace.target!(workspace, "nix"))

    assert result.status == "blocked_missing_cli"
    assert Enum.any?(result.messages, &String.contains?(&1, "install Nix"))
    refute Enum.any?(result.messages, &String.contains?(&1, "off-PATH"))
  end

  test "launcher bootstraps Nix from the standard profile script before invoking mix", %{root: root} do
    fake_mix_log = Path.join(root, "fake-mix.log")
    fake_bin = Path.join(root, "fake-bin")
    nix_bin = Path.join(root, "fake-nix/bin")
    nix_profile = Path.join(root, "fake-nix/etc/profile.d/nix-daemon.sh")

    write_executable(
      root,
      "fake-bin/mix",
      """
      #!/bin/sh
      set -eu
      {
        printf 'nix=%s\\n' "$(command -v nix || true)"
        printf 'nix-build=%s\\n' "$(command -v nix-build || true)"
        printf 'nix-shell=%s\\n' "$(command -v nix-shell || true)"
        printf 'args=%s\\n' "$*"
      } >> #{Util.shell_escape(fake_mix_log)}
      exit 0
      """
    )

    write_executable(root, "fake-nix/bin/nix", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-nix/bin/nix-build", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-nix/bin/nix-shell", "#!/bin/sh\nexit 0\n")

    write_file(
      root,
      "fake-nix/etc/profile.d/nix-daemon.sh",
      """
      #!/bin/sh
      export PATH=#{Util.shell_escape(nix_bin)}:$PATH
      """
    )

    {_, 0} =
      System.cmd(
        "/bin/sh",
        [rollout_script_path(), "doctor", "--target", "nix"],
        env: [
          {"HOME", Path.join(root, "home")},
          {"PATH", "#{fake_bin}:#{System.get_env("PATH")}"},
          {"LINUX_ROLLOUT_NIX_PROFILE_SCRIPT", nix_profile},
          {"LINUX_ROLLOUT_NIX_BIN_DIR", Path.join(root, "unused-nix-bin")}
        ]
      )

    mix_log = File.read!(fake_mix_log)
    assert mix_log =~ "nix=#{nix_bin}/nix"
    assert mix_log =~ "nix-build=#{nix_bin}/nix-build"
    assert mix_log =~ "nix-shell=#{nix_bin}/nix-shell"
  end

  test "launcher leaves an existing nix on PATH alone", %{root: root} do
    fake_mix_log = Path.join(root, "fake-mix.log")
    fake_bin = Path.join(root, "fake-bin")
    existing_nix_bin = Path.join(root, "existing-nix/bin")
    sourced_nix_bin = Path.join(root, "sourced-nix/bin")
    nix_profile = Path.join(root, "sourced-nix/etc/profile.d/nix-daemon.sh")

    write_executable(
      root,
      "fake-bin/mix",
      """
      #!/bin/sh
      set -eu
      printf 'nix=%s\\n' "$(command -v nix || true)" >> #{Util.shell_escape(fake_mix_log)}
      exit 0
      """
    )

    write_executable(root, "existing-nix/bin/nix", "#!/bin/sh\nexit 0\n")
    write_executable(root, "sourced-nix/bin/nix", "#!/bin/sh\nexit 0\n")

    write_file(
      root,
      "sourced-nix/etc/profile.d/nix-daemon.sh",
      """
      #!/bin/sh
      export PATH=#{Util.shell_escape(sourced_nix_bin)}:$PATH
      """
    )

    {_, 0} =
      System.cmd(
        "/bin/sh",
        [rollout_script_path(), "doctor", "--target", "nix"],
        env: [
          {"HOME", Path.join(root, "home")},
          {"PATH", "#{existing_nix_bin}:#{fake_bin}:#{System.get_env("PATH")}"},
          {"LINUX_ROLLOUT_NIX_PROFILE_SCRIPT", nix_profile},
          {"LINUX_ROLLOUT_NIX_BIN_DIR", sourced_nix_bin}
        ]
      )

    assert File.read!(fake_mix_log) =~ "nix=#{existing_nix_bin}/nix"
  end

  test "mail submit dry run supports Guix intro and threaded resume", %{root: root} do
    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    guix_bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "guix"))

    intro_result =
      MailSubmit.submit(workspace, guix_bundle, Workspace.target!(workspace, "guix"),
        dry_run: true
      )

    resume_result =
      MailSubmit.submit(
        workspace,
        guix_bundle,
        Workspace.target!(workspace, "guix"),
        dry_run: true,
        thread_address: "12345@debbugs.gnu.org"
      )

    assert intro_result.status == "dry_run"
    assert resume_result.status == "dry_run"
    assert File.exists?(intro_result.preview)
    assert File.dir?(intro_result.patches)
  end

  test "mail submit can send Guix patch replies without git send-email", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")
    real_git = System.find_executable("git") || raise "git is required for this test"

    write_file(root, "fake-bin/git", """
    #!/bin/sh
    if [ "$1" = "send-email" ]; then
      echo "git: 'send-email' is not a git command." >&2
      exit 1
    fi
    exec #{real_git} "$@"
    """)

    File.chmod!(Path.join(fake_bin, "git"), 0o755)
    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "guix"))

    result =
      MailSubmit.submit(workspace, bundle, Workspace.target!(workspace, "guix"),
        dry_run: true,
        thread_address: "12345@debbugs.gnu.org"
      )

    patch_previews = Path.wildcard(Path.join(result.patches, "*.patch.eml"))
    patch_payloads = Path.wildcard(Path.join(result.patches, "*.patch.mail.json"))

    assert result.status == "dry_run"
    assert patch_previews != []
    assert patch_payloads != []
  end

  test "conditional git submit falls back to manual packet when gated access is absent", %{
    root: root
  } do
    workspace = Workspace.load!(root)
    Validator.run!(workspace)
    bundle = LinuxRollout.Bundles.create!(workspace, Workspace.target!(workspace, "pkgsrc"))
    preflight = Doctor.inspect_target(workspace, Workspace.target!(workspace, "pkgsrc"))

    result =
      ConditionalGitSubmit.submit(
        workspace,
        bundle,
        Workspace.target!(workspace, "pkgsrc"),
        preflight,
        dry_run: true
      )

    assert result.status == "dry_run"
    assert File.exists?(result.handoff)
  end

  test "doctor distinguishes config, cli, and auth blockers" do
    config_target = %{"required_config" => ["destination.repo"], "destination" => %{}}
    cli_target = %{"required_cli" => ["command-that-does-not-exist-123"]}

    auth_target = %{
      "auth_probe" => %{
        "type" => "file",
        "path" => "/definitely/missing/auth/file",
        "help" => "log in first"
      }
    }

    launchpad_auth_target = %{"auth_probe" => %{"type" => "launchpad_auth"}}

    bugzilla_auth_target = %{
      "auth_probe" => %{"type" => "bugzilla_auth", "url" => "https://bugs.example.invalid"}
    }

    assert Doctor.inspect_target(nil, config_target).status == "blocked_missing_config"
    assert Doctor.inspect_target(nil, cli_target).status == "blocked_missing_cli"
    assert Doctor.inspect_target(nil, auth_target).status == "blocked_missing_auth"
    assert Doctor.inspect_target(nil, launchpad_auth_target).status == "blocked_missing_auth"
    assert Doctor.inspect_target(nil, bugzilla_auth_target).status == "blocked_missing_auth"
  end

  test "doctor reports missing abuild for alpine", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")
    write_executable(root, "fake-bin/git", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-bin/glab", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-bin/ssh", "#!/bin/sh\nexit 1\n")
    System.put_env("PATH", fake_bin)

    workspace = Workspace.load!(root)
    result = Doctor.inspect_target(workspace, Workspace.target!(workspace, "alpine"))

    assert result.status == "blocked_missing_cli"
    assert Enum.any?(result.messages, &String.contains?(&1, "abuild"))
  end

  test "doctor uses local config guidance for pkgsrc when the wip user is missing", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")
    write_file(root, "fake-bin/ssh", "#!/bin/sh\nexit 1\n")
    File.chmod!(Path.join(fake_bin, "ssh"), 0o755)
    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    workspace = Workspace.load!(root)
    result = Doctor.inspect_target(workspace, Workspace.target!(workspace, "pkgsrc"))

    assert result.status == "blocked_missing_config"

    assert Enum.any?(result.messages, fn message ->
             message =~ "config set accounts.pkgsrc_wip_user"
           end)
  end

  test "doctor becomes ready for launchpad and obs-backed targets once local auth state exists",
       %{
         root: root
       } do
    write_file(root, ".local-share/launchpad/bin/python", "#!/bin/sh\nexit 0\n")
    File.chmod!(Path.join(root, ".local-share/launchpad/bin/python"), 0o755)
    write_file(root, ".local-share/launchpad/credentials", "token\n")
    write_file(root, "home/.oscrc", "[general]\nuser = obs-tester\n")

    LocalConfig.set!("accounts.launchpad_owner", "launchpad-tester")
    LocalConfig.set!("accounts.obs_owner", "obs-tester")
    LocalConfig.set!("accounts.obs_project", "home:obs-tester:wireshare")

    workspace = Workspace.load!(root)

    assert Doctor.inspect_target(workspace, Workspace.target!(workspace, "snap")).status == "ok"
    assert Doctor.inspect_target(workspace, Workspace.target!(workspace, "ppa")).status == "ok"
    assert Doctor.inspect_target(workspace, Workspace.target!(workspace, "obs")).status == "ok"

    assert Doctor.inspect_target(workspace, Workspace.target!(workspace, "opensuse")).status ==
             "ok"
  end

  test "doctor blocks guru when SSH access is missing", %{root: root} do
    prepare_fake_guru_repo_env!(root,
      ssh_access: false,
      commit_gpgsign: true,
      push_gpgsign: true
    )
    workspace = Workspace.load!(root)
    result = Doctor.inspect_target(workspace, Workspace.target!(workspace, "guru"))

    assert result.status == "blocked_missing_auth"

    assert Enum.any?(result.messages, fn message ->
             message =~ "SSH key has GURU contributor access"
           end)
  end

  test "doctor blocks guru when user.signingkey is missing", %{root: root} do
    prepare_fake_guru_repo_env!(root,
      signingkey: nil,
      commit_gpgsign: true,
      push_gpgsign: true
    )
    workspace = Workspace.load!(root)
    result = Doctor.inspect_target(workspace, Workspace.target!(workspace, "guru"))

    assert result.status == "blocked_missing_auth"

    assert Enum.any?(result.messages, fn message ->
             message =~ "user.signingkey"
           end)
  end

  test "doctor blocks guru when GPG signing config is disabled", %{root: root} do
    prepare_fake_guru_repo_env!(root, commit_gpgsign: false, push_gpgsign: false)
    workspace = Workspace.load!(root)
    result = Doctor.inspect_target(workspace, Workspace.target!(workspace, "guru"))

    assert result.status == "blocked_missing_auth"

    assert Enum.any?(result.messages, fn message ->
             message =~ "commit.gpgsign true"
           end)

    assert Enum.any?(result.messages, fn message ->
             message =~ "push.gpgsign true"
           end)
  end

  test "submit headless-only includes gated targets and skips manual ones", %{root: root} do
    prepare_fake_gitlab_repo_env!(root)
    CLI.main(["prepare", "--cwd", root, "--skip-gradle", "--dry-run"])

    output =
      capture_io(fn ->
        CLI.main(["submit", "--cwd", root, "--headless-only", "--dry-run", "--no-open"])
      end)

    state = Workspace.load!(root) |> State.load()

    assert output =~ "[linux-rollout] submit: starting alpine"
    assert Map.has_key?(state["targets"], "flathub")
    assert Map.has_key?(state["targets"], "snap")
    assert Map.has_key?(state["targets"], "guru")
    assert Map.has_key?(state["targets"], "pkgsrc")
    refute Map.has_key?(state["targets"], "homebrew")
    refute Map.has_key?(state["targets"], "haiku")
    refute Map.has_key?(state["targets"], "nix")
    refute Map.has_key?(state["targets"], "debian")
  end

  test "github duplicate PR detection accepts existing PR URLs in CLI output" do
    output = """
    pull request create failed: a pull request for branch "tester:linux-rollout-9.9-homebrew" into branch "main" already exists:
    https://github.com/tester/homebrew-wireshare/pull/12
    """

    assert GitSubmit.detect_existing_github_pr(
             output,
             "tester/homebrew-wireshare",
             "tester:linux-rollout-9.9-homebrew",
             "linux-rollout-9.9-homebrew",
             "main"
           ) == %{pull_request: "https://github.com/tester/homebrew-wireshare/pull/12"}
  end

  test "submitter records a target failure and continues with later targets", %{root: root} do
    fake_bin = Path.join(root, "fake-bin")
    write_file(root, "fake-bin/gh", "#!/bin/sh\nexit 0\n")
    File.chmod!(Path.join(fake_bin, "gh"), 0o755)
    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")

    workspace = Workspace.load!(root)
    Validator.run!(workspace)

    broken_target = %{
      "name" => "broken",
      "driver" => "unsupported",
      "submission_mode" => "nope",
      "headless_capability" => "full",
      "submission_paths" => ["packaging/does-not-exist"]
    }

    updated_workspace = %{
      workspace
      | targets: Map.put(workspace.targets, "broken", broken_target)
    }

    Submitter.submit!(updated_workspace, ["broken", "homebrew"], dry_run: true, no_open: true)

    state = State.load(updated_workspace)
    assert get_in(state, ["targets", "broken", "status"]) == "failed"
    assert get_in(state, ["targets", "homebrew", "status"]) == "dry_run"
  end

  test "open_url stays headless unless explicitly enabled" do
    assert :ok = Util.open_url("https://example.invalid", [])
    assert :ok = Util.open_url("https://example.invalid", open: false)
    assert :ok = Util.open_url("https://example.invalid", open: true, no_open: true)
  end

  defp write_fixture_workspace(root) do
    write_file(root, "packaging/release_manifest.yaml", """
    app:
      name: WireShare
      id: cx.hermes.WireShare
      vendor: Hermes
      version: 9.9
      legacy_ids:
        primary: org.teamhermes.WireShare
        secondary: org.team_hermes.WireShare
    release:
      tag: release/9.9
      date: 2026-05-20
      notes: |
        Fixture release notes.
    repository:
      slug: example/wireshare
      default_branch: main
      homepage_url: https://example.invalid/wireshare
      issues_url: https://example.invalid/wireshare/issues
    assets:
      source_tarball:
        name: WireShare-9.9-source.tar.gz
        url: https://example.invalid/WireShare-9.9-source.tar.gz
        sha256: ""
        nix_base32: ""
      jar:
        name: WireShare.jar
        url: https://example.invalid/WireShare.jar
        sha256: ""
      checksums:
        name: SHA256SUMS
        url: https://example.invalid/SHA256SUMS
      dependency_inventory:
        name: DEPENDENCY_INVENTORY.txt
        url: https://example.invalid/DEPENDENCY_INVENTORY.txt
    screenshots:
      - path: screenshots/main.png
        caption: Main view
    accounts:
      github_owner: tester
      gitlab_owner: tester
      flathub_fork_repo: tester/flathub
      homebrew_tap_repo: tester/homebrew-wireshare
      homebrew_base_branch: main
      launchpad_owner: tester
      launchpad_project: wireshare
      copr_owner: tester
      nix_maintainer_github_id: 424242
      submission_name: Test Submitter
      submission_email: tester@example.invalid
      pkgsrc_wip_user: ""
    architectures:
      primary: x86_64
      optional_arm64:
        enabled: false
        jar_url: ""
        jar_sha256: ""
    targets:
      flathub:
        enabled: true
      aur:
        enabled: true
      alpine:
        enabled: true
      obs:
        enabled: true
        destination:
          project: home:tester:wireshare
          package: wireshare
      opensuse:
        enabled: true
        destination:
          project: home:tester:wireshare
          package: wireshare
          target_project: openSUSE:Factory
          target_package: wireshare
      copr:
        enabled: true
        destination:
          owner: tester
          project: wireshare
          package_name: wireshare
          source_repo: tester/wireshare-packaging
          source_base_branch: main
          chroots:
            - fedora-rawhide-x86_64
      gentoo:
        enabled: true
      guru:
        enabled: true
      homebrew:
        enabled: true
      solus:
        enabled: true
      haiku:
        enabled: true
      nix:
        enabled: true
      snap:
        enabled: true
      ppa:
        enabled: true
      guix:
        enabled: true
      pkgsrc:
        enabled: true
      openbsd:
        enabled: true
      mageia:
        enabled: true
      freebsd:
        enabled: true
      debian:
        enabled: true
    """)

    write_file(root, "packaging/targets.yaml", """
    defaults:
      rendered_root: build/linux-rollout
      branch_template: linux-rollout-@release_version@-@target@
      commit_template: "linux: publish @target@ packaging for @release_version@"
    targets:
      flathub:
        driver: human_gate
        submission_mode: review_packet
        headless_capability: gated
        submission_paths:
          - packaging/common
          - packaging/flathub
        required_files:
          - packaging/flathub/cx.hermes.WireShare.yaml
        destination:
          repo: flathub/flathub
          fork_repo: "@flathub_fork_repo@"
          submission_url: https://example.invalid/flathub
      aur:
        driver: git_submit
        submission_mode: push_only
        headless_capability: full
        submission_paths:
          - packaging/aur
        units:
          - name: wireshare
            required_files:
              - packaging/aur/wireshare/PKGBUILD
              - packaging/aur/wireshare/.SRCINFO
            path_map:
              packaging/aur/wireshare/PKGBUILD: PKGBUILD
              packaging/aur/wireshare/.SRCINFO: .SRCINFO
            destination:
              repo: ssh://aur@aur.archlinux.org/wireshare.git
              base_branch: master
          - name: wireshare-bin
            required_files:
              - packaging/aur/wireshare-bin/PKGBUILD
              - packaging/aur/wireshare-bin/.SRCINFO
            path_map:
              packaging/aur/wireshare-bin/PKGBUILD: PKGBUILD
              packaging/aur/wireshare-bin/.SRCINFO: .SRCINFO
            destination:
              repo: ssh://aur@aur.archlinux.org/wireshare-bin.git
              base_branch: master
      alpine:
        driver: git_submit
        submission_mode: gitlab_mr
        headless_capability: full
        required_cli:
          - abuild
          - glab
          - git
        submission_paths:
          - packaging/alpine
        required_files:
          - packaging/alpine/APKBUILD
        path_map:
          packaging/alpine/APKBUILD: community/wireshare/APKBUILD
        pre_submit_checks:
          - name: Refresh APKBUILD checksums with abuild
            mode: format
            run_on_dry_run: true
            command: cd community/wireshare && abuild checksum
        destination:
          host: gitlab.alpinelinux.org
          repo: alpine/aports
          fork_repo: tester/aports
          base_branch: master
      obs:
        driver: service_cli
        service: obs
        submission_mode: obs_home_project
        required_cli:
          - osc
        auth_probe:
          type: file
          path: ~/.oscrc
        headless_capability: full
        submission_paths:
          - packaging/obs
        required_files:
          - packaging/obs/wireshare.spec
        path_map:
          packaging/obs/wireshare.spec: wireshare.spec
        destination:
          project: home:tester:wireshare
          package: wireshare
      opensuse:
        driver: service_cli
        service: obs
        submission_mode: obs_submit_request
        required_cli:
          - osc
        auth_probe:
          type: file
          path: ~/.oscrc
        headless_capability: full
        submission_paths:
          - packaging/opensuse
        required_files:
          - packaging/opensuse/wireshare.spec
        path_map:
          packaging/opensuse/wireshare.spec: wireshare.spec
        destination:
          project: home:tester:wireshare
          package: wireshare
          target_project: openSUSE:Factory
          target_package: wireshare
      copr:
        driver: service_cli
        service: copr
        submission_mode: copr_scm
        headless_capability: full
        submission_paths:
          - packaging/copr
        required_files:
          - packaging/copr/wireshare.spec
        path_map:
          packaging/copr/wireshare.spec: packaging/copr/wireshare.spec
        destination:
          owner: tester
          project: wireshare
          package_name: wireshare
          source_repo: tester/wireshare-packaging
          source_base_branch: main
          chroots:
            - fedora-rawhide-x86_64
      gentoo:
        driver: git_submit
        forge: github
        submission_mode: github_pr
        pull_request_handoff: manual_template
        git_commit_signoff: true
        commit_message: "net-p2p/wireshare: add @release_version@"
        suggested_pr_title: "net-p2p/wireshare: add @release_version@"
        required_cli:
          - gh
          - git
        auth_probe:
          type: command
          command:
            - gh
            - auth
            - status
        headless_capability: manual
        submission_paths:
          - packaging/gentoo
        required_files:
          - packaging/gentoo/metadata.xml
          - packaging/gentoo/wireshare-@release_version@.ebuild
        path_map:
          packaging/gentoo/metadata.xml: net-p2p/wireshare/metadata.xml
          packaging/gentoo/wireshare-@release_version@.ebuild: net-p2p/wireshare/wireshare-@release_version@.ebuild
        destination:
          repo: gentoo/gentoo
          fork_repo: tester/gentoo
          base_branch: master
      guru:
        driver: git_submit
        submission_mode: push_only
        git_commit_signoff: true
        commit_message: "net-p2p/wireshare: add @release_version@"
        branch_name: dev
        push_command:
          - pkgdev
          - push
          - --pull
        required_cli:
          - git
          - pkgdev
          - pkgcheck
          - gpg
        auth_probe:
          - type: command
            command:
              - ssh
              - -o
              - BatchMode=yes
              - -o
              - ConnectTimeout=10
              - -o
              - StrictHostKeyChecking=accept-new
              - -T
              - git@git.gentoo.org
            success_codes:
              - 0
              - 1
            help: Ensure your SSH key has GURU contributor access for `git@git.gentoo.org:repo/proj/guru.git`.
          - type: git_config
            key: user.name
            help: Set your real name with `git config --global user.name "Your Full Name"` for GURU commits.
          - type: git_config
            key: user.email
            help: Set your GURU commit address with `git config --global user.email "you@example.com"`.
          - type: git_config
            key: user.signingkey
            help: Set your OpenPGP signing key with `git config --global user.signingkey KEY-FINGERPRINT`.
          - type: command
            command:
              - sh
              - -c
              - '[ "$(git config --bool --get commit.gpgsign 2>/dev/null)" = true ]'
            help: Enable signed GURU commits with `git config --global commit.gpgsign true`.
          - type: command
            command:
              - sh
              - -c
              - '[ "$(git config --bool --get push.gpgsign 2>/dev/null)" = true ]'
            help: Enable signed GURU pushes with `git config --global push.gpgsign true`.
        headless_capability: full
        submission_paths:
          - packaging/gentoo
        required_files:
          - packaging/gentoo/metadata.xml
          - packaging/gentoo/wireshare-@release_version@.ebuild
        path_map:
          packaging/gentoo/metadata.xml: net-p2p/wireshare/metadata.xml
          packaging/gentoo/wireshare-@release_version@.ebuild: net-p2p/wireshare/wireshare-@release_version@.ebuild
        pre_submit_checks:
          - name: Run pkgcheck scan --net
            mode: check
            cwd: checkout
            run_on_dry_run: true
            command:
              - pkgcheck
              - scan
              - --net
        destination:
          repo: git@git.gentoo.org:repo/proj/guru.git
          base_branch: dev
      solus:
        driver: git_submit
        forge: github
        submission_mode: github_pr
        pull_request_handoff: manual_template
        suggested_pr_title: "wireshare: Add at v@release_version@"
        required_cli:
          - gh
          - git
        auth_probe:
          type: command
          command:
            - gh
            - auth
            - status
        headless_capability: manual
        submission_paths:
          - packaging/solus
        required_files:
          - packaging/solus/package.yml
        path_map:
          packaging/solus/package.yml: packages/w/wireshare/package.yml
        destination:
          repo: getsolus/packages
          fork_repo: tester/packages
          base_branch: main
      homebrew:
        driver: git_submit
        forge: github
        submission_mode: github_pr
        pull_request_handoff: manual_template
        suggested_pr_title: "wireshare @release_version@"
        required_cli:
          - gh
          - git
        auth_probe:
          type: command
          command:
            - gh
            - auth
            - status
        headless_capability: manual
        submission_paths:
          - packaging/homebrew
        required_files:
          - packaging/homebrew/wireshare.rb
        path_map:
          packaging/homebrew/wireshare.rb: Formula/wireshare.rb
        destination:
          repo: "@homebrew_tap_repo@"
          fork_repo: "@homebrew_tap_repo@"
          base_branch: "@homebrew_base_branch@"
      haiku:
        driver: git_submit
        forge: github
        submission_mode: github_pr
        pull_request_handoff: manual_template
        suggested_pr_title: "net-p2p/wireshare: Add @release_version@"
        required_cli:
          - gh
          - git
        auth_probe:
          type: command
          command:
            - gh
            - auth
            - status
        headless_capability: manual
        submission_paths:
          - packaging/haiku
        required_files:
          - packaging/haiku/wireshare.recipe
        path_map:
          packaging/haiku/wireshare.recipe: net-p2p/wireshare/wireshare-@release_version@.recipe
        destination:
          repo: haikuports/haikuports
          fork_repo: tester/haikuports
          base_branch: master
      nix:
        driver: git_submit
        forge: github
        submission_mode: github_pr
        pull_request_handoff: manual_template
        required_cli:
          - gh
          - git
          - nix
          - nix-build
          - nix-shell
        auth_probe:
          type: command
          command:
            - gh
            - auth
            - status
        headless_capability: manual
        arm64_support: supported
        submission_paths:
          - packaging/nix
        required_files:
          - packaging/nix/maintainer-entry.nix
          - packaging/nix/package.nix
          - packaging/nix/deps.json
        path_map:
          packaging/nix/package.nix: pkgs/by-name/wi/wireshare/package.nix
          packaging/nix/deps.json: pkgs/by-name/wi/wireshare/deps.json
        checkout_mutations:
          - name: Add maintainer entry
            type: nix_maintainer_entry
            source: packaging/nix/maintainer-entry.nix
            destination: maintainers/maintainer-list.nix
            handle: "@nix_maintainer_handle@"
        commit_plan:
          - name: Add maintainer identity
            message: "maintainers: add @nix_maintainer_handle@"
            paths:
              - maintainers/maintainer-list.nix
          - name: Add the package
            message: "wireshare: init at @release_version@"
            paths:
              - pkgs/by-name/wi/wireshare/package.nix
              - pkgs/by-name/wi/wireshare/deps.json
        pre_submit_checks:
          - name: Format generated Nix files
            mode: format
            cwd: checkout
            run_on_dry_run: true
            command:
              - nix-shell
              - --run
              - treefmt maintainers/maintainer-list.nix pkgs/by-name/wi/wireshare/package.nix
          - name: Evaluate the generated package
            mode: check
            cwd: checkout
            run_on_dry_run: true
            command:
              - nix
              - --extra-experimental-features
              - nix-command flakes
              - eval
              - .#legacyPackages.@nix_host_system@.wireshare.drvPath
          - name: Build the generated package on Darwin hosts
            mode: check
            cwd: checkout
            run_on_dry_run: true
            command: if [ "@nix_host_os@" = "darwin" ]; then nix --extra-experimental-features 'nix-command flakes' build .#legacyPackages.@nix_host_system@.wireshare; fi
          - name: Run nixpkgs-vet
            mode: check
            cwd: checkout
            run_on_dry_run: true
            command:
              - ./ci/nixpkgs-vet.sh
              - master
              - https://github.com/NixOS/nixpkgs.git
        destination:
          repo: NixOS/nixpkgs
          fork_repo: tester/nixpkgs
          base_branch: master
        validated_system: "@nix_host_system@"
        suggested_pr_title: "wireshare: init at @release_version@"
      snap:
        driver: launchpad_api
        submission_mode: launchpad_snap
        required_cli:
          - launchpadlib
        auth_probe:
          type: launchpad_auth
        required_config:
          - destination.owner
          - destination.launchpad_project
          - destination.import_repo_name
          - destination.import_source_url
          - destination.import_branch
          - destination.snap_name
        headless_capability: full
        submission_paths:
          - packaging/common
          - packaging/snap
        required_files:
          - packaging/snap/snapcraft.yaml
        destination:
          owner: "@launchpad_owner@"
          launchpad_project: "@launchpad_project@"
          import_repo_name: wireshare
          import_source_url: https://github.com/@release_repo_slug@.git
          import_branch: "@release_repo_default_branch@"
          snap_name: wireshare
          pocket: Updates
      ppa:
        driver: launchpad_api
        submission_mode: launchpad_ppa
        required_cli:
          - launchpadlib
        auth_probe:
          type: launchpad_auth
        required_config:
          - destination.owner
          - destination.launchpad_project
          - destination.import_repo_name
          - destination.import_source_url
          - destination.import_branch
          - destination.recipe_name
          - destination.ppa_name
          - destination.ubuntu_series
        headless_capability: full
        submission_paths:
          - packaging/common
          - packaging/debian
          - packaging/ppa
        required_files:
          - packaging/ppa/launchpad-recipe.txt
          - packaging/ppa/wireshare.recipe
        destination:
          owner: "@launchpad_owner@"
          launchpad_project: "@launchpad_project@"
          import_repo_name: wireshare
          import_source_url: https://github.com/@release_repo_slug@.git
          import_branch: "@release_repo_default_branch@"
          recipe_name: wireshare
          recipe_description: WireShare Launchpad recipe
          ppa_name: wireshare
          ppa_display_name: WireShare PPA
          ppa_description: WireShare preview builds
          ubuntu_series: noble
          pocket: Updates
      guix:
        driver: mail_submit
        submission_mode: email_patch_thread
        required_cli:
          - git
          - sendmail
        auth_probe:
          - type: git_config
            key: user.name
          - type: git_config
            key: user.email
        headless_capability: full
        submission_paths:
          - packaging/guix
        required_files:
          - packaging/guix/wireshare.scm
        path_map:
          packaging/guix/wireshare.scm: gnu/packages/wireshare.scm
        destination:
          email: guix-patches@gnu.org
      pkgsrc:
        driver: conditional_git_submit
        forge: pkgsrc_wip
        submission_mode: push_only
        required_cli:
          - git
        auth_probe:
          type: command
          command:
            - ssh
            - -o
            - BatchMode=yes
            - -o
            - ConnectTimeout=10
            - -o
            - StrictHostKeyChecking=accept-new
            - -T
            - "@pkgsrc_wip_user@@wip.pkgsrc.org"
          success_codes:
            - 0
            - 1
        required_config:
          - destination.wip_user
        headless_capability: conditional
        submission_paths:
          - packaging/pkgsrc
        required_files:
          - packaging/pkgsrc/ROOT.Makefile
          - packaging/pkgsrc/COMMIT_MSG
          - packaging/pkgsrc/Makefile
          - packaging/pkgsrc/distinfo
          - packaging/pkgsrc/DESCR
          - packaging/pkgsrc/PLIST
        path_map:
          packaging/pkgsrc/ROOT.Makefile: Makefile
          packaging/pkgsrc/COMMIT_MSG: COMMIT_MSG
          packaging/pkgsrc/Makefile: wireshare/Makefile
          packaging/pkgsrc/distinfo: wireshare/distinfo
          packaging/pkgsrc/DESCR: wireshare/DESCR
          packaging/pkgsrc/PLIST: wireshare/PLIST
        destination:
          wip_user: "@pkgsrc_wip_user@"
          repo: "@pkgsrc_wip_user@@wip.pkgsrc.org:/pkgsrc-wip.git"
          base_branch: master
          submission_url: https://example.invalid/pkgsrc
      openbsd:
        driver: mail_submit
        submission_mode: email_attachment
        required_cli:
          - sendmail
        auth_probe:
          - type: git_config
            key: user.name
          - type: git_config
            key: user.email
        headless_capability: full
        submission_paths:
          - packaging/common
          - packaging/openbsd
        required_files:
          - packaging/openbsd/Makefile
          - packaging/openbsd/distinfo
          - packaging/openbsd/pkg/DESCR
          - packaging/openbsd/pkg/PLIST
        destination:
          email: ports@openbsd.org
      mageia:
        driver: bugzilla_api
        submission_mode: bugzilla_request
        headless_capability: full
        submission_paths:
          - packaging/common
          - packaging/mageia
        required_files:
          - packaging/mageia/wireshare.spec
        path_map:
          packaging/mageia/wireshare.spec: wireshare.spec
        destination:
          bugzilla_url: https://bugs.mageia.org
          product: Mageia
          component: New RPM package request
          version: Cauldron
          platform: All
          severity: Enhancement
          summary: wireshare request
          submission_url: https://bugs.mageia.org/enter_bug.cgi?product=Mageia
      freebsd:
        driver: bugzilla_api
        submission_mode: bugzilla_patch
        headless_capability: full
        submission_paths:
          - packaging/common
          - packaging/freebsd
        required_files:
          - packaging/freebsd/Makefile
          - packaging/freebsd/distinfo
          - packaging/freebsd/pkg-descr
          - packaging/freebsd/pkg-plist
        path_map:
          packaging/freebsd/Makefile: net-p2p/wireshare/Makefile
          packaging/freebsd/distinfo: net-p2p/wireshare/distinfo
          packaging/freebsd/pkg-descr: net-p2p/wireshare/pkg-descr
          packaging/freebsd/pkg-plist: net-p2p/wireshare/pkg-plist
        destination:
          bugzilla_url: https://bugs.freebsd.org/bugzilla
          product: Ports & Packages
          component: Individual Port(s)
          version: Latest
          platform: Any
          severity: Affects Only Me
          summary: "[NEW PORT] net-p2p/wireshare"
          submission_url: https://bugs.freebsd.org/bugzilla/enter_bug.cgi?product=Ports%20%26%20Packages
      debian:
        driver: manual_handoff
        submission_mode: manual_packet
        headless_capability: manual
        submission_paths:
          - packaging/debian
        required_files:
          - packaging/debian/control
        destination:
          submission_url: https://example.invalid/debian
    """)

    write_file(root, "packaging/common/app/cx.hermes.WireShare.desktop", """
    [Desktop Entry]
    Name=@release_app_name@
    Icon=@release_app_id@
    """)

    write_file(root, "packaging/common/app/cx.hermes.WireShare.metainfo.xml", """
    <component>
      <id>@release_app_id@</id>
      <url type="homepage">@release_homepage_url@</url>
    </component>
    """)

    write_file(
      root,
      "packaging/common/launchers/WireShare",
      "#!/bin/sh\nexec java -jar /usr/share/wireshare/WireShare.jar \"$@\"\n"
    )

    write_file(
      root,
      "packaging/flathub/cx.hermes.WireShare.yaml",
      "id: @release_app_id@\nsha256: @source_tarball_sha256@\n"
    )

    write_file(root, "packaging/aur/wireshare/PKGBUILD", "pkgver=@release_version@\n")
    write_file(root, "packaging/aur/wireshare/.SRCINFO", "pkgbase = wireshare\n")
    write_file(root, "packaging/aur/wireshare-bin/PKGBUILD", "pkgver=@release_version@\n")
    write_file(root, "packaging/aur/wireshare-bin/.SRCINFO", "pkgbase = wireshare-bin\n")

    write_file(root, "packaging/alpine/APKBUILD", """
    source="$pkgname-$pkgver.tar.gz::https://github.com/@release_repo_slug@/releases/download/release/$pkgver/WireShare-$pkgver-source.tar.gz"
    pkgver=@release_version@
    sha256sums="@source_tarball_sha256@  $pkgname-$pkgver.tar.gz"
    """)

    write_file(root, "packaging/obs/wireshare.spec", "Version: @release_version@\n")
    write_file(root, "packaging/opensuse/wireshare.spec", "Version: @release_version@\n")
    write_file(root, "packaging/copr/wireshare.spec", "Version: @release_version@\n")
    write_file(
      root,
      "packaging/gentoo/metadata.xml",
      "<pkgmetadata><maintainer><email>tester@example.invalid</email></maintainer></pkgmetadata>\n"
    )
    write_file(
      root,
      "packaging/gentoo/wireshare-9.9.ebuild",
      "EAPI=8\nDESCRIPTION=\"WireShare\"\n"
    )
    write_file(
      root,
      "packaging/solus/package.yml",
      "name : wireshare\nversion : 9.9\nrelease : 1\nsummary : WireShare\n"
    )

    write_file(
      root,
      "packaging/homebrew/wireshare.rb",
      "class Wireshare < Formula\n  sha256 \"@source_tarball_sha256@\"\nend\n"
    )

    write_file(
      root,
      "packaging/haiku/wireshare.recipe",
      """
      SUMMARY="WireShare"
      DESCRIPTION="WireShare is a peer-to-peer client for decentralized file sharing. It lets \
      you manage downloads from a desktop application. The Haiku package runs on the system \
      Java runtime."
      HOMEPAGE="https://example.invalid/"
      COPYRIGHT="2026 Hermes"
      LICENSE="GPL v3"
      REVISION="1"
      SOURCE_URI="https://example.invalid/wireshare.jar#noarchive"
      CHECKSUM_SHA256="deadbeef"
      SOURCE_FILENAME="wireshare.jar"

      ARCHITECTURES="x86_64"
      DISABLE_SOURCE_PACKAGE="yes"
      """
    )
    write_file(
      root,
      "packaging/nix/maintainer-entry.nix",
      """
        @nix_maintainer_handle@ = {
          email = "@nix_maintainer_email@";
          github = "@nix_maintainer_github@";
          githubId = @nix_maintainer_github_id@;
          name = "@nix_maintainer_name@";
        };
      """
    )

    write_file(
      root,
      "packaging/nix/package.nix",
      """
      {
        lib,
      }:
      {
        maintainers = with lib.maintainers; [ @nix_maintainer_handle@ ];
        platforms = lib.platforms.linux ++ lib.platforms.darwin;
        wrapperRoot = "$out/share/wireshare";
        version = "@release_version@";
      }
      """
    )

    write_file(root, "packaging/nix/deps.json", "{\"!version\":1}\n")

    write_file(root, "packaging/snap/snapcraft.yaml", "name: wireshare\n")
    write_file(root, "packaging/ppa/launchpad-recipe.txt", "owner=@launchpad_owner@\n")

    write_file(
      root,
      "packaging/ppa/wireshare.recipe",
      "# git-build-recipe format 0.4 deb-version @release_version@+{revtime}-0~ppa1\nlp:~@launchpad_owner@/@launchpad_project@/+git/wireshare\n"
    )

    write_file(
      root,
      "packaging/guix/wireshare.scm",
      "(define-public wireshare '@source_tarball_nix_base32@)\n"
    )

    write_file(root, "packaging/pkgsrc/ROOT.Makefile", "SUBDIR+= wireshare\n")

    write_file(
      root,
      "packaging/pkgsrc/COMMIT_MSG",
      "net/wireshare: import wireshare-@release_version@\n"
    )

    write_file(
      root,
      "packaging/pkgsrc/Makefile",
      "DISTNAME= WireShare-@release_version@-source\n"
    )

    write_file(
      root,
      "packaging/pkgsrc/distinfo",
      "SHA512 (WireShare-@release_version@-source.tar.gz) = @source_tarball_sha512@\nSize (WireShare-@release_version@-source.tar.gz) = @source_tarball_size@ bytes\n"
    )

    write_file(root, "packaging/pkgsrc/DESCR", "WireShare\n")
    write_file(root, "packaging/pkgsrc/PLIST", "bin/WireShare\n")

    write_file(
      root,
      "packaging/openbsd/Makefile",
      "DISTNAME = WireShare-@release_version@-source\n"
    )

    write_file(
      root,
      "packaging/openbsd/distinfo",
      "SHA256 (WireShare-@release_version@-source.tar.gz) = @source_tarball_sha256@\nSIZE (WireShare-@release_version@-source.tar.gz) = @source_tarball_size@\n"
    )

    write_file(root, "packaging/openbsd/pkg/DESCR", "WireShare\n")
    write_file(root, "packaging/openbsd/pkg/PLIST", "bin/WireShare\n")
    write_file(root, "packaging/mageia/wireshare.spec", "Version: @release_version@\n")

    write_file(
      root,
      "packaging/freebsd/Makefile",
      "PORTNAME=\twireshare\nDISTVERSION=\t@release_version@\n"
    )

    write_file(
      root,
      "packaging/freebsd/distinfo",
      "TIMESTAMP = 0\nSHA256 (WireShare-@release_version@-source.tar.gz) = @source_tarball_sha256@\nSIZE (WireShare-@release_version@-source.tar.gz) = @source_tarball_size@\n"
    )

    write_file(root, "packaging/freebsd/pkg-descr", "WireShare\n")
    write_file(root, "packaging/freebsd/pkg-plist", "bin/WireShare\n")
    write_file(root, "packaging/debian/control", "Source: wireshare\n")

    write_file(root, "tools/linux_rollout/scripts/launchpad_helper.py", """
    #!/usr/bin/env python3
    import json
    import sys

    command = sys.argv[1]

    if command == 'auth-login':
        print('Authenticated to Launchpad as Launchpad Tester (~launchpad-tester)')
    elif command == 'auth-logout':
        print('Logged out')
    elif command == 'whoami':
        print(json.dumps({'name': 'launchpad-tester', 'display_name': 'Launchpad Tester'}))
    else:
        print('{}')
    """)

    write_file(root, "WireShare.jar", "jar")
    write_file(root, "build/release-artifacts/WireShare-9.9-source.tar.gz", "src")

    write_file(root, "build/release-artifacts/SHA256SUMS", """
    #{String.duplicate("a", 64)}  WireShare-9.9-source.tar.gz
    #{String.duplicate("b", 64)}  WireShare.jar
    """)

    write_file(root, "build/release-artifacts/DEPENDENCY_INVENTORY.txt", "deps\n")
  end

  defp write_file(root, relative_path, contents) do
    path = Path.join(root, relative_path)
    File.mkdir_p!(Path.dirname(path))
    File.write!(path, contents)
  end

  defp write_executable(root, relative_path, contents) do
    write_file(root, relative_path, contents)
    File.chmod!(Path.join(root, relative_path), 0o755)
  end

  defp prepare_fake_github_repo_env!(root, opts \\ []) do
    run_git!(["config", "--global", "user.name", "Fixture Submitter"], cd: root)
    run_git!(["config", "--global", "user.email", "fixture-submit@example.invalid"], cd: root)

    gentoo_repo =
      seed_github_repo!(
        root,
        "gentoo",
        "master",
        Keyword.get(opts, :gentoo_files, fake_gentoo_repo_files())
      )

    nix_repo =
      seed_github_repo!(
        root,
        "nixpkgs",
        "master",
        Keyword.get(opts, :nix_files, fake_nix_repo_files())
      )

    homebrew_repo =
      seed_github_repo!(
        root,
        "homebrew-wireshare",
        "main",
        Keyword.get(opts, :homebrew_files, fake_homebrew_repo_files())
      )

    pr_log = Path.join(root, "fake-pr-create.log")
    pr_state = Path.join(root, "fake-pr-state.tsv")
    nix_log = Path.join(root, "fake-nix.log")
    fake_bin = Path.join(root, "fake-bin")

    write_executable(
      root,
      "fake-bin/gh",
      """
      #!/bin/sh
      set -eu

      case "$1 $2" in
        "auth status")
          exit 0
          ;;
        "api user")
          echo '{"login":"tester","id":424242}'
          exit 0
          ;;
        "api repos/tester/nixpkgs")
          echo '{"full_name":"tester/nixpkgs"}'
          exit 0
          ;;
        "api repos/tester/gentoo")
          echo '{"full_name":"tester/gentoo"}'
          exit 0
          ;;
        "api repos/tester/homebrew-wireshare")
          echo '{"full_name":"tester/homebrew-wireshare"}'
          exit 0
          ;;
        "repo clone")
          repo="$3"
          dest="$4"

          case "$repo" in
            tester/gentoo)
              exec git clone #{Util.shell_escape(gentoo_repo)} "$dest"
              ;;
            tester/nixpkgs)
              git clone #{Util.shell_escape(nix_repo)} "$dest"
              mkdir -p "$dest/ci"
              printf '%s\\n' \
                '#!/bin/sh' \
                'set -eu' \
                "grep -q '# formatted by fake treefmt' maintainers/maintainer-list.nix" \
                "grep -q '# formatted by fake treefmt' pkgs/by-name/wi/wireshare/package.nix" \
                'git diff --quiet HEAD -- maintainers/maintainer-list.nix pkgs/by-name/wi/wireshare/package.nix' \
                > "$dest/ci/nixpkgs-vet.sh"
              chmod +x "$dest/ci/nixpkgs-vet.sh"
              exit 0
              ;;
            tester/homebrew-wireshare)
              exec git clone #{Util.shell_escape(homebrew_repo)} "$dest"
              ;;
            *)
              echo "unknown fake GitHub repo: $repo" >&2
              exit 1
              ;;
          esac
          ;;
        "pr list")
          repo=""
          head=""
          base=""

          while [ "$#" -gt 0 ]; do
            case "$1" in
              --repo)
                repo="$2"
                shift 2
                ;;
              --head)
                head="$2"
                shift 2
                ;;
              --base)
                base="$2"
                shift 2
                ;;
              *)
                shift
                ;;
            esac
          done

          if [ -f #{Util.shell_escape(pr_state)} ]; then
            while IFS='|' read -r saved_repo saved_head saved_base saved_url; do
              if [ "$saved_repo" = "$repo" ] && [ "$saved_head" = "$head" ] && [ "$saved_base" = "$base" ]; then
                printf '[{"url":"%s"}]\\n' "$saved_url"
                exit 0
              fi
            done < #{Util.shell_escape(pr_state)}
          fi

          echo '[]'
          exit 0
          ;;
        "pr create")
          args_line="$*"
          repo=""
          head=""
          base=""

          while [ "$#" -gt 0 ]; do
            case "$1" in
              --repo)
                repo="$2"
                shift 2
                ;;
              --head)
                head="$2"
                shift 2
                ;;
              --base)
                base="$2"
                shift 2
                ;;
              *)
                shift
                ;;
            esac
          done

          printf '%s\\n' "$args_line" >> #{Util.shell_escape(pr_log)}

          if [ "#{if Keyword.get(opts, :fail_pr_create, false), do: "1", else: "0"}" = "1" ]; then
            echo "gh pr create should not have been called" >&2
            exit 1
          fi

          url="https://github.com/$repo/pull/1"
          printf '%s|%s|%s|%s\\n' "$repo" "$head" "$base" "$url" >> #{Util.shell_escape(pr_state)}
          exit 0
          ;;
      esac

      echo "unsupported fake gh invocation: $*" >&2
      exit 1
      """
    )

    write_executable(
      root,
      "fake-bin/nix-shell",
      """
      #!/bin/sh
      set -eu

      if [ "$1" = "--run" ]; then
        printf '%s\\n' '# formatted by fake treefmt' >> maintainers/maintainer-list.nix
        printf '%s\\n' '# formatted by fake treefmt' >> pkgs/by-name/wi/wireshare/package.nix
      fi
      """
    )

    write_executable(
      root,
      "fake-bin/nix",
      """
      #!/bin/sh
      set -eu

      printf '%s\\n' "$*" >> #{Util.shell_escape(nix_log)}

      for arg in "$@"; do
        if [ "$arg" = "eval" ]; then
          echo /nix/store/fake-wireshare.drv
          exit 0
        fi

        if [ "$arg" = "build" ]; then
          exit 0
        fi
      done

      exit 0
      """
    )

    write_executable(root, "fake-bin/nix-build", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-bin/ssh", "#!/bin/sh\nexit 1\n")

    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")
    %{pr_log: pr_log, nix_log: nix_log}
  end

  defp prepare_fake_guru_repo_env!(root, opts \\ []) do
    put_global_git_config!(root, "user.name", "Fixture Submitter")
    put_global_git_config!(root, "user.email", "fixture-submit@example.invalid")
    put_global_git_config!(root, "user.signingkey", Keyword.get(opts, :signingkey, "ABCDEF1234567890"))
    put_global_git_config!(
      root,
      "commit.gpgsign",
      bool_string(Keyword.get(opts, :commit_gpgsign, false))
    )

    put_global_git_config!(
      root,
      "push.gpgsign",
      bool_string(Keyword.get(opts, :push_gpgsign, false))
    )

    guru_repo =
      seed_github_repo!(
        root,
        "guru",
        "dev",
        Keyword.get(opts, :guru_files, fake_guru_repo_files())
      )

    pkgcheck_log = Path.join(root, "fake-pkgcheck.log")
    pkgdev_log = Path.join(root, "fake-pkgdev.log")
    fake_bin = Path.join(root, "fake-bin")
    ssh_exit = if Keyword.get(opts, :ssh_access, true), do: "1", else: "255"

    write_executable(
      root,
      "fake-bin/pkgcheck",
      """
      #!/bin/sh
      set -eu
      printf '%s\\n' "$*" >> #{Util.shell_escape(pkgcheck_log)}
      exit 0
      """
    )

    write_executable(
      root,
      "fake-bin/pkgdev",
      """
      #!/bin/sh
      set -eu
      printf '%s\\n' "$*" >> #{Util.shell_escape(pkgdev_log)}

      if [ "$1" = "push" ]; then
        branch="$(git branch --show-current)"
        git push --set-upstream origin HEAD:"$branch" >/dev/null
        exit 0
      fi

      echo "unsupported fake pkgdev invocation: $*" >&2
      exit 1
      """
    )

    write_executable(root, "fake-bin/gpg", "#!/bin/sh\nexit 0\n")
    write_executable(root, "fake-bin/ssh", "#!/bin/sh\nexit #{ssh_exit}\n")

    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")
    %{guru_repo: guru_repo, pkgcheck_log: pkgcheck_log, pkgdev_log: pkgdev_log}
  end

  defp prepare_fake_gitlab_repo_env!(root) do
    fake_bin = Path.join(root, "fake-bin")
    abuild_log = Path.join(root, "fake-abuild.log")
    aports_repo = seed_github_repo!(root, "aports", "master", %{"community/.keep" => "\n"})

    write_executable(
      root,
      "fake-bin/glab",
      """
      #!/bin/sh
      set -eu

      case "$1 $2" in
        "auth status")
          exit 0
          ;;
        "repo clone")
          repo="$3"
          dest="$4"

          case "$repo" in
            tester/aports|alpine/aports)
              exec git clone #{Util.shell_escape(aports_repo)} "$dest"
              ;;
            *)
              echo "unknown fake GitLab repo: $repo" >&2
              exit 1
              ;;
          esac
          ;;
      esac

      echo "unsupported fake glab invocation: $*" >&2
      exit 1
      """
    )

    write_executable(
      root,
      "fake-bin/abuild",
      """
      #!/bin/sh
      set -eu

      printf '%s\\n' "$*" >> #{Util.shell_escape(abuild_log)}

      if [ "$#" -eq 1 ] && [ "$1" = "checksum" ]; then
        awk '
          /^sha256sums=/ {
            print "sha256sums=\\"#{String.duplicate("c", 64)}  $pkgname-$pkgver.tar.gz\\""
            next
          }
          { print }
        ' APKBUILD > APKBUILD.tmp
        mv APKBUILD.tmp APKBUILD
        exit 0
      fi

      echo "unsupported fake abuild invocation: $*" >&2
      exit 1
      """
    )

    System.put_env("PATH", "#{fake_bin}:#{System.get_env("PATH")}")
    %{aports_repo: aports_repo, abuild_log: abuild_log}
  end

  defp seed_github_repo!(root, name, branch, extra_files) do
    repo_root = Path.join(root, "seed/#{name}")
    bare_root = Path.join(root, "remotes/#{name}.git")

    File.rm_rf!(repo_root)
    File.rm_rf!(bare_root)
    File.mkdir_p!(repo_root)

    run_git!(["init"], cd: repo_root)
    run_git!(["config", "user.name", "Fixture Tester"], cd: repo_root)
    run_git!(["config", "user.email", "fixture@example.invalid"], cd: repo_root)
    run_git!(["checkout", "-B", branch], cd: repo_root)

    write_file(Path.join(root, "seed/#{name}"), "README.md", "#{name}\n")

    Enum.each(extra_files, fn {relative_path, contents} ->
      write_file(Path.join(root, "seed/#{name}"), relative_path, contents)

      if String.ends_with?(relative_path, ".sh") do
        File.chmod!(Path.join(repo_root, relative_path), 0o755)
      end
    end)

    run_git!(["add", "-A"], cd: repo_root)
    run_git!(["-c", "commit.gpgsign=false", "commit", "-m", "Initial fixture"], cd: repo_root)
    run_git!(["clone", "--bare", repo_root, bare_root], cd: root)
    bare_root
  end

  defp fake_nix_repo_files do
    %{
      ".git-blame-ignore-revs" => "\n",
      ".github/PULL_REQUEST_TEMPLATE.md" => "## Things done\n\n- [ ] Built on platform:\n",
      "maintainers/maintainer-list.nix" => """
      {
        naelstrof = {
          github = "naelstrof";
          githubId = 1;
          name = "Nael Strof";
        };

        ocharles = {
          github = "ocharles";
          githubId = 2;
          name = "Oliver Charles";
        };
      }
      """,
      "ci/nixpkgs-vet.sh" => """
      #!/bin/sh
      set -eu
      grep -q '# formatted by fake treefmt' maintainers/maintainer-list.nix
      grep -q '# formatted by fake treefmt' pkgs/by-name/wi/wireshare/package.nix
      git diff --quiet HEAD -- maintainers/maintainer-list.nix pkgs/by-name/wi/wireshare/package.nix
      """
    }
  end

  defp fake_gentoo_repo_files do
    %{
      "net-p2p/wireshare/.keep" => "\n"
    }
  end

  defp fake_guru_repo_files do
    %{}
  end

  defp fake_homebrew_repo_files do
    %{
      ".github/pull_request_template.md" => "### Checklist\n\n- [ ] My PR follows Homebrew style.\n",
      "Formula/.keep" => "\n"
    }
  end

  defp rollout_script_path do
    Path.expand("../../../scripts/release_linux_rollout.sh", __DIR__)
  end

  defp run_git!(args, opts) do
    case System.cmd("git", args, Keyword.put_new(opts, :stderr_to_stdout, true)) do
      {_output, 0} -> :ok
      {output, status} -> raise "git #{Enum.join(args, " ")} failed with exit #{status}: #{output}"
    end
  end

  defp put_global_git_config!(root, key, nil) do
    System.cmd("git", ["config", "--global", "--unset-all", key], cd: root, stderr_to_stdout: true)
    :ok
  end

  defp put_global_git_config!(root, key, value) do
    System.cmd("git", ["config", "--global", "--unset-all", key], cd: root, stderr_to_stdout: true)
    run_git!(["config", "--global", key, value], cd: root)
  end

  defp bool_string(true), do: "true"
  defp bool_string(false), do: "false"
end
