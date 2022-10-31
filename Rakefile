require "bundler/gem_tasks"
require "rspec/core/rake_task"
require "rake/extensiontask"

RSpec::Core::RakeTask.new(:spec)

task default: [:compile, :spec]

Rake::ExtensionTask.new "oj-introspect" do |ext|
  ext.name = "introspect_ext"
  ext.lib_dir = "lib/oj/introspect"
end
