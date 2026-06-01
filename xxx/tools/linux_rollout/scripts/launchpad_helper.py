#!/usr/bin/env python3

import argparse
import json
import os
import shutil
import sys

from launchpadlib.launchpad import Launchpad


APP_NAME = "wireshare-linux-rollout"


def _launchpad_dir() -> str:
    return os.environ.get("LINUX_ROLLOUT_LAUNCHPAD_DIR", os.path.expanduser("~/.cache/linux_rollout/launchpadlib"))


def _credentials_file() -> str:
    return os.environ.get(
        "LINUX_ROLLOUT_LAUNCHPAD_CREDENTIALS",
        os.path.expanduser("~/.local/share/linux_rollout/launchpad/credentials"),
    )


def _login() -> Launchpad:
    os.makedirs(_launchpad_dir(), exist_ok=True)
    os.makedirs(os.path.dirname(_credentials_file()), exist_ok=True)
    return Launchpad.login_with(
        APP_NAME,
        "production",
        version="devel",
        launchpadlib_dir=_launchpad_dir(),
        credentials_file=_credentials_file(),
    )


def _load_payload(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def _dump(result: dict) -> None:
    json.dump(result, sys.stdout, sort_keys=True)
    sys.stdout.write("\n")


def _load_existing_snap(lp: Launchpad, owner_name: str, snap_name: str):
    try:
        return lp.load(f"~{owner_name}/+snap/{snap_name}")
    except Exception:
        return None


def _load_existing_recipe(owner, recipe_name: str):
    try:
        return owner.getRecipe(name=recipe_name)
    except Exception:
        return None


def _load_existing_ppa(owner, ubuntu, ppa_name: str):
    try:
        return owner.getPPAByName(distribution=ubuntu, name=ppa_name)
    except Exception:
        return None


def _save_if_changed(resource, values: dict) -> None:
    changed = False

    for key, value in values.items():
        if value in (None, ""):
            continue

        try:
            current = getattr(resource, key)
        except Exception:
            continue

        if current != value:
            try:
                setattr(resource, key, value)
            except Exception:
                continue

            changed = True

    if changed:
        resource.lp_save()


def _ensure_code_import(lp: Launchpad, payload: dict):
    owner = lp.people[payload["owner"]]
    project = lp.projects[payload["launchpad_project"]]
    repo_name = payload["import_repo_name"]
    repo_path = f"~{payload['owner']}/{payload['launchpad_project']}/+git/{repo_name}"

    try:
        repository = lp.git_repositories.getByPath(path=repo_path)
    except Exception:
        code_import = project.newCodeImport(
            branch_name=repo_name,
            owner=owner,
            rcs_type="Git",
            target_rcs_type="Git",
            url=payload["import_source_url"],
        )

        return {
            "status": "awaiting_import_approval",
            "code_import_web_link": getattr(code_import, "web_link", ""),
            "import_path": repo_path,
        }, None, owner

    code_import = getattr(repository, "code_import_link", None)

    if code_import is None:
        return {
            "status": "awaiting_remote_review",
            "repository_web_link": getattr(repository, "web_link", ""),
            "import_path": repo_path,
        }, repository, owner

    review_status = getattr(code_import, "review_status", "") or ""
    date_last_successful = getattr(code_import, "date_last_successful", None)

    if review_status and review_status != "Reviewed":
        return {
            "status": "awaiting_import_approval",
            "review_status": review_status,
            "code_import_web_link": getattr(code_import, "web_link", ""),
            "import_path": repo_path,
        }, repository, owner

    if date_last_successful in (None, ""):
        return {
            "status": "awaiting_import_sync",
            "code_import_web_link": getattr(code_import, "web_link", ""),
            "repository_web_link": getattr(repository, "web_link", ""),
            "import_path": repo_path,
        }, repository, owner

    return {
        "status": "ok",
        "code_import_web_link": getattr(code_import, "web_link", ""),
        "repository_web_link": getattr(repository, "web_link", ""),
        "import_path": repo_path,
    }, repository, owner


def _submit_snap(payload: dict) -> dict:
    lp = _login()
    import_state, repository, owner = _ensure_code_import(lp, payload)

    if import_state["status"] != "ok":
        return import_state

    snap = _load_existing_snap(lp, payload["owner"], payload["snap_name"])

    if snap is None:
        snap = lp.snaps.new(
            name=payload["snap_name"],
            owner=owner,
            git_repository_url=repository,
            git_path=payload["import_branch"],
        )
    else:
        _save_if_changed(
            snap,
            {
                "git_repository_url": repository,
                "git_path": payload["import_branch"],
            },
        )

    ubuntu_archive = lp.distributions["ubuntu"].main_archive
    request_kwargs = {
        "archive": ubuntu_archive.self_link,
        "pocket": payload["pocket"],
    }

    if payload.get("channels"):
        request_kwargs["channels"] = payload["channels"]

    build_request = snap.requestBuilds(**request_kwargs)

    return {
        "status": "submitted",
        "snap_web_link": getattr(snap, "web_link", ""),
        "build_request_web_link": getattr(build_request, "web_link", ""),
        "import_path": import_state["import_path"],
    }


def _submit_ppa(payload: dict) -> dict:
    lp = _login()
    import_state, repository, owner = _ensure_code_import(lp, payload)

    if import_state["status"] != "ok":
        return import_state

    ubuntu = lp.distributions["ubuntu"]
    distroseries = ubuntu.getSeries(name_or_version=payload["ubuntu_series"])
    ppa = _load_existing_ppa(owner, ubuntu, payload["ppa_name"])

    if ppa is None:
        ppa = owner.createPPA(
            name=payload["ppa_name"],
            displayname=payload["ppa_display_name"],
            description=payload["ppa_description"],
            distribution=ubuntu,
            private=False,
            suppress_subscription_notifications=True,
        )

    recipe = _load_existing_recipe(owner, payload["recipe_name"])

    if recipe is None:
        recipe = owner.createRecipe(
            name=payload["recipe_name"],
            description=payload["recipe_description"],
            distroseries=distroseries,
            recipe_text=payload["recipe_text"],
            build_daily=False,
            daily_build_archive=ppa,
        )
    else:
        _save_if_changed(
            recipe,
            {
                "description": payload["recipe_description"],
                "recipe_text": payload["recipe_text"],
                "distroseries": distroseries,
                "build_daily": False,
                "daily_build_archive": ppa,
            },
        )

    build = recipe.requestBuild(
        archive=ppa,
        distroseries=distroseries,
        pocket=payload["pocket"],
    )

    return {
        "status": "submitted",
        "recipe_web_link": getattr(recipe, "web_link", ""),
        "build_web_link": getattr(build, "web_link", ""),
        "ppa_web_link": getattr(ppa, "web_link", ""),
        "import_path": import_state["import_path"],
        "repository_web_link": getattr(repository, "web_link", ""),
    }


def _auth_login() -> int:
    lp = _login()
    me = getattr(lp, "me", None)
    name = getattr(me, "name", "unknown")
    display_name = getattr(me, "display_name", name)
    sys.stdout.write(f"Authenticated to Launchpad as {display_name} (~{name})\n")
    return 0


def _whoami() -> int:
    lp = _login()
    me = getattr(lp, "me", None)
    name = getattr(me, "name", "unknown")
    display_name = getattr(me, "display_name", name)
    _dump({"name": name, "display_name": display_name})
    return 0


def _auth_logout() -> int:
    credentials = _credentials_file()
    cache_dir = _launchpad_dir()
    removed = []

    if os.path.exists(credentials):
        os.remove(credentials)
        removed.append(credentials)

    if os.path.isdir(cache_dir):
        shutil.rmtree(cache_dir)
        removed.append(cache_dir)

    if removed:
        sys.stdout.write("Removed Launchpad auth state:\n")
        for path in removed:
            sys.stdout.write(f"- {path}\n")
    else:
        sys.stdout.write("No Launchpad auth state was present.\n")

    return 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("auth-login")
    subparsers.add_parser("auth-logout")
    subparsers.add_parser("whoami")

    snap_parser = subparsers.add_parser("submit-snap")
    snap_parser.add_argument("payload")

    ppa_parser = subparsers.add_parser("submit-ppa")
    ppa_parser.add_argument("payload")

    args = parser.parse_args(argv)

    if args.command == "auth-login":
        return _auth_login()

    if args.command == "auth-logout":
        return _auth_logout()

    if args.command == "whoami":
        return _whoami()

    if args.command == "submit-snap":
        _dump(_submit_snap(_load_payload(args.payload)))
        return 0

    if args.command == "submit-ppa":
        _dump(_submit_ppa(_load_payload(args.payload)))
        return 0

    raise ValueError(f"unsupported command {args.command}")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
