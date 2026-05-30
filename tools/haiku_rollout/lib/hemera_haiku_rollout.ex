defmodule HemeraHaikuRollout do
  @tool_root Path.expand("..", __DIR__)
  @repo_root Path.expand("../..", @tool_root)
  @template_dir Path.join(@repo_root, "packaging/haikuports/mail-client/hemera")

  def tool_root do
    @tool_root
  end

  def repo_root do
    @repo_root
  end

  def default_config_path do
    System.get_env("HEMERA_HAIKU_ROLLOUT_CONFIG") || Path.join(@tool_root, "config.yml")
  end

  def example_config_path do
    Path.join(@tool_root, "config.example.yml")
  end

  def recipe_template_dir do
    @template_dir
  end

  def recipe_template_path do
    Path.join(@template_dir, "hemera.recipe.in")
  end
end
