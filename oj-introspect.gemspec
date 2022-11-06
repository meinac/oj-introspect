require_relative 'lib/oj/introspect/version'

Gem::Specification.new do |spec|
  spec.name          = "oj-introspect"
  spec.version       = Oj::Introspect::VERSION
  spec.authors       = ["Mehmet Emin INAC"]
  spec.email         = ["mehmetemininac@gmail.com"]

  spec.summary       = "Oj introspect parser."
  spec.description   = "Embeds start and end byte offsets of JSON objects into generated Ruby hashes."
  spec.homepage      = "https://github.com/meinac/oj-introspect"
  spec.license       = "MIT"
  spec.required_ruby_version = Gem::Requirement.new(">= 2.3.0")

  # spec.metadata["homepage_uri"] = spec.homepage

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files         = Dir.chdir(File.expand_path('..', __FILE__)) do
    `git ls-files -z`.split("\x0").reject { |f| f.match(%r{^(test|spec|features)/}) }
  end
  spec.bindir        = "exe"
  spec.executables   = spec.files.grep(%r{^exe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions    = ["ext/oj-introspect/extconf.rb"]

  spec.add_dependency "oj", ">=3.13.23"
end
