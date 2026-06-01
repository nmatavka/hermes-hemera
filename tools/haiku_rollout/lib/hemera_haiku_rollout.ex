defmodule HemeraHaikuRollout do
  @tool_root Path.expand("..", __DIR__)
  @repo_root Path.expand("../..", @tool_root)
  @managed_share_dir Path.join(System.user_home!(), ".local/share/hemera_haiku_rollout")

  def tool_root do
    @tool_root
  end

  def repo_root do
    @repo_root
  end

  def managed_share_dir do
    System.get_env("HEMERA_HAIKU_ROLLOUT_SHARE_DIR") || @managed_share_dir
  end

  def default_config_path do
    System.get_env("HEMERA_HAIKU_ROLLOUT_CONFIG") || Path.join(managed_share_dir(), "config.yml")
  end

  def example_config_path do
    Path.join(@tool_root, "config.example.yml")
  end

  def default_manifest_path(root \\ repo_root()) do
    Path.join(root, "packaging/haiku/release_manifest.yml")
  end

  def recipe_template_dir(root \\ repo_root()) do
    Path.join(root, "packaging/haikuports/mail-client/hemera")
  end

  def recipe_template_path(root \\ repo_root()) do
    Path.join(recipe_template_dir(root), "hemera.recipe.in")
  end
end
