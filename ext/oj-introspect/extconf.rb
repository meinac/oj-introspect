require "mkmf"
require "bundler"

oj_gemspec = Bundler.definition.specs.find { _1.name == "oj" }

OJ_HEADERS  = oj_gemspec.full_gem_path + "/ext/oj"

dir_config('oj', [OJ_HEADERS], [])

create_makefile("oj-introspect/introspect_ext")
