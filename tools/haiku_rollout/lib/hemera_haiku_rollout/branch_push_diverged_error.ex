defmodule HemeraHaikuRollout.BranchPushDivergedError do
  defexception [
    :message,
    :branch,
    :checkout_path,
    :local_sha,
    :tracked_remote_sha,
    :remote_sha,
    :pr_number,
    :pr_url,
    :recovery_commands
  ]
end
